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
#include <mlag_api_defs.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include <mlnx_lib/lib_event_disp.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/health_manager/health_dispatcher.h>
#include <libs/port_manager/port_manager.h>
#include <libs/lacp_manager/lacp_manager.h>
#include <oes_types.h>
#include "mlag_manager.h"
#include "mlag_manager_db.h"
#include "mlag_master_election.h"
#include "mlag_l3_interface_manager.h"
#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_dispatcher.h"
#include "health_manager.h"

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_DISPATCHER

#define MLAG_DISPATCHER_C

/************************************************
 *  Local Macros
 ***********************************************/
#define MLAG_DISPATCHER_CONF_SET(index, u_fd, u_prio, u_handler, u_data, buf, \
                                 size) \
    DISPATCHER_CONF_SET(mlag_dispatcher_conf, index, u_fd, u_prio, u_handler, \
                        u_data, buf, size)
#define MLAG_DISPATCHER_CONF_RESET(index) \
    DISPATCHER_CONF_RESET(mlag_dispatcher_conf, index)

#define MLAG_DISP_SYS_EVENT_BUF_SIZE 16000

/************************************************
 *  Local Type definitions
 ***********************************************/

static int dispatch_start_event(uint8_t *data);
static int dispatch_stop_event(uint8_t *data);
static int dispatch_port_cfg_event(uint8_t *data);
static int dispatch_peer_state_change_event(uint8_t *data);
static int dispatch_add_peer_event(uint8_t *data);
static int dispatch_delete_peer_event(uint8_t *data);
static int dispatch_ipl_ip_addr_config_event(uint8_t *data);
static int dispatch_switch_status_change_event(uint8_t *data);
static int dispatch_vlan_local_state_change_event(uint8_t *data);
static int dispatch_vlan_local_state_change_event_from_peer(uint8_t *data);
static int dispatch_vlan_global_state_change_event(uint8_t *data);
static int dispatch_l3_interface_local_sync_done_event(uint8_t *data);
static int dispatch_l3_interface_master_sync_done_event(uint8_t *data);
static int dispatch_peer_start_event(uint8_t *data);
static int dispatch_peer_enable_event(uint8_t *data);
static int dispatch_conn_notify_event(uint8_t *data);
static int dispatch_ipl_port_set_event(uint8_t *data);
static int dispatch_l3_sync_start_event(uint8_t *data);
static int dispatch_l3_sync_finish_event(uint8_t *data);
static int dispatch_port_oper_state_change_event(uint8_t *data);
static int dispatch_peer_port_oper_state_change_event(uint8_t *data);
static int dispatch_reload_delay_expired_event(uint8_t *data);
static int dispatch_port_global_state_change_event(uint8_t *data);
static int dispatch_port_sync_data(uint8_t *data);
static int dispatch_ports_update_event(uint8_t *data);
static int dispatch_ports_oper_update_event(uint8_t *data);
static int dispatch_ports_sync_finish_event(uint8_t *data);
static int dispatch_peer_sync_done_event(uint8_t *data);
static int dispatch_reload_delay_interval_set(uint8_t *buffer);
static int dispatch_peer_delete_sync_event(uint8_t *buffer);
static int dispatch_stop_done_event(uint8_t *data);
static int dispatch_reconnect_event(uint8_t *data);
static int dispatch_port_mode_set_event(uint8_t *data);
static int dispatch_lacp_sys_id_set(uint8_t *data);
static int dispatch_lacp_selection_request(uint8_t *data);
static int dispatch_lacp_selection_event(uint8_t *data);
static int dispatch_lacp_release_event(uint8_t *data);
static int dispatch_lacp_sys_id_update_event(uint8_t *data);

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static cl_thread_t disp_thread;
static cmd_db_handle_t *mlag_cmd_db;
static event_disp_fds_t event_fds;
static struct dispatcher_conf mlag_dispatcher_conf;
static char mlag_disp_sys_event_buf[MLAG_DISP_SYS_EVENT_BUF_SIZE];

static int medium_prio_events[] = {
    MLAG_START_EVENT,
    MLAG_STOP_EVENT,
    MLAG_PEER_ADD_EVENT,
    MLAG_PEER_DEL_EVENT,
    MLAG_IPL_IP_ADDR_CONFIG_EVENT,
    MLAG_PEER_STATE_CHANGE_EVENT,
    MLAG_PEER_ENABLE_EVENT,
    MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
    MLAG_PEER_START_EVENT,
    MLAG_L3_INTERFACE_LOCAL_SYNC_DONE_EVENT,
    MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT,
    MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_EVENT,
    MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT,
    MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
    MLAG_CONN_NOTIFY_EVENT,
    MLAG_RECONNECT_EVENT,
    MLAG_IPL_PORT_SET_EVENT,
    MLAG_L3_SYNC_START_EVENT,
    MLAG_L3_SYNC_FINISH_EVENT,
    MLAG_PEER_RELOAD_DELAY_EXPIRED,
    MLAG_PORT_CFG_EVENT,
    MLAG_PORT_OPER_STATE_CHANGE_EVENT,
    MLAG_PEER_PORT_OPER_STATE_CHANGE,
    MLAG_PORT_GLOBAL_STATE_EVENT,
    MLAG_PORTS_SYNC_DATA,
    MLAG_PORTS_UPDATE_EVENT,
    MLAG_PORTS_OPER_SYNC_DONE,
    MLAG_PORTS_SYNC_FINISH_EVENT,
    MLAG_PEER_SYNC_DONE,
    MLAG_DEINIT_EVENT,
    MLAG_RELOAD_DELAY_INTERVAL_CHANGE,
    MLAG_PEER_DEL_SYNC_EVENT,
    MLAG_STOP_DONE_EVENT,
    MLAG_PORT_MODE_SET_EVENT,
    MLAG_LACP_SYS_ID_SET,
    MLAG_LACP_SELECTION_REQUEST,
    MLAG_LACP_SELECTION_EVENT,
    MLAG_LACP_RELEASE_EVENT,
    MLAG_LACP_SYS_ID_UPDATE_EVENT,
};


static handler_command_t mlag_dispatcher_commands[] = {
    {MLAG_START_EVENT, "Start event", dispatch_start_event, NULL},
    {MLAG_STOP_EVENT, "Stop event", dispatch_stop_event, NULL},

    {MLAG_PEER_STATE_CHANGE_EVENT, "Peer State Change event",
     dispatch_peer_state_change_event, NULL},
    {MLAG_PEER_ADD_EVENT, "Add peer event",
     dispatch_add_peer_event, NULL},
    {MLAG_PEER_DEL_EVENT, "Del peer event",
     dispatch_delete_peer_event, NULL},
    {MLAG_IPL_IP_ADDR_CONFIG_EVENT, "IPL IP addr config event",
     dispatch_ipl_ip_addr_config_event, NULL},
    {MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
     "Master election switch status change event",
     dispatch_switch_status_change_event, NULL},
    {MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_EVENT,
     "L3 interface vlan local state change event",
     dispatch_vlan_local_state_change_event, NULL},
    {MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT,
     "L3 interface vlan local state change event from peer",
     dispatch_vlan_local_state_change_event_from_peer, NULL},
    {MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
     "L3 interface vlan global state change event",
     dispatch_vlan_global_state_change_event, NULL},
    {MLAG_L3_INTERFACE_LOCAL_SYNC_DONE_EVENT,
     "L3 interface local sync done event",
     dispatch_l3_interface_local_sync_done_event, NULL},
    {MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT,
     "L3 interface master sync done event",
     dispatch_l3_interface_master_sync_done_event, NULL},
    {MLAG_PEER_START_EVENT, "Peer start event",
     dispatch_peer_start_event, NULL},
    {MLAG_PEER_ENABLE_EVENT, "Peer enable event",
     dispatch_peer_enable_event, NULL},
    {MLAG_CONN_NOTIFY_EVENT, "Conn notify event",
     dispatch_conn_notify_event, NULL},
    {MLAG_RECONNECT_EVENT, "Reconnect event",
     dispatch_reconnect_event, NULL},
    {MLAG_IPL_PORT_SET_EVENT, "IPL port set event",
     dispatch_ipl_port_set_event, NULL},
    {MLAG_L3_SYNC_START_EVENT, "L3 Sync start event",
     dispatch_l3_sync_start_event, NULL},
    {MLAG_L3_SYNC_FINISH_EVENT, "L3 Sync finish event",
     dispatch_l3_sync_finish_event, NULL},

    {MLAG_PORT_CFG_EVENT, "Port Config event", dispatch_port_cfg_event, NULL },
    {MLAG_PORT_OPER_STATE_CHANGE_EVENT, "Port oper state change",
     dispatch_port_oper_state_change_event, NULL },
    {MLAG_PEER_PORT_OPER_STATE_CHANGE, "Peer port oper state change",
     dispatch_peer_port_oper_state_change_event, NULL },
    {MLAG_PEER_RELOAD_DELAY_EXPIRED, "Reload delay expired",
     dispatch_reload_delay_expired_event, NULL },
    {MLAG_PORT_GLOBAL_STATE_EVENT, "Port global state change",
     dispatch_port_global_state_change_event, NULL},
    {MLAG_PORTS_SYNC_DATA, "Ports sync data", dispatch_port_sync_data, NULL },
    {MLAG_PORTS_UPDATE_EVENT, "Ports update data",
     dispatch_ports_update_event, NULL },
    {MLAG_PORTS_OPER_SYNC_DONE, "Ports oper states sync done",
     dispatch_ports_oper_update_event, NULL },
    {MLAG_PORTS_SYNC_FINISH_EVENT, "Ports sync finish event",
     dispatch_ports_sync_finish_event, NULL},
    {MLAG_PEER_SYNC_DONE, "Peer sync done event",
     dispatch_peer_sync_done_event, NULL},
    {MLAG_DEINIT_EVENT, "Deinit event", dispatch_deinit_event, NULL},
    {MLAG_RELOAD_DELAY_INTERVAL_CHANGE, "Reload delay interval change",
     dispatch_reload_delay_interval_set, NULL},
    {MLAG_PEER_DEL_SYNC_EVENT, "Peer delete sync event",
     dispatch_peer_delete_sync_event, NULL},
    {MLAG_STOP_DONE_EVENT, "Mlag_stop_done event",
     dispatch_stop_done_event, NULL},
    {MLAG_PORT_MODE_SET_EVENT, "Mlag port mode set event",
     dispatch_port_mode_set_event, NULL },
    {MLAG_LACP_SYS_ID_SET, "Mlag lacp system ID set",
     dispatch_lacp_sys_id_set, NULL},
    {MLAG_LACP_SELECTION_REQUEST, "Mlag lacp selection request",
     dispatch_lacp_selection_request, NULL},
    {MLAG_LACP_SELECTION_EVENT, "Mlag LACP selection event",
     dispatch_lacp_selection_event, NULL },
     {MLAG_LACP_RELEASE_EVENT, "Mlag LACP release event",
      dispatch_lacp_release_event, NULL },
     {MLAG_LACP_SYS_ID_UPDATE_EVENT, "Mlag LACP system id update event",
      dispatch_lacp_sys_id_update_event, NULL },
     {0, "", NULL, NULL}
};

static struct mlag_comm_layer_wrapper_data comm_layer_wrapper;
static cmd_db_handle_t *mlag_dispatcher_ibc_msg_db;

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
 *  This function returns module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_dispatcher_log_verbosity_get(void)
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
mlag_dispatcher_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/*
 *  This function dispatches start event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_start_event(uint8_t *data)
{
    int err = 0;

    comm_layer_wrapper.current_switch_status = DEFAULT_SWITCH_STATUS;

    err = port_manager_start();
    MLAG_BAIL_ERROR_MSG(err, "Port manager module start failed\n");

    err = lacp_manager_start(data);
    MLAG_BAIL_ERROR_MSG(err, "LACP manager module start failed\n");

    err = mlag_manager_start(data);
    MLAG_BAIL_ERROR_MSG(err, "Mlag manager module start failed\n");

    err = mlag_l3_interface_start(data);
    MLAG_BAIL_ERROR_MSG(err, "L3 interface manager module start failed\n");

    err = mlag_master_election_start(data);
    MLAG_BAIL_ERROR_MSG(err, "Master election module start failed\n");

bail:
    return err;
}

/*
 *  This function dispatches stop event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_stop_event(uint8_t *data)
{
    int err = 0;
    struct sync_event_data stop_done;

    err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper, SERVER_STOP, 0);
    MLAG_BAIL_ERROR_MSG(err, "Comm layer wrapper module stop failed\n");

    err = mlag_l3_interface_stop(data);
    MLAG_BAIL_ERROR_MSG(err, "L3 interface manager module stop failed\n");

    err = mlag_master_election_stop(data);
    MLAG_BAIL_ERROR_MSG(err, "Master election module stop failed\n");

    err = lacp_manager_stop();
    MLAG_BAIL_ERROR_MSG(err, "LACP manager module stop failed\n");

    err = port_manager_stop();
    MLAG_BAIL_ERROR_MSG(err, "Port manager module stop failed\n");

    err = mlag_manager_stop(data);
    MLAG_BAIL_ERROR_MSG(err, "Mlag manager module stop failed\n");

bail:
    /* Send MLAG_STOP_DONE_EVENT to MLag Manager */
    stop_done.sync_type = MLAG_MANAGER_STOP_DONE;
    err = send_system_event(MLAG_STOP_DONE_EVENT,
                            &stop_done, sizeof(stop_done));
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "send_system_event returned error %d\n", err);
    }

    return err;
}

/*
 *  This function dispatches connection notification event
 *  and calls comm layer wrapper handler
 *
 *  @param[in] data - struct with connection info
 *
 * @return int as error code.
 */
static int
dispatch_conn_notify_event(uint8_t *data)
{
    int err = 0;
    struct tcp_conn_notification_event_data *ev =
        (struct tcp_conn_notification_event_data *) data;

    if (ev->data == &comm_layer_wrapper) {
        err = mlag_comm_layer_wrapper_connection_notification(
            ev->new_handle, ev->port, ev->ipv4_addr, ev->data, &ev->rc);
        MLAG_BAIL_ERROR_MSG(err,
                            "Connection notification event dispatch failed\n");

        if ((comm_layer_wrapper.is_started) &&
            (ev->rc == 0)) {
            err = mlag_manager_peer_connected(ev);
            MLAG_BAIL_ERROR_MSG(err,
                                "Peer connected notification handling failed\n");
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
        MLAG_BAIL_ERROR_MSG(err, "Reconnect event dispatch failed\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches port add event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_port_cfg_event(uint8_t *data)
{
    int err = 0;
    struct port_conf_event_data *port_cfg;

    port_cfg = (struct port_conf_event_data *)data;

    if (port_cfg->del_ports == FALSE) {
        err = port_manager_mlag_ports_add(port_cfg->ports, port_cfg->port_num);
    }
    else {
        if (port_cfg->port_num != 1) {
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "port number to delete is %d, supported only one port\n",
                     port_cfg->port_num);
        }
        err = port_manager_mlag_port_delete(port_cfg->ports[0]);
    }
    MLAG_BAIL_ERROR_MSG(err, "Port configuration handling failed\n");

bail:
    return err;
}

/*
 *  This function dispatches peer state change event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_peer_state_change_event(uint8_t *data)
{
    int err = 0;
    struct peer_state_change_data *state_change;

    state_change = (struct peer_state_change_data *)data;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "dispatch peer [%d] state [%s] \n", state_change->mlag_id,
             health_state_str[state_change->state]);

    if ((comm_layer_wrapper.current_switch_status == MASTER) &&
        (((state_change->state == HEALTH_PEER_DOWN) ||
          ((state_change->state == HEALTH_PEER_COMM_DOWN) ||
           (state_change->state == HEALTH_PEER_DOWN_WAIT))))) {
        err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper, PEER_STOP,
                                           state_change->mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to stop comm layer wrapper\n");
    }

    /* Open TCP connection from client side - only for SLAVE */
    if ((comm_layer_wrapper.current_switch_status == SLAVE) &&
        (state_change->state == HEALTH_PEER_UP)) {
        err = mlag_comm_layer_wrapper_start(&comm_layer_wrapper);
        MLAG_BAIL_ERROR_MSG(err, "Failed to connect to master\n");
    }
    else if ((comm_layer_wrapper.current_switch_status == SLAVE) &&
             ((state_change->state == HEALTH_PEER_DOWN) ||
              (state_change->state == HEALTH_PEER_COMM_DOWN) ||
              (state_change->state == HEALTH_PEER_DOWN_WAIT))) {
        err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper, PEER_STOP,
                                           ((struct peer_state_change_data*)
                                            data)->mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to stop comm layer wrapper\n");
    }

    err = lacp_manager_peer_state_change(state_change);
    MLAG_BAIL_ERROR_MSG(err,
                        "LACP manager failure handling peer state change\n");

    err = port_manager_peer_state_change(state_change);
    MLAG_BAIL_ERROR_MSG(err,
                        "Port manager failure handling peer state change\n");

    err = mlag_master_election_peer_status_change(state_change);
    MLAG_BAIL_ERROR_MSG(err,
                        "Master election failure handling peer state change\n");

    err = mlag_l3_interface_peer_status_change(state_change);
    MLAG_BAIL_ERROR_MSG(err,
                        "L3 interface manager failure handling peer state change\n");

    err = mlag_manager_peer_status_change(state_change);
    MLAG_BAIL_ERROR_MSG(err,
                        "Mlag manager failure handling peer state change\n");

bail:
    return err;
}

/*
 *  This function dispatches add peer event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_add_peer_event(uint8_t *data)
{
    int err = 0;
    struct config_change_event config_change_data;

    config_change_data.ipl_id = ((struct peer_conf_event_data*)data)->ipl_id;
    config_change_data.peer_id = 0;
    config_change_data.my_ip_addr_cmd = NO_CHANGE;
    config_change_data.my_ip_addr = 0;
    config_change_data.peer_ip_addr_cmd = MODIFY;
    config_change_data.peer_ip_addr =
        ((struct peer_conf_event_data*)data)->peer_ip;
    config_change_data.vlan_id =
        ((struct peer_conf_event_data*)data)->vlan_id;

    err = mlag_master_election_config_change(&config_change_data);
    MLAG_BAIL_ERROR_MSG(err, "Master election failure handling peer add\n");

    /* Add IPL to vlan of l3 interface on IPL for control messages */
    err = mlag_l3_interface_peer_add((struct peer_conf_event_data*)data);
    MLAG_BAIL_ERROR_MSG(err,
                        "L3 interface manager failure handling peer add\n");

    err = mlag_manager_peer_add((struct peer_conf_event_data*)data);
    MLAG_BAIL_ERROR_MSG(err, "Mlag manager failure handling peer add\n");

bail:
    return err;
}

/*
 *  This function dispatches delete peer event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_delete_peer_event(uint8_t *data)
{
    int err = 0;
    struct config_change_event config_change_data;

    config_change_data.ipl_id = ((struct peer_conf_event_data*)data)->ipl_id;
    config_change_data.peer_id = 0;
    config_change_data.my_ip_addr_cmd = NO_CHANGE;
    config_change_data.my_ip_addr = 0;
    config_change_data.peer_ip_addr_cmd = MODIFY;
    config_change_data.peer_ip_addr = 0;

    err = mlag_master_election_config_change(&config_change_data);
    MLAG_BAIL_ERROR_MSG(err, "Master election failure handling peer delete\n");

    /* Remove IPL from vlan of l3 interface on IPL for control messages */
    err = mlag_l3_interface_peer_del();
    MLAG_BAIL_ERROR_MSG(err,
                        "L3 interface manager failure handling peer delete\n");

bail:
    return err;
}

/*
 *  This function dispatches IPL IP address config event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_ipl_ip_addr_config_event(uint8_t *data)
{
    int err = 0;
    struct conf_ipl_ip_event_data *ipl_ip_data;
    struct config_change_event config_change_data;

    ipl_ip_data = (struct conf_ipl_ip_event_data *) data;

    err = mlag_manager_db_local_ip_set(ipl_ip_data->local_ip_addr);
    MLAG_BAIL_ERROR(err);

    config_change_data.ipl_id = ipl_ip_data->ipl_id;
    config_change_data.peer_id = 1;
    config_change_data.my_ip_addr_cmd = MODIFY;
    config_change_data.my_ip_addr = ipl_ip_data->local_ip_addr;
    config_change_data.peer_ip_addr_cmd = NO_CHANGE;
    config_change_data.peer_ip_addr = 0;

    err = mlag_master_election_config_change(&config_change_data);
    MLAG_BAIL_ERROR_MSG(err,
                        "Master election failure handling IPL IP change\n");

    if (ipl_ip_data->local_ip_addr == 0) {
        /* Remove IPL from vlan of l3 interface on IPL for control messages */
        err = mlag_l3_interface_peer_del();
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/*
 *  This function dispatches Master Election switch status change event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_switch_status_change_event(uint8_t *data)
{
    int err = 0;
    struct switch_status_change_event_data *ev =
        (struct switch_status_change_event_data *) data;
    char *current_status_str;
    char *previous_status_str;

    err = mlag_master_election_get_switch_status_str(ev->current_status,
                                                     &current_status_str);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME current status str\n");
    err = mlag_master_election_get_switch_status_str(ev->previous_status,
                                                     &previous_status_str);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME previous status str\n");

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Master Election switch status change event: current %s, previous %s\n",
             current_status_str, previous_status_str);

    /* Close what we had previously before changing the status in the wrapper */
    MLAG_LOG(MLAG_LOG_NOTICE, "Closing all connections and/or listener\n");
    err = mlag_comm_layer_wrapper_stop(&comm_layer_wrapper,
                                       SERVER_STOP,
                                       ev->my_peer_id);
    MLAG_BAIL_ERROR(err);

    comm_layer_wrapper.current_switch_status = ev->current_status;

    if (comm_layer_wrapper.current_switch_status == MASTER) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Start listener on Master \n");
        err = mlag_comm_layer_wrapper_start(&comm_layer_wrapper);
        MLAG_BAIL_ERROR_MSG(err, "Failed starting mlag listener socket\n");
    }

    err = mlag_l3_interface_switch_status_change(ev);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed handling role change in L3 interface manager\n");

    err = lacp_manager_role_change(ev->current_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed handling role change in lacp manager\n");

    err = port_manager_role_change(ev->current_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed handling role change in port manager\n");

    err = mlag_manager_role_change(ev->previous_status, ev->current_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed handling role change in mlag manager\n");

bail:
    return err;
}

/*
 *  This function dispatches L3 interface vlan local state change event
 *
 * @return int as error code.
 */
static int
dispatch_vlan_local_state_change_event(uint8_t *data)
{
    int err = 0;

    err = mlag_l3_interface_vlan_local_state_change(
        (struct vlan_state_change_event_data *)data);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches L3 interface vlan local state change event from peer
 *
 * @return int as error code.
 */
static int
dispatch_vlan_local_state_change_event_from_peer(uint8_t *data)
{
    int err = 0;

    err = mlag_l3_interface_vlan_state_change_from_peer(
        (struct vlan_state_change_event_data *)data);

    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches peer port oper status change event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_peer_port_oper_state_change_event(uint8_t *data)
{
    int err = 0;
    struct port_oper_state_change_data *oper_chg =
        (struct port_oper_state_change_data *)data;

    err = port_manager_peer_oper_state_change(oper_chg);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches reload delay timer expiration event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_reload_delay_expired_event(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    err = mlag_manager_enable_ports();
    MLAG_BAIL_ERROR_MSG(err, "Failed in reload delay expired handling\n");

bail:
    return err;
}

/*
 *  This function dispatches peer start event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_port_global_state_change_event(uint8_t *data)
{
    int err = 0;
    struct port_global_state_event_data *port_states;

    port_states = (struct port_global_state_event_data *)data;

    err = port_manager_global_oper_state_set(port_states->port_id,
                                             port_states->state,
                                             port_states->port_num);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches L3 interface vlan global state change event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_vlan_global_state_change_event(uint8_t *data)
{
    int err = 0;

    err = mlag_l3_interface_vlan_global_state_change(
        (struct vlan_state_change_event_data *)data);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches port oper status change event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_port_oper_state_change_event(uint8_t *data)
{
    int err = 0;
    struct port_oper_state_change_data *oper_chg =
        (struct port_oper_state_change_data *)data;

    if (oper_chg->is_ipl == FALSE) {
        if (oper_chg->state == OES_PORT_UP) {
            err = port_manager_local_port_up(oper_chg->port_id);
        }
        else {
            err = port_manager_local_port_down(oper_chg->port_id);
        }
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/*
 *  This function dispatches L3 interface local sync done event
 *
 * @return int as error code.
 */
static int
dispatch_l3_interface_local_sync_done_event(uint8_t *data)
{
    int err = 0;

    err = mlag_l3_interface_local_sync_done(data);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches L3 interface master sync done event
 *
 * @return int as error code.
 */
static int
dispatch_l3_interface_master_sync_done_event(uint8_t *data)
{
    int err = 0;

    err = mlag_l3_interface_master_sync_done(data);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches peer start event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_peer_start_event(uint8_t *data)
{
    int err = 0;

    struct peer_state_change_data *peer_start;

    peer_start = (struct peer_state_change_data *)data;

    err = mlag_l3_interface_peer_start_event(peer_start);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in peer start dispatch to L3 interface manager\n");

    err = port_manager_peer_start(peer_start->mlag_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in peer start dispatch to port manager\n");

    err = lacp_manager_peer_start(peer_start->mlag_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in peer start dispatch to lacp manager\n");

bail:
    return err;
}

/*
 *  This function dispatches peer enable event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_peer_enable_event(uint8_t *data)
{
    int err = 0;
    struct peer_state_change_data *peer_en;

    peer_en = (struct peer_state_change_data *)data;

    err = mlag_l3_interface_peer_enable(peer_en);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in peer enable dispatch to L3 interface manager\n");

    err = port_manager_peer_enable(peer_en->mlag_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in peer enable dispatch to port manager\n");

    err = mlag_manager_peer_enable(peer_en);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in peer enable dispatch to mlag manager\n");

bail:
    return err;
}

/*
 *  This function dispatches sync start event
 *
 * @return int as error code.
 */
static int
dispatch_l3_sync_start_event(uint8_t *data)
{
    int err = 0;

    err = mlag_l3_interface_sync_start((struct sync_event_data *)data);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function dispatches sync done event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_l3_sync_finish_event(uint8_t *data)
{
    int err = 0;
    struct sync_event_data *sync_done;

    sync_done = (struct sync_event_data *)data;

    err = mlag_l3_interface_sync_finish(sync_done);
    MLAG_BAIL_ERROR_MSG(err, "L3 manager sync finish dispatch failed\n");

bail:
    return err;
}

/*
 *  This function dispatches port sync event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_port_sync_data(uint8_t *data)
{
    int err = 0;
    struct peer_port_sync_message *sync_msg;

    sync_msg = (struct peer_port_sync_message *)data;

    err = port_manager_port_sync(sync_msg);
    MLAG_BAIL_ERROR_MSG(err, "Dispatch port sync data failed\n");

bail:
    return err;
}

/*
 *  This function dispatches ports update event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_ports_update_event(uint8_t *data)
{
    int err = 0;
    struct peer_port_sync_message *sync_msg;

    sync_msg = (struct peer_port_sync_message *)data;

    err = port_manager_port_update(sync_msg);
    MLAG_BAIL_ERROR_MSG(err, "Dispatch ports update event failed\n");

bail:
    return err;
}

/*
 *  This function dispatches oper states sync done event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_ports_oper_update_event(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    err = port_manager_oper_sync_done();
    MLAG_BAIL_ERROR_MSG(err, "Dispatch ports oper update event failed\n");

bail:
    return err;
}

/*
 *  This function dispatches ipl port set event
 *
 *  @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_ipl_port_set_event(uint8_t *data)
{
    int err = 0;

    err =
        mlag_l3_interface_ipl_port_set((struct ipl_port_set_event_data *)data);
    MLAG_BAIL_ERROR_MSG(err, "Dispatch IPL port set event failed\n");

bail:
    return err;
}

/*
 *  This function dispatches port sync done event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_ports_sync_finish_event(uint8_t *data)
{
    int err = 0;
    struct sync_event_data *sync_done;

    sync_done = (struct sync_event_data *)data;

    err = port_manager_sync_finish(sync_done);
    MLAG_BAIL_ERROR_MSG(err, "Dispatch ports sync finish event failed\n");

bail:
    return err;
}

/*
 *  This function dispatches peer sync done event
 *
 * @param data - event data
 *
 * @return int as error code.
 */
static int
dispatch_peer_sync_done_event(uint8_t *data)
{
    int err = 0;
    struct sync_event_data *sync_done;

    sync_done = (struct sync_event_data *)data;

    err = mlag_manager_sync_done_notify(sync_done->peer_id,
                                        sync_done->sync_type);
    MLAG_BAIL_ERROR_MSG(err, "Dispatch peer sync done event failed\n");

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
    cmd_db_handle_t *cmd_db_handle = mlag_dispatcher_ibc_msg_db;
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

    opcode = *(uint16_t*)msg_data;

    err = get_command(cmd_db_handle, opcode, &cmd_data);
    MLAG_BAIL_ERROR_MSG(err, "Command [%d] not found in cmd db\n", opcode);

    MLAG_LOG(MLAG_LOG_INFO,
             "%s (opcode=%d) received by mlag dispatcher from remote switch, payload length %d\n",
             cmd_data.name, opcode, len);

    if (cmd_data.func == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Empty function\n");
        goto bail;
    }
    err = cmd_data.func((uint8_t *)payload_data);
    if (err != -ECANCELED) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Unexpected error handling message opcode [%d]\n",
                            opcode);
    }

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
    cmd_db_handle_t *cmd_db_handle = mlag_dispatcher_ibc_msg_db;
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
        MLAG_LOG(MLAG_LOG_NOTICE, "Empty network order function\n");
        goto bail;
    }
    err = cmd_data.net_order_func((uint8_t *)payload, oper);
    if (err != -ECANCELED) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Unexpected error in net order handle opcode [%d]",
                            opcode);
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
mlag_dispatcher_add_fd(int fd_index, int fd, fd_handler_func handler)
{
    MLAG_LOG(MLAG_LOG_INFO,
             "mlag dispatcher added FD %d to index %d\n",
             fd, fd_index);

    MLAG_DISPATCHER_CONF_SET(fd_index, fd, HIGH_PRIORITY, handler,
                             &comm_layer_wrapper, 0, 0);
    return 0;
}

/*
 *  This function deletes fd from dispatcher with specified index
 *
 * @return ERROR if operation failed.
 */
static int
mlag_dispatcher_delete_fd(int fd_index)
{
    MLAG_LOG(MLAG_LOG_INFO,
             "mlag dispatcher deleted FD from index %d\n",
             fd_index);

    MLAG_DISPATCHER_CONF_RESET(fd_index);
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
        err = mlag_dispatcher_add_fd(
            MLAG_DISPATCHER_FD_INDEX,
            new_handle,
            mlag_comm_layer_wrapper_message_dispatcher);
        MLAG_BAIL_ERROR_MSG(err, "Failed to add FD to mlag dispatcher\n");
    }
    else {
        /* Delete fd from fd set */
        err = mlag_dispatcher_delete_fd(MLAG_DISPATCHER_FD_INDEX);
        MLAG_BAIL_ERROR_MSG(err, "Failed to delete FD from mlag dispatcher\n");
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
mlag_dispatcher_message_send(enum mlag_events opcode,
                             void* payload, uint32_t payload_len,
                             uint8_t dest_peer_id,
                             enum message_originator orig)
{
    return mlag_comm_layer_wrapper_message_send(&comm_layer_wrapper,
                                                opcode, (uint8_t *) payload,
                                                payload_len,
                                                dest_peer_id, orig);
}

/**
 *  This function prints comm layer wrapper information to log
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_dispatcher_comm_layer_wrapper_print(void (*dump_cb)(const char *, ...))
{
    int err = 0;
    char *current_status_str;

    err = mlag_master_election_get_switch_status_str(
        comm_layer_wrapper.current_switch_status,
        &current_status_str);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting switch role string\n");

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Mlag dispatcher comm layer wrapper info:");
        MLAG_LOG(MLAG_LOG_NOTICE, "current_switch_status = %s\n",
                 current_status_str);
        MLAG_LOG(MLAG_LOG_NOTICE, "dest_ip_addr = 0x%08x\n",
                 comm_layer_wrapper.dest_ip_addr);
        MLAG_LOG(MLAG_LOG_NOTICE, "tcp_port = %d\n",
                 comm_layer_wrapper.tcp_port);
        MLAG_LOG(MLAG_LOG_NOTICE, "tcp_server_id = %d\n",
                 comm_layer_wrapper.tcp_server_id);
        if (comm_layer_wrapper.current_switch_status == MASTER) {
            MLAG_LOG(MLAG_LOG_NOTICE, "tcp_sock_handle[1] = %d\n",
                     comm_layer_wrapper.tcp_sock_handle[1]);
        }
        else {
            MLAG_LOG(MLAG_LOG_NOTICE, "tcp_sock_handle[0] = %d\n",
                     comm_layer_wrapper.tcp_sock_handle[0]);
        }
        MLAG_LOG(MLAG_LOG_NOTICE, "\n");

        MLAG_LOG(MLAG_LOG_NOTICE, "wrapper counters:");
        mlag_comm_layer_wrapper_print_counters(&comm_layer_wrapper, dump_cb);
        MLAG_LOG(MLAG_LOG_NOTICE, "\n");
    }
    else {
        dump_cb("Mlag dispatcher comm layer wrapper info:");
        dump_cb("current_switch_status = %s\n",
                current_status_str);
        dump_cb("dest_ip_addr = 0x%08x\n",
                comm_layer_wrapper.dest_ip_addr);
        dump_cb("tcp_port = %d\n", comm_layer_wrapper.tcp_port);
        dump_cb("tcp_server_id = %d\n",
                comm_layer_wrapper.tcp_server_id);
        if (comm_layer_wrapper.current_switch_status == MASTER) {
            dump_cb("tcp_sock_handle[1] = %d\n",
                    comm_layer_wrapper.tcp_sock_handle[1]);
        }
        else {
            dump_cb("tcp_sock_handle[0] = %d\n",
                    comm_layer_wrapper.tcp_sock_handle[0]);
        }
        dump_cb("\n");

        dump_cb("wrapper counters:");
        mlag_comm_layer_wrapper_print_counters(&comm_layer_wrapper, dump_cb);
        dump_cb("\n");
    }

bail:
    return 0;
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
mlag_dispatcher_insert_msgs(handler_command_t commands[])
{
    int err = 0;

    err = insert_to_command_db(mlag_dispatcher_ibc_msg_db, commands);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function Inits Mlag dispatcher's modules
 *
 * @return int as error code.
 */
static int
modules_init(void)
{
    int err = 0;

    err = lacp_manager_init(mlag_dispatcher_insert_msgs);
    MLAG_BAIL_ERROR_MSG(err, "Failed to init lacp manager module\n");

    err = port_manager_init(mlag_dispatcher_insert_msgs);
    MLAG_BAIL_ERROR_MSG(err, "Failed to init port manager module\n");

    err = mlag_manager_init(mlag_dispatcher_insert_msgs);
    MLAG_BAIL_ERROR_MSG(err, "Failed to init mlag manager module\n");

    err = mlag_master_election_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init mlag master election module\n");

    err = mlag_l3_interface_init(mlag_dispatcher_insert_msgs);
    MLAG_BAIL_ERROR_MSG(err, "Failed to init L3 interface manager module\n");

bail:
    return err;
}

/*
 *  This function deinits Mlag dispatcher's modules
 *
 * @return int as error code.
 */
static int
modules_deinit(void)
{
    int err = 0;

    err = mlag_manager_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit mlag manager module\n");

    err = mlag_master_election_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit mlag master election module\n");

    err = mlag_l3_interface_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit L3 interface manager module\n");

    err = port_manager_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit port manager module\n");

    err = lacp_manager_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit lacp manager module\n");

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
mlag_dispacher_thread(void * data)
{
    dispatcher_thread_routine(data);
}

/**
 *  This function inits mlag dispatcher
 *
 * @return ERROR if operation failed.
 */
int
mlag_dispatcher_init(void)
{
    int err;
    int i;
    cl_status_t cl_err;
    int sndbuf = MLAG_DISP_WMEM_SOCK_BUF_SIZE;
    int rcvbuf = MLAG_DISP_RMEM_SOCK_BUF_SIZE;

    err = register_events(&event_fds,
                          NULL,
                          0,
                          medium_prio_events,
                          NUM_ELEMS(medium_prio_events),
                          NULL,
                          0
                          );
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* We have 3 fds/handlers in this thread dispatcher:
     * sys_events, mlag dispatcher */
    mlag_dispatcher_conf.handlers_num = MLAG_DISPATCHER_HANDLERS_NUM;

    strncpy(mlag_dispatcher_conf.name, "MLAG", DISPATCHER_NAME_MAX_CHARS);

    for (i = 0; i < mlag_dispatcher_conf.handlers_num; i++) {
        mlag_dispatcher_conf.handler[i].fd = 0;
        mlag_dispatcher_conf.handler[i].msg_buf = NULL;
        mlag_dispatcher_conf.handler[i].buf_size = 0;
    }

    err = init_command_db(&mlag_cmd_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = insert_to_command_db(mlag_cmd_db,
                               mlag_dispatcher_commands);
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_DISPATCHER_CONF_SET(SYS_EVENTS_HIGH_FD_INDEX, event_fds.high_fd,
                             HIGH_PRIORITY,
                             dispatcher_sys_event_handler,
                             mlag_cmd_db,
                             mlag_disp_sys_event_buf,
                             sizeof(mlag_disp_sys_event_buf));
    MLAG_DISPATCHER_CONF_SET(SYS_EVENTS_LOW_FD_INDEX, event_fds.med_fd,
                             MEDIUM_PRIORITY,
                             dispatcher_sys_event_handler,
                             mlag_cmd_db,
                             mlag_disp_sys_event_buf,
                             sizeof(mlag_disp_sys_event_buf));

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

    err = init_command_db(&mlag_dispatcher_ibc_msg_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_comm_layer_wrapper_init(&comm_layer_wrapper,
                                       MLAG_DISPATCHER_TCP_PORT,
                                       rcv_msg_handler,
                                       net_order_msg_handler,
                                       add_fd_handler,
                                       NO_SOCKET_PROTECTION);
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* Open Dispatcher context */
    cl_err = cl_thread_init(&disp_thread, mlag_dispacher_thread,
                            &mlag_dispatcher_conf, NULL);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not create Mlag dispatcher thread\n");
    }

    /* Init this Dispatcher's modules */
    err = modules_init();
    MLAG_BAIL_ERROR_MSG(err, "Mlag dispatcher modules init failed\n");

bail:
    return err;
}

/**
 *  This function deinits mlag dispatcher
 *
 * @return ERROR if operation failed.
 */
int
mlag_dispatcher_deinit(void)
{
    int err = 0;

    /* Wait for orderly termination */
    cl_thread_destroy(&disp_thread);

    err = mlag_comm_layer_wrapper_deinit(&comm_layer_wrapper);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "Comm layer wrapper deinit failed\n");
    }

    err = modules_deinit();
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "mlag dispatcher modules deinit failed\n");
    }

    err = close_events(&event_fds);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "mlag dispatcher event sources deinit failed\n");
    }

    err = deinit_command_db(mlag_cmd_db);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "mlag dispatcher commands DB deinit failed\n");
    }

    err = deinit_command_db(mlag_dispatcher_ibc_msg_db);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "mlag dispatcher ibc commands DB deinit failed\n");
    }

    return err;
}

/*
 * This function dispatches peer delete sync event.
 *
 * @param[in] buffer - event data.
 *
 * @return 0 - Operation completed successfully.
 */
static int
dispatch_peer_delete_sync_event(uint8_t *buffer)
{
    int err = 0;

    err = mlag_manager_peer_delete_handle(buffer);
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer delete sync dispatch\n");

bail:
    return err;
}

/*
 * This function dispatches reload delay interval set event.
 *
 * @param[in] buffer - event data.
 *
 * @return 0 - Operation completed successfully.
 */
static int
dispatch_reload_delay_interval_set(uint8_t *buffer)
{
    int err = 0;
    struct reload_delay_change_data *change;

    change = (struct reload_delay_change_data *)buffer;
    err = mlag_manager_reload_delay_interval_set(change->msec);
    MLAG_BAIL_ERROR_MSG(err, "Reload relay interval set dispatch failed\n");

bail:
    return err;
}

/*
 * This function dispatches stop done event.
 *
 * @param[in] data - event data.
 *
 * @return 0 - Operation completed successfully.
 */
static int
dispatch_stop_done_event(uint8_t *data)
{
    int err = 0;

    err = mlag_manager_stop_done_handle(data);
    MLAG_BAIL_ERROR_MSG(err, "Dispatch stop done handle failed\n");

bail:
    return err;
}

/*
 *  This function dispatches port mode set event
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
dispatch_port_mode_set_event(uint8_t *data)
{
    int err = 0;
    struct port_mode_set_event_data *ev =
        (struct port_mode_set_event_data *) data;

    err = port_manager_port_mode_set(ev->port_id, ev->port_mode);
    MLAG_BAIL_ERROR_MSG(err, "Failed setting port mode\n");

bail:
    return err;
}

/*
 *  This function dispatches lacp local system Id set event
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
dispatch_lacp_sys_id_set(uint8_t *data)
{
    int err = 0;
    struct lacp_sys_id_set_data *ev =
        (struct lacp_sys_id_set_data *) data;

    err = lacp_manager_system_id_set(ev->sys_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed setting lacp system ID\n");

bail:
    return err;
}

/*
 *  This function dispatches lacp aggregator selection request
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
dispatch_lacp_selection_request(uint8_t *data)
{
    int err = 0;
    struct lacp_selection_request_event_data *request =
        (struct lacp_selection_request_event_data *) data;

    if (request->unselect == FALSE) {
        err = lacp_manager_aggregator_selection_request(request->request_id,
                                                        request->port_id,
                                                        request->partner_sys_id,
                                                        request->partner_key,
                                                        request->force);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling LACP selection ADD\n");
    }
    else {
        err = lacp_manager_aggregator_selection_release(request->request_id,
                                                        request->port_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed handling LACP selection release\n");
    }

bail:
    return err;
}

/*
 *  This function dispatches lacp aggregator free event
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
dispatch_lacp_release_event(uint8_t *data)
{
    int err = 0;
    struct lacp_aggregator_release_message *msg =
        (struct lacp_aggregator_release_message *) data;

    err = lacp_manager_aggregator_free_handle(msg);
    MLAG_BAIL_ERROR_MSG(err, "Failed to handle aggregation free event\n");

bail:
    return err;
}

/*
 *  This function dispatches lacp system id update event
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
dispatch_lacp_sys_id_update_event(uint8_t *data)
{
    int err = 0;

    err = port_manager_lacp_sys_id_update_handle(data);
    MLAG_BAIL_ERROR_MSG(err, "Failed to handle system id update event\n");

bail:
    return err;
}

/*
 *  This function dispatches lacp aggregator selection request
 *
 *  @param[in] data - pointer to wrapper data structure
 *
 * @return int as error code.
 */
static int
dispatch_lacp_selection_event(uint8_t *data)
{
    int err = 0;
    struct lacp_aggregation_message_data *request =
        (struct lacp_aggregation_message_data *) data;

    err = lacp_manager_aggregator_selection_handle(request);
    MLAG_BAIL_ERROR_MSG(err, "Failed to handle aggregation selection event\n");

bail:
    return err;
}
