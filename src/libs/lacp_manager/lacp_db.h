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

#ifndef LACP_DB_H_
#define LACP_DB_H_

#include <utils/mlag_log.h>
#include <complib/cl_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_list.h>
#include <complib/cl_map.h>
#include "mlag_api_defs.h"

#ifdef LACP_DB_C_

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ LACP_MANAGER
/************************************************
 *  Local Macros
 ***********************************************/
#define MAP_ITEM_DEINIT(POOL_P, ITEM_P) do {                 \
        if (ITEM_P != NULL) {                                   \
            cl_qpool_put(POOL_P, &(ITEM_P->pool_item));         \
        } } while (0)


/************************************************
 *  Local Type definitions
 ***********************************************/




#endif

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

enum lm_counters {
    LM_CNT_PROTOCOL_TX = 0,
    LM_CNT_PROTOCOL_RX,

    LACP_MANAGER_LAST_COUNTER
};

struct mlag_lacp_data {
    unsigned long port_id;
    unsigned int partner_key;
    unsigned long long partner_id;
    int peer_state[MLAG_MAX_PEERS];
};

struct mlag_lacp_entry {
    cl_pool_item_t pool_item;
    cl_map_item_t map_item;
    struct mlag_lacp_data lacp_info;
};

struct lacp_manager_counters {
    uint64_t rx_protocol_msg;
    uint64_t tx_protocol_msg;
};

struct lacp_db {
    cl_qpool_t port_pool;
    cl_qpool_t pending_pool;
    cl_qmap_t port_map;
    cl_qmap_t pending_map;
    unsigned long long local_sys_id;
    unsigned long long master_sys_id;
    struct lacp_manager_counters counters;
};

struct __attribute__((__packed__)) lacp_aggregation_message_data {
    uint16_t opcode;
    uint16_t is_response;
    uint16_t response;
    uint16_t force;
    uint16_t select;
    int mlag_id;
    unsigned int request_id;
    unsigned long port_id;
    unsigned long long partner_id;
    unsigned int partner_key;
};

struct lacp_pending_entry {
    cl_pool_item_t pool_item;
    cl_map_item_t map_item;
    struct lacp_aggregation_message_data request;
};

typedef int (*lacp_db_pfn_t)(struct mlag_lacp_data *, void *);
typedef int (*lacp_db_pending_pfn_t)(struct lacp_aggregation_message_data *,
                                     void *data);

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
void
lacp_db_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits port DB, it allocates MLAG_MAX_PORTS
 *  port entries in pool
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_init(void);

/**
 *  This function deinits port DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_deinit(void);

/**
 *  This function sets local system ID and system priority
 *  in LACP DB
 *
 * @param[in] local_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_local_system_id_set(unsigned long long local_sys_id);

/**
 *  This function gets local system ID and system priority
 *  in LACP DB
 *
 * @param[out] local_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_local_system_id_get(unsigned long long *local_sys_id);

/**
 *  This function sets master system ID and system priority
 *  in LACP DB
 *
 * @param[in] master_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_master_system_id_set(unsigned long long master_sys_id);

/**
 *  This function gets master system ID and system priority
 *  in LACP DB
 *
 * @param[out] master_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_master_system_id_get(unsigned long long *master_sys_id);

/**
 *  This function gets lacp entry pointer from DB,
 *
 * @param[in] port_id - port ID
 * @param[out] lacp_info - pointer to lacp info
 *
 * @return 0 if successful
 * @return -ENOENT if port not found
 */
int
lacp_db_entry_get(unsigned long port_id, struct mlag_lacp_data **lacp_info);

/**
 *  This function sets lacp info in DB, copies info to DB entry
 *  struct
 *
 * @param[in]  port_id - port ID
 * @param[out] lacp_info - the allocated lacp_info
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOSPC if port pool is full
 */
int
lacp_db_entry_allocate(unsigned long port_id,
                       struct mlag_lacp_data **lacp_info);

/**
 *  This function deletes lacp info from DB
 *
 * @param[in] port_id - port ID
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if lacp entry no found
 */
int
lacp_db_entry_delete(unsigned long port_id);

/**
 *  Inserts a selection request to pending
 *  requests DB
 *
 * @param[in] port_id - port ID (key for request)
 * @param[in] data - request data
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if lacp entry no found
 */
int
lacp_db_pending_request_add(unsigned long port_id,
                            struct lacp_aggregation_message_data *data);

/**
 *  Removes a selection request from pending
 *  requests DB
 *
 * @param[in] port_id - port ID (key for request)
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if lacp entry no found
 */
int
lacp_db_pending_request_delete(unsigned long port_id);

/**
 *  Gets a pending selection request data pointer
 *
 * @param[in] port_id - port ID (key for request)
 * @param[in] data - request data
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if lacp entry no found
 */
int
lacp_db_pending_request_get(unsigned long port_id,
                            struct lacp_aggregation_message_data **data);

/**
 *  Apply a function on each item
 *
 * @param[in] apply_entry_func - function pointer
 * @param[in] data -  data for the applied function
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_port_foreach(lacp_db_pfn_t apply_entry_func, void *data);

/**
 *  clear all port DB entries
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_port_clear(void);

/**
 *  Apply a function on each lacp pending request item
 *
 * @param[in] apply_entry_func - function pointer
 * @param[in] data -  data for the applied function
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_pending_foreach(lacp_db_pending_pfn_t apply_entry_func, void *data);

/**
 *  This function gets lacp manager counters
 *
 * @param[out] counters - mlag counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_counters_get(struct mlag_counters *counters);

/**
 *  This function clears lacp manager counters
 *
 * @return void
 */
void
lacp_db_counters_clear(void);

/**
 *  This function increments specific counter
 *
 * @param[in] counter - counter to increment
 *
 * @return void
 */
void
lacp_db_counters_inc(enum lm_counters counter);


#endif /* LACP_DB_H_ */
