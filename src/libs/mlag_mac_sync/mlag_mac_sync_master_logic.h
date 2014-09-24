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

#ifndef MLAG_MAC_SYNC_MASTER_LOGIC_H_
#define MLAG_MAC_SYNC_MASTER_LOGIC_H_

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

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
void mlag_mac_sync_master_logic_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_mac_sync_master_logic_log_verbosity_get(void);

/**
 *  This function inits mlag mac sync master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_init(void);

/**
 *  This function deinits mlag mac sync master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_deinit(void);

/**
 *  This function starts mlag mac sync master_logic sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_start(uint8_t *data);

/**
 *  This function stops mlag mac sync master_logic sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_stop(uint8_t *data);

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_sync_start(struct sync_event_data *data);

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_sync_finish(struct sync_event_data *data);

/**
 *  This function handles peer enable event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_peer_enable(struct peer_state_change_data *data);

/**
 *  This function handles peer status change event: only down state is interesting
 *
 *  @param[in] data - event data
 *
 * @return int as error code.
 */
int mlag_mac_sync_master_logic_peer_status_change(
    struct peer_state_change_data *data);

/**
 *  This function prints module's internal attributes to log
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 * @param[in]  verbosity
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_print(void (*dump_cb)(const char *,
                                                     ...), int verbosity);


/**
 *  This function prints master pointer
 *
 * @param[in]  data  - pointer to the master instance
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_print_master(void * data, void (*dump_cb)(
                                                const char *,
                                                ...));


/**
 *  This function checkes whether peer is enabled
 * @param[in]  peer_id - ID of the peer
 * @param[out] res -result
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_is_peer_enabled(int peer_id, int *res);

/**
 *  This function is called when the fsm is moved to idle state
 *
 * @param[in]  key - fsm key id

 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_notify_fsm_idle(uint64_t key, int timeout);

/**
 *  This function handles FDB export message from the Peer
 *
 *  @param[in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_fdb_export(uint8_t *data);

/**
 *  This function handles flush stop
 *
 *  @param[in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_flush_stop(uint8_t *data);

/**
 *  This function handles global flush initiated by peer manager
 *
 *  @param[in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_flush_start(uint8_t *data);

/**
 *  This function handles global flush
 *
 *  @param data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_global_flush_start(uint8_t *data);


/**
 *  This function handles flush ack
 *
 *  @param[in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_flush_ack(uint8_t *data);

/**
 *  This function handles timers in flush FSM
 *
 *  @param data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_flush_fsm_timer(uint8_t *data);

/**
 *  This function handles local learn message from the Peer
 *
 *  @param[in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_local_learn(void *data);


/**
 *  This function handles local age message from the Peer
 *
 *  @param data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_local_aged(void *data);

/**
 *  This function performs actions upon cookie allocated/deallocated
 * @param[in] oper - operation :allocate or de-allocate
 * @param[in/out] cookie
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_master_logic_cookie_func(
    int oper, void ** cookie);

/**
 *  This function prints specific mac entry from FDB + master data
 * @param[in] data oes mac entry with filled mac and vlan fields
 * @return
 */
int master_print_mac_params(void * data, void (*dump_cb)(const char *, ...));


/**
 *  This function prints free pool count
 *
 * @return
 */
void master_print_free_cookie_pool_cnt(void (*dump_cb)(const char *, ...));

/**
 *  This function returns free pool count
 *  @param[out]  cnt  - pointer to the returned number of free master pools
 *  @return
 */
int mlag_mac_sync_master_logic_get_free_cookie_cnt(uint32_t *cnt);


#endif /* MLAG_MAC_SYNC_MASTER_LOGIC_H_ */
