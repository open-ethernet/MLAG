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

#ifndef MLAG_L3_INTERFACE_PEER_H_
#define MLAG_L3_INTERFACE_PEER_H_

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
void mlag_l3_interface_peer_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_l3_interface_peer_log_verbosity_get(void);

/**
 *  This function inits mlag l3 interface peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_init(void);

/**
 *  This function deinits mlag l3 interface peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_deinit(void);

/**
 *  This function starts mlag l3 interface peer sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_start(uint8_t *data);

/**
 *  This function stops mlag l3 interface peer sub-module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_stop(uint8_t *data);

/**
 *  This function adds IPL to vlan of l3 interface
 *  for control messages exchange
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_vlan_interface_add(struct peer_conf_event_data *data);

/**
 *  This function removes IPL from vlan of l3 interface
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_vlan_interface_del();

/**
 *  This function handles peer start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_peer_start(struct peer_state_change_data *data);

/**
 *  This function handles ipl port set event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_ipl_port_set(struct ipl_port_set_event_data *data);

/**
 *  This function handles local sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_local_sync_done(uint8_t *data);

/**
 *  This function handles master sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_master_sync_done(uint8_t *data);

/**
 *  This function handles vlan local state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_vlan_local_state_change(
    struct vlan_state_change_event_data *data);

/**
 *  This function handles vlan global state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_vlan_global_state_change(
    struct vlan_state_change_event_data *data);

/**
 *  This function dumps sub-module's internal attributes to log
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_print(void (*dump_cb)(const char *,...));

/**
 *  This function returns ipl_vlan_id
 *
 * @return ipl_vlan_id
 */
uint16_t mlag_l3_interface_peer_get_ipl_vlan_id(void);

#endif /* MLAG_L3_INTERFACE_PEER_H_ */
