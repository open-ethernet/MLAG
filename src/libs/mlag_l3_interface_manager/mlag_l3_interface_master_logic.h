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

#ifndef MLAG_L3_INTERFACE_MASTER_LOGIC_H_
#define MLAG_L3_INTERFACE_MASTER_LOGIC_H_

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
void mlag_l3_interface_master_logic_log_verbosity_set(
    mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_l3_interface_master_logic_log_verbosity_get(void);

/**
 *  This function inits mlag l3 interface master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_init(void);

/**
 *  This function deinits mlag l3 interface master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_deinit(void);

/**
 *  This function starts mlag l3 interface master_logic sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_start(uint8_t *data);

/**
 *  This function stops mlag l3 interface master_logic sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_stop(uint8_t *data);

/**
 *  This function handles peer vlan local state change event
 *  that means vlan local event received from peer and destined to master logic
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_vlan_state_change(
    struct vlan_state_change_event_data *data);

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_sync_start(struct sync_event_data *data);

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_sync_finish(struct sync_event_data *data);

/**
 *  This function handles peer enable event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_peer_enable(
    struct peer_state_change_data *data);

/**
 *  This function handles peer status change event: only down state is interesting
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_peer_status_change(
    struct peer_state_change_data *data);

/**
 *  This function dumps sub-module's internal attributes to log
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_logic_print(void (*dump_cb)(const char *,...));

#endif /* MLAG_L3_INTERFACE_MASTER_LOGIC_H_ */
