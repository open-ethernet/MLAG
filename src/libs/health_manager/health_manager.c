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

#include <complib/cl_init.h>
#include <complib/cl_timer.h>
#include <complib/cl_mem.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/mlag_manager/mlag_manager.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include <mlnx_lib/lib_commu.h>
#include "mlag_api_defs.h"
#include <oes_types.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include <errno.h>
#include "heartbeat.h"
#include "health_fsm.h"

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_HEALTH

#define HEALTH_MANAGER_C

#include "health_manager.h"

/************************************************
 *  Local Macros
 ***********************************************/
#define DUMP_OR_LOG(str, arg ...) do {    if (dump_cb != NULL) {        \
                                              dump_cb(str, ## arg);      \
                                          } else {                      \
                                              MLAG_LOG(MLAG_LOG_NOTICE, \
                                                       str, ## arg);     \
                                          }                             \
} while (0)
/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static cl_timer_t heartbeat_timer;
static int heartbeat_timer_msec;
static health_fsm mlag_health_fsm[MLAG_MAX_PEERS];
static enum oes_port_oper_state ipl_states[MLAG_MAX_IPLS];

static unsigned long long active_sys_ids[MLAG_MAX_PEERS];
static int send_sock;
static int started;

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

static void heartbeat_tick_cb(void *data);

/************************************************
 *  Function implementations
 ***********************************************/

/*
 * This function for FSM state change notify
 *
 * @param[in] peer_id - Peer that changed state
 * @param[in] state_id - new state ID
 *
 * @return 0 if operation completes successfully.
 */
void
health_fsm_notify_state(int peer_id, int state_id)
{
    int err = 0;
    int mlag_id;
    struct peer_state_change_data peer_state;
    uint32_t peer_ip;

    err = mlag_manager_db_peer_ip_from_local_index_get(peer_id, &peer_ip);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get ip from local index [%d]\n",
                        peer_id);

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Health : peer_ip [0x%x] new state [%u - %s]\n", peer_ip,
             state_id, health_state_str[state_id]);

    peer_state.state = state_id;
    if (started == TRUE) {
        /* send global peer ID */
        err = mlag_manager_db_mlag_peer_id_get(peer_ip, &mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed getting mlag id for peer [%d]\n",
                            peer_ip);
        peer_state.mlag_id = mlag_id;

        if(mlag_id != INVALID_MLAG_PEER_ID){

             err = send_system_event(MLAG_PEER_STATE_CHANGE_EVENT, &peer_state,
                                sizeof(peer_state));
             MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending peer state change event\n");
        }
        else{
           MLAG_LOG(MLAG_LOG_NOTICE,
                    "Health_illegal_mlag_id : peer_ip [0x%x] %d \n", peer_ip,
        	        mlag_id );
        }
    }

bail:
    return;
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
health_fsm_user_trace(char *buf, int len)
{
    UNUSED_PARAM(len);
    MLAG_LOG(MLAG_LOG_INFO, "health_fsm %s\n", buf);
}

/*
 * This function is the timer callback called when timer expires
 *
 * @param[in] data - data passed by the user upon timer construction
 *
 * @return void
 */
static void
health_fsm_timer_cb(void *data)
{
    int err = 0;

    struct timer_event_data timer_data;

    timer_data.data = data;

    err = send_system_event(MLAG_HEALTH_FSM_TIMER, &timer_data,
                            sizeof(timer_data));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending time tick event\n");

bail:
    return;
}

/*
 * This function is a timer scheduling service for FSM
 *
 * @param[in] timeout - msec timeout
 * @param[in] data - data to be used when timer expires
 * @param[out] timer_handler - timer handler returned by timer service
 *
 * @return 0 if operation completes successfully.
 */
static int
health_sched_func(int timeout, void *data, void ** timer_handler)
{
    int err = 0;
    cl_status_t cl_err;
    cl_timer_t *concrete_timer_handler = NULL;
    ASSERT(timer_handler != NULL);

    concrete_timer_handler = (cl_timer_t *)cl_malloc(sizeof(cl_timer_t));
    if (concrete_timer_handler == NULL) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR(err);
    }

    /* Init timer */
    cl_err = cl_timer_init(concrete_timer_handler, health_fsm_timer_cb, data);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init health FSM timer\n");
    }

    cl_err = cl_timer_start(concrete_timer_handler, timeout);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR(err);
    }

bail:
    *timer_handler = concrete_timer_handler;
    return err;
}

/*
 * This function is a timer unscheduling service for FSM
 *
 * @param[in] timer_handler - timer handler as returned by timer service
 *
 * @return 0 if operation completes successfully.
 */
static int
health_unsched_func(void ** timer_handler)
{
    int err = 0;
    cl_status_t cl_err;
    ASSERT(*timer_handler != NULL);
    cl_timer_stop(*timer_handler);

    /* Destroy timer */
    cl_timer_destroy(*timer_handler);

    cl_err = cl_free(*timer_handler);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}
/*
 * This function handles notification of state change from heartbeat module
 *
 * @param[in] peer_id - peer id
 * @param[in] heartbeat_state - new heartbeat state
 *
 * @return 0 if operation completes successfully.
 */
static void
health_heartbeat_state_change(int peer_id, unsigned long long sys_id,
                              heartbeat_state_t heartbeat_state)
{
    int err = 0;
    ASSERT(peer_id < MLAG_MAX_PEERS);
    MLAG_LOG(MLAG_LOG_INFO,
             "Peer [%d] sys_id [%llu] heartbeat state is [%u]\n",
             peer_id, sys_id, heartbeat_state);
    if (heartbeat_state == HEARTBEAT_DOWN) {
        err = health_fsm_ka_down_ev(&mlag_health_fsm[peer_id]);
        if (err) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "Error on heartbeat down FSM event [%d]\n", err);
        }
    }
    else {
        err = mlag_manager_db_peer_system_id_set(peer_id, sys_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed seting system id for peer [%d]\n",
                            peer_id);
        err = health_fsm_ka_up_ev(&mlag_health_fsm[peer_id]);
        if (err) {
            MLAG_LOG(MLAG_LOG_ERROR, "Error on heartbeat up FSM event [%d]\n",
                     err);
        }
    }
bail:
    return;
}

/*
 * This function handles sending heartbeat messages
 *
 * @param[in] peer_id - peer id
 * @param[in] heartbeat_payload_t - message payload
 *
 * @return 0 if operation completes successfully.
 */
static int
health_heartbeat_send_message(int peer_id, heartbeat_payload_t *payload)
{
    int err = 0;
    struct addr_info dest;
    uint32_t dest_ip, len;
    uint64_t sys_id;

    err = mlag_manager_db_peer_ip_from_local_index_get(peer_id, &dest_ip);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get peer [%d] ip\n", peer_id);

    len = sizeof(*payload);
    dest.ipv4_addr = htonl(dest_ip);
    dest.port = htons(HEARTBEAT_PORT);
    sys_id = mlag_manager_db_local_system_id_get();
    SAFE_MEMCPY(&(payload->system_id), &sys_id);
    err = comm_lib_udp_send(send_sock, dest, (uint8_t *)payload, &len);
    if (err != ENOKEY) {
        goto bail;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err, "Unexpected fault in heartbeat send\n");
    }

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
health_manager_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    heartbeat_log_verbosity_set(verbosity);
}

/**
 *  This function inits the health module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_init(int send_fd)
{
    int i;
    int err = 0;
    cl_status_t cl_err;

    heartbeat_timer_msec = DEFAULT_HEARTBEAT_MSEC;

    cl_err = cl_timer_init(&heartbeat_timer, heartbeat_tick_cb, NULL);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init heartbeat timer\n");
    }

    send_sock = send_fd;

    err = heartbeat_init();
    MLAG_BAIL_ERROR_MSG(err, "Heartbeat init failed\n");

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        err = health_fsm_init(&mlag_health_fsm[i], health_fsm_user_trace,
                              health_sched_func, health_unsched_func);
        MLAG_BAIL_ERROR_MSG(err, "Health FSM init failed\n");
        mlag_health_fsm[i].ka_state = OES_PORT_DOWN;
        mlag_health_fsm[i].notify_state_cb = NULL;

        /* Init mgmt states */
        active_sys_ids[i] = INVALID_SYS_ID;
    }

    for (i = 0; i < MLAG_MAX_IPLS; i++) {
        ipl_states[i] = OES_PORT_DOWN;
    }

bail:
    return err;
}

/**
 *  This function de-inits the health module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_deinit(void)
{
    int err = 0;

    cl_timer_destroy(&heartbeat_timer);

    err = heartbeat_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Heartbeat deinit failed\n");

bail:
    return err;
}

/**
 *  This function starts the health module. When Health module
 *  starts it may send PDUs to peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_start(void)
{
    int err = 0;
    cl_status_t cl_err;

    /* register heartbeat notify cb */
    err = heartbeat_register_state_cb(health_heartbeat_state_change);
    MLAG_BAIL_ERROR(err);

    err = heartbeat_register_send_cb(health_heartbeat_send_message);
    MLAG_BAIL_ERROR(err);

    err = heartbeat_start();
    MLAG_BAIL_ERROR_MSG(err, "Heartbeat start failed\n");

    /* start timer for heartbeat time tick */
    cl_err = cl_timer_start(&heartbeat_timer, heartbeat_timer_msec);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to start heartbeat timer\n");
    }

    started = TRUE;

bail:
    return err;
}

/**
 *  This function starts the health module. When Health module
 *  starts it may send PDUs to peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_stop(void)
{
    int err = 0;
    int i;
    notify_state_cb_t state_cb;

    err = heartbeat_register_state_cb(NULL);
    MLAG_BAIL_ERROR(err);

    cl_timer_stop(&heartbeat_timer);

    err = heartbeat_stop();
    MLAG_BAIL_ERROR_MSG(err, "Failed to stop heartbeat module\n");

    err = heartbeat_register_send_cb(NULL);
    MLAG_BAIL_ERROR(err);

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (mlag_health_fsm[i].notify_state_cb != NULL) {
            state_cb = mlag_health_fsm[i].notify_state_cb;
            mlag_health_fsm[i].notify_state_cb = NULL;
            err = health_fsm_peer_del_ev(&mlag_health_fsm[i]);
            MLAG_BAIL_ERROR_MSG(err, "Failure in health FSM peer delete\n");
            err = health_fsm_peer_add_ev(&mlag_health_fsm[i], 0);
            MLAG_BAIL_ERROR_MSG(err, "Failure in health FSM peer add\n");
            mlag_health_fsm[i].notify_state_cb = state_cb;
            mlag_health_fsm[i].ka_state = OES_PORT_DOWN;
        }
    }

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        /* Init mgmt states */
        active_sys_ids[i] = INVALID_SYS_ID;
    }

bail:
    started = FALSE;
    return err;
}

/**
 *  This function is for handling a received message over the
 *  comm channel
 *
 * @param[in] peer_id - origin peer index
 * @param[in] msg - message data
 * @param[in] msg_size - message data size
 *
 * @return 0 when successful, otherwise ERROR
 * @return ECOMM on unexpected error
 */
int
health_manager_recv(int peer_id, void *msg, int msg_size)
{
    int err = -ECOMM;
    ASSERT(msg_size == sizeof(heartbeat_payload_t));
    return heartbeat_recv(peer_id, (heartbeat_payload_t *)msg);
bail:
    return err;
}

/**
 *  This function adds a peer for health monitoring
 *  if started, the health module will monitor peer connectivity
 *  by monitoring its IPL link and heartbeat states
 *
 * @param[in] peer_id - peer index
 * @param[in] ipl_id - related ipl index
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_peer_add(int peer_id, int ipl_id, uint32_t ip)
{
    int err = 0;
    UNUSED_PARAM(ip);

    ASSERT(peer_id < MLAG_MAX_PEERS);

    if ((mlag_manager_db_is_local(peer_id) == TRUE) ||
        (ip == 0)) {
        goto bail;
    }

    /* Init FSM attributes */
    mlag_health_fsm[peer_id].peer_id = peer_id;
    mlag_health_fsm[peer_id].notify_state_cb = health_fsm_notify_state;
    mlag_health_fsm[peer_id].ka_state = OES_PORT_DOWN;

    err = health_fsm_peer_add_ev(&mlag_health_fsm[peer_id], ipl_id);
    MLAG_BAIL_ERROR_MSG(err, "Failure in health FSM peer add\n");

    err = heartbeat_peer_add(peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failure in heartbeat peer add\n");

bail:
    return err;
}

/**
 *  This function removes a peer for health monitoring
 *  The health module will no longer monitor peer connectivity
 *  after this action
 *
 * @param[in] peer_id - peer index
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_peer_remove(int peer_id)
{
    int err = 0;
    ASSERT(peer_id < MLAG_MAX_PEERS);

    if (mlag_manager_db_is_local(peer_id) == TRUE) {
        goto bail;
    }

    /* Unregister mgmt events*/
    err = health_fsm_peer_del_ev(&mlag_health_fsm[peer_id]);
    MLAG_BAIL_ERROR_MSG(err, "Failure in health FSM peer delete\n");

    err = heartbeat_peer_remove(peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failure in heartbeat peer delete\n");

bail:
    return err;
}

/**
 *  This function sets the heartbeat interval
 *
 * @param[in] interval_msec - interval in milliseconds
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_heartbeat_interval_set(unsigned int interval_msec)
{
    int err = 0;

    cl_timer_stop(&heartbeat_timer);

    heartbeat_timer_msec = interval_msec;

    if (started == TRUE) {
        err = health_manager_timer_event();
        MLAG_BAIL_ERROR_MSG(err, "Failure in timer event handler\n");
    }

bail:
    return err;
}

/**
 * This function gets the heartbeat interval.
 *
 * @param[out] interval_msec - Interval in milliseconds.
 *
 * @return 0 - Operation completed successfully.
 */
int
health_manager_heartbeat_interval_get(unsigned int *interval_msec)
{
    int err = 0;

    ASSERT(interval_msec != NULL);

    *interval_msec = heartbeat_timer_msec;
    goto bail;

bail:
    return err;
}

/**
 *  This function deals with FSM timer event
 *
 * @param[in] data - user data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_fsm_timer_event(void *data)
{
    int err = 0;

    err =
        fsm_timer_trigger_operation(data);
    MLAG_BAIL_ERROR_MSG(err, "Error in health FSM timer operation\n");

bail:
    return err;
}

/**
 *  This function sets the heartbeat local health state.
 *  Local health state is assumed to be OK, setting this state
 *  will trigger a peer down notification in the remote peer.
 *
 * @param[in] health_state - indicates local health, 0 (Zero) is
 *       OK
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_set_local_health(int health_state)
{
    return heartbeat_local_defect_set((uint8_t)health_state);
}

/**
 *  This function notifies the mlag health on OOB (management)
 *  connection state.
 *
 * @param[in] system_id - peer System ID
 * @param[in] connection_state -  peer OOB connection state
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_mgmt_connection_state_change(unsigned long long system_id,
                                            enum mgmt_oper_state mgmt_connection_state)
{
    int err = 0;
    int peer_id;
    int peer_index = MLAG_MAX_PEERS;

    /* (De)Activate system ID, we hold only active system IDs,
     * When system ID is down it is cleared */
    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        if (active_sys_ids[peer_id] == system_id) {
            peer_index = peer_id;
            break;
        }
        else if ((peer_index == MLAG_MAX_PEERS) &&
                 (active_sys_ids[peer_id] == INVALID_SYS_ID)) {
            peer_index = peer_id;
            /* Do not break here, since we still want to know
             * if we have our peer configured */
        }
    }
    ASSERT(peer_index != MLAG_MAX_PEERS);
    if (mgmt_connection_state == MGMT_UP) {
        active_sys_ids[peer_index] = system_id;
    }
    else {
        active_sys_ids[peer_index] = INVALID_SYS_ID;
    }

    /* It may be that we get unknown system ID, if KA has not yet begun */
    err = mlag_manager_db_local_index_from_system_id_get(system_id, &peer_id);
    if (err == 0) {
        if (mgmt_connection_state == MGMT_UP) {
            err = health_fsm_mgmt_up_ev(&mlag_health_fsm[peer_id]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to handle mgmt up in health FSM\n");
        }
        else {
            err = health_fsm_mgmt_down_ev(&mlag_health_fsm[peer_id]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to handle mgmt down in health FSM\n");
        }
    }
    else if (err != -ENOENT) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get local index from system id [%llu]\n",
                            system_id);
    }
    else {
        err = 0;
    }

bail:
    return err;
}

/**
 *  This function notifies the mlag health on IPL oper state
 *  changes.
 *
 * @param[in] ipl_id - IPL index
 * @param[in] ipl_state -  IPL oper state
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_ipl_state_change(int ipl_id, enum oes_port_oper_state ipl_state)
{
    int i, err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    ipl_states[ipl_id] = ipl_state;
    MLAG_LOG(MLAG_LOG_NOTICE, "IPL [%d] state change to [%u]\n", ipl_id,
             ipl_state);

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        err = health_fsm_ipl_change_ev(&mlag_health_fsm[i], ipl_id);
        MLAG_BAIL_ERROR_MSG(err, "Health FSM failure in IPL change event\n");
    }

bail:
    return err;
}

/**
 *  This function tells the current IPL oper state
 *  changes.
 *
 * @param[in] ipl_id - IPL index
 *
 * @return IPL port state
 */
enum oes_port_oper_state
health_manager_ipl_state_get(int ipl_id)
{
    enum oes_port_oper_state oper = OES_PORT_DOWN;

    if (ipl_id >= MLAG_MAX_IPLS) {
        MLAG_LOG(
            MLAG_LOG_ERROR,
            "Trying to get IPL state with IPL ID %d which is higher than the max value (%d)\n",
            ipl_id, MLAG_MAX_IPLS);
        goto bail;
    }

    oper = ipl_states[ipl_id];

bail:
    return oper;
}

/**
 *  This function tells the current MGMT oper state
 *  with a certain peer
 *
 * @param[in] peer_id - Peer local index
 *
 * @return MGMT connection state
 */
enum mgmt_oper_state
health_manager_mgmt_state_get(int peer_id)
{
    int i;
    unsigned long long sys_id;
    enum oes_port_oper_state oper = MGMT_DOWN;

    if (peer_id >= MLAG_MAX_PEERS) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Trying to get the state with PEER ID %d which is over the maximal value (%d)\n",
                 peer_id, MLAG_MAX_PEERS);
        goto bail;
    }

    sys_id = mlag_manager_db_peer_system_id_get(peer_id);

    /* search if we have the state of this peer */
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (active_sys_ids[i] == sys_id) {
            oper = MGMT_UP;
        }
    }

bail:
    return oper;
}

/**
 *  This function returns peer states
 *
 * @param[out] peer_states - array of peer states
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_peer_states_get(int peer_states[MLAG_MAX_PEERS])
{
    int i, err = 0;

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (health_fsm_idle_in(&mlag_health_fsm[i])) {
            peer_states[i] = HEALTH_PEER_NOT_EXIST;
        }
        else if (health_fsm_peer_down_in(&mlag_health_fsm[i])) {
            peer_states[i] = HEALTH_PEER_DOWN;
        }
        else if (health_fsm_comm_down_in(&mlag_health_fsm[i])) {
            peer_states[i] = HEALTH_PEER_COMM_DOWN;
        }
        else if (health_fsm_peer_up_in(&mlag_health_fsm[i])) {
            peer_states[i] = HEALTH_PEER_UP;
        }
        else if (health_fsm_down_wait_in(&mlag_health_fsm[i])) {
            peer_states[i] = HEALTH_PEER_DOWN_WAIT;
        }
    }
    return err;
}

/**
 *  This function handles timeout
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_timer_event(void)
{
    int err = 0;
    cl_status_t cl_err;

    cl_err = cl_timer_start(&heartbeat_timer, heartbeat_timer_msec);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR(err);
    }

    err = heartbeat_tick();
    MLAG_BAIL_ERROR_MSG(err, "Failure in heartbeat tick handling\n");

bail:
    return err;
}

/**
 *  This function return health module counters
 *
 * @param[in] peer_id - peer index
 * @param[in] counters -  struct containing peer counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_counters_peer_get(int peer_id, struct health_counters *counters)
{
    int err = 0;

    ASSERT(peer_id < MLAG_MAX_PEERS);
    ASSERT(counters != NULL);

    err = heartbeat_peer_stats_get(peer_id, &(counters->hb_stats));
    MLAG_BAIL_ERROR_MSG(err, "Failed to get peer [%d] heartbeat stats\n",
                        peer_id);

bail:
    return err;
}

/**
 * This function return health module counters.
 *
 * @param[in] counters - struct containing peer counters.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_counters_get(struct mlag_counters *counters)
{
    int err = 0;
    int i = 0;
    heartbeat_peer_stats_t hb_stats;

    ASSERT(counters != NULL);

    /* assume zero represent local machine */
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        /* check if peer exists */
        if (mlag_manager_is_peer_valid(i)) {
            err = heartbeat_peer_stats_get(mlag_health_fsm[i].peer_id,
                                           &hb_stats);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to get peer [%d] heartbeat stats\n",
                                mlag_health_fsm[i].peer_id);

            counters->tx_heartbeat += hb_stats.tx_heartbeat;
            counters->rx_heartbeat += hb_stats.rx_heartbeat;
        }
    }

bail:
    return err;
}

/**
 * This function clears health module counters.
 *
 * @return 0 - success
 */
void
mlag_health_manager_counters_clear(void)
{
    int err = 0;
    int i = 0;

    /* assume zero represent local machine */
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (mlag_manager_is_peer_valid(i)) {
            err = heartbeat_peer_stats_clear(mlag_health_fsm[i].peer_id);
            if (err) {
                MLAG_LOG(MLAG_LOG_ERROR,
                         "Failed to clear peer [%d] heartbeat counters\n",
                         mlag_health_fsm[i].peer_id);
            }
        }
    }
}

/**
 *  This function handles role change in switch
 *  Role change in switch triggers sending states of all peers
 *
 * @param[in] role_change - Role change event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_role_change(struct switch_status_change_event_data *role_change)
{
    int i;
    int err = 0;

    ASSERT(role_change != NULL);

    if (role_change->current_status != STANDALONE) {
        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            err = health_fsm_role_change_ev(&mlag_health_fsm[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failure handling role change in health FSM\n");
        }
    }

bail:
    return err;
}

/*
 *  This function return string that describes the state of the peer health FSM
 *  state
 *
 * @param[in] fsm - health FSM
 *
 * @return string - FSM state
 */
static char*
get_health_fsm_state_str(health_fsm *fsm)
{
    if (health_fsm_idle_in(fsm)) {
        return "IDLE";
    }
    if (health_fsm_peer_down_in(fsm)) {
        return "Peer Down";
    }
    if (health_fsm_comm_down_in(fsm)) {
        return "Comm Down";
    }
    if (health_fsm_peer_up_in(fsm)) {
        return "Peer Up";
    }
    if (health_fsm_down_wait_in(fsm)) {
        return "Down Wait";
    }
    return "Unknown";
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
health_manager_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;
    uint32_t peer_ip;
    heartbeat_state_t hb_states[MLAG_MAX_PEERS];
    heartbeat_peer_stats_t hb_stats;
    int idx;
    char *hb_states_str[] = {"Inactive", "Down", "Up"};

    DUMP_OR_LOG("=================\nHealth manager dump\n=================\n");
    DUMP_OR_LOG("Started [%d] Heartbeat rate (msec) [%u]\n", started,
                heartbeat_timer_msec);
    for (idx = 0; idx < MLAG_MAX_IPLS; idx++) {
        DUMP_OR_LOG("IPL ID [%d] state [%s]\n", idx,
                    (ipl_states[idx] == OES_PORT_UP) ? "Up" : "Down" );
    }
    DUMP_OR_LOG("----------------------------------------------------------\n");
    DUMP_OR_LOG("Heartbeat states\n");
    err = heartbeat_peer_states_get(hb_states);
    if (err) {
        DUMP_OR_LOG("Failed getting Heartbeat states err [%d]\n", err);
    }
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        DUMP_OR_LOG("Peer local ID [%d] state [%u - %s]\n", idx,
                    hb_states[idx],
                    hb_states_str[hb_states[idx]]);
    }
    DUMP_OR_LOG("----------------------------------------------------------\n");
    DUMP_OR_LOG("Active System IDs list\n");
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (active_sys_ids[idx] != INVALID_SYS_ID) {
            DUMP_OR_LOG("System ID [%llu] is active\n", active_sys_ids[idx]);
        }
    }
    DUMP_OR_LOG("----------------------------------------------------------\n");
    DUMP_OR_LOG("Active PEERs list\n");
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (health_fsm_idle_in(&mlag_health_fsm[idx])) {
            continue;
        }
        else {
            err = mlag_manager_db_peer_ip_from_local_index_get(idx, &peer_ip);
            if (err) {
                DUMP_OR_LOG("Failed getting IP for peer local ID [%d]\n", idx);
                peer_ip = 0;
            }
            DUMP_OR_LOG("Peer local ID [%d] IP [%x] state [%s]\n",
                        mlag_health_fsm[idx].peer_id,  peer_ip,
                        get_health_fsm_state_str(&mlag_health_fsm[idx]));
        }
    }
    DUMP_OR_LOG("----------------------------------------------------------\n");
    DUMP_OR_LOG("Heartbeat statistics\n");
    for (idx = 1; idx < MLAG_MAX_PEERS; idx++) {
        err = heartbeat_peer_stats_get(idx, &hb_stats);
        if (err) {
            DUMP_OR_LOG("Failed getting Heartbeat stats peer [%d] err [%d]\n",
                        idx, err);
        }
        DUMP_OR_LOG("Local peer ID [%d]\n", idx);
        DUMP_OR_LOG("Tx : %" PRIu64 " \n", hb_stats.tx_heartbeat);
        DUMP_OR_LOG("Rx : %" PRIu64 " \n", hb_stats.rx_heartbeat);
        DUMP_OR_LOG("Tx Errors: %" PRIu64 " \n", hb_stats.tx_errors);
        DUMP_OR_LOG("Rx miss: %" PRIu64 " \n", hb_stats.rx_miss);
        DUMP_OR_LOG("RDI : %" PRIu64 " \n", hb_stats.remote_defect);
        DUMP_OR_LOG("Local defect : %" PRIu64 " \n", hb_stats.local_defect);
    }

    return err;
}

/*
 *  This function is the action of timer thread callback
 *
 * @param[in] data - timer event data
 *
 * @return 0 if operation completes successfully.
 */
static void
heartbeat_tick_cb(void *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    err = send_system_event(MLAG_HEALTH_HEARTBEAT_TICK, NULL, 0);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending time tick event\n");

bail:
    return;
}
