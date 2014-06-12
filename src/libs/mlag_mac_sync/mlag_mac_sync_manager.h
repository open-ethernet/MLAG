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

#ifndef MLAG_MAC_SYNC_MANAGER_H_
#define MLAG_MAC_SYNC_MANAGER_H_

/************************************************
 *  Defines
 ***********************************************/
#define MAC_SYNC_ORIGINATOR  0x23416324
#define NON_MLAG             0xffffffff
/*TODO !!*/

#define FIRST_MLAG_IFINDEX 29000
#define LAST_MLAG_IFINDEX 30001

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

enum mac_sync_counts {
    MAC_SYNC_PEER_STATE_CHANGE_EVENTS_RCVD = 0,
    MAC_SYNC_PEER_START_EVENTS_RCVD,
    MAC_SYNC_PEER_ENABLE_EVENTS_RCVD,
    MAC_SYNC_SWITCH_STATUS_CHANGE_EVENTS_RCVD,
    MAC_SYNC_START_EVENTS_RCVD,
    MAC_SYNC_STOP_EVENTS_RCVD,
    MAC_SYNC_IPL_PORT_SET_EVENTS_RCVD,
    MAC_SYNC_FDB_GET_EVENTS_RCVD,
    MAC_SYNC_FDB_EXPORT_EVENTS_RCVD,
    MAC_SYNC_LOCAL_LEARNED_EVENT,
    MAC_SYNC_LOCAL_LEARNED_NEW_EVENT,
    MAC_SYNC_LOCAL_LEARNED_NEW_EVENT_PROCESSED,
    MAC_SYNC_LOCAL_LEARNED_MIGRATE_EVENT,
    MAC_SYNC_LOCAL_LEARNED_DURING_FLUSH_EVENT,
    MAC_SYNC_TIMER_IN_WRONG_STATE_EVENT,

    MAC_SYNC_NOTIFY_LEARNED_EVENT,
    MAC_SYNC_NOTIFY_AGED_EVENT,
    MAC_SYNC_LOCAL_AGED_EVENT,
    MAC_SYNC_WRONG_1_LOCAL_AGED_EVENT,
    MAC_SYNC_WRONG_2_LOCAL_AGED_EVENT,

    MAC_SYNC_GLOBAL_LEARNED_EVENT,
    MAC_SYNC_PROCESS_LOCAL_AGED_EVENT,
    MAC_SYNC_GLOBAL_AGED_EVENT,
    MAC_SYNC_LOCAL_LEARN_REGECTED_BY_MASTER,
    MAC_SYNC_FDB_FULL,
    MAC_SYNC_ERROR_FDB_SET,
    MASTER_TX,
    MASTER_RX,
    SLAVE_TX,
    SLAVE_RX,
    MAC_SYNC_LAST_COUNTER
};

#ifdef MLAG_MAC_SYNC_MANAGER_C
char *mac_sync_counters_str[MAC_SYNC_LAST_COUNTER] = {
    "MAC_SYNC_PEER_STATE_CHANGE_EVENTS_RCVD",
    "MAC_SYNC_PEER_START_EVENTS_RCVD",
    "MAC_SYNC_PEER_ENABLE_EVENTS_RCVD",
    "MAC_SYNC_SWITCH_STATUS_CHANGE_EVENTS_RCVD",
    "MAC_SYNC_START_EVENTS_RCVD",
    "MAC_SYNC_STOP_EVENTS_RCVD",
    "MAC_SYNC_IPL_PORT_SET_EVENTS_RCVD",
    "MAC_SYNC_FDB_GET_EVENTS_RCVD",
    "MAC_SYNC_FDB_EXPORT_EVENTS_RCVD",
    "MAC_SYNC_LOCAL_LEARNED_EVENT",
    "MAC_SYNC_LOCAL_LEARNED_NEW_EVENT",
    "MAC_SYNC_LOCAL_LEARNED_NEW_EVENT_PROCESSED",

    "MAC_SYNC_LOCAL_LEARNED_MIGRATE_EVENT",
    "MAC_SYNC_LOCAL_LEARNED_DURING_FLUSH_EVENT",
    "MAC_SYNC_TIMER_IN_WRONG_STATE_EVENT",

    "MAC_SYNC_NOTIFY_LEARNED_EVENT"  ,
    "MAC_SYNC_NOTIFY_AGED_EVENT",
    "MAC_SYNC_LOCAL_AGED_EVENT",
    "MAC_SYNC_WRONG_1_LOCAL_AGED_EVENT",
    "MAC_SYNC_WRONG_2_LOCAL_AGED_EVENT",

    "MAC_SYNC_GLOBAL_LEARNED_EVENT",
    "MAC_SYNC_PROCESS_LOCAL_AGED_EVENT",
    "MAC_SYNC_GLOBAL_AGED_EVENT",
    "MAC_SYNC_LOCAL_LEARN_REGECTED_BY_MASTER",
    "MAC_SYNC_FDB_FULL",
    "MAC_SYNC_ERROR_FDB_SET",
    "MASTER_TX",
    "MASTER_RX",
    "SLAVE_TX",
    "SLAVE_RX",
};
#endif

#define MAC_SYNC_INC_CNT(cnt) {if (cnt < \
                                   MAC_SYNC_LAST_COUNTER) { counters.counter[ \
                                                                cnt]++; } }

#define MAC_SYNC_INC_CNT_NUM(cnt, num) {if (cnt < \
                                   MAC_SYNC_LAST_COUNTER) { counters.counter[ \
                                                                cnt]+=num; } }

#define MAC_SYNC_CLR_CNT(cnt) {if (cnt < \
                                   MAC_SYNC_LAST_COUNTER) { counters.counter[ \
                                                                cnt] = 0; } }


#define PRINT_MAC(entry)   entry.mac_addr_params.mac_addr.ether_addr_octet[0], \
                           entry.mac_addr_params.mac_addr.ether_addr_octet[1], \
                           entry.mac_addr_params.mac_addr.ether_addr_octet[2], \
                           entry.mac_addr_params.mac_addr.ether_addr_octet[3], \
                           entry.mac_addr_params.mac_addr.ether_addr_octet[4], \
                           entry.mac_addr_params.mac_addr.ether_addr_octet[5]


#define PRINT_MAC_OES(entry) entry.mac_addr.ether_addr_octet[0], \
                             entry.mac_addr.ether_addr_octet[1], \
                             entry.mac_addr.ether_addr_octet[2], \
                             entry.mac_addr.ether_addr_octet[3], \
                             entry.mac_addr.ether_addr_octet[4], \
                             entry.mac_addr.ether_addr_octet[5]



struct  mac_sync_counters {
    int counter[MAC_SYNC_LAST_COUNTER];
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
void mlag_mac_sync_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_mac_sync_log_verbosity_get(void);

/**
 *  This function initializes mlag mac sync module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_init(insert_to_command_db_func insert_msgs);

/**
 *  This function deinits mlag mac sync module
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_deinit(void);

/**
 *  This function starts mlag mac sync module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_start(uint8_t *data);

/**
 *  This function stops mlag mac sync module
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_stop(uint8_t *data);

/**
 *  This function handles peer status notification event: up or down
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_status_change(struct peer_state_change_data *data);

/**
 *  This function handles peer start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_start_event(struct peer_state_change_data *data);

/**
 *  This function handles fdb get event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_fdb_get_event(uint8_t *data);

/**
 *  This function handles fdb export event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_fdb_export_event(uint8_t *data);

/**
 *  This function handles peer enable event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_peer_enable(struct peer_state_change_data *data);

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_sync_start(struct sync_event_data *data);

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_sync_finish(struct sync_event_data *data);

/**
 *  This function handles sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_done_to_peer( struct sync_event_data * data);


/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_local_learn(void *data);


/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_global_learn(void *data);


/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_local_age(void *data);


/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_global_age(void *data);

/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_global_reject(void *data);

/**
 *  This function handles ipl port set event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_ipl_port_set(struct ipl_port_set_event_data *data);

/**
 *  This function handles vlan local state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_switch_status_change(
    struct switch_status_change_event_data *data);

/**
 *  This function prints module's internal attributes to log
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_dump(void (*dump_cb)(const char *,...));

/**
 *  This function clears mac sync module counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_counters_clear(void);

/**
 *  This function increments mac sync module's counter
 *
 * @return 0 when successful, otherwise ERROR
 */
void mlag_mac_sync_inc_cnt(enum mac_sync_counts cnt);



/**
 *  This function increments mac sync module's counter
 *
 * @return 0 when successful, otherwise ERROR
 */
void mlag_mac_sync_inc_cnt_num(enum mac_sync_counts cnt ,int num);


/**
 * Populates mac sync relevant counters.
 *
 * @param[out] counters - Mlag protocol counters. Pointer to an already
 *                        allocated memory structure. Only the fdb
 *                        counters are relevant to this function
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 */
int mlag_mac_sync_counters_get(struct mlag_counters *mlag_counters);




/**
 *  This function increments mac sync module's counter
 *
 * @return 0 when successful, otherwise ERROR
 */
void mlag_mac_sync_inc_cnt_num(enum mac_sync_counts cnt ,int num);


/**
 *  This function returns switch current status
 *
 * @return switch current status
 */
enum master_election_switch_status mlag_mac_sync_get_current_status(void);



/*
 *  This function returns whether port is non mlag port
 *
 * @param[in] ifindex_num -ifindex
 *
 * @return 0 when successful, otherwise ERROR
 */
int PORT_MNGR_IS_NON_MLAG_PORT(int ifindex_num);

#endif /* MLAG_MAC_SYNC_MANAGER_H_ */
