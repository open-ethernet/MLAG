/* Copyright (c) 2014  Mellanox Technologies, Ltd. All rights reserved.
 *
 * This software is available to you under BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define PORT_MANAGER_C_
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mlag_api_defs.h>
#include <oes_types.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_defs.h>
#include <errno.h>
#include <libs/mlag_manager/mlag_manager.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/health_manager/health_manager.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include "lib_commu.h"
#include "mlag_common.h"
#include "mlag_comm_layer_wrapper.h"
#include "port_db.h"
#include "port_manager.h"
#include <libs/mlag_manager/mlag_dispatcher.h>
#include "service_layer.h"

#define IS_MASTER() (current_role != SLAVE)


/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
/*static struct port_manager_counters counters;*/
static int current_role;
static uint8_t my_mlag_id;
static int in_split_brain;
static int started;
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;


static handler_command_t port_manager_ibc_msgs[] = {
    {MLAG_PORT_GLOBAL_STATE_EVENT, "Port global state change event",
     rcv_msg_handler, net_order_msg_handler },
    {MLAG_PORTS_SYNC_DATA, "Port sync message", rcv_msg_handler,
     net_order_msg_handler },
    {MLAG_PORTS_UPDATE_EVENT, "Port update message", rcv_msg_handler,
     net_order_msg_handler },
    {MLAG_PORTS_SYNC_FINISH_EVENT, "Port sync done", rcv_msg_handler,
     net_order_msg_handler },
    {MLAG_PORTS_OPER_UPDATE, "Ports oper update message", rcv_msg_handler,
     net_order_msg_handler},
    {MLAG_PORTS_OPER_SYNC_DONE, "Port oper state sync done", rcv_msg_handler,
     net_order_msg_handler },
    {MLAG_PEER_PORT_OPER_STATE_CHANGE, "Port state change event",
     rcv_msg_handler, net_order_msg_handler},

    {0, "", NULL, NULL}
};
/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/
/*
 *  This function is used for sending messages with counter
 *  with implementation of counting mechanism
 *
 * @param[in] opcode - message opcode
 * @param[in] payload - pointer to message data
 * @param[in] payload_len - message data length
 * @param[in] dest_peer_id - mlag id of dest peer
 * @param[in] orig - message originator
 *
 * @return 0 if operation completes successfully.
 */
static int
port_manager_message_send(enum mlag_events opcode, void* payload,
                          uint32_t payload_len, uint8_t dest_peer_id,
                          enum message_originator orig)
{
    int err = 0;

    err = mlag_dispatcher_message_send(opcode, payload, payload_len,
                                       dest_peer_id, orig);
    MLAG_BAIL_CHECK_NO_MSG(err);

    if ((dest_peer_id != my_mlag_id) ||
        ((orig == PEER_MANAGER) && (current_role == SLAVE))) {
        /* increment IPL counter */
        port_db_counters_inc(PM_CNT_PROTOCOL_TX);
    }

bail:
    return err;
}

/*
 *  This function sets state bit vector according to peer id new state
 *
 * @param[in] state_vector - bit vector field
 * @param[in] peer_id - peer index
 * @param[in] state - peer state
 *
 * @return 0 if operation completes successfully.
 */
static void
port_peer_state_set(uint32_t *state_vector, int peer_id, uint32_t state)
{
    uint32_t idx = peer_id;
    *state_vector &= ~(1 << idx);
    *state_vector |= ((state & 1) << peer_id);
}

/*
 *  This function gets state bit vector according to peer id new state
 *
 * @param[in] state_vector - bit vector field
 * @param[in] peer_id - peer index
 *
 * @return TRUE if bit is set
 * @return FALSE otherwise
 */
static int
port_peer_state_get(uint32_t state_vector, int peer_id)
{
    uint32_t idx = peer_id;
    if ((state_vector & (1 << idx))) {
        return TRUE;
    }
    return FALSE;
}

/*
 * This function is a logging cb for FSM
 *
 * @param[in] buf - message content
 * @param[in] len - message len
 *
 * @return 0 if operation completes successfully.
 */
static void
ports_local_fsm_user_trace(char *buf, int len)
{
    UNUSED_PARAM(len);
    MLAG_LOG(MLAG_LOG_INFO, "local %s\n", buf);
}

/*
 * This function is a logging cb for FSM
 *
 * @param[in] buf - message content
 * @param[in] len - message len
 *
 * @return 0 if operation completes successfully.
 */
static void
ports_remote_fsm_user_trace(char *buf, int len)
{
    UNUSED_PARAM(len);
    MLAG_LOG(MLAG_LOG_INFO, "remote %s\n", buf);
}

/*
 * This function is a logging cb for FSM
 *
 * @param[in] buf - message content
 * @param[in] len - message len
 *
 * @return 0 if operation completes successfully.
 */
static void
ports_master_fsm_user_trace(char *buf, int len)
{
    UNUSED_PARAM(len);
    MLAG_LOG(MLAG_LOG_INFO, "master %s\n", buf);
}

/*
 *  This function send sync end notification.
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
static int
notify_peer_sync_done(int mlag_peer_id)
{
    int err = 0;
    struct sync_event_data peer_sync;

    /* Notify sync done */
    peer_sync.peer_id = mlag_peer_id;
    peer_sync.state = TRUE;
    peer_sync.sync_type = PORT_PEER_SYNC;
    err = port_manager_message_send(MLAG_PORTS_SYNC_FINISH_EVENT,
                                    &peer_sync, sizeof(peer_sync),
                                    mlag_peer_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync finish message \n");

bail:
    return err;
}

/*
 *  This function handles peer port up notification
 *  this message comes as a notification from peer (local and
 *  remote)
 *
 * @param[in] peer_id - peer index
 * @param[in] mlag_port - port id
 *
 * @return 0 if operation completes successfully.
 */
static int
port_manager_peer_port_up(int peer_id, unsigned long mlag_port)
{
    int err = 0;
    struct mlag_port_data *port_info;
    ASSERT(peer_id < MLAG_MAX_PEERS);

    err = port_db_entry_lock(mlag_port, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "port [%lu] not found in DB\n", mlag_port);
    /* Update DB */
    port_peer_state_set(&(port_info->peers_oper_state), peer_id, TRUE);

    /* Update logic */
    if (peer_id != mlag_manager_db_local_peer_id_get()) {
        err = port_peer_remote_peer_port_up_ev(&port_info->peer_remote_fsm,
                                               peer_id);
        if (err) {
            port_db_entry_unlock(mlag_port);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] state change to peer remote FSM\n",
                                mlag_port);
        }
    }

    if (IS_MASTER()) {
        err =
            port_master_logic_port_up_ev(&port_info->port_master_fsm, peer_id);
        if (err) {
            port_db_entry_unlock(mlag_port);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] state change to master FSM\n",
                                mlag_port);
        }
    }

    err = port_db_entry_unlock(mlag_port);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function handles peer port down notification
 *  this message comes as a notification from peer (local and
 *  remote)
 *
 * @param[in] peer_id - peer index
 * @param[in] mlag_port - port id
 *
 * @return 0 if operation completes successfully.
 */
static int
port_manager_peer_port_down(int peer_id, unsigned long mlag_port)
{
    int err = 0;
    struct mlag_port_data *port_info;
    ASSERT(peer_id < MLAG_MAX_PEERS);

    err = port_db_entry_lock(mlag_port, &port_info);
    MLAG_BAIL_ERROR_MSG(err, " port [%lu] not found in DB\n", mlag_port);
    /* Update DB */
    port_peer_state_set(&(port_info->peers_oper_state), peer_id, FALSE);

    /* Update logic */
    if (peer_id != mlag_manager_db_local_peer_id_get()) {
        err = port_peer_remote_peer_port_down_ev(&port_info->peer_remote_fsm,
                                                 peer_id);
        if (err) {
            port_db_entry_unlock(mlag_port);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] state change to peer remote FSM\n",
                                mlag_port);
        }
    }

    if (IS_MASTER()) {
        err = port_master_logic_port_down_ev(&port_info->port_master_fsm,
                                             peer_id);
        if (err) {
            port_db_entry_unlock(mlag_port);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] state change to master FSM\n",
                                mlag_port);
        }
    }

    err = port_db_entry_unlock(mlag_port);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function is called to handle IBC message
 *
 * @param[in] data - message body
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
rcv_msg_handler(uint8_t *data)
{
    int err = 0;
    uint32_t idx;
    int local_peer_index;
    struct recv_payload_data *payload_data = (struct recv_payload_data*) data;
    uint16_t opcode;
    struct port_global_state_event_data *port_states;
    struct port_oper_state_change_data *oper_change;
    struct peer_port_sync_message *port_sync;
    struct peer_port_oper_sync_message *oper_sync;
    struct sync_event_data *sync_event;

    opcode = *((uint16_t*)(payload_data->payload[0]));

    if (started == FALSE) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "port manager rcv_msg_handler: opcode=%d\n", opcode);

    port_db_counters_inc(PM_CNT_PROTOCOL_RX);

    switch (opcode) {
    case MLAG_PORT_GLOBAL_STATE_EVENT:
        port_states =
            (struct port_global_state_event_data *)payload_data->payload[0];
        err = port_manager_global_oper_state_set(port_states->port_id,
                                                 port_states->state,
                                                 port_states->port_num);
        MLAG_BAIL_ERROR_MSG(err, "Failed to handle ports global state\n");
        break;
    case MLAG_PORTS_SYNC_DATA:
        port_sync = (struct peer_port_sync_message *)payload_data->payload[0];
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "port manager sync data received: del_ports [%d] ports_num [%u]\n",
                 port_sync->del_ports, port_sync->port_num);

        err = port_manager_port_sync(port_sync);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling port sync data\n");
        break;
    case MLAG_PORTS_UPDATE_EVENT:
        port_sync = (struct peer_port_sync_message *)payload_data->payload[0];
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "port manager sync data received: del_ports [%d] ports_num [%u]\n",
                 port_sync->del_ports, port_sync->port_num);
        err = port_manager_port_update(port_sync);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling ports update event\n");
        break;
    case MLAG_PORTS_OPER_UPDATE:
        oper_sync =
            (struct peer_port_oper_sync_message*) payload_data->payload[0];

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "port manager oper sync data: ports_num [%u] \n",
                 oper_sync->port_num);

        err = mlag_manager_db_local_index_from_mlag_id_get(oper_sync->mlag_id,
                                                           &local_peer_index);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling ports oper update\n");

        for (idx = 0; idx < oper_sync->port_num; idx++) {
            if (oper_sync->oper_state[idx] == MLAG_PORT_OPER_UP) {
                err = port_manager_peer_port_up(local_peer_index,
                                                oper_sync->port_id[idx]);
            }
            else {
                err =
                    port_manager_peer_port_down(local_peer_index,
                                                oper_sync->port_id[idx]);
            }
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed handling port [%lu] state update\n",
                                oper_sync->port_id[idx]);
        }
        break;
    case MLAG_PORTS_OPER_SYNC_DONE:
        err = port_manager_oper_sync_done();
        MLAG_BAIL_ERROR_MSG(err, "Failed handling port oper sync done msg\n");
        break;
    case MLAG_PORTS_SYNC_FINISH_EVENT:
        sync_event = (struct sync_event_data*)payload_data->payload[0];
        err = port_manager_sync_finish(sync_event);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling port sync finish msg\n");
        break;
    case MLAG_PEER_PORT_OPER_STATE_CHANGE:
        oper_change =
            (struct port_oper_state_change_data*) payload_data->payload[0];
        ASSERT(oper_change->is_ipl == FALSE);

        err = port_manager_peer_oper_state_change(oper_change);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling peer oper state change\n");
        break;
    default:
        /* Unknown opcode */
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Unknown opcode [%u] in port manager\n",
                            opcode);
        break;
    }

bail:
    return err;
}

/*
 *  Convert struct between network and host order
 *
 * @param[in] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_port_global_state_event_data(
    struct port_global_state_event_data *data, int oper)
{
    int idx;
    if (oper == MESSAGE_SENDING) {
        for (idx = 0; idx < MLAG_MAX_PORTS; idx++) {
            data->port_id[idx] = htonl(data->port_id[idx]);
            data->state[idx] = htonl(data->state[idx]);
        }
        data->port_num = htonl(data->port_num);
    }
    else {
        for (idx = 0; idx < MLAG_MAX_PORTS; idx++) {
            data->port_id[idx] = ntohl(data->port_id[idx]);
            data->state[idx] = ntohl(data->state[idx]);
        }
        data->port_num = ntohl(data->port_num);
    }
}

/*
 *  Convert struct between network and host order
 *
 * @param[in] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_peer_port_sync_message(struct peer_port_sync_message *data, int oper)
{
    int idx;
    if (oper == MESSAGE_SENDING) {
        data->del_ports = htonl(data->del_ports);
        data->mlag_id = htonl(data->mlag_id);
        for (idx = 0; idx < MLAG_MAX_PORTS; idx++) {
            data->port_id[idx] = htonl(data->port_id[idx]);
        }
        data->port_num = htonl(data->port_num);
    }
    else {
        data->del_ports = ntohl(data->del_ports);
        data->mlag_id = ntohl(data->mlag_id);
        for (idx = 0; idx < MLAG_MAX_PORTS; idx++) {
            data->port_id[idx] = ntohl(data->port_id[idx]);
        }
        data->port_num = ntohl(data->port_num);
    }
}

/**
 *  Convert struct between network and host order
 *
 * @param[in] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_peer_port_oper_sync_message(struct peer_port_oper_sync_message *data,
                                      int oper)
{
    int idx;
    if (oper == MESSAGE_SENDING) {
        data->mlag_id = htonl(data->mlag_id);
        for (idx = 0; idx < MLAG_MAX_PORTS; idx++) {
            data->port_id[idx] = htonl(data->port_id[idx]);
            data->oper_state[idx] = htonl(data->oper_state[idx]);
        }
        data->port_num = htonl(data->port_num);
    }
    else {
        data->mlag_id = ntohl(data->mlag_id);
        for (idx = 0; idx < MLAG_MAX_PORTS; idx++) {
            data->port_id[idx] = ntohl(data->port_id[idx]);
            data->oper_state[idx] = ntohl(data->oper_state[idx]);
        }
        data->port_num = ntohl(data->port_num);
    }
}

/**
 *  Convert struct between network and host order
 *
 * @param[in] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_sync_event_data(struct sync_event_data *data, int oper)
{
    if (oper == MESSAGE_SENDING) {
        data->peer_id = htonl(data->peer_id);
        data->state = htonl(data->state);
        data->sync_type = htonl(data->sync_type);
    }
    else {
        data->peer_id = ntohl(data->peer_id);
        data->state = ntohl(data->state);
        data->sync_type = ntohl(data->sync_type);
    }
}

/**
 *  Convert struct between network and host order
 *
 * @param[in] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_port_oper_state_change_data(struct port_oper_state_change_data *data,
                                      int oper)
{
    if (oper == MESSAGE_SENDING) {
        data->mlag_id = htonl(data->mlag_id);
        data->state = htonl(data->state);
        data->is_ipl = htonl(data->is_ipl);
        data->port_id = htonl(data->port_id);
    }
    else {
        data->mlag_id = ntohl(data->mlag_id);
        data->state = ntohl(data->state);
        data->is_ipl = ntohl(data->is_ipl);
        data->port_id = ntohl(data->port_id);
    }
}

/*
 *  This function is called to handle network order for IBC message
 *
 * @param[in] payload_data - message body
 * @param[in] oper - on send or receive
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
net_order_msg_handler(uint8_t *data, int oper)
{
    int err = 0;
    uint16_t opcode;
    struct port_global_state_event_data *port_states;
    struct port_oper_state_change_data *oper_change;
    struct peer_port_sync_message *port_sync;
    struct peer_port_oper_sync_message *oper_sync;
    struct sync_event_data *sync_event;

    if (oper == MESSAGE_SENDING) {
        opcode = *(uint16_t*)data;
        *(uint16_t*)data = htons(opcode);
    }
    else {
        opcode = ntohs(*(uint16_t*)data);
        *(uint16_t*)data = opcode;
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "port_manager net_order_msg_handler: opcode=%d\n",
             opcode);

    switch (opcode) {
    case MLAG_PORT_GLOBAL_STATE_EVENT:
        port_states =
            (struct port_global_state_event_data *) data;
        net_order_port_global_state_event_data(port_states, oper);
        break;
    case MLAG_PORTS_SYNC_DATA:
        port_sync = (struct peer_port_sync_message *) data;
        net_order_peer_port_sync_message(port_sync, oper);
        break;
    case MLAG_PORTS_UPDATE_EVENT:
        port_sync = (struct peer_port_sync_message *) data;
        net_order_peer_port_sync_message(port_sync, oper);
        break;
    case MLAG_PORTS_OPER_UPDATE:
        oper_sync = (struct peer_port_oper_sync_message*) data;
        net_order_peer_port_oper_sync_message(oper_sync, oper);
        break;
    case MLAG_PORTS_OPER_SYNC_DONE:
        /* Do nothing */
        break;
    case MLAG_PORTS_SYNC_FINISH_EVENT:
        sync_event = (struct sync_event_data*) data;
        net_order_sync_event_data(sync_event, oper);
        break;
    case MLAG_PEER_PORT_OPER_STATE_CHANGE:
        oper_change =
            (struct port_oper_state_change_data*) data;
        net_order_port_oper_state_change_data(oper_change, oper);
        break;
    default:
        /* Unknown opcode */
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Unknown opcode [%u] in port manager\n",
                            opcode);
        break;
    }
bail:
    return err;
}

/*
 *  This function handles peer port oper state sync info prepare
 *
 *
 * @param[in] peer_id - peer index
 *
 * @return 0 if operation completes successfully.
 */
static int
port_peer_oper_sync_msg_prepare(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    struct peer_port_oper_sync_message *oper_sync;

    oper_sync = (struct peer_port_oper_sync_message *)data;

    if (port_peer_state_get(port_info->peers_oper_state,
                            oper_sync->mlag_id) == TRUE) {
        oper_sync->port_id[oper_sync->port_num] = port_info->port_id;
        oper_sync->oper_state[oper_sync->port_num] = OES_PORT_UP;
        oper_sync->port_num++;
    }

    return err;
}

/*
 *  This function handles peer port sync info prepare
 *
 *
 * @param[in] peer_id - peer index
 *
 * @return 0 if operation completes successfully.
 */
static int
port_peer_sync_msg_prepare(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    struct peer_port_sync_message *sync_message;

    sync_message = (struct peer_port_sync_message *)data;
    sync_message->del_ports = FALSE;
    if (port_peer_state_get(port_info->peers_conf_state,
                            sync_message->mlag_id) == TRUE) {
        sync_message->port_id[sync_message->port_num] = port_info->port_id;
        sync_message->port_num++;
    }

    return err;
}

/*
 * This function handles creation of new port in port DB
 *
 * @param[in] port_id - port ID
 * @param[out] port_info - pointer to new port data
 *
 * @return 0 if operation completes successfully.
 */
static int
create_new_port(unsigned long port_id, struct mlag_port_data **port_info)
{
    int err = 0;
    ASSERT(port_info != NULL);

    err = port_db_entry_allocate(port_id, port_info);
    MLAG_BAIL_ERROR_MSG(err, "Failed to create port [%lu]\n", port_id);
    (*port_info)->port_mode = MLAG_PORT_MODE_STATIC;
    (*port_info)->peers_conf_state = 0;
    (*port_info)->peers_oper_state = 0;
    err = port_peer_local_init(&((*port_info)->peer_local_fsm),
                               ports_local_fsm_user_trace, NULL, NULL);
    MLAG_BAIL_ERROR(err);
    (*port_info)->peer_local_fsm.port_id = port_id;
    (*port_info)->peer_local_fsm.oper_state = MLAG_PORT_OPER_DOWN;
    (*port_info)->peer_local_fsm.admin_state = PORT_DISABLED;
    err = port_peer_remote_init(&((*port_info)->peer_remote_fsm),
                                ports_remote_fsm_user_trace, NULL, NULL);
    MLAG_BAIL_ERROR(err);
    (*port_info)->peer_remote_fsm.port_id = port_id;
    (*port_info)->peer_remote_fsm.isolated = FALSE;

    err = port_master_logic_init(&((*port_info)->port_master_fsm),
                                 ports_master_fsm_user_trace, NULL, NULL);
    MLAG_BAIL_ERROR(err);
    (*port_info)->port_master_fsm.port_id = port_id;
    (*port_info)->port_master_fsm.message_send_func =
        port_manager_message_send;
    (*port_info)->port_id = port_id;

bail:
    return err;
}

/*
 * This function update in master FSM new ports configuration (Add / Delete
 * that were received from peer FSM
 *
 * @param[in] peer_id - peer ID, local index
 * @param[out] port_sync - port sync message
 *
 * @return 0 if operation completes successfully.
 */
static int
update_ports_routine(int peer_id,
                     struct peer_port_sync_message *port_sync)
{
    int err = 0;
    uint32_t i;
    uint32_t states = 0;
    struct mlag_port_data *port_info;

    for (i = 0; i < port_sync->port_num; i++) {
        if (IS_MASTER()) {
            err = port_db_entry_lock(port_sync->port_id[i], &port_info);
            MLAG_BAIL_ERROR_MSG(err, " port [%lu] not found in DB\n",
                                port_sync->port_id[i]);

            if (port_sync->del_ports == FALSE) {
                err = port_master_logic_port_add_ev(
                    &port_info->port_master_fsm,
                    peer_id);
                if (err) {
                    port_db_entry_unlock(port_sync->port_id[i]);
                    MLAG_BAIL_ERROR_MSG(err,
                                        "Failed in port [%lu] add to master FSM\n",
                                        port_sync->port_id[i]);
                }
            }
            else {
                err = port_master_logic_port_del_ev(
                    &port_info->port_master_fsm,
                    peer_id);
                if (err) {
                    port_db_entry_unlock(port_sync->port_id[i]);
                    MLAG_BAIL_ERROR_MSG(err,
                                        "Failed in port [%lu] del to master FSM\n",
                                        port_sync->port_id[i]);
                }
            }
            err = port_db_entry_unlock(port_sync->port_id[i]);
            MLAG_BAIL_ERROR(err);
        }

        err =
            port_db_peer_conf_state_vector_get(port_sync->port_id[i], &states);
        MLAG_BAIL_ERROR(err);
        /* if port is not configured in any peer remove from DB */
        if (states == 0) {
            err = port_db_entry_delete(port_sync->port_id[i]);
            MLAG_BAIL_ERROR_MSG(err, "Failed to remove port [%lu] from DB\n",
                                port_sync->port_id[i]);
        }
    }

bail:
    return err;
}

/**
 *  This function handles port configuration update message receive
 *
 * @param[in] port_sync - contains port sync data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_port_update(struct peer_port_sync_message *port_sync)
{
    int err = 0;
    int peer_id;

    ASSERT(port_sync != NULL);

    if (started == FALSE) {
        goto bail;
    }

    err = mlag_manager_db_local_index_from_mlag_id_get(port_sync->mlag_id,
                                                       &peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting local id from mlag id [%d]",
                        port_sync->mlag_id );

    if (port_sync->del_ports == FALSE) {
        err = port_manager_peer_mlag_ports_add(peer_id, port_sync->port_id,
                                               port_sync->port_num);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling peer ports add\n");
    }
    else {
        err = port_manager_peer_mlag_ports_delete(peer_id, port_sync->port_id,
                                                  port_sync->port_num);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling peer ports delete\n");
    }

    /* Add/Remove in master logic, check if DB entry still needed */
    err = update_ports_routine(peer_id, port_sync);
    MLAG_BAIL_ERROR_MSG(err, "Master logic port update failed\n");

bail:
    return err;
}

/**
 *  This function handles oper states sync done event
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_oper_sync_done(void)
{
    int err = 0;

    if (started == FALSE) {
        goto bail;
    }

    struct mlag_master_election_status me_status;

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    /* Notify sync done */
    err = notify_peer_sync_done(me_status.my_peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed notifying peer port sync done\n");

bail:
    return err;
}

/**
 *  This function sends port conf sync message to peer
 *
 * @param[in] current_peer_id - peer on which to send configuration, given in local index
 * @param[in] dest_mlag_id - mlag ID of the destination peer
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
send_port_conf_sync_message(int current_peer_id, int dest_mlag_id)
{
    int err = 0;
    int src_mlag_id;
    uint32_t i;
    struct peer_port_sync_message sync_message;

    sync_message.port_num = 0;
    sync_message.mlag_id = current_peer_id;

    err = port_db_foreach(port_peer_sync_msg_prepare, &sync_message);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_manager_db_mlag_id_from_local_index_get(current_peer_id,
                                                       &src_mlag_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting mlag id for peer local index [%d]\n",
                        current_peer_id);
    sync_message.mlag_id = src_mlag_id;
    sync_message.del_ports = FALSE;
    /* Send message to peer */
    MLAG_LOG(MLAG_LOG_DEBUG,
             "Peer [%d] ports sync send to peer (%u ports) msg_size [%u]:\n",
             src_mlag_id, sync_message.port_num,
             (uint32_t)sizeof(sync_message));
    for (i = 0; i < sync_message.port_num; i++) {
        MLAG_LOG(MLAG_LOG_DEBUG, "[%lu]\n ", sync_message.port_id[i]);
    }

    if (sync_message.port_num > 0) {
        /* Send to master logic sync message */
        err = port_manager_message_send(MLAG_PORTS_SYNC_DATA,
                                        &sync_message,
                                        sizeof(sync_message),
                                        dest_mlag_id,
                                        MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync message \n");
    }

bail:
    return err;
}

/**
 *  This function sends port oper sync message to peer
 *
 * @param[in] current_peer_id - peer on which to send configuration, given in local index
 * @param[in] dest_mlag_id - mlag ID of the destination peer
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
send_port_oper_sync_message(int current_peer_id, int dest_mlag_id)
{
    int err = 0;
    int src_mlag_id;
    uint32_t i;
    struct peer_port_oper_sync_message oper_sync;

    oper_sync.port_num = 0;
    oper_sync.mlag_id = current_peer_id;

    err = port_db_foreach(port_peer_oper_sync_msg_prepare, &oper_sync);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_manager_db_mlag_id_from_local_index_get(current_peer_id,
                                                       &src_mlag_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting mlag id for peer local index [%d]\n",
                        current_peer_id);
    oper_sync.mlag_id = src_mlag_id;

    /* Send message to peer */
    MLAG_LOG(MLAG_LOG_DEBUG,
             "Peer [%d] ports oper sync send to peer (%u ports) msg_size [%u]:\n",
             src_mlag_id, oper_sync.port_num, (uint32_t)sizeof(oper_sync));
    for (i = 0; i < oper_sync.port_num; i++) {
        MLAG_LOG(MLAG_LOG_DEBUG, "port [%lu] oper [%d]\n ",
                 oper_sync.port_id[i], oper_sync.oper_state[i]);
    }

    if (oper_sync.port_num > 0) {
        /* Send to master logic sync message */
        err = port_manager_message_send(MLAG_PORTS_OPER_UPDATE,
                                        &oper_sync,
                                        sizeof(oper_sync),
                                        dest_mlag_id,
                                        MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending ports oper sync message \n");
    }

bail:
    return err;
}

/**
 *  This function handles port sync message receive
 *
 * @param[in] port_sync - contains port sync data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_port_sync(struct peer_port_sync_message *port_sync)
{
    int err = 0;
    int peer_local_id;
    int current_peer_id = 0;
    struct peer_port_sync_message sync_message;
    uint32_t peer_states_vector;

    ASSERT(port_sync != NULL);

    if (started == FALSE) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_DEBUG,
             "sync from mlag_id [%d] port# [%u] del_ports [%d]\n",
             port_sync->mlag_id, port_sync->port_num, port_sync->del_ports);

    /* update local DB */
    err = port_manager_port_update(port_sync);
    MLAG_BAIL_ERROR(err);

    if (current_role == MASTER) {
        err = port_db_peer_state_vector_get(&peer_states_vector);
        MLAG_BAIL_ERROR(err);

        err = mlag_manager_db_local_index_from_mlag_id_get(port_sync->mlag_id,
                                                           &peer_local_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed getting peer local index from mlag id [%d]\n",
                            port_sync->mlag_id);

        /* Send per local & per existing peer which is not the remote */
        peer_states_vector |= (1 << mlag_manager_db_local_peer_id_get());
        MLAG_LOG(MLAG_LOG_DEBUG,
                 "Peer states [0x%x] current_peer_id [%d] peer_local_id [%d]\n",
                 peer_states_vector, current_peer_id, peer_local_id);
        while (peer_states_vector != 0) {
            if ((current_peer_id != peer_local_id) &&
                (peer_states_vector & 0x1)) {
                err = send_port_conf_sync_message(current_peer_id,
                                                  port_sync->mlag_id);
                MLAG_BAIL_ERROR(err);
            }
            current_peer_id++;
            peer_states_vector >>= 1;
        }
        /* Send back operational states */
        current_peer_id = 0;
        err = port_db_peer_state_vector_get(&peer_states_vector);
        MLAG_BAIL_ERROR_MSG(err, "Failed getting peer state vector\n");
        MLAG_LOG(MLAG_LOG_DEBUG,
                 "Peer states [0x%x] current_peer_id [%d] peer_local_id [%d]\n",
                 peer_states_vector, current_peer_id, peer_local_id);
        while (peer_states_vector != 0) {
            if ((current_peer_id != peer_local_id) &&
                (peer_states_vector & 0x1)) {
                err = send_port_oper_sync_message(current_peer_id,
                                                  port_sync->mlag_id);
                MLAG_BAIL_ERROR(err);
            }
            current_peer_id++;
            peer_states_vector >>= 1;
        }

        err = port_db_peer_state_set(peer_local_id, PM_PEER_TX_ENABLED);
        MLAG_BAIL_ERROR_MSG(err, "Failed setting peer state\n");
    }
    if (current_role != SLAVE) {
        /* Notify end of ports sync */
        err = port_manager_message_send(MLAG_PORTS_OPER_SYNC_DONE,
                                        &sync_message, sizeof(sync_message),
                                        port_sync->mlag_id, MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync message \n");
    }

bail:
    return err;
}

/**
 *  This function handles port sync done message receive
 *
 * @param[in] port_sync - contains port sync data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_sync_finish(struct sync_event_data *sync_data)
{
    int err = 0;

    if (started == FALSE) {
        goto bail;
    }

    err =
        send_system_event(MLAG_PEER_SYNC_DONE, sync_data, sizeof(*sync_data));
    MLAG_BAIL_ERROR_MSG(err, "Failed sending MLAG_PEER_SYNC_DONE message\n");

bail:
    return err;
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_manager_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    port_db_log_verbosity_set(verbosity);
    port_peer_remote_log_verbosity_set(verbosity);
    port_peer_local_log_verbosity_set(verbosity);
    port_master_logic_log_verbosity_set(verbosity);
}

/**
 *  This function returns module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
port_manager_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits port_manager
 *
 *  @param[in] insert_msgs_cb - callback for registering PDU handlers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_init(insert_to_command_db_func insert_msgs_cb)
{
    int err = 0;

    err = insert_msgs_cb(port_manager_ibc_msgs);
    MLAG_BAIL_ERROR(err);

    err = port_db_init();
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function deinits port_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_deinit(void)
{
    int err = 0;

    err = port_db_deinit();
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function starts port_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_start(void)
{
    int err = 0;

    started = TRUE;
    in_split_brain = FALSE;

    return err;
}

/*
 *  This function handles per port configuration cleanup
 *
 * @param[in] port_info - peer index
 * @param[in] data - optional data
 *
 * @return 0 if operation completes successfully.
 */
static int
clear_port_conf(struct mlag_port_data *port_info, void *data)
{
    int err = 0;

    UNUSED_PARAM(data);
    /* Update DB */
    port_info->peers_oper_state = 0;
    port_info->peers_conf_state &= 0x1;

    err = port_peer_remote_port_global_dis(&port_info->peer_remote_fsm);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in port [%lu] global disable to remote FSM\n",
                        port_info->port_id);

    err = port_peer_local_port_global_dis(&port_info->peer_local_fsm);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in port [%lu] global disable to remote FSM\n",
                        port_info->port_id);

bail:
    return err;
}

/**
 *  This function clears all  port_manager configurations
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
clear_port_manager_state(void)
{
    int err = 0;
    int peer_id;

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        err = port_db_peer_state_set(peer_id, PM_PEER_DOWN);
        MLAG_BAIL_CHECK_NO_MSG(err);
    }

    err = port_db_foreach(clear_port_conf, NULL);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 *  This function stops port_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_manager_stop(void)
{
    int err = 0;

    if (started == FALSE) {
        goto bail;
    }

    started = FALSE;

    err = clear_port_manager_state();
    MLAG_BAIL_ERROR_MSG(err, "Failed clearing port manager states\n");

bail:
    port_manager_counters_clear();
    return err;
}

/**
 *  This function adds configured ports
 *
 * @param[in] mlag_ports - array of ports to add
 * @param[in] port_num - number of ports to add
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_mlag_ports_add(unsigned long *mlag_ports, int port_num)
{
    int err = 0;
    int i, peer_id;
    struct peer_port_sync_message port_sync;
    struct mlag_port_data *port_info;
    struct mlag_master_election_status me_status;

    ASSERT(mlag_ports != NULL);
    ASSERT(port_num < MLAG_MAX_PORTS);

    for (i = 0; i < port_num; i++) {
        err = port_db_entry_lock(mlag_ports[i], &port_info);
        if (err == -ENOENT) {
            /* create port , init state machines */
            err = create_new_port(mlag_ports[i], &port_info);
            MLAG_BAIL_ERROR_MSG(err, "Failed in port [%lu] creation\n",
                                mlag_ports[i]);
        }
        MLAG_BAIL_ERROR_MSG(err, "Failure in getting port [%lu]\n",
                            mlag_ports[i]);
        /* set conf state of the port */
        peer_id = mlag_manager_db_local_peer_id_get();
        port_peer_state_set(&port_info->peers_conf_state, peer_id,
                            PORT_CONFIGURED);

        /* add port to local peer logic */
        err = port_peer_local_port_add_ev(&port_info->peer_local_fsm);
        if (err) {
            port_db_entry_unlock(mlag_ports[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] add to peer local FSM\n",
                                mlag_ports[i]);
        }
        err = port_db_entry_unlock(mlag_ports[i]);
        MLAG_BAIL_ERROR(err);
    }

    if (started == TRUE) {
        /* Notify master logic on new port */
        err = mlag_master_election_get_status(&me_status);
        MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

        /* If master is up then send peer start */
        port_sync.mlag_id = me_status.my_peer_id;
        port_sync.port_num = port_num;
        port_sync.del_ports = FALSE;
        for (i = 0; i < port_num; i++) {
            port_sync.port_id[i] = mlag_ports[i];
        }

        /* Notify master on new ports */
        err = port_manager_message_send(MLAG_PORTS_UPDATE_EVENT,
                                        &port_sync, sizeof(port_sync),
                                        port_sync.mlag_id, PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync message \n");
    }

bail:
    return err;
}

/**
 *  This function sends deleted event
 *
 * @param[in] port id
 * @param[in] status: 1 if port deleted successfully,
 *                    0 in case of error
 *
 * @return 0 if operation completes successfully.
 */

static int
send_port_deleted_event(unsigned long port, int status)
{
    struct mpo_port_deleted_event_data port_deleted;
    int err = 0;

    port_deleted.port = port;
    port_deleted.status = status;

    err = send_system_event(MLAG_PORT_DELETED_EVENT, &port_deleted,
                            sizeof(port_deleted));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending port deleted event\n");

bail:
    return err;
}

/**
 *  This function deletes configured local port
 *
 * @param[in] mlag_port - port to delete
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_mlag_port_delete(unsigned long mlag_port)
{
    int err = 0;
    int peer_id;
    int port_delete_status = 1;
    uint32_t states;
    struct peer_port_sync_message port_sync;
    struct mlag_port_data *port_info;
    struct mlag_master_election_status me_status;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Delete port [%lu] procedure started\n", mlag_port);

    err = port_db_entry_lock(mlag_port, &port_info);
    if (err == -ENOENT) {
        /* ignore port that was not found */
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "mlag port %lu is not found in db\n", mlag_port);
        port_delete_status = 0;
        err = 0;
        goto bail;
    }
    MLAG_BAIL_ERROR_MSG(err, "Failed in getting port [%lu]\n",
                        mlag_port);

    /* set conf state of the port */
    peer_id = mlag_manager_db_local_peer_id_get();
    port_peer_state_set(&port_info->peers_conf_state, peer_id, PORT_DELETED);

    /* del port in local peer logic */
    err = port_peer_local_port_del_ev(&port_info->peer_local_fsm);
    if (err) {
        port_db_entry_unlock(mlag_port);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] delete to peer local FSM\n",
                            mlag_port);
    }
    err = port_db_entry_unlock(mlag_port);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to unlock port [%lu]\n", mlag_port);

    if (started == TRUE) {
        /* Notify master logic on deleted port */
        err = mlag_master_election_get_status(&me_status);
        MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

        /* If master is up then send peer start */
        port_sync.mlag_id = me_status.my_peer_id;
        port_sync.port_num = 1;
        port_sync.del_ports = TRUE;
        port_sync.port_id[0] = mlag_port;

        /* Notify master on deleted ports */
        err = port_manager_message_send(MLAG_PORTS_UPDATE_EVENT,
                                        &port_sync, sizeof(port_sync),
                                        port_sync.mlag_id, PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync message\n");
    }

    /* When no master logic present we need to delete ports ourselves */
    if ((started == FALSE) || (current_role == SLAVE)) {
        err = port_db_peer_conf_state_vector_get(mlag_port,
                                                 &states);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get peer conf state vector for port [%lu]\n",
                            mlag_port);
        if (states == 0) {
            /* Delete port from DB */
            err = port_db_entry_delete(mlag_port);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to remove port [%lu] from DB\n",
                                mlag_port);
        }
    }

bail:
    if (err) {
        port_delete_status = 0;
    }
    MLAG_LOG(MLAG_LOG_NOTICE,
             "Sending port deleted event for port=%lu status=%d\n",
             mlag_port, port_delete_status);
    send_port_deleted_event(mlag_port, port_delete_status);
    return err;
}

/**
 *  This function handles remote peers ports configuration
 *
 * @param[in] peer_id   - peer index
 * @param[in] mlag_ports - array of ports to add
 * @param[in] port_num - number of ports to add
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_peer_mlag_ports_add(int peer_id, unsigned long *mlag_ports,
                                 int port_num)
{
    int i, err = 0;
    struct mlag_port_data *port_info;

    ASSERT(mlag_ports != NULL);
    ASSERT(port_num < MLAG_MAX_PORTS);
    ASSERT(peer_id < MLAG_MAX_PEERS);

    for (i = 0; i < port_num; i++) {
        err = port_db_entry_lock(mlag_ports[i], &port_info);
        if (err == -ENOENT) {
            /* create port , init state machines */
            err = create_new_port(mlag_ports[i], &port_info);
            MLAG_BAIL_ERROR_MSG(err, "Failed in port [%lu] creation\n",
                                mlag_ports[i]);
        }
        MLAG_BAIL_ERROR_MSG(err, "Failure in getting port [%lu]\n",
                            mlag_ports[i]);
        /* set conf state of the port */
        port_peer_state_set(&port_info->peers_conf_state, peer_id,
                            PORT_CONFIGURED);

        /* call port logic for port add */
        err =
            port_peer_remote_port_add_ev(&port_info->peer_remote_fsm, peer_id);
        if (err) {
            port_db_entry_unlock(mlag_ports[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] add to peer remote FSM\n",
                                mlag_ports[i]);
        }

        err = port_db_entry_unlock(mlag_ports[i]);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 *  This function handles remote peers ports deletion
 *
 * @param[in] peer_id   - peer index
 * @param[in] mlag_ports - array of ports to add
 * @param[in] port_num - number of ports to add
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_peer_mlag_ports_delete(int peer_id, unsigned long *mlag_ports,
                                    int port_num)
{
    int i, err = 0;
    struct mlag_port_data *port_info;

    ASSERT(mlag_ports != NULL);
    ASSERT(port_num < MLAG_MAX_PORTS);
    ASSERT(peer_id < MLAG_MAX_PEERS);

    for (i = 0; i < port_num; i++) {
        err = port_db_entry_lock(mlag_ports[i], &port_info);
        MLAG_BAIL_ERROR_MSG(err, " port [%lu] not found in DB\n",
                            mlag_ports[i]);

        /* set conf state of the port */
        port_peer_state_set(&port_info->peers_conf_state, peer_id,
                            PORT_DELETED);

        err =
            port_peer_remote_port_del_ev(&port_info->peer_remote_fsm, peer_id);
        if (err) {
            port_db_entry_unlock(mlag_ports[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in port [%lu] del to peer remote FSM\n",
                                mlag_ports[i]);
        }
        err = port_db_entry_unlock(mlag_ports[i]);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/*
 *  This function handles global admin state notification
 *  Global admin state is set by the MLAG master port logic Port
 *  is Disabled when it is not configured in all active peers
 *
 * @param[in] port_info - port info entry
 * @param[in] admin_state - port state to set
 *
 * @return 0 if operation completes successfully.
 */
static int
port_admin_state_set(struct mlag_port_data *port_info, int admin_state)
{
    int err = 0;

    /* Handle remote before local - since local may enable the port */
    if (admin_state == MLAG_PORT_GLOBAL_DISABLED) {
        err = port_peer_remote_port_global_dis(&port_info->peer_remote_fsm);
    }
    else {
        err = port_peer_remote_port_global_en(&port_info->peer_remote_fsm);
    }
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in port [%lu] admin state change to peer remote FSM\n",
                        port_info->port_id);

    if (admin_state == MLAG_PORT_GLOBAL_DISABLED) {
        err = port_peer_local_port_global_dis(&port_info->peer_local_fsm);
    }
    else {
        err = port_peer_local_port_global_en(&port_info->peer_local_fsm);
    }
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in port [%lu] admin state change to peer local FSM\n",
                        port_info->port_id);

bail:
    return err;
}

/**
 *  This function handles global oper state notification
 *  Global oper state is set by the MLAG master port logic
 *  Down means that there are no active links for the mlag port
 *
 * @param[in] mlag_ports - array of ports
 * @param[in] new_states - port states, expected the same length
 *       as ports array
 * @param[in] port_num - number of ports in the arrays above
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_global_oper_state_set(unsigned long *mlag_ports, int *oper_states,
                                   int port_num)
{
    int i, err = 0;
    struct mlag_port_data *port_info;

    ASSERT(mlag_ports != NULL);
    ASSERT(oper_states != NULL);
    ASSERT(port_num < MLAG_MAX_PORTS);

    if (started == FALSE) {
        goto bail;
    }

    for (i = 0; i < port_num; i++) {
        err = port_db_entry_lock(mlag_ports[i], &port_info);
        /* Ignore If port not found */
        if (err == -ENOENT) {
            err = 0;
            continue;
        }
        MLAG_BAIL_ERROR_MSG(err, " port [%lu] not found in DB\n",
                            mlag_ports[i]);

        /* Update logic */
        switch (oper_states[i]) {
        case MLAG_PORT_GLOBAL_DISABLED:
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Port [%lu] global state is [Disabled]\n", mlag_ports[i]);
            err = port_admin_state_set(port_info, oper_states[i]);
            if (err) {
                port_db_entry_unlock(mlag_ports[i]);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in port [%lu] global disable \n",
                                    mlag_ports[i]);
            }
            break;
        case MLAG_PORT_GLOBAL_DOWN:
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Port [%lu] global state is [Down]\n", mlag_ports[i]);
            err = port_peer_local_port_global_down(&port_info->peer_local_fsm);
            if (err) {
                port_db_entry_unlock(mlag_ports[i]);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in port [%lu] global down in peer local FSM\n",
                                    mlag_ports[i]);
            }
            err =
                port_peer_remote_port_global_down(&port_info->peer_remote_fsm);
            if (err) {
                port_db_entry_unlock(mlag_ports[i]);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in port [%lu] global down to peer remote FSM\n",
                                    mlag_ports[i]);
            }
            break;
        case MLAG_PORT_GLOBAL_ENABLED:
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Port [%lu] global state is [Enabled]\n", mlag_ports[i]);
            err = port_admin_state_set(port_info, oper_states[i]);
            if (err) {
                port_db_entry_unlock(mlag_ports[i]);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in port [%lu] global enable \n",
                                    mlag_ports[i]);
            }
            break;
        case MLAG_PORT_GLOBAL_UP:
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Port [%lu] global state is [Up]\n", mlag_ports[i]);
            err = port_peer_local_port_global_up(&port_info->peer_local_fsm);
            if (err) {
                port_db_entry_unlock(mlag_ports[i]);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in port [%lu] global up in peer local FSM\n",
                                    mlag_ports[i]);
            }
            err = port_peer_remote_port_global_en(&port_info->peer_remote_fsm);
            if (err) {
                port_db_entry_unlock(mlag_ports[i]);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in port [%lu] global enable to peer remote FSM\n",
                                    mlag_ports[i]);
            }
            break;
        default:
            break;
        }
        err = port_db_entry_unlock(mlag_ports[i]);
        MLAG_BAIL_ERROR(err);
    }
bail:
    return err;
}

/**
 *  This function handles global admin state notification
 *  Global admin state is set by the MLAG master port logic Port
 *  is Disabled when it is not configured in all active peers
 *
 * @param[in] mlag_ports - array of ports
 * @param[in] admin_states - port states, expected the same
 *       length as ports array
 * @param[in] port_num - number of ports in the arrays above
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_port_admin_state_set(unsigned long *mlag_ports, int *admin_states,
                                  int port_num)
{
    int i, err = 0;
    struct mlag_port_data *port_info;

    ASSERT(mlag_ports != NULL);
    ASSERT(admin_states != NULL);
    ASSERT(port_num < MLAG_MAX_PORTS);

    if (started == FALSE) {
        goto bail;
    }

    for (i = 0; i < port_num; i++) {
        err = port_db_entry_lock(mlag_ports[i], &port_info);
        MLAG_BAIL_ERROR_MSG(err, "port [%lu] not found in DB\n",
                            mlag_ports[i]);

        /* Update logic */
        err = port_admin_state_set(port_info, admin_states[i]);
        if (err) {
            port_db_entry_unlock(mlag_ports[i]);
            goto bail;
        }
        err = port_db_entry_unlock(mlag_ports[i]);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 *  This function handles local port up notification
 *
 * @param[in] mlag_port - port id
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_local_port_up(unsigned long mlag_port)
{
    int err = 0;
    struct mlag_port_data *port_info;
    struct port_oper_state_change_data chg_event;
    struct mlag_master_election_status me_status;

    if (started == FALSE) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_DEBUG, "port [%lu] local state up\n", mlag_port);

    err = port_db_entry_lock(mlag_port, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "port [%lu] not found in DB\n", mlag_port);

    /* Update DB */
    port_peer_state_set(&(port_info->peers_oper_state),
                        mlag_manager_db_local_peer_id_get(), TRUE);

    /* Update logic */
    err = port_peer_local_port_up_ev(&port_info->peer_local_fsm);
    if (err) {
        port_db_entry_unlock(mlag_port);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] oper state in peer local FSM\n",
                            mlag_port);
    }

    err = port_db_entry_unlock(mlag_port);
    MLAG_BAIL_ERROR(err);

    /* Notify master logic on new state */
    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    chg_event.is_ipl = FALSE;
    chg_event.mlag_id = me_status.my_peer_id;
    chg_event.port_id = mlag_port;
    chg_event.state = MLAG_PORT_OPER_UP;

    /* Notify master on new state */
    err = port_manager_message_send(MLAG_PEER_PORT_OPER_STATE_CHANGE,
                                    &chg_event, sizeof(chg_event),
                                    chg_event.mlag_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending ports oper state up message \n");

bail:
    return err;
}

/**
 *  This function handles local port down notification
 *
 * @param[in] mlag_port - port id
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_local_port_down(unsigned long mlag_port)
{
    int err = 0;
    struct mlag_port_data *port_info;
    struct port_oper_state_change_data chg_event;
    struct mlag_master_election_status me_status;

    if (started == FALSE) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_DEBUG, "port [%lu] local state down\n", mlag_port);

    err = port_db_entry_lock(mlag_port, &port_info);
    MLAG_BAIL_ERROR_MSG(err, " port [%lu] not found in DB\n", mlag_port);

    /* Update DB */
    port_peer_state_set(&(port_info->peers_oper_state),
                        mlag_manager_db_local_peer_id_get(), FALSE);

    /* Update logic */
    err = port_peer_local_port_down_ev(&port_info->peer_local_fsm);
    if (err) {
        port_db_entry_unlock(mlag_port);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] oper state in peer local FSM\n",
                            mlag_port);
    }

    err = port_db_entry_unlock(mlag_port);
    MLAG_BAIL_ERROR(err);

    /* Notify master logic on new state */
    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    chg_event.is_ipl = FALSE;
    chg_event.mlag_id = me_status.my_peer_id;
    chg_event.port_id = mlag_port;
    chg_event.state = MLAG_PORT_OPER_DOWN;

    /* Notify master on new state */
    err = port_manager_message_send(MLAG_PEER_PORT_OPER_STATE_CHANGE,
                                    &chg_event, sizeof(chg_event),
                                    chg_event.mlag_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending ports oper state down message \n");

bail:
    return err;
}

/**
 *  This function handles peer port oper status change notification
 *  this message comes as a notification from peer (local and
 *  remote)
 *
 * @param[in] oper_chg - event carrying oper state change data
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_peer_oper_state_change(
    struct port_oper_state_change_data *oper_chg)
{
    int err = 0;
    int local_peer_index;

    if (started == FALSE) {
        goto bail;
    }

    err = mlag_manager_db_local_index_from_mlag_id_get(oper_chg->mlag_id,
                                                       &local_peer_index);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting local index from mlag id [%d]",
                        oper_chg->mlag_id);

    if (oper_chg->state == MLAG_PORT_OPER_UP) {
        MLAG_LOG(MLAG_LOG_NOTICE, " Peer port [%lu] oper change to [Up]\n",
                 oper_chg->port_id);
        err = port_manager_peer_port_up(local_peer_index, oper_chg->port_id);
    }
    else {
        MLAG_LOG(MLAG_LOG_NOTICE, " Peer port [%lu] oper change to [Down]\n",
                 oper_chg->port_id);
        err =
            port_manager_peer_port_down(local_peer_index, oper_chg->port_id);
    }
    MLAG_BAIL_ERROR_MSG(err, "Failed handling peer port [%lu] oper change",
                        oper_chg->port_id);

bail:
    return err;
}

/*
 *  This function handles peer down notification logic.
 *
 * @param[in] port_info - port info
 * @param[in] data - additional data
 *
 * @return 0 if operation completes successfully.
 */
static int
port_peer_down_event_handle(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    struct peer_down_ports_data *peer_down_data =
        ((struct peer_down_ports_data *)data);
    int peer_id = peer_down_data->peer_id;
    uint32_t states;
    /* Update DB */
    port_peer_state_set(&(port_info->peers_oper_state), peer_id, FALSE);
    port_peer_state_set(&(port_info->peers_conf_state), peer_id, FALSE);

    err = port_db_peer_conf_state_vector_get(port_info->port_id, &states);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get port [%lu] con states\n",
                        port_info->port_id);

    if (states == 0) {
        /* Mark for deletion */
        peer_down_data->ports_to_delete[peer_down_data->port_num] =
            port_info->port_id;
        peer_down_data->port_num++;
    }

    if (peer_id != mlag_manager_db_local_peer_id_get()) {
        err = port_peer_remote_peer_down_ev(&port_info->peer_remote_fsm,
                                            peer_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] peer down to remote FSM\n",
                            port_info->port_id);
    }
    if (IS_MASTER()) {
        err = port_master_logic_peer_down_ev(&port_info->port_master_fsm,
                                             peer_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] peer down to master FSM\n",
                            port_info->port_id);
    }
bail:
    return err;
}

/*
 *  This function handles role change to standalone
 *  per port
 *
 * @param[in] port_info - port info
 * @param[in] data - additional data
 *
 * @return 0 if operation completes successfully.
 */
static int
port_standalone_handle(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    int peer_id;
    UNUSED_PARAM(data);
    /* Update DB */
    peer_id = mlag_manager_db_local_peer_id_get();
    port_info->peers_oper_state &= 0x1;
    port_info->peers_conf_state &= 0x1;

    err = port_master_logic_port_add_ev(&port_info->port_master_fsm,
                                        peer_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in port [%lu] peer add to master FSM\n",
                        port_info->port_id);

bail:
    return err;
}

/*
 *  This function handles start of port sync in which all ports are
 *  shutting down.
 *
 * @param[in] port_info - port info
 * @param[in] data - additional data
 *
 * @return 0 if operation completes successfully.
 */
static int
port_shutdown(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    int peer_id;

    UNUSED_PARAM(data);

    peer_id = mlag_manager_db_local_peer_id_get();
    port_peer_state_set(&(port_info->peers_oper_state), peer_id, FALSE);

    err = port_peer_local_port_global_dis(&port_info->peer_local_fsm);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in port [%lu] toggle off\n",
                        port_info->port_id);

bail:
    return err;
}

/*
 *  This function handles regular role change in which ports are
 *  shutting down.
 *
 * @param[in] port_info - port info
 * @param[in] data - additional data
 *
 * @return 0 if operation completes successfully.
 */
static int
static_port_shutdown(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    int peer_id;

    UNUSED_PARAM(data);

    peer_id = mlag_manager_db_local_peer_id_get();
    port_peer_state_set(&(port_info->peers_oper_state), peer_id, FALSE);

    if (port_info->port_mode == MLAG_PORT_MODE_STATIC) {
        err = port_peer_local_port_global_dis(&port_info->peer_local_fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] toggle off\n",
                            port_info->port_id);
    }

bail:
    return err;
}

/*
 *  Handle LACP ports toggle when role change occurs
 *
 * @param[in] port_info - port info
 * @param[in] data - additional data
 *
 * @return 0 if operation completes successfully.
 */
static int
lacp_port_toggle(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    int peer_id;

    UNUSED_PARAM(data);

    peer_id = mlag_manager_db_local_peer_id_get();
    port_peer_state_set(&(port_info->peers_oper_state), peer_id, FALSE);

    if (port_info->port_mode == MLAG_PORT_MODE_LACP) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "lacp port [%lu] toggling\n", port_info->port_id);

        err = port_peer_local_port_global_dis(&port_info->peer_local_fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] toggle off\n",
                            port_info->port_id);

        /* Toggle all local ports */
        err = port_peer_local_port_global_en(&port_info->peer_local_fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] toggle on\n",
                            port_info->port_id);
    }
    else {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "port [%lu] is not in lacp mode, it's mode is %d\n",
                 port_info->port_id, port_info->port_mode);
    }

bail:
    return err;
}

/*
 *  This function handles peer down notification.Peer down may
 *  affect all ports states. When peer changes its state to down
 *  it may affect remote port state
 *
 * @param[in] mlag_id - peer MLAG ID index
 *
 * @return 0 if operation completes successfully.
 */
static int
port_peer_down(int mlag_id)
{
    int err = 0;
    int i, peer_id;
    struct peer_down_ports_data peer_down_data;

    ASSERT(mlag_id < MLAG_MAX_PEERS);

    MLAG_LOG(MLAG_LOG_NOTICE, "Peer [%d] down handling in PM\n", mlag_id);

    err = mlag_manager_db_local_index_from_mlag_id_get(mlag_id, &peer_id);
    if (err) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Failed to find peer [%d] local index\n",
                 mlag_id);
        err = 0;
        goto bail;
    }

    err = port_db_peer_state_set(peer_id, PM_PEER_DOWN);
    MLAG_BAIL_CHECK_NO_MSG(err);

    peer_down_data.peer_id = peer_id;
    peer_down_data.port_num = 0;

    err = port_db_foreach(port_peer_down_event_handle, &peer_down_data);
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* clear ports that are not configured in any peer */
    for (i = 0; i < peer_down_data.port_num; i++) {
        /* Delete port from DB */
        err = port_db_entry_delete(peer_down_data.ports_to_delete[i]);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to remove port [%lu] from DB\n",
                            peer_down_data.ports_to_delete[i]);
    }

bail:
    return err;
}

/*
 *  This function handles peer enable notification .
 *
 * @param[in] port_info - port info
 * @param[in] data - additional data
 *
 * @return 0 if operation completes successfully.
 */
static int
port_peer_enable_event_handle(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    int peer_id = *((int *)data);

    if (IS_MASTER()) {
        err = port_master_logic_peer_active_ev(&port_info->port_master_fsm,
                                               peer_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] peer active to master FSM\n",
                            port_info->port_id);
    }

    if (peer_id != mlag_manager_db_local_peer_id_get()) {
        err = port_peer_remote_peer_enable_ev(&port_info->peer_remote_fsm,
                                              peer_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in port [%lu] peer enable to remote FSM\n",
                            port_info->port_id);
    }

bail:
    return err;
}

/*
 *  This function handles split brain state. it closes all ports
 *
 *
 * @return 0 if operation completes successfully.
 */
static int
handle_split_brain()
{
    int err = 0;

    in_split_brain = TRUE;

    MLAG_LOG(MLAG_LOG_NOTICE, "Split brain detected - shutdown ports\n");
    err = port_db_foreach(static_port_shutdown, NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed in split brain port shutdown\n");

bail:
    return err;
}

/*
 *  This function handles sync start. it closes all ports
 *
 *
 * @return 0 if operation completes successfully.
 */
static int
disable_ports_sync_start()
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_NOTICE, "Sync start - shutdown ports\n");
    err = port_db_foreach(port_shutdown, NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sync start port shutdown\n");

bail:
    return err;
}

/**
 *  This function handles peer state change notification.
 *
 * @param[in] state_change - state  change data
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_peer_state_change(struct peer_state_change_data *state_change)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_NOTICE, "Port manager peer state change [%d]\n",
             state_change->state);

    if (started == FALSE) {
        goto bail;
    }

    in_split_brain = FALSE;

    if (state_change->state == HEALTH_PEER_UP) {
        /* Nothing to do */
    }
    else if ((current_role == MASTER) &&
             (state_change->state == HEALTH_PEER_DOWN)) {
        err = port_peer_down(state_change->mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed in port peer down handling\n");
    }
    else if ((current_role != STANDALONE) &&
             (state_change->state == HEALTH_PEER_COMM_DOWN)) {
        err = port_peer_down(state_change->mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed in port peer down handling\n");
        /* Split brain state */
        if (current_role == SLAVE) {
            err = handle_split_brain();
            MLAG_BAIL_ERROR_MSG(err, "Failed in split brain handling\n");
        }
    }
    if ((current_role == SLAVE) &&
        (state_change->state == HEALTH_PEER_DOWN_WAIT)) {
        err = port_db_foreach(lacp_port_toggle, NULL);
        MLAG_BAIL_ERROR_MSG(err, "Failed in lacp ports state toggle\n");
    }

bail:
    return err;
}

/**
 *  This function handles peer enable notification.Peer enable
 *  may affect all ports states. When peer changes its state to
 *  enabled it may affect remote port state
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_peer_enable(int mlag_peer_id)
{
    int err = 0;
    int peer_id;

    ASSERT(mlag_peer_id < MLAG_MAX_PEERS);

    if (started == FALSE) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "Peer enable mlag_id [%d]\n", mlag_peer_id);

    /* use local index of peers */
    err = mlag_manager_db_local_index_from_mlag_id_get(mlag_peer_id, &peer_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting peer local index from mlag id [%d]",
                        mlag_peer_id);

    err = port_db_peer_state_set(peer_id, PM_PEER_ENABLED);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = port_db_foreach(port_peer_enable_event_handle, &peer_id);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 *  This function handles peer start notification.Peer start
 *  triggers port sync process start. the local peer logic sends update
 *  of its configured ports to the Master logic
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_peer_start(int mlag_peer_id)
{
    int err = 0;
    int peer_id;
    uint32_t i;
    struct peer_port_sync_message sync_message;

    ASSERT(mlag_peer_id < MLAG_MAX_PEERS);

    if (started == FALSE) {
        goto bail;
    }

    /* make sure ports are down when slave starts sync */
    if (current_role == SLAVE) {
        err = disable_ports_sync_start();
        MLAG_BAIL_ERROR_MSG(err, "Failed in disabling ports before sync\n");
    }

    /* use local index of peers */
    err = mlag_manager_db_local_index_from_mlag_id_get(mlag_peer_id, &peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to find local index for mlag_id [%d]\n",
                        mlag_peer_id);

    MLAG_LOG(MLAG_LOG_DEBUG, "Peer start mlag_id [%d] peer_id [%d]\n",
             mlag_peer_id, peer_id);

    sync_message.port_num = 0;
    sync_message.mlag_id = peer_id;

    err = port_db_foreach(port_peer_sync_msg_prepare, &sync_message);
    MLAG_BAIL_CHECK_NO_MSG(err);

    sync_message.mlag_id = mlag_peer_id;
    sync_message.del_ports = FALSE;
    /* Send message to master */
    MLAG_LOG(MLAG_LOG_DEBUG,
             "Port peer [%d] sync send to master (%u ports) msg_size [%u]:\n",
             sync_message.mlag_id, sync_message.port_num,
             (uint32_t) sizeof(sync_message));
    for (i = 0; i < sync_message.port_num; i++) {
        MLAG_LOG(MLAG_LOG_DEBUG, "[%lu]\n ", sync_message.port_id[i]);
    }

    /* Send to master logic sync message */
    err = port_manager_message_send(MLAG_PORTS_SYNC_DATA,
                                    &sync_message, sizeof(sync_message),
                                    mlag_peer_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync message \n");

bail:
    return err;
}

/**
 *  This function handles local peer role change.
 *  port manager enables global FSMs according to role
 *
 * @param[in] new_role - current role of local peer
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_role_change(int new_role)
{
    int err = 0;
    int prev_role = current_role;
    struct mlag_master_election_status me_status;

    if (started == FALSE) {
        goto bail;
    }

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    my_mlag_id = me_status.my_peer_id;

    current_role = new_role;
    if ((prev_role == SLAVE) && (new_role == STANDALONE)) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Port manager role change slave->standalone\n");
        /* Clear all peers */
        err = port_db_foreach(port_standalone_handle, NULL);
        MLAG_BAIL_ERROR_MSG(err, "Failed in port standalone handle\n");

        err = port_db_foreach(lacp_port_toggle, NULL);
        MLAG_BAIL_ERROR_MSG(err, "Failed in lacp ports state toggle\n");
    }
    else {
        err = port_manager_stop();
        MLAG_BAIL_ERROR_MSG(err, "Failed stopping port manager module\n");

        err = port_manager_start();
        MLAG_BAIL_ERROR_MSG(err, "Failed starting port manager module\n");
    }

bail:
    return err;
}

/*
 *  This function gets port current state
 *
 * @param[in] port_info - pointer to port_info struct
 *
 * @return port state
 */
static enum mlag_port_oper_state
port_state_get(struct mlag_port_data *port_info)
{
    enum mlag_port_oper_state state;
    if (port_info->peer_local_fsm.admin_state == PORT_DISABLED) {
        state = DISABLED;
        goto bail;
    }
    state = INACTIVE;
    if (port_peer_local_local_fault_in(&(port_info->peer_local_fsm)) ||
        port_peer_remote_remote_fault_in(&(port_info->peer_remote_fsm))) {
        state = ACTIVE_PARTIAL;
        goto bail;
    }
    if ((current_role == STANDALONE) &&
        (port_peer_local_local_up_in(&(port_info->peer_local_fsm)))) {
        state = ACTIVE_FULL;
    }
    if (port_peer_local_local_up_in(&(port_info->peer_local_fsm)) &&
        port_peer_remote_remotes_up_in(&(port_info->peer_remote_fsm))) {
        state = ACTIVE_FULL;
        goto bail;
    }

bail:
    return state;
}

/*
 *  This function is a getter of port states
 *
 * @param[in] port_info - specific port DB item
 * @param[in,out] data - container for port states response
 *
 * @return 0 if operation completes successfully.
 */
static int
ports_state_update(struct mlag_port_data *port_info, void *data)
{
    int err = 0;

    ASSERT(data != NULL);

    struct ports_get_data *ports_data = (struct ports_get_data *)data;
    if (ports_data->port_num == ports_data->port_max) {
        goto bail;
    }
    ports_data->ports_arr[ports_data->port_num] = port_info->port_id;
    if (ports_data->states_arr != NULL) {
        ports_data->states_arr[ports_data->port_num] =
            port_state_get(port_info);
    }
    ports_data->port_num++;

bail:
    return err;
}

/**
 *  This function gets a mlag port state for a given list.
 *  Caller should allocate memory for the port ids and port
 *  states array and supply their size, then the actual number
 *  of ports (<= size) is returned
 *
 * @param[out] mlag_ports - array of port IDs, allocated by
 *       caller
 * @param[out] states - array of port states, allocated by the
 *       user, if NULL it is ignored
 * @param[in,out] port_num - input the size of the arrays above,
 *       output the actual number of ports given
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_mlag_ports_get(unsigned long *mlag_ports,
                            enum mlag_port_oper_state *states,
                            int *port_num)
{
    int err = 0;
    struct ports_get_data data;

    ASSERT(port_num != NULL);
    ASSERT(mlag_ports != NULL);

    data.port_max = *port_num;
    data.ports_arr = mlag_ports;
    data.states_arr = states;
    data.port_num = 0;
    *port_num = 0;

    err = port_db_foreach(ports_state_update, &data);
    MLAG_BAIL_CHECK_NO_MSG(err);
    *port_num = data.port_num;

bail:
    return err;
}

/**
 * This function gets mlag port state for a given id.
 * Caller should allocate memory for port info.
 *
 * @param[in] port_id - Port id.
 * @param[out] port_info - Port information, allocated by the
 *       user, if NULL it is ignored.
 *
 * @return 0 - Operation completed successfully.
 * @return -ENOENT - Entry not found.
 */
int
port_manager_mlag_port_get(unsigned long port_id,
                           struct mlag_port_info *port_info)
{
    int err = 0;
    struct mlag_port_data *port_entry_info;

    ASSERT(port_info != NULL);

    err = port_db_entry_lock(port_id,
                             &port_entry_info);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "Failed gets port information\n");

        err = port_db_entry_unlock(port_id);
        goto bail;
    }

    port_info->port_id = port_id;
    port_info->port_oper_state = port_state_get(port_entry_info);

    err = port_db_entry_unlock(port_id);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function gets port_manager module counters.
 *  counters are copied to the given struct
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_counters_get(struct mlag_counters *counters)
{
    return port_db_counters_get(counters);
}

/**
 *  This function clears port_manager module counters.
 *
 *
 * @return void
 */
void
port_manager_counters_clear(void)
{
    port_db_counters_clear();
}

/*
 *  This function dumps port master logic FSM state
 *
 * @param[in] fsm - pointer to fsm
 * @param[in] data - an optional dumping function
 *
 * @return 0 if operation completes successfully.
 */
static void
dump_peer_master_fsm(struct port_master_logic *fsm,
                     void (*dump_cb)(const char *, ...))
{
    int val = 0;
    static char *fsm_states[] =
    { "Idle", "Disabled", "Global Down", "Global Up" };

    if (port_master_logic_idle_in(fsm)) {
        val = 0;
    }
    else if (port_master_logic_disabled_in(fsm)) {
        val = 1;
    }
    else if (port_master_logic_global_down_in(fsm)) {
        val = 2;
    }
    else if (port_master_logic_global_up_in(fsm)) {
        val = 3;
    }
    DUMP_OR_LOG("Master Port Logic state [ %s ]\n", fsm_states[val]);
}

/*
 *  This function dumps port local peer logic FSM state
 *
 * @param[in] fsm - pointer to fsm
 * @param[in] data - an optional dumping function
 *
 * @return 0 if operation completes successfully.
 */
static void
dump_peer_local_fsm(struct port_peer_local *fsm,
                    void (*dump_cb)(const char *, ...))
{
    int val = 0;
    static char *fsm_states[] =
    { "Idle", "Global down", "Local Fault", "Local Up" };

    if (port_peer_local_idle_in(fsm)) {
        val = 0;
    }
    else if (port_peer_local_global_down_in(fsm)) {
        val = 1;
    }
    else if (port_peer_local_local_fault_in(fsm)) {
        val = 2;
    }
    else if (port_peer_local_local_up_in(fsm)) {
        val = 3;
    }
    DUMP_OR_LOG("Peer local port Logic state [ %s ]\n", fsm_states[val]);
    DUMP_OR_LOG("Port admin state [ %s ]\n",
                (fsm->admin_state == PORT_DISABLED) ? "Disabled" : "Enabled");
}

/*
 *  This function dumps port remote peer logic FSM state
 *
 * @param[in] fsm - pointer to fsm
 * @param[in] data - an optional dumping function
 *
 * @return 0 if operation completes successfully.
 */
static void
dump_peer_remote_fsm(struct port_peer_remote *fsm,
                     void (*dump_cb)(const char *, ...))
{
    int val = 0;
    static char *fsm_states[] =
    { "Idle", "Global down", "Remote Fault", "Remote Up" };

    if (port_peer_remote_idle_in(fsm)) {
        val = 0;
    }
    else if (port_peer_remote_global_down_in(fsm)) {
        val = 1;
    }
    else if (port_peer_remote_remote_fault_in(fsm)) {
        val = 2;
    }
    else if (port_peer_remote_remotes_up_in(fsm)) {
        val = 3;
    }
    DUMP_OR_LOG("Peer remote port Logic state [ %s ]\n", fsm_states[val]);
}

/*
 *  This function dumps specific port info
 *
 * @param[in] port_info - struct containing port data
 * @param[in] data - an optional dumping function
 *
 * @return 0 if operation completes successfully.
 */
static int
dump_ports_info(struct mlag_port_data *port_info, void *data)
{
    int err = 0;
    void (*dump_cb)(const char *, ...) = data;
    static char *port_modes_str[] = { "Static", "LACP"};

    ASSERT(port_info != NULL);

    DUMP_OR_LOG("Port ID [%lu] [0x%lx] Type [%s]\n", port_info->port_id,
                port_info->port_id, port_modes_str[port_info->port_mode]);
    DUMP_OR_LOG("===========================\n");
    DUMP_OR_LOG("peers_conf state [0x%x] peers_state [0x%x]\n",
                port_info->peers_conf_state, port_info->peers_oper_state);
    if (IS_MASTER()) {
        dump_peer_master_fsm(&port_info->port_master_fsm, dump_cb);
    }
    dump_peer_local_fsm(&port_info->peer_local_fsm, dump_cb);
    dump_peer_remote_fsm(&port_info->peer_remote_fsm, dump_cb);
    DUMP_OR_LOG("---------------------------\n");

bail:
    return err;
}

/**
 *  This function cause port dump, either using print_cb
 *  Or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;
    uint32_t peer_states;
    struct mlag_counters mlag_counters;

    err = port_db_peer_state_vector_get(&peer_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting peer states vector\n");

    DUMP_OR_LOG("=================\nPort manager Dump\n=================\n");
    DUMP_OR_LOG("started [%d] current role [%d]\n", started, current_role);
    DUMP_OR_LOG("In split brain [%s]\n",
                (in_split_brain == TRUE) ? "TRUE" : "FALSE");
    DUMP_OR_LOG("---------------------------\n");
    DUMP_OR_LOG("Port manager counters \n");
    err = port_manager_counters_get(&mlag_counters);
    if (err) {
        DUMP_OR_LOG("Error retrieving counters \n");
    }
    else {
        DUMP_OR_LOG("Tx PDU : %llu \n", mlag_counters.tx_port_notification);
        DUMP_OR_LOG("Rx PDU : %llu \n", mlag_counters.rx_port_notification);
    }
    DUMP_OR_LOG("---------------------------\n");
    DUMP_OR_LOG("Peers states [0x%x]\n", peer_states);
    DUMP_OR_LOG("---------------------------\n");
    err = port_db_foreach(dump_ports_info, dump_cb);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function sets port mode, either static LAG or LACP LAG
 *
 * @param[in] port_id - mlag port id
 * @param[in] port_mode - static or LACP LAG
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_port_mode_set(unsigned long port_id,
                           enum mlag_port_mode port_mode)
{
    int err = 0;
    struct mlag_port_data *port_info;

    err = port_db_entry_lock(port_id, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "port [%lu] not found\n", port_id);

    port_info->port_mode = port_mode;

    err = port_db_entry_unlock(port_id);
    MLAG_BAIL_ERROR_MSG(err, "Error in port DB unlock\n");

bail:
    return err;
}

/**
 *  This function gets port mode, either static LAG or LACP LAG
 *
 * @param[in] port_id - mlag port id
 * @param[out] port_mode - static or LACP LAG
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_port_mode_get(unsigned long port_id,
                           enum mlag_port_mode *port_mode)
{
    int err = 0;
    struct mlag_port_data *port_info;

    ASSERT(port_mode != NULL);

    err = port_db_entry_lock(port_id, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "port [%lu] not found\n", port_id);

    *port_mode = port_info->port_mode;

    err = port_db_entry_unlock(port_id);
    MLAG_BAIL_ERROR_MSG(err, "Error in port DB unlock\n");

bail:
    return err;
}

/**
 *  This function handles LACP system id change in Slave
 *
 * @return 0 if operation completes successfully.
 */
int
port_manager_lacp_sys_id_update_handle(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    MLAG_LOG(MLAG_LOG_NOTICE,
             "got lacp sync id update, toggle all mlag lacp ports\n");

    err = port_db_foreach(lacp_port_toggle, NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed in lacp ports state toggle\n");

bail:
    return err;
}
