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
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <complib/cl_init.h>
#include <complib/cl_thread.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include <mlnx_lib/lib_event_disp.h>
#include <mlnx_lib/lib_commu.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/mlag_manager/mlag_manager.h>
#include "health_manager.h"
#include "health_dispatcher.h"

/************************************************
 *  Local Defines
 ***********************************************/
#undef  __MODULE__
#define __MODULE__ MLAG_HEALTH_DISPATCHER
/************************************************
 *  Local Macros
 ***********************************************/

#define HEALTH_DISPATCHER_CONF_SET(index, u_fd, u_prio, u_handler, u_data, buf, \
                                   size) \
    DISPATCHER_CONF_SET(health_dispatcher_conf, index, u_fd, u_prio, u_handler, \
                        u_data, buf, size)
#define HEALTH_DISP_SYS_EVENT_BUF_SIZE 1500

/************************************************
 *  Global variables
 ***********************************************/


/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static event_disp_fds_t event_fds;
static cl_thread_t disp_thread;

static int medium_prio_events[] =
{   MLAG_START_EVENT,
    MLAG_STOP_EVENT,
    MLAG_PEER_ID_SET_EVENT,
    MLAG_PEER_DEL_EVENT,
    MLAG_HEALTH_KA_INTERVAL_CHANGE,
    MLAG_HEALTH_FSM_TIMER,
    MLAG_PORT_OPER_STATE_CHANGE_EVENT,
    MLAG_MGMT_OPER_STATE_CHANGE_EVENT,
    MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT };


static int low_prio_event[] =
{   MLAG_HEALTH_HEARTBEAT_TICK, };


static struct dispatcher_conf health_dispatcher_conf;
static cmd_db_handle_t *health_cmd_db;
static handle_t commlib_handle;
static char health_disp_sys_event_buf[HEALTH_DISP_SYS_EVENT_BUF_SIZE];

static int
dispatch_start_event(uint8_t *buffer);

static int
dispatch_stop_event(uint8_t *buffer);

static int
dispatch_peer_add_event(uint8_t *buffer);

static int
dispatch_peer_del_event(uint8_t *buffer);

static int
dispatch_timer_tick(uint8_t *buffer);

static int
dispatch_ka_interval_set(uint8_t *buffer);

static int
dispatch_health_fsm_timer(uint8_t *buffer);

static int
dispatch_port_state_change(uint8_t *buffer);

static int
dispatch_mgmt_state_change(uint8_t *buffer);

static int
dispatch_role_change(uint8_t *buffer);

static handler_command_t health_dispatcher_commands[] = {
    {MLAG_START_EVENT, "Start event", dispatch_start_event, NULL },
    {MLAG_STOP_EVENT, "Stop event", dispatch_stop_event, NULL },
    {MLAG_PEER_ID_SET_EVENT, "Peer add", dispatch_peer_add_event, NULL },
    {MLAG_PEER_DEL_EVENT, "Peer del", dispatch_peer_del_event, NULL },
    {MLAG_HEALTH_HEARTBEAT_TICK, "Heartbeat tick", dispatch_timer_tick, NULL },
    {MLAG_HEALTH_KA_INTERVAL_CHANGE, "KA interval change",
     dispatch_ka_interval_set, NULL },
    {MLAG_HEALTH_FSM_TIMER, "Health FSM timer event",
     dispatch_health_fsm_timer, NULL },
    {MLAG_PORT_OPER_STATE_CHANGE_EVENT, "Port oper state change",
     dispatch_port_state_change, NULL },
    {MLAG_MGMT_OPER_STATE_CHANGE_EVENT, "Mgmt oper state change",
     dispatch_mgmt_state_change, NULL },
    {MLAG_DEINIT_EVENT, "Deinit event", dispatch_deinit_event, NULL },
    {MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
     "role change event", dispatch_role_change, NULL},

    {0, "", NULL, NULL }
};

/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function sets module log verbosity level
 *
 *  @param[in] verbosity - new log verbosity
 *
 * @return void
 */
void
health_dispatcher_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/*
 *  This function is for handling comm sockets event
 *
 * @param[in] fd - file descriptor
 * @param[in] data - additional info
 *
 * @return 0 if operation completes successfully.
 */
static int
health_dispatcher_commlib_handler(int fd, void *data, char *buf, int buf_size)
{
    int err = 0, peer_id = 0;
    uint32_t len = MAX_UDP_PAYLOAD;
    uint8_t message[MAX_UDP_PAYLOAD];
    struct addr_info from;
    UNUSED_PARAM(data);
    UNUSED_PARAM(buf);
    UNUSED_PARAM(buf_size);

    err = comm_lib_udp_recv(fd, &from, message, &len);
    MLAG_BAIL_ERROR_MSG(err, "Failed to receive health udp message from ll\n");

    err =
        mlag_manager_db_peer_local_index_get(ntohl(from.ipv4_addr), &peer_id);
    if (err == -ENOENT) {
        err = 0;
        goto bail;
    }
    MLAG_BAIL_ERROR_MSG(err, "Failed to get local index from IP [0x%x]\n",
                        ntohl(from.ipv4_addr));

    err = health_manager_recv(peer_id, message, len);
    MLAG_BAIL_ERROR_MSG(err, "Failed in processing health message\n");

bail:
    return err;
}

/*
 *  This function dispatches start event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_start_event(uint8_t *buffer)
{
    UNUSED_PARAM(buffer);
    int err = 0;

    err = health_manager_start();
    MLAG_BAIL_ERROR_MSG(err, "Dispatch start event failed\n");

bail:
    return err;
}

/*
 *  This function dispatches Peer add event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_peer_add_event(uint8_t *buffer)
{
    int err = 0;
    int peer_id;
    struct peer_conf_event_data *peer_conf;

    peer_conf = (struct peer_conf_event_data *)buffer;

    err = mlag_manager_db_peer_add(peer_conf->peer_ip, &peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to add peer ip [0x%x] to db\n",
                        peer_conf->peer_ip);
    err = health_manager_peer_add(peer_id, peer_conf->ipl_id, peer_conf->peer_ip);
    MLAG_BAIL_ERROR_MSG(err, "Failed to add peer in health manager\n");

bail:
    return err;
}

/*
 *  This function dispatches Peer add event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_peer_del_event(uint8_t *buffer)
{
    int err = 0;
    int peer_id;
    struct peer_conf_event_data *peer_conf;

    peer_conf = (struct peer_conf_event_data *)buffer;

    err = mlag_manager_db_peer_local_index_get(peer_conf->peer_ip, &peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get peer local index from ip [0x%x]\n",
                        peer_conf->peer_ip);

    err = health_manager_peer_remove(peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Peer remove in health manager failed\n");

    /* Notify peer can be deleted */
    err = send_system_event(MLAG_PEER_DEL_SYNC_EVENT, peer_conf,
                            sizeof(struct peer_conf_event_data));
    MLAG_BAIL_ERROR_MSG(err, "Failed to send peer delete sync event\n");

bail:
    return err;
}

/*
 *  This function dispatches timer tick event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_timer_tick(uint8_t *buffer)
{
    int err = 0;
    UNUSED_PARAM(buffer);

    err = health_manager_timer_event();
    MLAG_BAIL_ERROR_MSG(err, "Failed to process timer tick\n");

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
dispatch_stop_event(uint8_t *buffer)
{
    int err = 0;
    UNUSED_PARAM(buffer);
    struct sync_event_data stop_done;

    err = health_manager_stop();
    MLAG_BAIL_ERROR_MSG(err, "Failed in stop event dispatch\n");

bail:
    /* Send MLAG_STOP_DONE_EVENT to MLag Manager */
    stop_done.sync_type = HEALTH_STOP_DONE;
	err = send_system_event(MLAG_STOP_DONE_EVENT,
			                &stop_done, sizeof(stop_done));
	if (err) {
	    MLAG_LOG(MLAG_LOG_ERROR,
	             "send_system_event returned error %d\n", err);
	}

    return err;
}

/*
 *  This function dispatches health FSM timer event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_health_fsm_timer(uint8_t *buffer)
{
    int err = 0;
    struct timer_event_data *timer_data = (struct timer_event_data *)buffer;

    err = health_manager_fsm_timer_event(timer_data->data);
    MLAG_BAIL_ERROR_MSG(err, "Failed to process timer event in health FSM\n");

bail:
    return err;
}

/*
 *  This function dispatches KA interval set event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_ka_interval_set(uint8_t *buffer)
{
    int err = 0;
    struct ka_interval_change_data *change;

    change = (struct ka_interval_change_data *)buffer;
    err = health_manager_heartbeat_interval_set(change->msec);
    MLAG_BAIL_ERROR_MSG(err, "Failed setting heartbeat interval\n");

bail:
    return err;
}

/*
 *  This function dispatches port state change event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_port_state_change(uint8_t *buffer)
{
    int err = 0;
    struct port_oper_state_change_data *state_change;

    state_change = (struct port_oper_state_change_data *)buffer;

    if (state_change->is_ipl == TRUE) {
        err = health_manager_ipl_state_change(state_change->port_id,
                                              state_change->state);
        MLAG_BAIL_ERROR_MSG(err, "Processing ipl state change failed\n");
    }
    else {
        /* Ignore, not interesting */
    }

bail:
    return err;
}

/*
 *  This function dispatches mgmt connection state change event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_mgmt_state_change(uint8_t *buffer)
{
    int err = 0;
    struct mgmt_oper_state_change_data *state_change;

    state_change = (struct mgmt_oper_state_change_data *)buffer;

    err = health_manager_mgmt_connection_state_change(state_change->system_id,
                                                      state_change->state);
    MLAG_BAIL_ERROR_MSG(err, "Failed to process mgmt connection state change\n");

bail:
    return err;
}

/*
 *  This function dispatches role change event
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
dispatch_role_change(uint8_t *buffer)
{
    int err = 0;
    struct switch_status_change_event_data *role_change =
        (struct switch_status_change_event_data *) buffer;

    err = health_manager_role_change(role_change);
    MLAG_BAIL_ERROR_MSG(err, "Failed to process role change\n");

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
health_dispacher_thread(void * data)
{
    dispatcher_thread_routine(data);
}

/**
 *  This function inits all health sub-modules
 *
 * @return ERROR if operation failed.
 */
int
mlag_health_dispatcher_init(void)
{
    int err = 0;
    cl_status_t cl_err;
    struct udp_params health_udp_conn;

    err = init_command_db(&health_cmd_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = insert_to_command_db(health_cmd_db, health_dispatcher_commands);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err =
        register_events(&event_fds, NULL, 0, medium_prio_events,
                        NUM_ELEMS(medium_prio_events),
                        low_prio_event, NUM_ELEMS(
                            low_prio_event));
    MLAG_BAIL_CHECK_NO_MSG(err);

    HEALTH_DISPATCHER_CONF_SET(0, event_fds.high_fd, 0,
                               dispatcher_sys_event_handler,
                               health_cmd_db,
                               health_disp_sys_event_buf,
                               sizeof(health_disp_sys_event_buf));

    health_udp_conn.type = CONN_TYPE_UDP_IP_UC;
    health_udp_conn.fields.ip_udp_params.connection_role = CONN_SERVER;
    health_udp_conn.fields.ip_udp_params.is_single_peer = 0;
    health_udp_conn.fields.ip_udp_params.session_params.msg_type = 0;
    health_udp_conn.fields.ip_udp_params.session_params.port = htons(
        HEARTBEAT_PORT);
    health_udp_conn.fields.ip_udp_params.session_params.s_ipv4_addr = htonl(
        INADDR_ANY);
    err = comm_lib_udp_session_start(&commlib_handle, &health_udp_conn);
    MLAG_BAIL_ERROR_MSG(err, "Failed to start health UDP session\n");

    HEALTH_DISPATCHER_CONF_SET(1, event_fds.med_fd, 1,
                               dispatcher_sys_event_handler, health_cmd_db,
                               health_disp_sys_event_buf,
                               sizeof(health_disp_sys_event_buf));
    HEALTH_DISPATCHER_CONF_SET(2, commlib_handle, 2,
                               health_dispatcher_commlib_handler, NULL, NULL,
                               0);
    HEALTH_DISPATCHER_CONF_SET(3, event_fds.low_fd, 3,
                               dispatcher_sys_event_handler, health_cmd_db,
                               health_disp_sys_event_buf,
                               sizeof(health_disp_sys_event_buf));
    health_dispatcher_conf.handlers_num = HEALTH_DISPATCHER_HANDLERS_NUM;
    strncpy(health_dispatcher_conf.name, "Health", DISPATCHER_NAME_MAX_CHARS);

    err = health_manager_init(commlib_handle);
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* Open Dispatcher context */
    cl_err = cl_thread_init(&disp_thread, health_dispacher_thread,
                            &health_dispatcher_conf, NULL);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err,
                            "Could not create Health dispatcher thread\n");
    }

bail:
    return err;
}

/**
 *  This function de-Inits Mlag health
 *
 * @return int as error code.
 */
int
mlag_health_dispatcher_deinit(void)
{
    int err;

    /* De-init all modules */
    cl_thread_destroy(&disp_thread);

    err = health_manager_deinit();
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = close_events(&event_fds);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = deinit_command_db(health_cmd_db);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = comm_lib_udp_session_stop(commlib_handle);
    MLAG_BAIL_ERROR_MSG(err, "Failed stopping health UDP session\n");

bail:
    return err;
}

