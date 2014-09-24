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

#ifndef MLAG_L3_INTERFACE_MANAGER_H_
#define MLAG_L3_INTERFACE_MANAGER_H_

/************************************************
 *  Defines
 ***********************************************/
#define VLAN_N_VID MLAG_VLAN_ID_MAX

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

#ifdef MLAG_L3_INTERFACE_MANAGER_C
char *l3_interface_vlan_local_state_str[] = {
    "VLAN UP",
    "VLAN DOWN",
};
#endif

enum l3_interface_vlan_global_state {
    VLAN_GLOBAL_UP = 0,
    VLAN_GLOBAL_DOWN = 1
};

#ifdef MLAG_L3_INTERFACE_MANAGER_C
char *l3_interface_vlan_global_state_str[] = {
    "VLAN_GLOBAL_UP",
    "VLAN_GLOBAL_DOWN",
};
#endif

enum l3_interface_counters {
    VLAN_LOCAL_STATE_EVENTS_RCVD = 0,
    VLAN_LOCAL_STATE_EVENTS_RCVD_FROM_PEER,
    VLAN_GLOBAL_STATE_EVENTS_RCVD,
    L3_INTERFACE_PEER_STATE_CHANGE_EVENTS_RCVD,
    L3_INTERFACE_PEER_START_EVENTS_RCVD,
    L3_INTERFACE_PEER_ENABLE_EVENTS_RCVD,
    L3_INTERFACE_SWITCH_STATUS_CHANGE_EVENTS_RCVD,
    ADD_IPL_TO_VLAN_EVENTS_SENT,
    DEL_IPL_FROM_VLAN_EVENTS_SENT,
    L3_INTERFACE_START_EVENTS_RCVD,
    L3_INTERFACE_STOP_EVENTS_RCVD,
    L3_INTERFACE_IPL_PORT_SET_EVENTS_RCVD,

    L3_INTERFACE_LAST_COUNTER
};

#ifdef MLAG_L3_INTERFACE_MANAGER_C
char *l3_interface_counters_str[L3_INTERFACE_LAST_COUNTER] = {
    "VLAN_LOCAL_STATE_EVENTS_RCVD",
    "VLAN_LOCAL_STATE_EVENTS_RCVD_FROM_PEER",
    "VLAN_GLOBAL_STATE_EVENTS_RCVD",
    "L3_INTERFACE_PEER_STATE_CHANGE_EVENTS_RCVD",
    "L3_INTERFACE_PEER_START_EVENTS_RCVD",
    "L3_INTERFACE_PEER_ENABLE_EVENTS_RCVD",
    "L3_INTERFACE_SWITCH_STATUS_CHANGE_EVENTS_RCVD",
    "ADD_IPL_TO_VLAN_EVENTS_SENT",
    "DEL_IPL_FROM_VLAN_EVENTS_SENT",
    "L3_INTERFACE_START_EVENTS_RCVD",
    "L3_INTERFACE_STOP_EVENTS_RCVD",
    "L3_INTERFACE_IPL_PORT_SET_EVENTS_RCVD",
};
#endif

#define L3_INTERFACE_INC_CNT(cnt) {if (cnt < \
                                       L3_INTERFACE_LAST_COUNTER) { counters. \
                                                                    counter[cnt \
                                                                    ]++; } }
#define L3_INTERFACE_CLR_CNT(cnt) {if (cnt < \
                                       L3_INTERFACE_LAST_COUNTER) { counters. \
                                                                    counter[cnt \
                                                                    ] = 0; } }

struct mlag_l3_interface_counters {
    uint64_t counter[L3_INTERFACE_LAST_COUNTER];
};

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
void mlag_l3_interface_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_l3_interface_log_verbosity_get(void);

/**
 *  This function inits mlag l3 interface
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_init(insert_to_command_db_func insert_msgs);

/**
 *  This function deinits mlag l3 interface
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_deinit(void);

/**
 *  This function starts mlag l3 interface module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_start(uint8_t *data);

/**
 *  This function stops mlag l3 interface module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_stop(uint8_t *data);

/**
 *  This function handles ipl port set event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_ipl_port_set(struct ipl_port_set_event_data *data);

/**
 *  This function handles local sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_local_sync_done(uint8_t *data);

/**
 *  This function handles master sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_master_sync_done(uint8_t *data);

/**
 *  This function handles vlan local state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_vlan_local_state_change(
    struct vlan_state_change_event_data *data);

/**
 *  This function handles peer vlan local state change event
 *  that means vlan local event received from peer and destined to master logic
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_vlan_state_change_from_peer(
    struct vlan_state_change_event_data *data);

/**
 *  This function handles vlan global state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_vlan_global_state_change(
    struct vlan_state_change_event_data *data);

/**
 *  This function handles peer status notification event: up or down
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_status_change(struct peer_state_change_data *data);

/**
 *  This function handles peer start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_start_event(struct peer_state_change_data *data);

/**
 *  This function handles peer enable event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_enable(struct peer_state_change_data *data);

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_sync_start(struct sync_event_data *data);

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_sync_finish(struct sync_event_data *data);

/**
 *  This function handles vlan local state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_switch_status_change(
    struct switch_status_change_event_data *data);

/**
 *  This function adds IPL to vlan of l3 interface
 *  for control messages exchange
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_add(struct peer_conf_event_data *data);

/**
 *  This function removes IPL from vlan of l3 interface
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_l3_interface_peer_del();

/**
 *  This function outputs module's dump, either using print_cb
 *  or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_l3_interface_dump(void (*dump_cb)(const char *, ...));

/**
 *  This function returns l3 interface module counters
 *
 * @param[in] _counters - pointer to structure to return counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_counters_get(struct mlag_l3_interface_counters *_counters);

/**
 *  This function clears l3 interface module counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_counters_clear(void);

/**
 *  This function increments mlag_l3_interface counter
 *
 * @param[in] cnt - counter
 *
 * @return none
 */
void
mlag_l3_interface_inc_cnt(enum l3_interface_counters cnt);

#endif /* MLAG_L3_INTERFACE_MANAGER_H_ */
