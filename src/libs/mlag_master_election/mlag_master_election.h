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

#ifndef MLAG_MASTER_ELECTION_H_
#define MLAG_MASTER_ELECTION_H_
#include <utils/mlag_events.h>
/************************************************
 *  Defines
 ***********************************************/
 #define SLAVE_PEER_ID  1
 #define MASTER_PEER_ID 0

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
enum master_election_switch_status {
    MASTER = 0,
    SLAVE = 1,
    STANDALONE = 2,
    NONE = 3,
    LAST_STATUS
};

#define DEFAULT_SWITCH_STATUS NONE

#ifdef MLAG_MASTER_ELECTION_C
char *master_election_switch_status_str[LAST_STATUS] = {
    "MASTER",
    "SLAVE",
    "STANDALONE",
    "NONE"
};
#endif

enum master_election_config_change_cmd {
    NO_CHANGE = 0,
    MODIFY = 1
};

struct config_change_event {
    int ipl_id;
    int peer_id;
    enum master_election_config_change_cmd my_ip_addr_cmd;
    uint32_t my_ip_addr;
    enum master_election_config_change_cmd peer_ip_addr_cmd;
    uint32_t peer_ip_addr;
    unsigned short vlan_id;
};

enum master_election_counters {
    CONFIG_CHANGE_EVENTS_RCVD = 0,
    PEER_STATE_CHANGE_EVENTS_RCVD,
    SWITCH_STATUS_CHANGE_EVENTS_SENT,
    START_EVENTS_RCVD,
    STOP_EVENTS_RCVD,
    LAST_COUNTER
};

#ifdef MLAG_MASTER_ELECTION_C
char *master_election_counters_str[LAST_COUNTER] = {
    "CONFIG_CHANGE_EVENTS_RCVD",
    "PEER_STATE_CHANGE_EVENTS_RCVD",
    "SWITCH_STATUS_CHANGE_EVENTS_SENT",
    "START_EVENTS_RCVD",
    "STOP_EVENTS_RCVD",
};
#endif

#define INC_CNT(cnt) {if (cnt < LAST_COUNTER) {counters.counter[cnt]++; }}
#define CLR_CNT(cnt) {if (cnt < LAST_COUNTER) {counters.counter[cnt] = 0; }}

struct  mlag_master_election_counters {
    int counter[LAST_COUNTER];
};

struct mlag_master_election_status {
    int current_status;
    int previous_status;
    uint32_t my_ip_addr;
    uint32_t peer_ip_addr;
    uint8_t my_peer_id;
    uint8_t master_peer_id;
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
void mlag_master_election_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_master_election_log_verbosity_get(void);

/**
 *  This function inits mlag master election
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_init(void);

/**
 *  This function deinits mlag master election
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_deinit(void);

/**
 *  This function starts mlag master election module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_start(uint8_t *data);

/**
 *  This function stops mlag master election module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_stop(uint8_t *data);

/**
 *  This function handles config change notification event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_config_change(struct config_change_event *data);

/**
 *  This function handles peer status notification event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_peer_status_change(
    struct peer_state_change_data *data);

/**
 *  This function is for getting current switch status / role
 *
 * @return current status value
 */
int mlag_master_election_get_status(
    struct mlag_master_election_status *master_election_current_status);

/**
 *  This function prints module's internal attributes to log
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_dump(void (*dump_cb)(const char *,...));

/**
 *  This function returns master election module counters
 *
 * @param[in] _counters - pointer to structure to return counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_counters_get(
    struct mlag_master_election_counters *_counters);

/**
 *  This function clears master election module counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_counters_clear(void);

/**
 *  This function returns master election switch status as string
 *
 * @param[in] switch_status - switch status requested
 * @param[out] switch_status_str - pointer to switch status string
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_get_switch_status_str(
    int switch_status, char** switch_status_str);

/**
 *  This function increments master election counter
 *
 * @param[in] cnt - counter
 *
 * @return none
 */
void mlag_master_election_inc_cnt(enum master_election_counters cnt);

#endif /* MLAG_MASTER_ELECTION_H_ */
