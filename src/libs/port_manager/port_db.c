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


#define PORT_DB_C_
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_defs.h>
#include <errno.h>
#include <complib/cl_init.h>
#include <complib/cl_mem.h>
#include "port_db.h"

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static struct port_db mlag_port_db;

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_db_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/*
 * This function init port_db entry
 *
 * @param[in] p_object - pool object
 * @param[in] context - user context
 * @param[in] pp_pool_item - pool item
 *
 * @return 0 if operation completes successfully.
 */
static cl_status_t
port_entry_init(void *const p_object,
                void *context,
                cl_pool_item_t ** const pp_pool_item)
{
    cl_status_t err = CL_SUCCESS;
    struct mlag_port_entry *port_entry = (struct mlag_port_entry *)p_object;
    UNUSED_PARAM(context);

    err = cl_plock_init(&port_entry->port_lock);
    if (err != CL_SUCCESS) {
        MLAG_LOG(MLAG_LOG_ERROR, "Failed to init port lock err [%d]\n", err);
        goto bail;
    }

    *pp_pool_item = &(port_entry->pool_item);

bail:
    return err;
}

/*
 * This function handles notification of state change from heartbeat module
 *
 * @param[in] p_pool_item - pool item
 * @param[in] context - user context
 *
 * @return 0 if operation completes successfully.
 */
static void
port_entry_deinit(const cl_pool_item_t * const p_pool_item,
                  void *context)
{
    UNUSED_PARAM(context);
    struct mlag_port_entry *port_entry = NULL;

    port_entry = PARENT_STRUCT(p_pool_item, struct mlag_port_entry, pool_item);
    cl_plock_destroy(&port_entry->port_lock);
}

/**
 *  This function applies a function on each port item
 *
 * @param[in] apply_port_func - function pointer
 * @param[in] data -  data for the applied function
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_foreach(port_db_pfn_t apply_port_func, void *data)
{
    int err = 0;
    cl_map_item_t *map_item;
    struct mlag_port_entry *port_entry;
    const cl_map_item_t *map_end = NULL;
    map_item = cl_qmap_head(&(mlag_port_db.port_map));
    map_end = cl_qmap_end(&(mlag_port_db.port_map));
    while (map_item != map_end) {
        port_entry = PARENT_STRUCT(map_item, struct mlag_port_entry, map_item);
        cl_plock_excl_acquire(&port_entry->port_lock);
        err = apply_port_func(&(port_entry->port_info), data);
        cl_plock_release(&port_entry->port_lock);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to apply func on port db element err [%d]\n",
                            err);
        map_item = cl_qmap_next(map_item);
    }

bail:
    return err;
}

/*
 * This function deinits map item
 *
 * @param[in] port_entry - port entry
 * @param[in] param_p - user data
 *
 * @return 0 if operation completes successfully.
 */
static int
map_item_deinit(struct mlag_port_entry *port_entry)
{
    int err = 0;

    if (port_entry != NULL) {
        cl_qpool_put(&(mlag_port_db.port_pool), &(port_entry->pool_item));
    }

    return err;
}

/**
 *  This function inits port DB, it allocates MLAG_MAX_PORTS
 *  port entries in pool
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_init(void)
{
    int err = 0;
    cl_status_t cl_status = CL_SUCCESS;

    cl_status = cl_qpool_init(&(mlag_port_db.port_pool),
                              MLAG_MAX_PEERS * MLAG_MAX_PORTS,
                              MLAG_MAX_PEERS * MLAG_MAX_PORTS,
                              0, sizeof(struct mlag_port_entry),
                              port_entry_init, port_entry_deinit, NULL );
    if (cl_status != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err, "Failed to Init port pool\n");
    }
    cl_qmap_init(&(mlag_port_db.port_map));

bail:
    return err;
}

/**
 *  This function deinits port DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_deinit(void)
{
    int err = 0;

    /* return all used elements in map */
    cl_map_item_t *map_item;
    struct mlag_port_entry *port_entry;
    const cl_map_item_t *map_end = NULL;
    map_item = cl_qmap_head(&(mlag_port_db.port_map));
    map_end = cl_qmap_end(&(mlag_port_db.port_map));
    while (map_item != map_end) {
        port_entry = PARENT_STRUCT(map_item, struct mlag_port_entry, map_item);
        err = map_item_deinit(port_entry);
        MLAG_BAIL_ERROR_MSG(err, "Failed to deinit port DB\n");
        map_item = cl_qmap_next(map_item);
    }

    cl_qmap_remove_all(&(mlag_port_db.port_map));

    cl_qpool_destroy(&(mlag_port_db.port_pool));

bail:
    return err;
}

/*
 *  This function gets port entry pointer from DB,
 *
 * @param[in] port_id - port ID
 *
 * @return pointer to port_entry, NULL if not found
 */
static struct mlag_port_entry *
port_entry_get(unsigned long port_id)
{
    cl_map_item_t *map_item = NULL;
    struct mlag_port_entry *port_entry = NULL;

    map_item = cl_qmap_get(&(mlag_port_db.port_map), (uint64_t)port_id);

    if (map_item != cl_qmap_end(&(mlag_port_db.port_map))) {
        port_entry = PARENT_STRUCT(map_item, struct mlag_port_entry, map_item);
    }
    else {
        port_entry = NULL;
        goto bail;
    }

bail:
    return port_entry;
}

/*
 *  This function gets port entry pointer from DB,
 *
 * @param[in] port_id - port ID
 * @param[out] port_inf - pointer to port info
 *
 * @return 0 if successful
 * @return -ENOENT if port not found
 */
static int
port_db_entry_get(unsigned long port_id, struct mlag_port_data **port_info)
{
    struct mlag_port_entry *port_entry = NULL;
    int err = 0;

    port_entry = port_entry_get(port_id);
    if (port_entry == NULL) {
        err = -ENOENT;
        goto bail;
    }

    *port_info = &port_entry->port_info;

bail:
    return err;
}

/**
 *  This function gets if port exist in DB.
 *  If it exist it returns TRUE otherwise FALSE
 *
 * @param[in] port_id - port ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
port_db_entry_exist(unsigned long port_id)
{
    struct mlag_port_entry *port_entry = NULL;
    int found = TRUE;

    port_entry = port_entry_get(port_id);
    if (port_entry == NULL) {
        found = FALSE;
        goto bail;
    }

bail:
    return found;
}

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
int
port_db_entry_lock(unsigned long port_id, struct mlag_port_data **port_info)
{
    struct mlag_port_entry *port_entry = NULL;
    int err = 0;

    ASSERT(port_info != NULL);

    port_entry = port_entry_get(port_id);
    if (port_entry == NULL) {
        err = -ENOENT;
        goto bail;
    }

    cl_plock_excl_acquire(&port_entry->port_lock);
    *port_info = &port_entry->port_info;

bail:
    return err;
}

/**
 *  This function unlocks port entry in DB
 *
 * @param[in] port_id - port index
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port not found
 */
int
port_db_entry_unlock(unsigned long port_id)
{
    int err = 0;
    struct mlag_port_entry *port_entry;
    cl_map_item_t *map_item = NULL;

    map_item = cl_qmap_get(&(mlag_port_db.port_map),
                           (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_port_db.port_map))) {
        err = -ENOENT;
        goto bail;
    }
    port_entry = PARENT_STRUCT(map_item, struct mlag_port_entry, map_item);

    cl_plock_release(&port_entry->port_lock);

bail:
    return err;
}

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
int
port_db_entry_allocate(unsigned long port_id,
                       struct mlag_port_data **port_info)
{
    int err = 0;
    cl_pool_item_t *pool_item = NULL;
    struct mlag_port_entry *port_entry;
    cl_map_item_t *map_item = NULL;

    ASSERT(port_info != NULL);

    map_item = cl_qmap_get(&(mlag_port_db.port_map),
                           (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_port_db.port_map))) {
        pool_item = cl_qpool_get(&(mlag_port_db.port_pool));
        if (pool_item == NULL) {
            err = -ENOSPC;
            MLAG_BAIL_ERROR_MSG(err, "Out of mlag ports\n");
        }
        port_entry =
            PARENT_STRUCT(pool_item, struct mlag_port_entry, pool_item);
    }
    else {
        port_entry = PARENT_STRUCT(map_item, struct mlag_port_entry, map_item);
    }

    *port_info = &(port_entry->port_info);
    cl_plock_excl_acquire(&port_entry->port_lock);
    cl_qmap_insert(&(mlag_port_db.port_map),
                   (uint64_t)(port_id),
                   &(port_entry->map_item));

bail:
    return err;
}

/**
 *  This function deletes port info from DB
 *
 * @param[in] port_id - port ID
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int
port_db_entry_delete(unsigned long port_id)
{
    cl_map_item_t *map_item = NULL;
    int err = 0;
    struct mlag_port_entry *port_entry = NULL;

    map_item = cl_qmap_remove(&(mlag_port_db.port_map), (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_port_db.port_map))) {
        err = -ENOENT;
        goto bail;
    }

    /* Clear entry */
    port_entry = PARENT_STRUCT(map_item, struct mlag_port_entry, map_item);

    /* return to pool */
    cl_qpool_put(&(mlag_port_db.port_pool), &(port_entry->pool_item));

bail:
    return err;
}

/**
 *  This function gets port's per selected peer configuration
 *  state
 *
 * @param[in] port_id - port ID
 * @param[in] peer_id -  peer index
 * @param[out] state - 0 if not configured
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int
port_db_peer_conf_state_get(unsigned long port_id, int peer_id, uint8_t *state)
{
    int err = 0;
    struct mlag_port_data *port_info;

    ASSERT(state != NULL);

    err = port_db_entry_get(port_id, &port_info);
    if (err == -ENOENT) {
        *state = 0;
        err = 0;
        goto bail;
    }
    MLAG_BAIL_ERROR_MSG(err, "Failed to get port [%lu] entry\n", port_id);

    *state = port_info->peers_conf_state & (1 << peer_id);

bail:
    return err;
}

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
int
port_db_peer_oper_state_get(unsigned long port_id, int peer_id, uint8_t *state)
{
    int err = 0;
    struct mlag_port_data *port_info;

    ASSERT(state != NULL);

    err = port_db_entry_get(port_id, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get port [%lu] entry\n", port_id);

    *state = port_info->peers_oper_state & (1 << peer_id);

bail:
    return err;
}

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
int
port_db_peer_conf_state_vector_get(unsigned long port_id, uint32_t *states)
{
    int err = 0;
    struct mlag_port_data *port_info;

    ASSERT(states != NULL);

    err = port_db_entry_get(port_id, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get port [%lu] entry\n", port_id);

    *states = port_info->peers_conf_state;

bail:
    return err;
}

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
int
port_db_peer_oper_state_vector_get(unsigned long port_id, uint32_t *states)
{
    int err = 0;
    struct mlag_port_data *port_info;

    ASSERT(states != NULL);

    err = port_db_entry_get(port_id, &port_info);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get port [%lu] entry\n", port_id);

    *states = port_info->peers_oper_state;

bail:
    return err;
}

/**
 *  This function sets peer operational state
 *
 * @param[in] port_id - port ID
 * @param[in] state - current state
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_peer_state_set(int peer_id, enum port_manager_peer_state state)
{
    int err = 0;

    ASSERT(peer_id < MLAG_MAX_PEERS);

    mlag_port_db.peer_state[peer_id] = state;

bail:
    return err;
}

/**
 *  This function gets peer operational state
 *
 * @param[in] port_id - port ID
 * @param[out] states - current states (TRUE if peer up, o/w FALSE)
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_peer_state_get(int peer_id, enum port_manager_peer_state *state)
{
    int err = 0;

    ASSERT(peer_id < MLAG_MAX_PEERS);
    ASSERT(state != NULL);

    *state = mlag_port_db.peer_state[peer_id];

bail:
    return err;
}

/**
 *  This function gets peer operational state vector
 *
 * @param[out] states - current states
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_peer_state_vector_get(uint32_t *states)
{
    int err = 0;
    int peer_id;
    uint32_t peer_index = 1;

    ASSERT(states != NULL);

    *states = 0;

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        if (mlag_port_db.peer_state[peer_id] == PM_PEER_ENABLED) {
            *states |= peer_index;
        }
        peer_index <<= 1;
    }

bail:
    return err;
}

/**
 *  This function gets port manager counters
 *
 * @param[out] counters - mlag counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
port_db_counters_get(struct mlag_counters *counters)
{
    int err = 0;

    ASSERT(counters != NULL);

    counters->rx_port_notification = mlag_port_db.counters.rx_protocol_msg;
    counters->tx_port_notification = mlag_port_db.counters.tx_protocol_msg;

bail:
    return err;
}

/**
 *  This function clears port manager counters
 *
 * @return void
 */
void
port_db_counters_clear(void)
{
    SAFE_MEMSET(&(mlag_port_db.counters), 0);
}

/**
 *  This function increments specific counter
 *
 * @param[in] counter - counter to increment
 *
 * @return void
 */
void
port_db_counters_inc(enum pm_counters counter)
{
    if (counter == PM_CNT_PROTOCOL_TX) {
        mlag_port_db.counters.tx_protocol_msg++;
    }
    else if (counter == PM_CNT_PROTOCOL_RX) {
        mlag_port_db.counters.rx_protocol_msg++;
    }
}

