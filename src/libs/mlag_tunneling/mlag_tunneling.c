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
#include <complib/cl_init.h>
#include <complib/cl_thread.h>
#include <net/ethernet.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include "lib_event_disp.h"
#include "lib_commu.h"
#include "mlag_manager_db.h"
#include "mlag_manager.h"
#include "mlag_common.h"
#include "service_layer.h"
#include "health_manager.h"
#include "mlag_master_election.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_topology.h"
#include "mlag_api_defs.h"
#include "mlag_tunneling.h"
#include "port_manager.h"

/************************************************
 *  Local Defines
 ***********************************************/
#undef  __MODULE__
#define __MODULE__ MLAG_TUNNELING

#define MAX_PACKET_SIZE 9600

#define TUNNEL_DISP_SYS_EVENT_BUF_SIZE 1500

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/
struct tunneling_counters {
    unsigned long long received_from_hw;
    unsigned long long sent_queries;
    unsigned long long received_from_peer;
    unsigned long long sent_to_peer;
};

struct __attribute__((__packed__)) igmp_packet_wrapper {
    int opcode;
    int sending_peer_id;
    struct sl_trap_receive_info receive_info;
    unsigned long pkt_size;
    uint8_t packet_buffer[MAX_PACKET_SIZE];
};

enum {
    TERMINATE_HANDLE,
    EVENTS_HANDLE,
    TCP_HANDLE,
    HW_HANDLE,
    NUM_OF_HANDLES
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local function declarations
 ***********************************************/

static int
tunnel_dispatch_peer_start_event(uint8_t *buffer);

static int
tunnel_dispatch_peer_state_change_event(uint8_t *buffer);

static int
tunnel_dispatch_port_state_change(uint8_t *buffer);

static int
tunnel_dispatch_status_change(uint8_t *buffer);

static int
tunnel_dispatch_conn_notify_event(uint8_t *data);

static int
tunnel_dispatch_start(uint8_t *buffer);

static int
tunnel_dispatch_stop(uint8_t *buffer);

static int
tunnel_dispatch_reconnect_event(uint8_t *data);

/************************************************
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

static int igmp_enabled = 0;

static int medium_prio_events[] =
{
    MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
    MLAG_PEER_START_EVENT,
    MLAG_PEER_STATE_CHANGE_EVENT,
    MLAG_CONN_NOTIFY_EVENT,
    MLAG_RECONNECT_EVENT,
    MLAG_PORT_OPER_STATE_CHANGE_EVENT,
    MLAG_START_EVENT,
    MLAG_STOP_EVENT
};

static event_disp_fds_t tunnel_event_fds;

static cmd_db_handle_t *tunnel_cmd_db;

static cl_thread_t tunneling_thread;

static handler_command_t tunnel_dispatcher_commands[] = {
    {MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT, "Status change event",
     tunnel_dispatch_status_change, NULL},
    {MLAG_PEER_START_EVENT, "Peer start", tunnel_dispatch_peer_start_event,
     NULL},
    {MLAG_PEER_STATE_CHANGE_EVENT, "Peer state change event",
     tunnel_dispatch_peer_state_change_event, NULL},
    {MLAG_CONN_NOTIFY_EVENT, "Conn notify event",
     tunnel_dispatch_conn_notify_event, NULL},
    {MLAG_RECONNECT_EVENT, "Reconnect event",
     tunnel_dispatch_reconnect_event, NULL},
    {MLAG_PORT_OPER_STATE_CHANGE_EVENT, "Port oper state change event",
     tunnel_dispatch_port_state_change, NULL},
    {MLAG_START_EVENT, "Start event", tunnel_dispatch_start, NULL},
    {MLAG_STOP_EVENT, "Stop event", tunnel_dispatch_stop, NULL},
    {0, "", NULL, NULL}
};

static struct dispatcher_conf tunnel_dispatcher_conf;

static char tunnel_disp_sys_event_buf[TUNNEL_DISP_SYS_EVENT_BUF_SIZE];

static struct tunneling_counters counters;

static struct mlag_comm_layer_wrapper_data comm_layer_wrapper;

static unsigned int peer_connected;

static int sl_fd;

/* This parameter will be used only from the tunneling thread, and therefore
 * can be a singleton.
 */
static struct igmp_packet_wrapper curr_packet;

struct sl_api_ctrl_pkt_data igmp_query_pkt_data;

/************************************************
 *  Function implementations
 ***********************************************/
/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_tunneling_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_tunneling_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/*
 *  This function is called to handle received message from peer
 *
 * @param[in] igmp_packet_wrapper - the IGMP packet wrapper
 * @param[in] ifindex - the ifindex to sent to
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
pkt_send_loopback_wrapper(struct igmp_packet_wrapper *packet_wrapper,
                          unsigned long ifindex)
{
    int err = 0;

    err = sl_api_pkt_send_loopback_ctrl(sl_fd,
                                        packet_wrapper->packet_buffer,
                                        packet_wrapper->pkt_size,
                                        packet_wrapper->receive_info.l2_trap_id,
                                        ifindex);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to send packet to the HW. Size=%lu, trap_id=%d, source port id=%lu. Err=%d\n",
                        packet_wrapper->pkt_size,
                        packet_wrapper->receive_info.l2_trap_id,
                        ifindex,
                        err);
bail:
    return err;
}

/*
 *  This function is called to handle received message from peer
 *
 * @param[in] ad_info - address info
 * @param[in] payload_data - received data
 *
 * @return 0 when successful, otherwise ERROR
 * @return -EINVAL if a received parameter is null
 */
static int
rcv_from_peer_msg_handler(struct addr_info *ad_info,
                          struct recv_payload_data *payload_data)
{
    int err = 0;
    unsigned int i;
    unsigned long ifindex;

    MLAG_BAIL_CHECK(ad_info != NULL, -EINVAL);
    MLAG_BAIL_CHECK(payload_data != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "received %d packets from peer\n",
             payload_data->msg_num_recv);

    for (i = 0; i < payload_data->msg_num_recv; i++) {
        struct igmp_packet_wrapper *packet_wrapper =
            (struct igmp_packet_wrapper *)payload_data->payload[i];
        int mlag_id;

        err =
            mlag_manager_db_mlag_peer_id_get(ntohl(ad_info->ipv4_addr),
                                             &mlag_id);
        MLAG_BAIL_ERROR(err);

        MLAG_LOG(MLAG_LOG_DEBUG, "Packet[%d] from peer %d with len %d\n",
                 i, mlag_id, payload_data->payload_len[i]);

        counters.received_from_peer++;

        if (!packet_wrapper->receive_info.is_mlag) {
            /* TODO: Currently, we support only a single IPL with ID 0 */
            err = mlag_topology_ipl_port_get(0, &ifindex);
            MLAG_BAIL_ERROR(err);
        }
        else {
            ifindex = packet_wrapper->receive_info.source_port_id;
        }

        MLAG_LOG(MLAG_LOG_DEBUG,
                 "sending packet %d/%d: mlag_id %d to ifindex %lu\n",
                 i, payload_data->msg_num_recv, mlag_id, ifindex);

        err = pkt_send_loopback_wrapper(packet_wrapper, ifindex);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/*
 *  This function is called to handle received message from hw
 *
 * @param[in] fd - the file descriptor
 * @param[in] data - the data (not used)
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
rcv_from_hw_msg_handler(int fd,
                        void *data,
                        char *msg_buf,
                        int buf_size)
{
    int err = 0;
    int peer;

    UNUSED_PARAM(data);
    UNUSED_PARAM(msg_buf);
    UNUSED_PARAM(buf_size);
    ASSERT(fd == sl_fd);

    curr_packet.pkt_size = MAX_PACKET_SIZE;
    err = sl_api_pkt_receive(sl_fd,
                             &curr_packet.receive_info,
                             curr_packet.packet_buffer,
                             &curr_packet.pkt_size);
    MLAG_BAIL_ERROR_MSG(err, "Failed to receive packet from HW. Err=%d\n",
                        err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Received packet from HW, size=%lu\n",
             curr_packet.pkt_size);

    counters.received_from_hw++;

    /* Send to all peers (there is only one active peer) */
    for (peer = 0; peer < MLAG_MAX_PEERS; peer++) {
        if (comm_layer_wrapper.tcp_sock_handle[peer]) {
            uint32_t packet_size =
                sizeof(curr_packet) - MAX_PACKET_SIZE + curr_packet.pkt_size;

            MLAG_LOG(MLAG_LOG_DEBUG, "sending packet (size=%d) from HW to peer %d:"
                     " sending peer ID %d,  is_mlag %u, trap %d, source port %lu\n",
                     packet_size, peer, curr_packet.sending_peer_id,
                     curr_packet.receive_info.is_mlag,
                     curr_packet.receive_info.l2_trap_id,
                     curr_packet.receive_info.source_port_id);
            err = mlag_comm_layer_wrapper_message_send(&comm_layer_wrapper,
                                                       MLAG_TUNNELING_IGMP_MESSAGE,
                                                       (uint8_t *)&curr_packet,
                                                       packet_size,
                                                       peer,
                                                       MASTER_LOGIC);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to send message to peer %d, err= %d\n",
                                peer, err);
            counters.sent_to_peer++;
        }
    }

bail:
    return err;
}

/*
 * This function is sending sync done message to the master, it should not be
 * called on the slave
 *
 * @param[in] mlag_id - This is the mlag ID
 *
 * @return 0 if operation completes successfully.
 */
static int
tunnel_send_sync_done(int mlag_id)
{
    int err = 0;
    struct sync_event_data data;

    data.peer_id = mlag_id;
    data.opcode = 0;
    data.sync_type = IGMP_PEER_SYNC;
    data.state = 1; /* finish */

    err = send_system_event(MLAG_PEER_SYNC_DONE, &data, sizeof(data));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_NOTICE, "Successfully sent SYNC_DONE for IGMP\n");

bail:
    return err;
}

/*
 * This function is register all the IGMP traps
 *
 * @param[in] access_cmd - register or unregister
 *
 * @return 0 if operation completes successfully.
 */
static int
tunnel_trap_igmp(const enum oes_access_cmd access_cmd)
{
    int err = 0;

    err = sl_api_trap_register(access_cmd, sl_fd, OES_PACKET_IGMP_TYPE_QUERY);
    MLAG_BAIL_ERROR(err);
    err = sl_api_trap_register(access_cmd, sl_fd,
                               OES_PACKET_IGMP_TYPE_V1_REPORT);
    MLAG_BAIL_ERROR(err);
    err = sl_api_trap_register(access_cmd, sl_fd,
                               OES_PACKET_IGMP_TYPE_V2_REPORT);
    MLAG_BAIL_ERROR(err);
    err =
        sl_api_trap_register(access_cmd, sl_fd, OES_PACKET_IGMP_TYPE_V2_LEAVE);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 * This function is activating the tunneling:
 *  - Clears the counters
 *  - Mark the peer as connected
 *  - Register for traffic towards the HAL for the first peer
 *
 * @return 0 if operation completes successfully.
 */
static int
tunnel_activate(void)
{
    int err = 0;

    counters.received_from_peer = 0;
    counters.sent_to_peer = 0;

    if (!peer_connected) {
        peer_connected = 1;

        err = sl_api_trap_fd_open(&sl_fd);
        MLAG_BAIL_ERROR_MSG(err, "failed to open socket, err=%d\n", err);

        err = tunnel_trap_igmp(OES_ACCESS_CMD_ENABLE);
        MLAG_BAIL_ERROR_MSG(err, "failed to register IGMP trap, err=%d\n",
                            err);

        DISPATCHER_CONF_SET(tunnel_dispatcher_conf, HW_HANDLE, sl_fd,
                            MEDIUM_PRIORITY, rcv_from_hw_msg_handler, NULL,
                            NULL, 0);
        MLAG_LOG(MLAG_LOG_NOTICE, "Successfully activated the IGMP tunnel\n");
    }
    else {
        MLAG_LOG(MLAG_LOG_NOTICE, "Tunnel was already active\n");
    }
bail:
    return err;
}

/*
 * This function is deactivating the tunneling:
 *  - Mark the peer as disconnected
 *  - Disable the trap
 *  - Close the file descriptor towards the HW
 *
 * @return 0 if operation completes successfully.
 */
static int
tunnel_deactivate(void)
{
    int err = 0;

    if (peer_connected) {
        peer_connected = 0;

        DISPATCHER_CONF_RESET(tunnel_dispatcher_conf, HW_HANDLE);

        MLAG_LOG(MLAG_LOG_NOTICE, "Closing IGMP trap and socket\n");
        err = tunnel_trap_igmp(OES_ACCESS_CMD_DISABLE);
        MLAG_BAIL_ERROR_MSG(err, "Failed to close IGMP trap, err=%d\n", err);

        err = sl_api_trap_fd_close(&sl_fd);
        MLAG_BAIL_ERROR_MSG(err, "failed to close IGMP socket, err=%d\n", err);
    }
bail:
    return err;
}

/*
 *  This function dispatches connection notification event
 *  and calls comm layer wrapper handler. This happens only on
 *  a master which is the only one to open a listener
 *
 * @param[in] data - tcp_conn_notification_event_data
 *
 * @return int as error code.
 */
static int
tunnel_dispatch_conn_notify_event(uint8_t *data)
{
    int err = 0;
    struct tcp_conn_notification_event_data *ev =
        (struct tcp_conn_notification_event_data *)data;
    int mlag_id;

    ASSERT(ev);

    if (ev->data == &comm_layer_wrapper) {
        err = mlag_comm_layer_wrapper_connection_notification(
            ev->new_handle, ev->port, ev->ipv4_addr, ev->data, &ev->rc);
        MLAG_BAIL_ERROR(err);

        if ((comm_layer_wrapper.is_started) &&
            (ev->rc == 0)) {
            /* Retrieve peer id by peer ip addr */
            err = mlag_manager_db_mlag_peer_id_get(
                ntohl(ev->ipv4_addr), &mlag_id);
            MLAG_BAIL_ERROR(err);

            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Received connection notification from peer %d, rc = %d\n",
                     mlag_id, ev->rc);

            if (comm_layer_wrapper.current_switch_status == SLAVE) {
                MLAG_LOG(MLAG_LOG_NOTICE, "Tunneling slave connected\n");
            }

            err = tunnel_activate();
            MLAG_BAIL_ERROR(err);

            if (comm_layer_wrapper.current_switch_status == MASTER) {
                err = tunnel_send_sync_done(mlag_id);
                MLAG_BAIL_ERROR(err);
            }
        }
    }

bail:
    return err;
}

/*
 *  This function is called to add fd to common select function
 *  upon socket handle notification
 *  when connection established with client
 *
 * @param[in] new_handle - socket fd
 * @param[in] state - add/remove
 *            (use enum comm_fd_operation)
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
add_fd_handler(handle_t new_handle, int state)
{
    if (state == COMM_FD_ADD) {
        /* Add fd to fd set */
        DISPATCHER_CONF_SET(tunnel_dispatcher_conf, TCP_HANDLE, new_handle,
                            MEDIUM_PRIORITY,
                            mlag_comm_layer_wrapper_message_dispatcher,
                            &comm_layer_wrapper, NULL, 0);
    }
    else {
        /* Delete fd from fd set */
        DISPATCHER_CONF_RESET(tunnel_dispatcher_conf, TCP_HANDLE);
    }

    return 0;
}

/*
 * This function is called by the communication wrapper to swap the data
 *
 * @param[in/out] payload - the data that should be swap
 * @param[in] oper - send or receive - determine if we should do hton or ntoh
 *
 * @return 0 if operation completes successfully.
 */
static int
tunneling_net_order_msg(uint8_t *payload,
                        enum message_operation oper)
{
    int err = 0;
    struct igmp_packet_wrapper *igmp = (struct igmp_packet_wrapper *)payload;

    ASSERT(igmp != NULL);

    /* Only one type of payload is supported: IGMP */
    if (oper == MESSAGE_SENDING) {
        igmp->opcode = htonl(igmp->opcode);
        igmp->sending_peer_id = htonl(igmp->sending_peer_id);
        igmp->receive_info.l2_trap_id = htonl(igmp->receive_info.l2_trap_id);
        igmp->receive_info.source_port_id =
            htonl(igmp->receive_info.source_port_id);
        igmp->receive_info.is_mlag = htonl(igmp->receive_info.is_mlag);
    }
    else {
        igmp->opcode = ntohl(igmp->opcode);
        igmp->sending_peer_id = ntohl(igmp->sending_peer_id);
        igmp->receive_info.l2_trap_id = ntohl(igmp->receive_info.l2_trap_id);
        igmp->receive_info.source_port_id =
            ntohl(igmp->receive_info.source_port_id);
        igmp->receive_info.is_mlag = ntohl(igmp->receive_info.is_mlag);
    }
bail:
    return err;
}

/*
 *  This function dispatches port state change event. We need to re-learn the
 * MC status on a port change event and we do that by sending IGMP query
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if a received parameter is null
 */
static int
tunnel_dispatch_port_state_change(uint8_t *buffer)
{
    int err = 0;
    struct port_oper_state_change_data *state_change =
        (struct port_oper_state_change_data *)buffer;
    MLAG_BAIL_CHECK(state_change != NULL, -EINVAL);

    if (!igmp_enabled) {
        goto bail;
    }

    /* No need to learn on the IPL */
    if ((state_change->is_ipl == FALSE) && (state_change->state
                                            == MLAG_PORT_OPER_UP)
        && peer_connected) {
        /* Send an IGMP v2 Query */
        igmp_query_pkt_data.l2_pkt_type = OES_PACKET_IGMP_TYPE_QUERY;
        igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.system_mac_id =
            mlag_manager_db_local_system_id_get();

        err = sl_api_ctrl_pkt_send(
            sl_fd,
            &igmp_query_pkt_data,
            sizeof(igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data
                   .query_packet),
            state_change->port_id);
        MLAG_BAIL_ERROR(err);

        MLAG_LOG(MLAG_LOG_DEBUG,
                 "Successfully sent IGMPv2 query on port %lu\n",
                 state_change->port_id);

        counters.sent_queries++;
    }

bail:
    return err;
}

/*
 *  This function dispatches Master Election switch status change event.
 *  Close the old connections and a new master should open a listener.
 *
 * @param[in] buffer - event data
 *
 * @return int as error code.
 * @return -EINVAL if a received parameter is null
 */
static int
tunnel_dispatch_status_change(uint8_t *buffer)
{
    int err = 0;
    struct switch_status_change_event_data *status_change =
        (struct switch_status_change_event_data *)buffer;

    MLAG_LOG(MLAG_LOG_DEBUG, "Received status change\n");
    if (!igmp_enabled) {
        goto bail;
    }

    /* Close what we had previously before changing the status in the wrapper */
    MLAG_LOG(MLAG_LOG_NOTICE, "Closing all connections and/or listener\n");
    err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper,
                                       SERVER_STOP,
                                       status_change->my_peer_id);
    MLAG_BAIL_ERROR(err);

    tunnel_deactivate();

    /* Now we can update the wrapper status */
    comm_layer_wrapper.current_switch_status = status_change->current_status;

    /* Master - open listener */
    if (status_change->current_status == MASTER) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Opening tunneling listener thread\n");
        err = mlag_comm_layer_wrapper_start(&comm_layer_wrapper);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/*
 * This function dispatches start event.
 * A slave should attempt to connect, a master or stand-alone should just send
 * sync done since there is nothing else to do for the local peer.
 *
 * The mlag ID in the data refers to the local entity
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL - if unknown state was received.
 */
static int
tunnel_dispatch_peer_start_event(uint8_t *buffer)
{
    int err = 0;
    struct mlag_master_election_status master_status;
    struct peer_state_change_data *peer_change =
        (struct peer_state_change_data *)buffer;

    MLAG_LOG(MLAG_LOG_NOTICE, "Received peer start for mlag_id %d\n",
             peer_change->mlag_id);

    /* Only master creates a listener */
    switch (comm_layer_wrapper.current_switch_status) {
    case MASTER:
    case STANDALONE:
        /* send the sync done for the local peer (my peer) */
        err = mlag_master_election_get_status(&master_status);
        MLAG_BAIL_ERROR(err);

        if (peer_change->mlag_id == master_status.my_peer_id) {
            err = tunnel_send_sync_done(peer_change->mlag_id);
            MLAG_BAIL_ERROR(err);
        }
        break;
    case SLAVE:
        if (!igmp_enabled) {
            goto bail;
        }

        err = mlag_comm_layer_wrapper_start(&comm_layer_wrapper);
        MLAG_BAIL_ERROR(err);
        break;
    default:
        MLAG_BAIL_ERROR_MSG(-EINVAL, "Unexpected current status %d\n",
                            comm_layer_wrapper.current_switch_status);
        break;
    }

bail:
    return err;
}

/*
 *  This function dispatches peer state change event. The mlag ID in the data
 *  refers to the peer on the other end of the wire.
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL - if unknown state was received.
 */
static int
tunnel_dispatch_peer_state_change_event(uint8_t *buffer)
{
    int err = 0;
    struct peer_state_change_data *data =
        (struct peer_state_change_data *)buffer;

    /* When STP will be added, this condition should change */
    if (!igmp_enabled) {
        goto bail;
    }

    switch (data->state) {
    case HEALTH_PEER_UP:
        MLAG_LOG(MLAG_LOG_NOTICE, "Peer %d Up\n", data->mlag_id);
        /* There is nothing to do with peer up */
        goto bail;
    case HEALTH_PEER_DOWN:
    case HEALTH_PEER_COMM_DOWN:
    case HEALTH_PEER_DOWN_WAIT:
        /* A peer start event will be received for this peer before it will
         * change state to up again, therefore we can close the connection
         * towards this peer.
         */
        MLAG_LOG(MLAG_LOG_NOTICE, "Peer %d Down\n", data->mlag_id);

        /* Only if the connection exist, we need to close it */
        if (peer_connected) {
            err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper,
                                               PEER_STOP, data->mlag_id);
            MLAG_BAIL_ERROR(err);

            /* Last one needs to close the tunnel */
            err = tunnel_deactivate();
            MLAG_BAIL_ERROR(err);
        }
        break;
    default:
        MLAG_BAIL_ERROR_MSG(-EINVAL, "Unexpected data state %d\n",
                            data->state);
        break;
    }

bail:
    return err;
}

/**
 * Populates tunneling relevant counters.
 *
 * @param[out] counters - Mlag protocol counters. Pointer to an already
 *                        allocated memory structure. Only the IGMP and xSTP
 *                        Counters are relevant to this function
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 */
int
mlag_tunneling_counters_get(struct mlag_counters *mlag_counters)
{
    int err = 0;

    MLAG_BAIL_CHECK(mlag_counters, -EINVAL);

    mlag_counters->rx_igmp_tunnel = counters.received_from_peer;
    mlag_counters->tx_igmp_tunnel = counters.sent_to_peer;

    mlag_counters->rx_xstp_tunnel = 0;
    mlag_counters->tx_xstp_tunnel = 0;

bail:
    return err;
}

/**
 * Clear the tunneling counters
 */
void
mlag_tunneling_clear_counters(void)
{
    SAFE_MEMSET(&counters, 0);
}

/*
 *  This function starts the IGMP, it is called by the start API, before
 *  any traffic is possible
 *
 * @param[in] buffer - The mlag parameters which includes the IGMP
 *                     enable/disable status
 *
 * @return -EINVAL - If any input parameter is invalid.
 */
static int
tunnel_dispatch_start(uint8_t *buffer)
{
    int err = 0;
    struct start_event_data *params = (struct start_event_data *)buffer;

    MLAG_BAIL_CHECK(params, -EINVAL);

    memset(&igmp_query_pkt_data, 0, sizeof(igmp_query_pkt_data));

    igmp_enabled = params->start_params.igmp_enable;
    MLAG_LOG(MLAG_LOG_NOTICE, "IGMP %s\n",
             (igmp_enabled ? "enabled" : "disabled"));

    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_vhl =
        IP_IGMP_Q_V2_VHL;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_tos = 0x0;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_len =
        htons(
            IP_IGMP_Q_V2_TOTAL_LEN);
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_id = 0;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_off = 0;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_ttl = 1;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_p =
        IGMP_PROTO;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_src = 0;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_dst =
        htonl(
            IGMP_ALL_HOSTS_GROUP);
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.ip_sum =
        htons(
            IP_IGMP_Q_V2_SUM);
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.pkt_type =
        IGMP_MEMBERSHIP_QUERY;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.max_resp_time
        =
            IGMP_MAX_RESP_TIME;
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.check_sum =
        htons(IGMP_QUERY_CHECK_SUM);
    igmp_query_pkt_data.ctrl_pkt.igmp_ctrl_pkt_data.query_packet.group_addr =
        0;

bail:
    return err;
}

/*
 *  This function dispatches stop event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
tunnel_dispatch_stop(uint8_t *buffer)
{
    int err = 0;
    UNUSED_PARAM(buffer);
    struct sync_event_data stop_done;

    err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper, SERVER_STOP, 0);
    /* Error or not, we go on and close it all */
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "Stop: Failed to close TCP connections: %d\n",
                 err);
        DISPATCHER_CONF_RESET(tunnel_dispatcher_conf, TCP_HANDLE);
    }

    err = tunnel_deactivate();
    /* Again: Error or not, we go on and close it all */
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "Stop: Failed to deactivate tunnel: %d\n",
                 err);
    }

    igmp_enabled = 0;

    /* Send MLAG_STOP_DONE_EVENT to MLag Manager */
    stop_done.sync_type = IGMP_STOP_DONE;
    err = send_system_event(MLAG_STOP_DONE_EVENT,
                            &stop_done, sizeof(stop_done));
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "send_system_event returned error %d\n", err);
    }

    return err;
}

/*
 * A wrapper function so the back trace of the thread will give data about the
 * thread nature (too many threads are just calling the common
 * dispatcher_thread_routine which makes it hard to debug).
 *
 * @param[in] data - pointer to configuration
 *
 * @return void
 */

static void
mlag_tunneling_dispacher_thread(void * data)
{
    dispatcher_thread_routine(data);
}

/**
 *  This function inits all tunneling sub-module
 *
 * @return ERROR if operation failed.
 */
int
mlag_tunneling_init(void)
{
    int err = 0;
    int i;
    cl_status_t cl_err;

    err = register_events(&tunnel_event_fds,
                          NULL, 0,
                          medium_prio_events,
                          NUM_ELEMS(medium_prio_events),
                          NULL, 0);
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* 4 fd/handlers: deinit, sys_events, peer socket, OES socket */
    tunnel_dispatcher_conf.handlers_num = NUM_OF_HANDLES;
    for (i = 0; i < tunnel_dispatcher_conf.handlers_num; i++) {
        tunnel_dispatcher_conf.handler[i].fd = 0;
        tunnel_dispatcher_conf.handler[i].msg_buf = 0;
        tunnel_dispatcher_conf.handler[i].buf_size = 0;
    }

    strncpy(tunnel_dispatcher_conf.name, "Tunneling",
            DISPATCHER_NAME_MAX_CHARS);

    err = init_command_db(&tunnel_cmd_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = insert_to_command_db(tunnel_cmd_db, tunnel_dispatcher_commands);
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* deinit event */
    DISPATCHER_CONF_SET(tunnel_dispatcher_conf, TERMINATE_HANDLE,
                        tunnel_event_fds.high_fd,
                        HIGH_PRIORITY, dispatcher_sys_event_handler,
                        tunnel_cmd_db, tunnel_disp_sys_event_buf,
                        sizeof(tunnel_disp_sys_event_buf));
    DISPATCHER_CONF_SET(tunnel_dispatcher_conf, EVENTS_HANDLE,
                        tunnel_event_fds.med_fd,
                        MEDIUM_PRIORITY, dispatcher_sys_event_handler,
                        tunnel_cmd_db, tunnel_disp_sys_event_buf,
                        sizeof(tunnel_disp_sys_event_buf));

    err = mlag_comm_layer_wrapper_init(&comm_layer_wrapper,
                                       TUNNELING_PORT,
                                       rcv_from_peer_msg_handler,
                                       tunneling_net_order_msg,
                                       add_fd_handler,
                                       NO_SOCKET_PROTECTION);
    MLAG_BAIL_CHECK_NO_MSG(err);

    peer_connected = 0;

    /* Open tunneling context */
    cl_err = cl_thread_init(&tunneling_thread, mlag_tunneling_dispacher_thread,
                            &tunnel_dispatcher_conf, NULL);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err, "Could not create tunneling thread\n");
    }
    MLAG_LOG(MLAG_LOG_NOTICE, "Tunneling thread is ready\n");

bail:
    return err;
}

/**
 *  This function de-Inits Mlag tunneling
 *
 * @return int as error code.
 */
int
mlag_tunneling_deinit(void)
{
    int err;

    cl_thread_destroy(&tunneling_thread);

    err = mlag_comm_layer_wrapper_deinit(&comm_layer_wrapper);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = close_events(&tunnel_event_fds);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = deinit_command_db(tunnel_cmd_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 *  This function cause dump, either using print_cb
 *  Or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_tunneling_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;

    DUMP_OR_LOG(
        "===================\nMlag Tunneling Dump\n===================\n");
    DUMP_OR_LOG("IGMP %s\n", (igmp_enabled ? "enabled" : "disabled"));
    DUMP_OR_LOG("%sConnected\n", (peer_connected ? "" : "Not "));
    DUMP_OR_LOG("Received from HW: %llu\n", counters.received_from_hw);
    DUMP_OR_LOG("Sent queries:     %llu\n", counters.sent_queries);
    DUMP_OR_LOG("Received:         %llu\n", counters.received_from_peer);
    DUMP_OR_LOG("Sent:             %llu\n", counters.sent_to_peer);

    DUMP_OR_LOG("\n");
    return err;
}

/*
 *  This function dispatches reconnect event
 *  and calls comm layer wrapper handler
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
tunnel_dispatch_reconnect_event(uint8_t *data)
{
    int err = 0;
    struct reconnect_event_data *ev =
        (struct reconnect_event_data *) data;

    if (ev->data == &comm_layer_wrapper) {
        err = mlag_comm_layer_wrapper_reconnect(
            (struct mlag_comm_layer_wrapper_data *)ev->data);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}
