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
#include <errno.h>
#include <unistd.h>
#include <complib/cl_init.h>
#include <complib/cl_timer.h>
#include <complib/cl_event.h>
#include <utils/mlag_log.h>
#include "lib_commu.h"
#include <utils/mlag_events.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include <libs/port_manager/port_manager.h>
#include <libs/health_manager/health_manager.h>
#include <libs/mlag_topology/mlag_topology.h>
#include "mlag_manager.h"
#include "mlag_manager_db.h"
#include "mlag_peering_fsm.h"
#include "mlag_common.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_dispatcher.h"

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_MANAGER

#define MLAG_MANAGER_C
#include <utils/mlag_bail.h>

/************************************************
 *  Local Macros
 ***********************************************/
#define MLAG_MANAGER_STOP_TIMEOUT 5000000 /* 5 sec */
#define MLAG_MANAGER_PORT_DELETE_DONE_TIMEOUT 5000000 /* 5 sec */

/************************************************
 *  Local Type definitions
 ***********************************************/
static int
rcv_msg_handler(uint8_t *data);

static int
net_order_msg_handler(uint8_t *data, int oper);

/************************************************
 *  Global variables
 ***********************************************/
/************************************************
 *  Local variables
 ***********************************************/

static handler_command_t mlag_mngr_ibc_msgs[] = {
    {MLAG_PEER_START_EVENT, "Peer start event", rcv_msg_handler,
     net_order_msg_handler },
    {MLAG_PEER_ENABLE_EVENT, "Peer Enable event", rcv_msg_handler,
     net_order_msg_handler},

    {0, "", NULL, NULL}
};

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_DEBUG;

static int current_role = NONE;
static uint8_t my_mlag_id;
static int slave_ports_enabled = FALSE;
static int reload_delay_done = FALSE; /* FALSE = not done, TRUE = done */
static int peer_enabled_event = FALSE; /* FALSE = not enabled, TRUE = enabled */
static int start_event = FALSE; /* FALSE = not enabled, TRUE = enabled */
static cl_timer_t reload_delay_timer;
static struct mlag_peering_fsm peering_fsm[MLAG_MAX_PEERS];
static uint32_t peer_states;
struct mlag_manager_counters counters;

cl_event_t stop_done_cl_event;
static int stop_done_states;
cl_event_t port_delete_done_cl_event;

/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function returns module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_manager_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_manager_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    mlag_manager_db_log_verbosity_set(verbosity);
    mlag_peering_fsm_log_verbosity_set(verbosity);
}

/*
 *  This function increments counter according to name
 *
 *  @param cnt - counter name
 *
 * @return void
 */
static void
mm_counters_inc(enum mm_counters cnt)
{
    if (cnt == MM_CNT_PROTOCOL_TX) {
        counters.tx_protocol_msg++;
    }
    else if (cnt == MM_CNT_PROTOCOL_RX) {
        counters.rx_protocol_msg++;
    }
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
peering_fsm_user_trace(char *buf, int len)
{
    UNUSED_PARAM(len);
    MLAG_LOG(MLAG_LOG_NOTICE, "mlag peering %s\n", buf);
}

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
mlag_manager_message_send(enum mlag_events opcode, void* payload,
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
        mm_counters_inc(PM_CNT_PROTOCOL_TX);
    }

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
    struct recv_payload_data *payload_data = (struct recv_payload_data*) data;
    uint16_t opcode;
    struct peer_state_change_data *state_change;

    opcode = *((uint16_t*)(payload_data->payload[0]));

    MLAG_LOG(MLAG_LOG_INFO,
             "port manager rcv_msg_handler: opcode=%d\n", opcode);

    mm_counters_inc(MM_CNT_PROTOCOL_RX);

    switch (opcode) {
    case MLAG_PEER_START_EVENT:
        state_change =
            (struct peer_state_change_data *) (payload_data->payload[0]);
        MLAG_LOG(MLAG_LOG_NOTICE, "Peer start ID [%d]\n",
                 state_change->mlag_id);
        err = send_system_event(MLAG_PEER_START_EVENT, state_change,
                                sizeof(struct peer_state_change_data));
        MLAG_BAIL_ERROR_MSG(err, "Failed sending peer start sys event\n");
        break;
    case MLAG_PEER_ENABLE_EVENT:
        state_change =
            (struct peer_state_change_data *) (payload_data->payload[0]);
        MLAG_LOG(MLAG_LOG_NOTICE, "Peer enable ID [%d]\n",
                 state_change->mlag_id);
        /* mark that ports were enabled on this machine */
        slave_ports_enabled = TRUE;
        err = send_system_event(MLAG_PEER_ENABLE_EVENT, state_change,
                                sizeof(struct peer_state_change_data));
        MLAG_BAIL_ERROR_MSG(err, "Failed sending peer enable sys event\n");
        break;
    default:
        /* Unknown opcode */
        MLAG_LOG(MLAG_LOG_ERROR, "Unknown opcode [%d] in mlag manager\n",
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
 * @param[in] oper - conversion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_peer_state_change_data(struct peer_state_change_data *data, int oper)
{
    if (oper == MESSAGE_SENDING) {
        data->mlag_id = htonl(data->mlag_id);
        data->state = htonl(data->state);
    }
    else {
        data->mlag_id = ntohl(data->mlag_id);
        data->state = ntohl(data->state);
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
    struct peer_state_change_data *state_change;

    if (oper == MESSAGE_SENDING) {
        opcode = *(uint16_t*)data;
        *(uint16_t*)data = htons(opcode);
    }
    else {
        opcode = ntohs(*(uint16_t*)data);
        *(uint16_t*)data = opcode;
    }

    switch (opcode) {
    case MLAG_PEER_START_EVENT:
        state_change =
            (struct peer_state_change_data *) (data);
        net_order_peer_state_change_data(state_change, oper);
        break;
    case MLAG_PEER_ENABLE_EVENT:
        state_change =
            (struct peer_state_change_data *) (data);
        net_order_peer_state_change_data(state_change, oper);
        break;
    default:
        /* Unknown opcode */
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Unknown opcode [%u] in mlag manager\n",
                            opcode);
        break;
    }

bail:
    return err;
}

/*
 * This function sets the whether the peer get enabled event.
 *
 * @param[in] state - State. Peer enable indication.
 *
 * @return 0 - Operation completed successfully.
 */
static int
mlag_manager_peer_enable_event_set(int state)
{
    int err = 0;

    peer_enabled_event = state;

    goto bail;

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
peer_state_set(uint32_t *state_vector, int peer_id, uint32_t state)
{
    uint32_t idx = peer_id;
    *state_vector &= ~(1 << idx);
    *state_vector |= ((state & 1) << peer_id);
}

/*
 *  This function implements timer event handling
 *  for reload delay timer
 *
 * @param[in] data - timer event data
 *
 * @return void
 */
static void
reload_delay_expired_cb(void *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    MLAG_LOG(MLAG_LOG_NOTICE, "*** Reload delay expired ***\n");

    err = send_system_event(MLAG_PEER_RELOAD_DELAY_EXPIRED, NULL, 0);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "Failed in sending time tick event\n");
    }

    reload_delay_done = TRUE;
}

/*
 *  This function implements peering FSM notification that
 *  sync is starting
 *
 * @param[in] peer_id - local peer index
 *
 * @return void
 */
static int
peer_sync_start_cb(int peer_id)
{
    int err = 0;
    struct peer_state_change_data peer_start;

    peer_start.mlag_id = peer_id;
    /* send message - peer start */
    MLAG_LOG(MLAG_LOG_NOTICE, "Send peer start, mlag ID [%d]\n", peer_id);
    err = mlag_manager_message_send(MLAG_PEER_START_EVENT, &peer_start,
                                    sizeof(peer_start), peer_id, MASTER_LOGIC);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending peer [%d] start event\n",
                        peer_id);

bail:
    return err;
}

/*
 *  This function implements peering FSM notification that
 *  sync is done
 *
 * @param[in] peer_id - local peer index
 *
 * @return void
 */
static int
peer_sync_done_cb(int peer_id)
{
    int err = 0;
    struct peer_state_change_data peer_en;
    struct mlag_master_election_status me_status;

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get Master election status in handling of peer sync done event\n");

    /* stop reload delay timer if peer is not local */
    if (peer_id != mlag_manager_db_local_peer_id_get()) {
        cl_timer_stop(&reload_delay_timer);
        reload_delay_done = TRUE;
        /* enable local ports */
        err = mlag_manager_enable_ports();
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in enable ports\n");

        peer_en.mlag_id = peer_id;
        peer_en.state = PEER_ENABLE;
        /* send message to all peers - peer enable */
        err = mlag_manager_message_send(MLAG_PEER_ENABLE_EVENT, &peer_en,
                                        sizeof(peer_en), peer_id,
                                        MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending peer [%d] enable event\n",
                            peer_id);

        err =
            send_system_event(MLAG_PEER_ENABLE_EVENT, &peer_en,
                              sizeof(peer_en));
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending peer enable event\n");
    }

    /* Immediately open new ports when active slave becomes Standalone */
    if ((current_role == STANDALONE) &&
        (me_status.previous_status == SLAVE) &&
        (slave_ports_enabled == TRUE)) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Slave->Std, sync done, enable ports\n");
        err = mlag_manager_enable_ports();
        if (err) {
            MLAG_LOG(MLAG_LOG_ERROR, "Failed in sending peer enable event\n");
            goto bail;
        }
    }

bail:
    return err;
}

/**
 *  This function Inits Mlag protocol
 *
 *  @param insert_msgs_cb - callback for command handler insertion
 *
 * @return int as error code.
 */
int
mlag_manager_init(insert_to_command_db_func insert_msgs_cb)
{
    int err = 0;
    cl_status_t cl_err;
    int peer_id;

    err = insert_msgs_cb(mlag_mngr_ibc_msgs);
    MLAG_BAIL_ERROR(err);

    err = mlag_manager_db_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init mlag manager DB\n");

    /* Init timers */
    cl_err = cl_timer_init(&reload_delay_timer, reload_delay_expired_cb, NULL);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init reload delay timer\n");
    }

    cl_event_construct(&stop_done_cl_event);

    cl_err = cl_event_init(&stop_done_cl_event, FALSE);
    if (cl_err == CL_ERROR) {
        err = cl_err;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init stop_done cl_event\n");
    }
    stop_done_states = 0;

    cl_event_construct(&port_delete_done_cl_event);

    cl_err = cl_event_init(&port_delete_done_cl_event, FALSE);
    if (cl_err == CL_ERROR) {
        err = cl_err;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init port delete_done cl_event\n");
    }

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        err = mlag_peering_fsm_init(&peering_fsm[peer_id],
                                    peering_fsm_user_trace, NULL, NULL);
        MLAG_BAIL_ERROR_MSG(err, "Failed to init peering FSM\n");
    }

bail:
    return err;
}

/**
 *  This function de-Inits Mlag protocol
 *
 * @return int as error code.
 */
int
mlag_manager_deinit(void)
{
    int err = 0;

    err = mlag_manager_db_deinit();
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, " Failed in manager DB deinit err [%d]\n",
                 err);
    }

    cl_timer_destroy(&reload_delay_timer);

    cl_event_destroy(&stop_done_cl_event);
    cl_event_destroy(&port_delete_done_cl_event);

    return err;
}

/**
 *  This function starts Mlag protocol
 *
 *  @param data - event data
 *
 * @return int as error code.
 */
int
mlag_manager_start(uint8_t *data)
{
    int err = 0;
    int i;
    UNUSED_PARAM(data);

    start_event = TRUE;
    struct start_event_data *params = (struct start_event_data *)data;

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        peering_fsm[i].igmp_enabled = params->start_params.igmp_enable;
        peering_fsm[i].lacp_enabled = params->start_params.lacp_enable;
    }

    return err;
}

/**
 *  This function stops Mlag protocol
 *
 *  @param data - event data
 *
 * @return int as error code.
 */
int
mlag_manager_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    cl_timer_stop(&reload_delay_timer);
    reload_delay_done = TRUE;

    current_role = NONE;
    slave_ports_enabled = FALSE;

    mlag_manager_counters_clear();

    start_event = FALSE;

    return err;
}

/*
 *  This function implements master logic from role decision
 *  to peering
 *
 * @return int as error code.
 */
static int
master_start(void)
{
    int err = 0;
    int peer_id, local_peer_id;
    cl_status_t cl_err;

    /* start peering state machine for local peer */
    local_peer_id = mlag_manager_db_local_peer_id_get();

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        err = mlag_peering_fsm_init(&peering_fsm[peer_id],
                                    peering_fsm_user_trace, NULL, NULL);
        MLAG_BAIL_ERROR_MSG(err, "Failed to init peering FSM\n");
        peering_fsm[peer_id].peer_id = peer_id;
        peering_fsm[peer_id].peer_sync_done_cb = peer_sync_done_cb;
        peering_fsm[peer_id].peer_sync_start_cb = peer_sync_start_cb;
    }
    reload_delay_done = FALSE;
    cl_err =
        cl_timer_start(&reload_delay_timer,
                       mlag_manager_db_reload_delay_get());
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to start timer [%u]\n", cl_err);
    }
    MLAG_LOG(MLAG_LOG_DEBUG, "Wait [%d] msec (Reload delay) \n",
             mlag_manager_db_reload_delay_get());

    /* Start local Peer sync */
    err = mlag_peering_fsm_peer_up_ev(&peering_fsm[local_peer_id]);
    MLAG_BAIL_ERROR_MSG(err, "Failed to start peering with local\n");
    err = mlag_peering_fsm_peer_conn_ev(&peering_fsm[local_peer_id]);
    MLAG_BAIL_ERROR_MSG(err, "Failed to start peering with local\n");

bail:
    return err;
}

/*
 *  This function implements standalone logic from role decision
 *  to peering
 *
 *  @param previous_role - this peer's previous role
 *
 * @return int as error code.
 */
static int
standalone_start(int previous_role)
{
    int err = 0;
    int local_peer_id;
    cl_status_t cl_err;

    /* start peering state machine for local peer */
    local_peer_id = mlag_manager_db_local_peer_id_get();
    err = mlag_peering_fsm_init(&peering_fsm[local_peer_id],
                                peering_fsm_user_trace, NULL, NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed to init peering FSM\n");
    peering_fsm[local_peer_id].peer_id = local_peer_id;
    peering_fsm[local_peer_id].peer_sync_done_cb = peer_sync_done_cb;
    peering_fsm[local_peer_id].peer_sync_start_cb = peer_sync_start_cb;

    /* Standalone waits reload delay until port enable
     * If previous role was not slave or if slave was not active
     */
    if ((previous_role != SLAVE) ||
        ((previous_role == SLAVE) && (slave_ports_enabled == FALSE))) {
        reload_delay_done = FALSE;
        cl_err =
            cl_timer_start(&reload_delay_timer,
                           mlag_manager_db_reload_delay_get());
        if (cl_err != CL_SUCCESS) {
            err = -EIO;
            MLAG_BAIL_ERROR_MSG(err, "Failed to start timer [%u]\n", cl_err);
        }
        MLAG_LOG(MLAG_LOG_DEBUG, "Wait [%d] msec (Reload delay) \n",
                 mlag_manager_db_reload_delay_get());
    }

    /* Start local Peer sync */
    err = mlag_peering_fsm_peer_up_ev(&peering_fsm[local_peer_id]);
    MLAG_BAIL_ERROR_MSG(err, "Failed to start peering with local\n");
    err = mlag_peering_fsm_peer_conn_ev(&peering_fsm[local_peer_id]);
    MLAG_BAIL_ERROR_MSG(err, "Failed to start peering with local\n");

bail:
    return err;
}

/*
 *  This function implements standalone logic from role decision
 *  to peering
 *
 * @return int as error code.
 */
static int
slave_start(void)
{
    int err = 0;
    struct mlag_master_election_status me_status;

    cl_timer_stop(&reload_delay_timer);
    reload_delay_done = TRUE;

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    MLAG_LOG(MLAG_LOG_NOTICE, "Slave, mlag_id [%d], waiting.. \n",
             me_status.my_peer_id);

bail:
    return err;
}

/**
 *  This function notifies new peer role
 *
 * @param[in] new_role - peer new role
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_role_change(int previous_role, int new_role)
{
    int err = 0;
    struct mlag_master_election_status me_status;

    MLAG_LOG(MLAG_LOG_NOTICE, "Role change from [%d] to [%d] \n",
             previous_role, new_role);
    current_role = new_role;

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    my_mlag_id = me_status.my_peer_id;

    switch (new_role) {
    case MASTER:
        err = master_start();
        MLAG_BAIL_ERROR_MSG(err, "Mlag manager master start failed\n");
        break;
    case STANDALONE:
        err = standalone_start(previous_role);
        MLAG_BAIL_ERROR_MSG(err, "Mlag manager standalone start failed\n");
        break;
    case SLAVE:
        slave_ports_enabled = FALSE;
        err = slave_start();
        MLAG_BAIL_ERROR_MSG(err, "Mlag manager slave start failed\n");
        break;
    }

bail:
    return err;
}

/**
 *  This function handles peer health change
 *
 * @param[in] state_change - peer state change notification
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_status_change(struct peer_state_change_data *state_change)
{
    int err = 0;

    if (state_change->state == HEALTH_PEER_UP) {
        peer_state_set(&peer_states, state_change->mlag_id, TRUE);
        err = mlag_peering_fsm_peer_up_ev(&peering_fsm[state_change->mlag_id]);
        MLAG_BAIL_ERROR_MSG(err, "Failed to start peering with peer [%d]\n",
                            state_change->mlag_id);
    }
    else if ((state_change->state == HEALTH_PEER_DOWN) ||
             (state_change->state == HEALTH_PEER_DOWN_WAIT) ||
             (state_change->state == HEALTH_PEER_COMM_DOWN)) {
        /* Peer down */
        peer_state_set(&peer_states, state_change->mlag_id, FALSE);
        /* unset slave global variable peer enable event */
        err = mlag_manager_peer_enable_event_set(FALSE);
        MLAG_BAIL_ERROR(err);

        if (current_role == MASTER) {
            /* Init the peering FSM for the peer */
            err =
                mlag_peering_fsm_peer_down_ev(&peering_fsm[state_change->
                                                           mlag_id]);
            MLAG_BAIL_ERROR_MSG(err, "Failed to stop peering with peer [%d]\n",
                                state_change->mlag_id);
        }
    }

bail:
    return err;
}

/**
 *  This function notifies module sync done to mlag
 *  manager. when all sync are done a peer becomes enabled
 *
 * @param[in] peer_id - peer index
 * @param[in] synced_module - identifies module that finished sync
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_sync_done_notify(int peer_id,
                              enum peering_sync_type synced_module)
{
    int err = 0;

    if (current_role == SLAVE) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Got sync done on slave on peer_id [%d] from module [%d]\n",
                 peer_id, synced_module);
        goto bail;
    }

    err =
        mlag_peering_fsm_sync_arrived_ev(&peering_fsm[peer_id], synced_module);
    MLAG_BAIL_ERROR_MSG(err, "Processing sync event from module [%d] failed\n",
                        synced_module);

bail:
    return err;
}

/**
 *  This function handles peer add event
 *  It takes care of synchronizing modules activity on peer add
 *
 * @param[in] data - peer add event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_add(struct peer_conf_event_data *data)
{
    int err = 0;

    err = send_system_event(MLAG_PEER_ID_SET_EVENT, data,
                            sizeof(struct peer_conf_event_data));
    MLAG_BAIL_ERROR_MSG(err, "Failed to send peer id set event\n");

bail:
    return err;
}

/**
 *  This function notifies a new peer has successfully connected
 *  to this entity (Master)
 *
 * @param[in] conn - connection info
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_connected(struct tcp_conn_notification_event_data *conn)
{
    int err = 0;
    int mlag_id;

    ASSERT(conn != NULL);

    if (current_role != MASTER) {
        goto bail;
    }

    err = mlag_manager_db_mlag_peer_id_get(ntohl(conn->ipv4_addr), &mlag_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting peer id from ip [0x%x]\n",
                        ntohl(conn->ipv4_addr));

    MLAG_LOG(MLAG_LOG_NOTICE, "start peering with mlag_ID [%d] IP [0x%x]\n",
             mlag_id, ntohl(conn->ipv4_addr));

    err = mlag_peering_fsm_peer_conn_ev(&peering_fsm[mlag_id]);
    MLAG_BAIL_ERROR_MSG(err, "Failed to start peering with peer [%d]\n",
                        mlag_id);

bail:
    return err;
}

/**
 *  This function will allow local ports to enable
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_enable_ports(void)
{
    int err = 0;
    struct peer_state_change_data peer_state;

    MLAG_LOG(MLAG_LOG_DEBUG, "Mlag Manager : Enable Ports \n");

    /* Send Local peer enable event */
    peer_state.mlag_id = mlag_manager_db_local_peer_id_get();
    peer_state.state = PEER_ENABLE;

    err = send_system_event(MLAG_PEER_ENABLE_EVENT, &peer_state,
                            sizeof(peer_state));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending peer enable event\n");

bail:
    return err;
}

/*
 *  This function return string that describes the state of the peering FSM
 *  state
 *
 * @param[in] fsm - peering FSM
 *
 * @return string - FSM state
 */
static char*
get_peering_fsm_state_str(mlag_peering_fsm *fsm)
{
    if (mlag_peering_fsm_configured_in(fsm)) {
        return "Configured";
    }
    if (mlag_peering_fsm_peering_in(fsm)) {
        return "Peering";
    }
    if (mlag_peering_fsm_peer_up_in(fsm)) {
        return "Peer synced";
    }
    return "Unknown";
}

/*
 *  This function dumps per peer information
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
static void
dump_peers_info(void (*dump_cb)(const char *, ...))
{
    uint32_t peer_id;
    int err = 0;
    int mlag_id;

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        err = mlag_manager_db_mlag_id_from_local_index_get(peer_id, &mlag_id);
        if (err != -ENOENT) {
            DUMP_OR_LOG("MLAG ID [%d] \n", mlag_id);
            DUMP_OR_LOG("===========================\n");
            DUMP_OR_LOG("Peering FSM state [%s] sync_states [0x%x]\n",
                        get_peering_fsm_state_str(&peering_fsm[peer_id]),
                        (&peering_fsm[peer_id])->sync_states);
        }
    }
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
mlag_manager_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;
    unsigned long port_id;
    char *role_string[] = {"Master", "Slave", "Standalone", "N/A" };
    struct mlag_counters mlag_counters;

    DUMP_OR_LOG("=================\nMlag manager Dump\n=================\n");
    DUMP_OR_LOG("current role [%d - %s]\n", current_role,
                role_string[current_role]);
    DUMP_OR_LOG("---------------------------\n");
    err = mlag_manager_counters_get(&mlag_counters);
    if (err) {
        DUMP_OR_LOG("Error retrieving counters \n");
    }
    else {
        DUMP_OR_LOG("Tx PDU : %llu \n", mlag_counters.tx_mlag_notification);
        DUMP_OR_LOG("Rx PDU : %llu \n", mlag_counters.rx_mlag_notification);
    }
    DUMP_OR_LOG("---------------------------\n");
    DUMP_OR_LOG("Peers states [0x%x]\n", peer_states);
    DUMP_OR_LOG("---------------------------\n");
    err = mlag_topology_ipl_port_get(0, &port_id);
    if (err == 0) {
        DUMP_OR_LOG("IPL info : IPL ID [0] port [%lu]\n", port_id);
    }
    else {
        DUMP_OR_LOG("IPL Not configured\n");
        err = 0;
    }
    dump_peers_info(dump_cb);
    DUMP_OR_LOG("Stop done states : [%d]\n", stop_done_states);

    return err;
}

/**
 * This function sets the reload delay interval.
 *
 * @param[in] interval_msec - interval in milliseconds.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_reload_delay_interval_set(unsigned int interval_msec)
{
    int err = 0;

    mlag_manager_db_reload_delay_set(interval_msec);
    goto bail;

bail:
    return err;
}

/**
 * This function gets the reload delay interval.
 *
 * @param[out] interval_msec - Interval in milliseconds.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_reload_delay_interval_get(unsigned int *interval_msec)
{
    int err = 0;

    ASSERT(interval_msec != NULL);

    *interval_msec = mlag_manager_db_reload_delay_get();
    goto bail;

bail:
    return err;
}

/**
 * This function gets the protocol operational state.
 *
 * @param[out] protocol_oper_state - Protocol operational state. Pointer to an already allocated memory
 *                                   structure.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_protocol_oper_state_get(
    enum protocol_oper_state *protocol_oper_state)
{
    int err = 0;
    unsigned int i = 0;
    struct peer_state peers_list[MLAG_MAX_PEERS];
    unsigned int peers_list_cnt = MLAG_MAX_PEERS;

    ASSERT(protocol_oper_state != NULL);

    if (!start_event) {
        *protocol_oper_state = MLAG_INIT;
    }
    else {
        if (reload_delay_done) {
            if (current_role == MLAG_MASTER) {
                *protocol_oper_state = MLAG_UP;
            }
            else if (current_role == MLAG_SLAVE) {
                err = mlag_manager_peers_state_list_get(peers_list,
                                                        &peers_list_cnt);
                MLAG_BAIL_ERROR_MSG(err, "Failed in getting peers state\n");

                *protocol_oper_state = MLAG_UP;

                for (i = 0; i < peers_list_cnt; i++) {
                    if (peers_list[i].peer_state == MLAG_PEER_DOWN) {
                        *protocol_oper_state = MLAG_DOWN;
                        break;
                    }
                }
            }
            else {
                /* standalone and none are configured with DOWN state */
                *protocol_oper_state = MLAG_DOWN;
            }
        }
        else {
            *protocol_oper_state = MLAG_RELOAD_DELAY;
        }
    }

bail:
    return err;
}

/**
 * This function gets the current system role state.
 *
 * @param[out] system_role - System role state. Pointer to an already allocated memory
 *                           structure.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_system_role_state_get(enum system_role_state *system_role_state)
{
    int err = 0;

    ASSERT(system_role_state != NULL);

    *system_role_state = current_role;

    goto bail;

bail:
    return err;
}

/**
 * This function gets the current peers list state.
 *
 * @param[out] peers_list - Peers list state. Pointer to an already allocated memory
 *                          structure.
 * @param[in,out] peers_list_cnt - Number of peers to retrieve. If fewer peers have been
 *                                 successfully retrieved, then peers_list_cnt will contain the
 *                                 number of successfully retrieved peers.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_peers_state_list_get(struct peer_state *peers_list,
                                  unsigned int *peers_list_cnt)
{
    int err = 0;
    uint32_t peer_id = 0;
    uint32_t count = 0;
    int peer_states[MLAG_MAX_PEERS];
    int local_peer_id;

    ASSERT(peers_list != NULL);

    memset(peer_states, 0, MLAG_MAX_PEERS);

    if (current_role == MLAG_MASTER) {
        for (peer_id =
                 0; (peer_id < *peers_list_cnt) && (peer_id < MLAG_MAX_PEERS);
             peer_id++) {
            peers_list[peer_id].system_peer_id =
                mlag_manager_db_peer_system_id_get(peer_id);

            if (peers_list[peer_id].system_peer_id == 0) {
                continue;
            }

            if (mlag_peering_fsm_configured_in(&peering_fsm[peer_id])) {
                peers_list[peer_id].peer_state = MLAG_PEER_DOWN;
            }
            else if (mlag_peering_fsm_peering_in(&peering_fsm[peer_id])) {
                peers_list[peer_id].peer_state = MLAG_PEER_PEERING;
            }
            else {
                peers_list[peer_id].peer_state = MLAG_PEER_UP;
            }

            count++;
        }

        *peers_list_cnt = count;
    }
    else if ((current_role == MLAG_STANDALONE) ||
             (current_role == MLAG_NONE)) {
        /* standalone or none role does not have peers */
        *peers_list_cnt = 0;
    }
    else {
        err = health_manager_peer_states_get(peer_states);
        MLAG_BAIL_ERROR_MSG(err, "Failed in getting peer states\n");

        local_peer_id = mlag_manager_db_local_peer_id_get();

        for (peer_id =
                 0; (peer_id < *peers_list_cnt) && (peer_id < MLAG_MAX_PEERS);
             peer_id++) {
            if (peer_id == (unsigned int)local_peer_id) {
                /* Assume slave has only 1 peer */
                /* first get local state */
                if ((peer_states[peer_id] != HEALTH_PEER_UP) &&
                    (peer_states[peer_id] != HEALTH_PEER_NOT_EXIST)) {
                    peers_list[peer_id].peer_state = MLAG_PEER_DOWN;
                }
                else if (!peer_enabled_event) {
                    peers_list[peer_id].peer_state = MLAG_PEER_PEERING;
                }
                else {
                    peers_list[peer_id].peer_state = MLAG_PEER_UP;
                }
            }
            else {
                /* return master state according to health state */
                if (peer_states[peer_id] == HEALTH_PEER_UP) {
                    peers_list[peer_id].peer_state = MLAG_PEER_UP;
                }
                else {
                    peers_list[peer_id].peer_state = MLAG_PEER_DOWN;
                }
            }

            peers_list[peer_id].system_peer_id =
                mlag_manager_db_peer_system_id_get(peer_id);

            if (peers_list[peer_id].system_peer_id == 0) {
                continue;
            }

            count++;
        }

        *peers_list_cnt = count;
    }

bail:
    return err;
}

/**
 * This function handles peer delete sync event
 *
 * @param[in] event - event data
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_peer_delete_handle(uint8_t *event)
{
    int err = 0;
    struct peer_conf_event_data *peer_conf =
        (struct peer_conf_event_data *) event;

    err = mlag_manager_db_peer_delete(peer_conf->peer_ip);
    mlag_manager_db_unlock();
    MLAG_BAIL_ERROR_MSG(err, "Failed to delete Peer [0x%x]\n",
                        peer_conf->peer_ip);

bail:
    return err;
}

/*
 *  This function checks all modules stop done was accepted
 *
 * @param[in] sync_type - last arrived sync type
 *
 * @return 1 if all done, 0 - not yet.
 */
static int
mlag_manager_is_all_stop_done_arrived(int stop_done_type)
{
    stop_done_states |= stop_done_type;
    if ((stop_done_states & ALL_STOP_DONE) == ALL_STOP_DONE) {
        stop_done_states = 0;
        return TRUE;
    }
    return FALSE;
}

/**
 * This function waits for stop process done
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_wait_for_stop_done(void)
{
    int err = 0;
    cl_status_t cl_err = CL_SUCCESS;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Waiting for stop done event\n");

    cl_err = cl_event_wait_on(&stop_done_cl_event,
                              MLAG_MANAGER_STOP_TIMEOUT, 1);
    if (cl_err == CL_TIMEOUT) {
        err = cl_err;
        MLAG_LOG(MLAG_LOG_WARNING,
                 "Timeout in waiting for stop done event\n");
        goto bail;
    }
    if (cl_err == CL_ERROR) {
        err = cl_err;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in wait for stop_done cl_event, err=%d\n",
                            err);
    }

bail:
    return err;
}

/**
 * This function handles stop done event
 *
 * @param[in] event - event data
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_stop_done_handle(uint8_t *event)
{
    int err = 0;
    cl_status_t cl_err = CL_SUCCESS;
    struct sync_event_data *stop_done = (struct sync_event_data *) event;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Stop done event accepted from [%d] client\n",
             stop_done->sync_type);

    if (mlag_manager_is_all_stop_done_arrived(stop_done->sync_type)) {
        /* All done */
        cl_err = cl_event_signal(&stop_done_cl_event);
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Stop done events accepted from all clients\n");
        if (cl_err == CL_ERROR) {
            err = cl_err;
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to signal stop_done cl_event, err=%d\n",
                                err);
        }
    }

bail:
    return err;
}

/**
 * This function waits for port delete process done
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_wait_for_port_delete_done(void)
{
    int err = 0;
    cl_status_t cl_err = CL_SUCCESS;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Waiting for port delete done event\n");

    cl_err = cl_event_wait_on(&port_delete_done_cl_event,
                              MLAG_MANAGER_PORT_DELETE_DONE_TIMEOUT, 1);
    if (cl_err == CL_TIMEOUT) {
        err = cl_err;
        MLAG_LOG(MLAG_LOG_WARNING,
                 "Timeout in waiting on port delete done event\n");
        goto bail;
    }
    if (cl_err == CL_ERROR) {
        err = cl_err;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in waiting for port delete done event, err=%d\n",
                            err);
    }

bail:
    return err;
}

/**
 * This function handles port delete done event
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_port_delete_done(void)
{
    int err = 0;
    cl_status_t cl_err = CL_SUCCESS;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Port delete done event accepted\n");

    cl_err = cl_event_signal(&port_delete_done_cl_event);
    if (cl_err == CL_ERROR) {
        err = cl_err;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to signal port delete_done cl_event, err=%d\n",
                            err);
    }

bail:
    return err;
}

/**
 * This function handles peer enable event.
 *
 * @param[in] data - Event data.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_enable(struct peer_state_change_data *data)
{
    int err = 0;

    UNUSED_PARAM(data);

    /* set slave global variable peer enable event */
    err = mlag_manager_peer_enable_event_set(TRUE);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function gets mlag_manager module counters.
 *  counters are copied to the given struct
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_counters_get(struct mlag_counters *mlag_counters)
{
    int err = 0;

    mlag_counters->rx_mlag_notification = counters.rx_protocol_msg;
    mlag_counters->tx_mlag_notification = counters.tx_protocol_msg;

    return err;
}

/**
 * This function clears port_manager module counters.
 *
 * @return void
 */
void
mlag_manager_counters_clear(void)
{
    SAFE_MEMSET(&counters, 0);
}

/**
 * This function check if the given local id represent a valid peer.
 *
 * @param[in] local_index - Peer local index
 *
 * @return true if peer exists. Otherwise, false.
 */
int
mlag_manager_is_peer_valid(int local_index)
{
    int is_peer_valid = FALSE;

    if (mlag_manager_db_peer_system_id_get(local_index)) {
        is_peer_valid = TRUE;
    }

    return is_peer_valid;
}
