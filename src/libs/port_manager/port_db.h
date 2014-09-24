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

#ifndef PORT_DB_H_
#define PORT_DB_H_

#include <utils/mlag_log.h>
#include <complib/cl_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_list.h>
#include <complib/cl_map.h>
#include "port_peer_local.h"
#include "port_peer_remote.h"
#include "port_master_logic.h"
#include "port_manager.h"

#ifdef PORT_DB_C_

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ PORT_MANAGER
/************************************************
 *  Local Macros
 ***********************************************/


/************************************************
 *  Local Type definitions
 ***********************************************/

struct port_db {
    cl_qpool_t port_pool;
    cl_qmap_t port_map;
    int peer_state[MLAG_MAX_PEERS];
    struct port_manager_counters counters;
};


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

enum port_manager_peer_state {
    PM_PEER_DOWN = 0,
    PM_PEER_ENABLED = 1,
    PM_PEER_TX_ENABLED = 2
};

struct mlag_port_data {
    unsigned long port_id;
    enum mlag_port_mode port_mode;
    uint32_t peers_conf_state;
    uint32_t peers_oper_state;
    /* port logic */
    struct port_peer_local peer_local_fsm;
    struct port_peer_remote peer_remote_fsm;
    struct port_master_logic port_master_fsm;
};

struct mlag_port_entry {
    cl_pool_item_t pool_item;
    cl_map_item_t map_item;
    cl_plock_t port_lock;
    struct mlag_port_data port_info;
};


typedef int (*port_db_pfn_t)(struct mlag_port_data *, void *);

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
void port_db_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits port DB, it allocates MLAG_MAX_PORTS
 *  port entries in pool
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_init(void);

/**
 *  This function deinits port DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_deinit(void);

/**
 *  This function gets if port exist in DB.
 *  If it exist it returns TRUE otherwise FALSE
 *
 * @param[in] port_id - port ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int port_db_entry_exist(unsigned long port_id);

/**
 *  This function gets port info from DB, port_info passed as
 *  pointer to DB, and port is locked
 *
 * @param[in] port_id - port ID
 * @param[out] port_info -  pointer to port info
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int port_db_entry_lock(unsigned long port_id,
                       struct mlag_port_data **port_info);

/**
 *  This function unlocks port entry in DB
 *
 * @param[in] port_id - port index
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port not found
 */
int port_db_entry_unlock(unsigned long port_id);

/**
 *  This function sets port info in DB, copies info to DB entry
 *  struct
 *
 * @param[in] port_id - port ID
 * @param[out] port_info - the allocated port_info
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOSPC if port pool is full
 */
int port_db_entry_allocate(unsigned long port_id,
                           struct mlag_port_data **port_info);

/**
 *  This function deletes port info from DB
 *
 * @param[in] port_id - port ID
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int port_db_entry_delete(unsigned long port_id);

/**
 *  This function applies a function on each port item
 *
 * @param[in] apply_port_func - function pointer
 * @param[in] data -  data for the applied function
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_foreach(port_db_pfn_t apply_port_func, void *data);

/**
 *  This function gets port's per selected peer configuration
 *  state
 *
 * @param[in] port_id - port ID
 * @param[in] peer_id -  peer index
 * @param[out] state - new state
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int port_db_peer_conf_state_get(unsigned long port_id, int peer_id,
                                uint8_t *state);

/**
 *  This function gets port's per selected peer operational
 *  state
 *
 * @param[in] port_id - port ID
 * @param[in] peer_id -  peer index
 * @param[out] state - current state
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int port_db_peer_oper_state_get(unsigned long port_id, int peer_id,
                                uint8_t *state);

/**
 *  This function gets port's peer configuration
 *  state vector
 *
 * @param[in] port_id - port ID
 * @param[out] states - current states
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int port_db_peer_conf_state_vector_get(unsigned long port_id,
                                       uint32_t *states);

/**
 *  This function gets port's peer operational
 *  state vector
 *
 * @param[in] port_id - port ID
 * @param[out] states - current states
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int port_db_peer_oper_state_vector_get(unsigned long port_id,
                                       uint32_t *states);

/**
 *  This function sets peer operational state
 *
 * @param[in] port_id - port ID
 * @param[in] state - current state
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_peer_state_set(int peer_id, enum port_manager_peer_state state);

/**
 *  This function gets peer operational state
 *
 * @param[in] port_id - port ID
 * @param[out] states - current states (TRUE if peer up, o/w FALSE)
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_peer_state_get(int peer_id, enum port_manager_peer_state *state);

/**
 *  This function gets peer operational state vector
 *
 * @param[out] states - current states
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_peer_state_vector_get(uint32_t *states);

/**
 *  This function gets port manager counters
 *
 * @param[out] counters - mlag counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_db_counters_get(struct mlag_counters *counters);

/**
 *  This function clears port manager counters
 *
 * @return void
 */
void port_db_counters_clear(void);

/**
 *  This function increments specific counter
 *
 * @param[in] counter - counter to increment
 *
 * @return void
 */
void port_db_counters_inc(enum pm_counters counter);

#endif /* PORT_DB_H_ */
