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
#include <complib/cl_thread.h>
#include <complib/cl_init.h>
#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_defs.h"
#include "mlag_events.h"
#include "mac_sync_events.h"
#include <mlnx_lib/lib_event_disp.h>
#include "mlag_common.h"
#include "mlag_master_election.h"
#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_mac_sync_dispatcher.h"
#include "mlag_mac_sync_manager.h"
#include "mlag_manager.h"
#include "mlag_mac_sync_master_logic.h"
#include "mlag_mac_sync_peer_manager.h"
#include "mlag_mac_sync_router_mac_db.h"
#include "mlag_dispatcher.h"
#include "health_manager.h"

#undef  __MODULE__
#define __MODULE__ MLAG_MAC_SYNC_DISPATCHER

/************************************************
 *  Local Defines
 ***********************************************/

/************************************************
 *  Local Macros
 ***********************************************/
#define MAC_SYNC_DISPATCHER_CONF_SET(index, u_fd, u_prio, u_handler, u_data, \
                                     buf, size) \
    DISPATCHER_CONF_SET(mac_sync_dispatcher_conf, index, u_fd, u_prio, \
                        u_handler, \
                        u_data, buf, size)
#define MAC_SYNC_DISPATCHER_CONF_RESET(index) \
    DISPATCHER_CONF_RESET(mac_sync_dispatcher_conf, index)

#define MAC_SYNC_DISP_SYS_EVENT_BUF_SIZE 600000

/************************************************
 *  Local Type definitions
 ***********************************************/
static int dispatch_start_event(uint8_t *data);
static int dispatch_peer_start_event(uint8_t *data);
static int dispatch_mac_sync_peer_finish(uint8_t *data);
static int dispatch_mac_sync_master_done_to_peer(uint8_t *data);
static int dispatch_master_flush_start_event(uint8_t *data);
static int dispatch_peer_flush_start_event(uint8_t *data);
static int dispatch_peer_flush_ack_event(uint8_t *data);
static int dispatch_master_flush_timer_event(uint8_t *data);
static int dispatch_port_global_state(uint8_t *data);
static int dispatch_stop_event(uint8_t *data);
static int dispatch_peer_state_change_event(uint8_t *data);
static int dispatch_switch_status_change_event(uint8_t *data);
static int dispatch_global_learned_event(uint8_t *data);
static int dispatch_global_aged_event(uint8_t *data);
static int dispatch_internal_age_event(uint8_t *data);
static int dispatch_router_mac_conf_event(uint8_t *data);
static int dispatch_global_reject_event(uint8_t *data);
static int dispatch_local_learn_event(uint8_t *data);
static int dispatch_local_aged_event(uint8_t *data);
static int dispatch_fdb_export_event(uint8_t *data);
static int dispatch_fdb_get_event(uint8_t *data);
static int dispatch_peer_enable_event(uint8_t *data);
static int dispatch_ipl_port_set_event(uint8_t *data);
static int dispatch_conn_notify_event(uint8_t *data);
static int dispatch_reconnect_event(uint8_t *data);
static int dispatch_mpo_port_deleted_event(uint8_t *data);

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static cl_thread_t mac_sync_thread;
static cmd_db_handle_t *mac_sync_cmd_db;
static event_disp_fds_t event_fds;
static struct dispatcher_conf mac_sync_dispatcher_conf;
static char mac_sync_disp_sys_event_buf[MAC_SYNC_DISP_SYS_EVENT_BUF_SIZE];

static int medium_prio_events[] = {
    MLAG_CONN_NOTIFY_EVENT,
    MLAG_RECONNECT_EVENT,
    MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT,
    MLAG_MAC_SYNC_GLOBAL_AGED_EVENT,
    MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT,
    MLAG_START_EVENT,
    MLAG_STOP_EVENT,
    MLAG_PEER_START_EVENT,
    MLAG_PEER_ENABLE_EVENT,
    MLAG_IPL_PORT_SET_EVENT,
    MLAG_PEER_STATE_CHANGE_EVENT,
    MLAG_FLUSH_FSM_TIMER,
    MLAG_PORT_GLOBAL_STATE_EVENT,
    MLAG_MAC_SYNC_SYNC_FINISH_EVENT,
    MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT,
    MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
    MLAG_PORT_DELETED_EVENT
};

static int low_prio_events[] = {
    MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT,
    MLAG_MAC_SYNC_LOCAL_AGED_EVENT,
    MLAG_MAC_SYNC_AGE_INTERNAL_EVENT,
    MLAG_MAC_SYNC_ALL_FDB_GET_EVENT,
    MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT,
    MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT,
    MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT,
    MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT,
    MLAG_ROUTER_MAC_CFG_EVENT
};

static handler_command_t mac_sync_dispatcher_commands[] = {
    {MLAG_START_EVENT, "Start event", dispatch_start_event, NULL},
    {MLAG_STOP_EVENT, "Stop event", dispatch_stop_event, NULL},
    {MLAG_CONN_NOTIFY_EVENT, "Conn notify event", dispatch_conn_notify_event,
     NULL},
    {MLAG_RECONNECT_EVENT, "Reconnect event",
     dispatch_reconnect_event, NULL},
    {MLAG_PEER_STATE_CHANGE_EVENT, "Peer State Change event",
     dispatch_peer_state_change_event, NULL },
    {MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
     "Master election switch status change event",
     dispatch_switch_status_change_event, NULL},
    {MLAG_IPL_PORT_SET_EVENT, "IPL port set event",
     dispatch_ipl_port_set_event, NULL},
    {MLAG_MAC_SYNC_SYNC_FINISH_EVENT,
     "Mac Sync Sync finish event arrived to Master",
     dispatch_mac_sync_peer_finish, NULL},
    {MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT,
     "Mac Sync done event sent by master",
     dispatch_mac_sync_master_done_to_peer, NULL},
    {MLAG_PEER_START_EVENT, "Peer start event", dispatch_peer_start_event,
     NULL},
    {MLAG_PEER_ENABLE_EVENT, "Peer enable event", dispatch_peer_enable_event,
     NULL},
    {MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT, "Global learned event",
     dispatch_global_learned_event, NULL},
    {MLAG_MAC_SYNC_GLOBAL_AGED_EVENT, "Global aged event",
     dispatch_global_aged_event, NULL},
    {MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT, "Global rejected event",
     dispatch_global_reject_event, NULL},
    {MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT, "local learned event",
     dispatch_local_learn_event, NULL},
    {MLAG_MAC_SYNC_LOCAL_AGED_EVENT, "local aged event",
     dispatch_local_aged_event, NULL},
    {MLAG_MAC_SYNC_ALL_FDB_GET_EVENT, "FDB get event",
     dispatch_fdb_get_event, NULL},
    {MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT, "FDB export event",
     dispatch_fdb_export_event, NULL},
    {MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT,
     "Global flush Master sends start event",
     dispatch_peer_flush_start_event, NULL},
    {MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT,
     "Global flush Peer sends start event",
     dispatch_master_flush_start_event, NULL},
    {MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT, "Global flush ack event",
     dispatch_peer_flush_ack_event, NULL},
    {MLAG_FLUSH_FSM_TIMER, "Flush FSM timer",
     dispatch_master_flush_timer_event, NULL},
    {MLAG_PORT_GLOBAL_STATE_EVENT, "Port global state event",
     dispatch_port_global_state, NULL},
    {MLAG_MAC_SYNC_AGE_INTERNAL_EVENT, "Internal age notification",
     dispatch_internal_age_event, NULL},
    {MLAG_ROUTER_MAC_CFG_EVENT, "Router MAC configuration event",
     dispatch_router_mac_conf_event, NULL},
    {MLAG_PORT_DELETED_EVENT, "MLAG MPO port deleted event",
     dispatch_mpo_port_deleted_event, NULL},

    {0, "", NULL, NULL}
};

static struct mlag_comm_layer_wrapper_data comm_layer_wrapper;
static cmd_db_handle_t *mac_sync_dispatcher_ibc_msg_db;

/************************************************
 *  Local function declarations
 ***********************************************/
static int add_fd_handler(handle_t new_handle, int state);
static int rcv_msg_handler(struct addr_info *ad_info,
                           struct recv_payload_data *payload_data);
static int net_order_msg_handler(uint8_t *payload,
                                 enum message_operation oper);

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function sets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_mac_sync_dispatcher_log_verbosity_get(void)
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
mlag_mac_sync_dispatcher_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/*
 *  This function dispatches start event
 *
 * @return int as error code.
 */
static int
dispatch_start_event(uint8_t *data)
{
    int err = 0;

    comm_layer_wrapper.current_switch_status = DEFAULT_SWITCH_STATUS;

    err = mlag_mac_sync_start(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed start event\n");

bail:
    return err;
}

/*
 *  This function dispatches stop event
 *
 * @return int as error code.
 */
static int
dispatch_stop_event(uint8_t *data)
{
    int err = 0;
    struct sync_event_data stop_done;

    err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper, SERVER_STOP, 0);
    MLAG_BAIL_ERROR_MSG(err, "Failed in stop event wrapper stop\n");

    err = mlag_mac_sync_stop(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in stop event  sync stop\n");

bail:
    /* Send MLAG_STOP_DONE_EVENT to MLag Manager */
    stop_done.sync_type = MAC_SYNC_STOP_DONE;
    err = send_system_event(MLAG_STOP_DONE_EVENT,
                            &stop_done, sizeof(stop_done));
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "send_system_event returned error %d\n", err);
    }

    return err;
}

/**
 *  This function prints module's fd db
 *
 * @return current status value
 */
int
mlag_mac_sync_dispatcher_print(void)
{
    MLAG_LOG(MLAG_LOG_NOTICE, "name=%s, handlers_num=%d\n",
             mac_sync_dispatcher_conf.name,
             mac_sync_dispatcher_conf.handlers_num);
    MLAG_LOG(MLAG_LOG_NOTICE, "handlers=%d %d %d\n",
             mac_sync_dispatcher_conf.handler[0].fd,
             mac_sync_dispatcher_conf.handler[1].fd,
             mac_sync_dispatcher_conf.handler[2].fd);
    return 0;
}

/*
 *  This function dispatches peer start event
 *
 * @return int as error code.
 */
static int
dispatch_peer_start_event(uint8_t *data)
{
    int err = 0;

    /* Open TCP connection from client side */
    if (comm_layer_wrapper.current_switch_status == SLAVE) {
        err = mlag_comm_layer_wrapper_start(&comm_layer_wrapper);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in peer start event: wrapper start\n");
    }
    else if (comm_layer_wrapper.current_switch_status == MASTER) {
        err = mlag_mac_sync_peer_start_event(
            (struct peer_state_change_data *) data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in peer start event: wrapper start\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches sync finish event
 *
 * @return int as error code.
 */
static int
dispatch_mac_sync_peer_finish(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_sync_finish((struct sync_event_data *) data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer finish\n");

bail:
    return err;
}

/*
 *  This function dispatches master sync done event
 *
 * @return int as error code.
 */
static int
dispatch_mac_sync_master_done_to_peer(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_done_to_peer((struct sync_event_data *) data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in master done event\n");

bail:
    return err;
}

/*
 *  This function dispatches peer enable event
 *
 * @return int as error code.
 */
static int
dispatch_peer_enable_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_peer_enable((struct peer_state_change_data *) data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer enable event\n");

bail:
    return err;
}

/*
 *  This function dispatches connection notification event
 *  and calls comm layer wrapper handler
 *
 * @return int as error code.
 */
static int
dispatch_conn_notify_event(uint8_t *data)
{
    int err = 0;
    ASSERT(data);
    struct tcp_conn_notification_event_data *ev =
        (struct tcp_conn_notification_event_data *) data;
    struct peer_state_change_data peer_start;
    struct mlag_master_election_status master_election_current_status;

    if (ev->data == &comm_layer_wrapper) {
        err = mlag_comm_layer_wrapper_connection_notification(
            ev->new_handle, ev->port, ev->ipv4_addr, ev->data, &ev->rc);
        MLAG_BAIL_ERROR_MSG(err, "Failed in conn notify event\n");

        if ((comm_layer_wrapper.is_started) &&
            (ev->rc == 0) &&
            (comm_layer_wrapper.current_switch_status == SLAVE)) {
            err = mlag_master_election_get_status(
                &master_election_current_status);
            MLAG_BAIL_ERROR(err);

            peer_start.opcode = MLAG_PEER_START_EVENT;
            peer_start.mlag_id = master_election_current_status.my_peer_id;

            err = mlag_mac_sync_peer_start_event(&peer_start);
            MLAG_BAIL_ERROR(err);
        }
    }

bail:
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
dispatch_reconnect_event(uint8_t *data)
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

/*
 *  This function dispatches mlag port deleted event from
 *  Port manager
 *
 *  @param[in] data - pointer to message
 *
 * @return int as error code.
 */
static int
dispatch_mpo_port_deleted_event(uint8_t *data)
{
    int err = 0;
    struct mpo_port_deleted_event_data *ev =
        (struct mpo_port_deleted_event_data *)data;

    if (ev->status) {
        err = mlag_mac_sync_peer_manager_port_local_flush(ev->port);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in flush deleted port %lu\n",
                            ev->port);
    }

bail:
    /* Signal port delete done to Internal API module */
    err = mlag_manager_port_delete_done();
    if (err) {
        MLAG_LOG(MLAG_LOG_WARNING,
                 "Failed in sending port [%lu] delete done event\n",
                 ev->port);
    }
    MLAG_LOG(MLAG_LOG_NOTICE,
             "Delete port [%lu] procedure finished\n",
             ev->port);
    return err;
}

/*
 *  This function dispatches peer state change event
 *
 * @return int as error code.
 */
static int
dispatch_peer_state_change_event(uint8_t *data)
{
    int err = 0;
    ASSERT(data);
    struct peer_state_change_data *event =
        (struct peer_state_change_data*)data;

    if (((comm_layer_wrapper.current_switch_status == MASTER) ||
         (comm_layer_wrapper.current_switch_status == SLAVE)) &&
        ((event->state == HEALTH_PEER_DOWN) ||
         (event->state == HEALTH_PEER_COMM_DOWN) ||
         (event->state == HEALTH_PEER_DOWN_WAIT))) {
        err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper, PEER_STOP,
                                           ((struct peer_state_change_data*)
                                            data)->mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed in peer state change wrapper stop\n");
    }

    err = mlag_mac_sync_peer_status_change(
        (struct peer_state_change_data *) data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer state change event\n");

bail:
    return err;
}

/*
 *  This function dispatches Master Election switch status change event
 *
 * @return int as error code.
 */
static int
dispatch_switch_status_change_event(uint8_t *data)
{
    int err = 0;
    ASSERT(data);

    struct switch_status_change_event_data *ev =
        (struct switch_status_change_event_data *) data;
    char *current_status_str;
    char *previous_switch_str;


    err = mlag_master_election_get_switch_status_str(ev->current_status,
                                                     &current_status_str);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer status change event\n");
    err = mlag_master_election_get_switch_status_str(ev->previous_status,
                                                     &previous_switch_str);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer status change event 1\n");

    MLAG_LOG(MLAG_LOG_INFO,
             "Master Election switch status change event: currently %s, previously %s\n",
             current_status_str, previous_switch_str);

    err = mlag_mac_sync_switch_status_change(
        (struct switch_status_change_event_data *) data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer status change event 2\n");


    /* Close what we had previously before changing the status in the wrapper */
    MLAG_LOG(MLAG_LOG_NOTICE, "Closing all connections and/or listener\n");
    err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper,
                                        SERVER_STOP, ev->my_peer_id);

    comm_layer_wrapper.current_switch_status = ev->current_status;

    if (comm_layer_wrapper.current_switch_status == MASTER) {
         err = mlag_comm_layer_wrapper_start(&comm_layer_wrapper);
         MLAG_BAIL_ERROR_MSG(err, "Failed in peer status change event 3\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches ipl port set event
 *
 * @return int as error code.
 */
static int
dispatch_ipl_port_set_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_ipl_port_set((struct ipl_port_set_event_data *) data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in port set event\n");

bail:
    return err;
}

/*
 *  This function dispatches peer flush start event
 *
 * @return int as error code.
 */
static int
dispatch_peer_flush_start_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_INFO, "Start Global flush  in Peer\n");

    err = mlag_mac_sync_peer_mngr_flush_start(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer flush start event\n");

bail:
    return err;
}

/*
 *  This function dispatches master flush start event
 *
 * @return int as error code.
 */
static int
dispatch_master_flush_start_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_INFO, "Start Global flush FSM in Master\n");
    if (comm_layer_wrapper.current_switch_status == MASTER) {
        err = mlag_mac_sync_master_logic_flush_start(data);
        MLAG_BAIL_ERROR_MSG(err, "Failed in master flush start event\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches peer flush ack event
 *
 * @return int as error code.
 */
static int
dispatch_peer_flush_ack_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_INFO, "peer acks  to Master flush FSM\n");
    if (comm_layer_wrapper.current_switch_status == MASTER) {
        err = mlag_mac_sync_master_logic_flush_ack(data);
        MLAG_BAIL_ERROR_MSG(err, "Failed in flush ack event\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches timer event for Flush FSM
 *
 * @return int as error code.
 */
static int
dispatch_master_flush_timer_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_NOTICE, "Timeout in Flush FSM\n");
    if (comm_layer_wrapper.current_switch_status == MASTER) {
        err = mlag_mac_sync_master_logic_flush_fsm_timer(data);
        MLAG_BAIL_ERROR_MSG(err, "Failed in flush timer event\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches MLAG port global state event
 *
 * @return int as error code.
 */
static int
dispatch_port_global_state(uint8_t *data)
{
    int err = 0;
    MLAG_LOG(MLAG_LOG_INFO, "Global port state event\n");

    ASSERT(data);

    if (comm_layer_wrapper.current_switch_status == MASTER) {
        err = mlag_mac_sync_master_logic_global_flush_start(data);
        MLAG_BAIL_ERROR_MSG(err, "Failed in global state event\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches local learn event
 *
 * @return int as error code.
 */
static int
dispatch_local_learn_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_master_logic_local_learn(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in local learn event\n");

bail:
    return err;
}

/*
 *  This function dispatches local age event
 *
 * @return int as error code.
 */
static int
dispatch_local_aged_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_master_logic_local_aged(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in local age event\n");

bail:
    return err;
}

/*
 *  This function dispatches global learn event
 *
 * @return int as error code.
 */
static int
dispatch_global_learned_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_peer_mngr_global_learned(data, 1);
    if (err == -EXFULL) {
        err = 0;
    }
    MLAG_BAIL_ERROR_MSG(err, "Failed in global learn event\n");

bail:
    return err;
}

/*
 *  This function dispatches global age event
 *
 * @return int as error code.
 */
static int
dispatch_global_aged_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_peer_mngr_global_aged(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in global aged event\n");

bail:
    return err;
}

int
dispatch_internal_age_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_INFO, "Internal  age event\n");

    err = mlag_mac_sync_peer_mngr_internal_aged(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in internal age event\n");

bail:
    return err;
}

/*
 *  This function dispatches Router mac configuration event
 *
 * @return int as error code.
 */
int
dispatch_router_mac_conf_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_INFO, "Router MAC event\n");

    err = mlag_mac_sync_router_mac_db_conf(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in mac conf event\n");

bail:
    return err;
}





/*
 *  This function dispatches global reject event to peer
 *
 * @return int as error code.
 */
static int
dispatch_global_reject_event(uint8_t *data)
{
    int err = 0;

    err = mlag_mac_sync_peer_mngr_global_reject(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in global reject event\n");

bail:
    return err;
}


/*
 *  This function dispatches FDB export event
 *
 * @return int as error code.
 */
static int
dispatch_fdb_export_event(uint8_t *data)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_NOTICE, "FDB export event to peer\n");

    err = mlag_mac_sync_peer_mngr_process_fdb_export(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in fdb export event\n");

bail:
    return err;
}

/*
 *  This function dispatches FDB get event
 *
 * @return int as error code.
 */
static int
dispatch_fdb_get_event(uint8_t *data)
{
    int err = 0;
    MLAG_LOG(MLAG_LOG_NOTICE, "FDB get event to master\n");

    err = mlag_mac_sync_master_logic_fdb_export(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in fdb get event\n");

bail:
    return err;
}

/*
 *  This function is called to handle network order for IBC message
 *
 * @param[in] payload - received message
 * @param[in] oper    - on receive or send
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
net_order_msg_handler(uint8_t *payload, enum message_operation oper)
{
    int err = 0;
    handler_command_t cmd_data;
    cmd_db_handle_t *cmd_db_handle = mac_sync_dispatcher_ibc_msg_db;
    uint16_t opcode;

    ASSERT(payload != NULL);

    if (oper == MESSAGE_SENDING) {
        opcode = *(uint16_t*)payload;
    }
    else {
        opcode = ntohs(*(uint16_t*)payload);
    }

    err = get_command(cmd_db_handle, opcode, &cmd_data);
    MLAG_BAIL_ERROR_MSG(err, "Command [%d] not found in cmd db\n", opcode);

    MLAG_LOG(MLAG_LOG_INFO,
             "%s (opcode=%d) handled by network order function\n",
             cmd_data.name, opcode);

    if (cmd_data.net_order_func == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Empty net order function\n");
        goto bail;
    }
    err = cmd_data.net_order_func((uint8_t *)payload, oper);
    if (err != -ECANCELED) {
        MLAG_BAIL_ERROR_MSG(err, "Failed in  net order func\n");
    }

bail:
    return err;
}

/*
 *  This function is called to handle received message
 *  from established TCP socket connection
 *
 * @param[in] conn_info - connection info
 * @param[in] payload_data - received data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
rcv_msg_handler(struct addr_info *ad_info,
                struct recv_payload_data *payload_data)
{
    int err = 0;
    handler_command_t cmd_data;
    cmd_db_handle_t *cmd_db_handle = mac_sync_dispatcher_ibc_msg_db;
    uint8_t *msg_data = NULL;
    uint16_t opcode;
    int len;

    ASSERT(ad_info != NULL);
    ASSERT(payload_data != NULL);

    if (payload_data->payload_len[0]) {
        msg_data = payload_data->payload[0];
        len = payload_data->payload_len[0];
    }
    else if (payload_data->jumbo_payload_len) {
        msg_data = payload_data->jumbo_payload;
        len = payload_data->jumbo_payload_len;
    }

    opcode = *(uint16_t*)(msg_data);

    err = get_command(cmd_db_handle, opcode, &cmd_data);
    MLAG_BAIL_ERROR_MSG(err, "Command [%d] not found in cmd db\n",
                        opcode);

    MLAG_LOG(MLAG_LOG_INFO,
             "%s (opcode=%d) received by mac sync dispatcher from remote switch, payload length %d\n",
             cmd_data.name, opcode, len);

    if (cmd_data.func == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Empty function\n");
        goto bail;
    }
    err = cmd_data.func((uint8_t *) payload_data);
    if (err != -ECANCELED) {
        MLAG_BAIL_ERROR_MSG(err, "Failed in cmd data func\n");
    }

bail:
    return err;
}

/*
 *  This function adds fd to dispatcher with specified index
 *
 * @return ERROR if operation failed.
 */
static int
mac_sync_dispatcher_add_fd(int fd_index, int fd,
                           fd_handler_func handler)
{
    MLAG_LOG(MLAG_LOG_INFO,
             "mac sync dispatcher added FD %d to index %d\n",
             fd, fd_index);

    MAC_SYNC_DISPATCHER_CONF_SET(fd_index, fd, HIGH_PRIORITY, handler,
                                 &comm_layer_wrapper, NULL, 0);
    return 0;
}

/*
 *  This function deletes fd from dispatcher with specified index
 *
 * @return ERROR if operation failed.
 */
static int
mac_sync_dispatcher_delete_fd(int fd_index)
{
    MLAG_LOG(MLAG_LOG_INFO,
             "mlag dispatcher deleted FD from index %d\n",
             fd_index);

    MAC_SYNC_DISPATCHER_CONF_RESET(fd_index);
    return 0;
}

/*
 *  This function is called to add fd to commomn select function
 *  upon socket handle notification
 *  when connection established with client
 *
 * @param[in] new_handle - socket fd
 * @param[in] state -- 0 - add, 1 - remove
 *            (use enum comm_fd_operation)
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
add_fd_handler(handle_t new_handle, int state)
{
    int err = 0;

    if (state == COMM_FD_ADD) {
        /* Add fd to fd set */
        err = mac_sync_dispatcher_add_fd(
            MAC_SYNC_FD_INDEX, new_handle,
            mlag_comm_layer_wrapper_message_dispatcher);
        MLAG_BAIL_ERROR_MSG(err, "Failed in fd handler event\n");
    }
    else {
        /* Delete fd from fd set */
        err = mac_sync_dispatcher_delete_fd(MAC_SYNC_FD_INDEX);
        MLAG_BAIL_ERROR_MSG(err, "Failed in dispatcher delete file decr.\n");
    }

bail:
    return err;
}

/**
 *  This function sends message to destination
 *
 * @param[in] opcode - message id
 * @param[in] payload - message data
 * @param[in] payload_len - message data length
 * @param[in] dest_peer_id - peer id to send message to
 *            used for messages originated by Master Logic only
 * @param[in] orig - message originator - master logic or peer manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_dispatcher_message_send(enum mlag_events opcode,
                                      uint8_t* payload, uint32_t payload_len,
                                      uint8_t dest_peer_id,
                                      enum message_originator orig)
{
    /*TODO  protect this!   check whether in exists and not deinited*/
    return mlag_comm_layer_wrapper_message_send(&comm_layer_wrapper, opcode,
                                                payload, payload_len,
                                                dest_peer_id, orig);
}

/*
 *  This function is called to insert message handlers to
 *  events data base
 *
 * @param[in] commands - data base to insert
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
mlag_mac_sync_dispatcher_insert_msgs(handler_command_t commands[])
{
    int err = 0;

    err = insert_to_command_db(mac_sync_dispatcher_ibc_msg_db, commands);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in dispatcher insert message to the command db\n");

bail:
    return err;
}

/*
 *  This function Inits mac sync dispatcher's modules
 *
 * @return int as error code.
 */
static int
modules_init(void)
{
    int err = 0;

    err = mlag_mac_sync_init(mlag_mac_sync_dispatcher_insert_msgs);
    MLAG_BAIL_ERROR_MSG(err, "Failed in init modules\n");

bail:
    return err;
}

/*
 *  This function deinits mac sync dispatcher's modules
 *
 * @return int as error code.
 */
static int
modules_deinit(void)
{
    int err = 0;

    err = mlag_mac_sync_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed in modules deinit event\n");

bail:
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
mlag_mac_sync_dispacher_thread(void * data)
{
    dispatcher_thread_routine(data);
}

/**
 *  This function inits mlag mac sync dispatcher
 *
 * @return ERROR if operation failed.
 */
int
mlag_mac_sync_dispatcher_init(void)
{
    int err;
    cl_status_t cl_err;
    int i;
    int sndbuf = MAC_SYNC_DISP_WMEM_SOCK_BUF_SIZE,
        rcvbuf = MAC_SYNC_DISP_RMEM_SOCK_BUF_SIZE;

    err = register_events(&event_fds, NULL, 0, medium_prio_events,
                          NUM_ELEMS(medium_prio_events), low_prio_events,
                          NUM_ELEMS(low_prio_events));
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* We have 3 fd/handlers in this thread dispatcher:
     * sys_events high, sys_events med, mas_sync dispatcher */
    mac_sync_dispatcher_conf.handlers_num =
        MLAG_MAC_SYNC_DISPATCHER_HANDLERS_NUM;

    strncpy(mac_sync_dispatcher_conf.name, "MAC SYNC",
            DISPATCHER_NAME_MAX_CHARS);

    MLAG_LOG(MLAG_LOG_NOTICE, "mlag_MAC_SYNC_dispatcher_init\n");

    for (i = 0; i < mac_sync_dispatcher_conf.handlers_num; i++) {
        mac_sync_dispatcher_conf.handler[i].fd = 0;
        mac_sync_dispatcher_conf.handler[i].msg_buf = 0;
        mac_sync_dispatcher_conf.handler[i].buf_size = 0;
    }

    err = init_command_db(&mac_sync_cmd_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = insert_to_command_db(mac_sync_cmd_db, mac_sync_dispatcher_commands);
    MLAG_BAIL_CHECK_NO_MSG(err);

    MAC_SYNC_DISPATCHER_CONF_SET(0, event_fds.high_fd, HIGH_PRIORITY,
                                 dispatcher_sys_event_handler,
                                 mac_sync_cmd_db,
                                 mac_sync_disp_sys_event_buf,
                                 sizeof(mac_sync_disp_sys_event_buf));
    MAC_SYNC_DISPATCHER_CONF_SET(1, event_fds.med_fd, MEDIUM_PRIORITY,
                                 dispatcher_sys_event_handler,
                                 mac_sync_cmd_db,
                                 mac_sync_disp_sys_event_buf,
                                 sizeof(mac_sync_disp_sys_event_buf));
    MAC_SYNC_DISPATCHER_CONF_SET(3, event_fds.low_fd, LOW_PRIORITY,
                                 dispatcher_sys_event_handler,
                                 mac_sync_cmd_db,
                                 mac_sync_disp_sys_event_buf,
                                 sizeof(mac_sync_disp_sys_event_buf));

    if (setsockopt(event_fds.high_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf,
                   sizeof(sndbuf)) < 0) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not setsockopt for SO_SNDBUF, high_fd\n");
    }
    if (setsockopt(event_fds.high_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf,
                   sizeof(rcvbuf)) < 0) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not setsockopt for SO_RCVBUF, high_fd\n");
    }
    if (setsockopt(event_fds.med_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf,
                   sizeof(sndbuf)) < 0) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not setsockopt for SO_SNDBUF, med_fd\n");
    }
    if (setsockopt(event_fds.med_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf,
                   sizeof(rcvbuf)) < 0) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not setsockopt for SO_RCVBUF, med_fd\n");
    }

    if (setsockopt(event_fds.low_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf,
                   sizeof(sndbuf)) < 0) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not setsockopt for SO_SNDBUF, low_fd\n");
    }
    if (setsockopt(event_fds.low_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf,
                   sizeof(rcvbuf)) < 0) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not setsockopt for SO_RCVBUF, low_fd\n");
    }

    err = init_command_db(&mac_sync_dispatcher_ibc_msg_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_comm_layer_wrapper_init(&comm_layer_wrapper,
                                       MAC_SYNC_TCP_PORT, rcv_msg_handler,
                                       net_order_msg_handler, add_fd_handler,
                                       SOCKET_PROTECTION);
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* Open mac sync context */
    cl_err = cl_thread_init(&mac_sync_thread, mlag_mac_sync_dispacher_thread,
                            &mac_sync_dispatcher_conf, NULL);
    if (cl_err != CL_SUCCESS) {
        err = ENOMEM;
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not create mac sync dispatcher thread\n");
    }

    /* Init the mac sync modules */
    err = modules_init();
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function deinits mlag mac sync dispatcher
 *
 * @return ERROR if operation failed.
 */
int
mlag_mac_sync_dispatcher_deinit(void)
{
    int err = 0;

    cl_thread_destroy(&mac_sync_thread);

    err = mlag_comm_layer_wrapper_deinit(&comm_layer_wrapper);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = modules_deinit();
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = close_events(&event_fds);
    MLAG_BAIL_ERROR(err);

    err = deinit_command_db(mac_sync_cmd_db);
    MLAG_BAIL_ERROR(err);

    err = deinit_command_db(mac_sync_dispatcher_ibc_msg_db);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}
