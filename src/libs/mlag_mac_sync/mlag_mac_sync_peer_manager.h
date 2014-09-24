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

#ifndef MLAG_MAC_SYNC_PEER_MANAGER_H_
#define MLAG_MAC_SYNC_PEER_MANAGER_H_

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
void mlag_mac_sync_peer_mngr_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_mac_sync_peer_mngr_log_verbosity_get(void);

/**
 *  This function inits mlag mac sync peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_init(void);




/**
 *  This function inits control learning library
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_manager_init_ctrl_learn_lib();

/**
 *  This function deinits mlag mac sync peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_deinit(void);

/**
 *  This function starts mlag mac sync peer sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_start(uint8_t *data);

/**
 *  This function stops mlag mac sync peer sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_stop(uint8_t *data);

/**
 *  This function handles peer start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_peer_start(struct peer_state_change_data *data);


/**
 *  This function notifies peer that Master completed initial sync wih peer
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_sync_done(uint8_t *data);

/**
 *  This function handles ipl port set event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_ipl_port_set(struct ipl_port_set_event_data *data);

/**
 *  This function handles master sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_master_sync_finish(uint8_t *data);

/**
 *  This function is for getting current
 *
 * @return current status value
 */
int mlag_mac_sync_peer_mngr_get_status(void);

/**
 *  This function prints module's internal attributes to log
 *
 *  @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_print(void (*dump_cb)(const char *, ...));

/**
 *  This function deletes STATIC mac on IPL from FDB
 *
 * @return current status value
 */


/**
 *  This function initializes delete list
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_init_delete_list(void);
/**
 *  This function  checks whether delete list is initialized
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_check_init_delete_list(void);
/**
 *  This function  passes through delete list and
 *  deletes all MAC entries in the list from the FDB
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_process_delete_list(void);

/**
 *  This function adds to delete list STATIC mac on IPL from FDB
 *
 * @return current status value
 */
int
mlag_mac_sync_peer_mngr_addto_delete_list_ipl_static_mac(
    struct oes_fdb_uc_mac_addr_params *mac_addr_params);

/**
 *  This function handles Flush command from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_flush_start(void *data);

/**
 *  This function handles Global learn command from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_global_learned(void *data, int need_lock);

/**
 *  This function handles Global age command from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_global_aged(void *data);



/**
 *  This function handles internal age event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_internal_aged(void *data);

/**
 *  This function handles Global reject from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_global_reject(void *data);

/**
 *  This function processes FDB export message from the Master.
 *  Parsed Global learn messages are set to FBD
 *
 * @param[in] data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_mngr_process_fdb_export(void *data);



/**
 *  This function checkes whether found at least one dynamic entry in the FDB
 *  applying specific filter . If found - master performs Global flush process
 *
 *
 * @param[in] filter - input filter
 * @param[out] need_flush -  result value
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_check_need_flush(   void *fltr, int *need_flush);



/**
 *  This function synchronizes router mac .
 *
 * @param[in]  mac
 * @param[in]  vid
 * @param[in]  add   =1 for add mac and 0 for remove mac
 *
 * @return 0 when successful, otherwise ERROR
 */

int mlag_mac_sync_peer_mngr_sync_router_mac(
    struct ether_addr mac, unsigned short vid, int add);

/**
 *  This function corrects port on transmitted Local learn message.
 *
 * @param[in] mac_addr_params - MAC address parameters
 * @param[in/out]data         - pointer to Local learn message
 *
 * @return 0 when successful, otherwise ERROR
 */
int _correct_port_on_tx(
    void * data, struct oes_fdb_uc_mac_addr_params *mac_addr_params);

/**
 *  This function performs local flush on mlag port
 *
 * @param[in] port id
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_manager_port_local_flush(unsigned long port);

/* Simulation code*/
int simulate_notify_local_learn(int mac, int port);
int simulate_notify_local_age(int mac, int port );
int simulate_notify_local_flush(int port);
int simulate_notify_multiple_local_learn(int num, int first_mac, int port,
                                         int astatic);
int peer_simulate_local_flush(void);

#endif /* MLAG_MAC_SYNC_PEER_MANAGER_H_ */
