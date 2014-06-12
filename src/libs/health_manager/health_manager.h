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

#ifndef MLAG_HEALTH_H_
#define MLAG_HEALTH_H_

#include "mlag_api_defs.h"
#include <oes_types.h>
#include <utils/mlag_defs.h>
#include <utils/mlag_events.h>
#include "heartbeat.h"

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/
/************************************************
 *  Type definitions
 ***********************************************/
enum health_state {
    HEALTH_PEER_UP,
    HEALTH_PEER_DOWN,
    HEALTH_PEER_COMM_DOWN,
    HEALTH_PEER_DOWN_WAIT,
    HEALTH_PEER_NOT_EXIST,
    LAST_HEALTH_STATE
};

#ifdef HEALTH_MANAGER_C
char *health_state_str[LAST_HEALTH_STATE] = {
    "PEER_UP",
    "PEER_DOWN",
    "PEER_COMM_DOWN",
    "PEER_DOWN_WAIT",
    "PEER_NOT_EXIST"
};
#else
extern char *health_state_str[LAST_HEALTH_STATE];
#endif

struct health_counters {
    heartbeat_peer_stats_t hb_stats;
};

#define INVALID_SYS_ID  0xFFFFFFFFFFFFFFFFULL

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void health_manager_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits the health module
 *
 *  @param sock - socket for heartbeat sending
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_init(int sock);

/**
 *  This function de-inits the health module
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_deinit(void);

/**
 *  This function starts the health module. When Health module
 *  starts it may send PDUs to peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_start(void);

/**
 *  This function starts the health module. When Health module
 *  starts it may send PDUs to peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_stop(void);

/**
 *  This function is for handling a received message over the
 *  comm channel
 *
 * @param[in] peer_id - origin peer index
 * @param[in] msg - message data
 * @param[in] msg_size - message data size
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_recv(int peer_id, void *msg, int msg_size);

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
int health_manager_peer_add(int peer_id, int ipl_id, uint32_t ip);

/**
 *  This function removes a peer for health monitoring
 *  The health module will no longer monitor peer connectivity
 *  after this action
 *
 * @param[in] peer_id - peer index
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_peer_remove(int peer_id);

/**
 *  This function sets the heartbeat interval
 *
 * @param[in] interval_msec - interval in milliseconds
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_heartbeat_interval_set(unsigned int interval_msec);

/**
 * This function gets the heartbeat interval.
 *
 * @param[out] interval_msec - Interval in milliseconds.
 *
 * @return 0 - Operation completed successfully.
 */
int
health_manager_heartbeat_interval_get(unsigned int *interval_msec);

/**
 *  This function deals with FSM timer event
 *
 * @param[in] data - user data
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_fsm_timer_event(void *data);

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
int health_manager_set_local_health(int health_state);

/**
 *  This function notifies the mlag health on OOB (management)
 *  connection state.
 *
 * @param[in] system_id - Peer system ID
 * @param[in] connection_state -  peer OOB connection state
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_mgmt_connection_state_change(unsigned long long system_id,
                                                enum mgmt_oper_state mgmt_connection_state);

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
health_manager_ipl_state_change(int ipl_id,
                                enum oes_port_oper_state ipl_state);

/**
 *  This function handles role change in switch
 *  Role change in switch triggers sending states of all peers
 *
 * @param[in] role_change - Role change event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
health_manager_role_change(struct switch_status_change_event_data *role_change);

/**
 *  This function returns peer states
 *
 * @param[out] peer_states - array of peer states
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_peer_states_get(int states[MLAG_MAX_PEERS]);

/**
 *  This function notifies the mlag health on IPL oper state
 *  changes.
 *
 * @param[in] ipl_id - IPL index
 *
 * @return IPL port state
 */
enum oes_port_oper_state
health_manager_ipl_state_get(int ipl_id);

/**
 *  This function tells the current MGMT oper state
 *  with a certain peer
 *
 * @param[in] peer_id - Peer local index
 *
 * @return MGMT connection state
 */
enum mgmt_oper_state
health_manager_mgmt_state_get(int peer_id);

/**
 *  This function handles timeout
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_timer_event(void);

/**
 *  This function return peer health module counters
 *
 * @param[in] peer_id - peer index
 * @param[in] counters -  struct containing peer counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int health_manager_counters_peer_get(int peer_id, struct health_counters *counters);

/**
 * This function return health module counters.
 *
 * @param[in] counters - struct containing peer counters.
 *
 * @return 0 when successful, otherwise ERROR
 */

int health_manager_counters_get(struct mlag_counters *counters);

/**
 * This function clears health module counters.
 *
 * @return 0 - success
 */
void
mlag_health_manager_counters_clear(void);

/**
 *  This function cause port dump, either using print_cb
 *  Or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int health_manager_dump(void (*dump_cb)(const char *, ...));



#endif /* MLAG_HEALTH_H_ */
