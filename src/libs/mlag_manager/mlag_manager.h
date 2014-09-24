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

#ifndef MLAG_MANAGER_H_
#define MLAG_MANAGER_H_
#include <utils/mlag_log.h>
#include <utils/mlag_events.h>
#include <libs/mlag_common/mlag_common.h>
#include "mlag_api_defs.h"
/************************************************
 *  Defines
 ***********************************************/
#define LOCAL_PEER_ID   0
#define REMOTE_PEERS_MASK   0xFFFE
#define LOCAL_PEERS_MASK    0x0001
/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

enum mm_counters {
    MM_CNT_PROTOCOL_TX = 0,
    MM_CNT_PROTOCOL_RX,

    MLAG_MANAGER_LAST_COUNTER
};

struct mlag_manager_counters {
    uint64_t rx_protocol_msg;
    uint64_t tx_protocol_msg;
};

enum mlag_manager_peer_state {
    PEER_ENABLE = 0,
    PEER_DOWN = 1,
    PEER_TX_ENABLE = 2
};

enum peering_sync_type {
    PORT_PEER_SYNC = 1,
    L3_PEER_SYNC = 2,
    IGMP_PEER_SYNC = 4,
    MAC_PEER_SYNC = 8,
    LACP_PEER_SYNC = 16,

    ALL_PEER_SYNC = (PORT_PEER_SYNC | MAC_PEER_SYNC |
                     IGMP_PEER_SYNC | L3_PEER_SYNC | LACP_PEER_SYNC)
};

enum stop_done_type {
    MLAG_MANAGER_STOP_DONE = 1,
    HEALTH_STOP_DONE = 2,
    IGMP_STOP_DONE = 4,
    MAC_SYNC_STOP_DONE = 8,

    ALL_STOP_DONE = (MLAG_MANAGER_STOP_DONE | HEALTH_STOP_DONE |
                     IGMP_STOP_DONE | MAC_SYNC_STOP_DONE)
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function returns module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_manager_log_verbosity_get(void);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_manager_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits mlag manager
 *
 *  @param[in] insert_msgs_cb - callback for registering PDU handlers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_init(insert_to_command_db_func insert_msgs_cb);

/**
 *  This function deinits mlag manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_deinit(void);

/**
 *  This function starts mlag manager with its sub-modules
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_start(uint8_t *data);

/**
 *  This function stops mlag manager with its sub-modules
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_stop(uint8_t *data);

/**
 *  This function notifies new peer role
 *
 * @param[in] previous_role - peer previous role
 * @param[in] new_role - peer new role
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_role_change(int previous_role, int new_role);

/**
 *  This function handles peer add event
 *  It takes care of synchronizing modules activity on peer add
 *
 * @param[in] data - peer add event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_add(struct peer_conf_event_data *data);

/**
 *  This function handles peer health change
 *
 * @param[in] state_change - peer state change notification
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_status_change(struct peer_state_change_data *state_change);

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
                              enum peering_sync_type synced_module);

/**
 *  This function notifies a new peer has successfully connected
 *  to this entity (Master)
 *
 * @param[in] conn - connection info
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_connected(struct tcp_conn_notification_event_data *conn);

/**
 *  This function will allow local ports to enable
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_enable_ports(void);

/**
 *  This function cause port dump, either using print_cb
 *  Or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_dump(void (*dump_cb)(const char *, ...));

/**
 * This function sets the reload delay interval.
 *
 * @param[in] interval_msec - interval in milliseconds.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_reload_delay_interval_set(unsigned int interval_msec);

/**
 * This function gets the reload delay interval.
 *
 * @param[out] interval_msec - Interval in milliseconds.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_reload_delay_interval_get(unsigned int *interval_msec);

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
    enum protocol_oper_state *protocol_oper_state);

/**
 * This function gets the current system role state.
 *
 * @param[out] system_role_state - System role state. Pointer to an already allocated memory
 *                                 structure.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_system_role_state_get(enum system_role_state *system_role_state);

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
                                  unsigned int *peers_list_cnt);

/**
 * This function handles peer delete sync event
 *
 * @param[in] event - event data
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_peer_delete_handle(uint8_t *event);

/**
 * This function waits for stop process done
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_wait_for_stop_done(void);

/**
 * This function handles stop done event
 *
 * @param[in] event - event data
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_stop_done_handle(uint8_t *event);



/**
 * This function waits for port delete process done
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_wait_for_port_delete_done(void);

/**
 * This function handles port delete done event
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_manager_port_delete_done(void);


/**
 * This function handles peer enable event.
 *
 * @param[in] data - Event data.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_peer_enable(struct peer_state_change_data *data);

/**
 *  This function gets mlag_manager module counters.
 *  counters are copied to the given struct
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_counters_get(struct mlag_counters *mlag_counters);

/**
 * This function clears mlag_manager module counters.
 *
 * @return void
 */
void
mlag_manager_counters_clear(void);

/**
 * This function check if the given local id represent a valid peer.
 *
 * @param[in] local_index - Peer local index
 *
 * @return true if peer exists. Otherwise, false.
 */
int
mlag_manager_is_peer_valid(int local_index);

#endif
