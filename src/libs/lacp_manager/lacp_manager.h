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

#ifndef LACP_MANAGER_H_
#define LACP_MANAGER_H_
#include <utils/mlag_log.h>
#include <mlag_api_defs.h>
#include <utils/mlag_events.h>
#include <libs/mlag_common/mlag_common.h>
#include "lacp_db.h"

#ifdef LACP_MANAGER_C_

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ LACP_MANAGER

/************************************************
 *  Local Macros
 ***********************************************/


/************************************************
 *  Local Type definitions
 ***********************************************/
struct lacp_peer_down_ports_data {
    int mlag_id;
    int port_num;
    unsigned long ports_to_delete[MLAG_MAX_PORTS];
};


#endif

/************************************************
 *  Defines
 ***********************************************/

enum lacp_sync_phase {
    LACP_SYNC_START,
    LACP_SYNC_DATA,
    LACP_SYNC_DONE,
    LACP_SYS_ID_UPDATE
};

enum aggregate_select_response {
    LACP_AGGREGATE_ACCEPT,
    LACP_AGGREGATE_DECLINE
};


/************************************************
 *  Macros
 ***********************************************/


/************************************************
 *  Type definitions
 ***********************************************/
struct __attribute__((__packed__)) peer_lacp_sync_message {
    uint16_t opcode;
    uint32_t phase;
    unsigned long long sys_id;
    int mlag_id;
};

struct __attribute__((__packed__)) lacp_aggregator_release_message {
    uint16_t opcode;
    unsigned long port_id;
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
lacp_manager_log_verbosity_get(void);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
lacp_manager_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits lacp_manager
 *
 *  @param[in] insert_msgs_cb - callback for registering PDU handlers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_init(insert_to_command_db_func insert_msgs_cb);

/**
 *  This function deinits lacp_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_deinit(void);

/**
 *  This function starts lacp_manager
 *
 *  param[in] data - start event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_start(uint8_t *data);

/**
 *  This function stops lacp_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_stop(void);

/**
 *  Handle local peer role change.
 *
 * @param[in] new_role - current role of local peer
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_role_change(int new_role);

/**
 *  This function handles peer state change notification.
 *
 * @param[in] state_change - state  change data
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_peer_state_change(struct peer_state_change_data *state_change);

/**
 *  Handles peer start notification.Peer start
 *  triggers lacp sync process start. The Master
 *  sends its actor attributes to the remotes.
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_peer_start(int mlag_peer_id);

/**
 *  Sets the local system Id
 *  The LACP module gives the suggested actor parameters according
 *  to mlag protocol state (possible answers are local parameters set in this
 *  function or the remote side parameters)
 *
 *  @param[in] local_sys_id - local system ID
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_system_id_set(unsigned long long local_sys_id);

/**
 * Returns actor attributes. The actor attributes are
 * system ID, system priority to be used in the LACP PDU
 * and a chassis ID which is an index of this node within the MLAG
 * cluster, with a value in the range of 0..15
 *
 * @param[out] actor_sys_id - actor sys ID (for LACP PDU)
 * @param[out] chassis_id - MLAG cluster chassis ID, range 0..15
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_actor_parameters_get(unsigned long long *actor_sys_id,
                                  unsigned int *chassis_id);

/**
 * Handles a selection query .
 * This query may involve a remote peer,
 * When a delete command is used, only port_id parameter is relevant.
 * Force option is given in order to allow
 * releasing currently used key and migrating to the given partner.
 *
 * @param[in] request_id - index given by caller that will appear in reply
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 * @param[in] partner_id - partner system ID (taken from LACP PDU)
 * @param[in] partner_key - partner operational Key (taken from LACP PDU)
 * @param[in] force - force positive notification (will release key in use)
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_selection_request(unsigned int request_id,
                                          unsigned long port_id,
                                          unsigned long long partner_id,
                                          unsigned int partner_key,
                                          unsigned char force);

/**
 * Handles an aggregator release
 * This may involve a remote peer,
 *
 * @param[in] request_id - index given by caller that will appear in reply
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_selection_release(unsigned int request_id,
                                          unsigned long port_id);


/**
 * Handles an incoming msg of aggregator selection
 *
 * @param[in] msg - aggregator selection message
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_selection_handle(
    struct lacp_aggregation_message_data *msg);

/**
 * Handles an incoming msg of aggregator selection
 *
 * @param[in] msg - aggregator release message
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_free_handle(struct lacp_aggregator_release_message *msg);

/**
 * Dump internal DB and data
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_dump(void (*dump_cb)(const char *, ...));

/**
 *  This function gets lacp_manager module counters.
 *  counters are copied to the given struct
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_counters_get(struct mlag_counters *counters);

/**
 *  This function clears lacp_manager module counters.
 *
 *
 * @return void
 */
void
lacp_manager_counters_clear(void);


#endif /* LACP_MANAGER_H_ */
