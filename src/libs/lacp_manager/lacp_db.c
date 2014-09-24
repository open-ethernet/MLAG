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


#define LACP_DB_C_
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_defs.h>
#include <errno.h>
#include <complib/cl_init.h>
#include <complib/cl_mem.h>
#include "lacp_db.h"

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static struct lacp_db mlag_lacp_db;

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
lacp_db_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/*
 * This function init lacp_db entry
 *
 * @param[in] p_object - pool object
 * @param[in] context - user context
 * @param[in] pp_pool_item - pool item
 *
 * @return 0 if operation completes successfully.
 */
static cl_status_t
lacp_entry_init(void *const p_object, void *context,
                cl_pool_item_t ** const pp_pool_item)
{
    cl_status_t err = CL_SUCCESS;
    struct mlag_lacp_entry *lacp_entry = (struct mlag_lacp_entry *)p_object;
    UNUSED_PARAM(context);

    *pp_pool_item = &(lacp_entry->pool_item);

    return err;
}

/*
 * This function deinit lacp_db entry
 *
 * @param[in] p_pool_item - pool item
 * @param[in] context - user context
 *
 * @return 0 if operation completes successfully.
 */
static void
lacp_entry_deinit(const cl_pool_item_t * const p_pool_item, void *context)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(p_pool_item);
}

/*
 * This function init lacp_pending entry
 *
 * @param[in] p_object - pool object
 * @param[in] context - user context
 * @param[in] pp_pool_item - pool item
 *
 * @return 0 if operation completes successfully.
 */
static cl_status_t
pending_entry_init(void *const p_object, void *context,
                   cl_pool_item_t ** const pp_pool_item)
{
    cl_status_t err = CL_SUCCESS;
    struct lacp_pending_entry *lacp_entry =
        (struct lacp_pending_entry *)p_object;
    UNUSED_PARAM(context);

    *pp_pool_item = &(lacp_entry->pool_item);

    return err;
}

/*
 * This function will deinit lacp_pending entry
 *
 * @param[in] p_pool_item - pool item
 * @param[in] context - user context
 *
 * @return 0 if operation completes successfully.
 */
static void
pending_entry_deinit(const cl_pool_item_t * const p_pool_item, void *context)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(p_pool_item);
}

/**
 *  This function inits lacp DB, it allocates MLAG_MAX_PORTS
 *  port entries in pool
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_init(void)
{
    int err = 0;
    cl_status_t cl_status = CL_SUCCESS;

    cl_status = cl_qpool_init(&(mlag_lacp_db.port_pool),
                              MLAG_MAX_PORTS, MLAG_MAX_PORTS, 0,
                              sizeof(struct mlag_lacp_entry), lacp_entry_init,
                              lacp_entry_deinit, NULL );
    if (cl_status != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err, "Failed to Init LACP DB port pool\n");
    }
    cl_qmap_init(&(mlag_lacp_db.port_map));

    cl_status = cl_qpool_init(&(mlag_lacp_db.pending_pool),
                              MLAG_MAX_PORTS, MLAG_MAX_PORTS, 0,
                              sizeof(struct lacp_pending_entry),
                              pending_entry_init, pending_entry_deinit, NULL );
    if (cl_status != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err, "Failed to Init LACP DB pending pool\n");
    }
    cl_qmap_init(&(mlag_lacp_db.pending_map));
    mlag_lacp_db.local_sys_id = 0;
    mlag_lacp_db.master_sys_id = 0;

bail:
    return err;
}

/**
 *  This function deinits lacp DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_deinit(void)
{
    int err = 0;
    cl_map_item_t *map_item;
    struct mlag_lacp_entry *lacp_entry;
    struct lacp_pending_entry *pending_entry;
    const cl_map_item_t *map_end = NULL;

    map_item = cl_qmap_head(&(mlag_lacp_db.port_map));
    map_end = cl_qmap_end(&(mlag_lacp_db.port_map));
    while (map_item != map_end) {
        lacp_entry = PARENT_STRUCT(map_item, struct mlag_lacp_entry, map_item);
        MAP_ITEM_DEINIT(&(mlag_lacp_db.port_pool), lacp_entry);
        map_item = cl_qmap_next(map_item);
    }
    cl_qmap_remove_all(&(mlag_lacp_db.port_map));
    cl_qpool_destroy(&(mlag_lacp_db.port_pool));

    map_item = cl_qmap_head(&(mlag_lacp_db.pending_map));
    map_end = cl_qmap_end(&(mlag_lacp_db.pending_map));
    while (map_item != map_end) {
        pending_entry = PARENT_STRUCT(map_item, struct lacp_pending_entry,
                                      map_item);
        MAP_ITEM_DEINIT(&(mlag_lacp_db.pending_pool), pending_entry);
        map_item = cl_qmap_next(map_item);
    }

    cl_qmap_remove_all(&(mlag_lacp_db.pending_map));
    cl_qpool_destroy(&(mlag_lacp_db.pending_pool));

    return err;
}

/*
 *  This function gets port entry pointer from DB
 *
 * @param[in] port_id - port ID
 *
 * @return pointer to port_entry, NULL if not found
 */
static struct mlag_lacp_entry *
lacp_entry_get(unsigned long port_id)
{
    cl_map_item_t *map_item = NULL;
    struct mlag_lacp_entry *lacp_entry = NULL;

    map_item = cl_qmap_get(&(mlag_lacp_db.port_map), (uint64_t)port_id);

    if (map_item != cl_qmap_end(&(mlag_lacp_db.port_map))) {
        lacp_entry = PARENT_STRUCT(map_item, struct mlag_lacp_entry, map_item);
    }

    return lacp_entry;
}

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
lacp_db_entry_get(unsigned long port_id, struct mlag_lacp_data **lacp_info)
{
    struct mlag_lacp_entry *lacp_entry = NULL;
    int err = 0;

    lacp_entry = lacp_entry_get(port_id);
    if (lacp_entry == NULL) {
        err = -ENOENT;
        goto bail;
    }

    *lacp_info = &lacp_entry->lacp_info;

bail:
    return err;
}

/**
 *  This function sets local system ID and system priority
 *  in LACP DB
 *
 * @param[in] local_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_local_system_id_set(unsigned long long local_sys_id)
{
    int err = 0;

    mlag_lacp_db.local_sys_id = local_sys_id;

    return err;
}

/**
 *  This function sets master system ID and system priority
 *  in LACP DB
 *
 * @param[in] master_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_master_system_id_set(unsigned long long master_sys_id)
{
    int err = 0;

    mlag_lacp_db.master_sys_id = master_sys_id;

    return err;
}

/**
 *  This function gets local system ID and system priority
 *  in LACP DB
 *
 * @param[out] local_sys_id - local system ID
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_local_system_id_get(unsigned long long *local_sys_id)
{
    int err = 0;
    ASSERT(local_sys_id != NULL);

    *local_sys_id = mlag_lacp_db.local_sys_id;

bail:
    return err;
}

/**
 *  This function gets master system ID and system priority
 *  in LACP DB
 *
 * @param[out] master_sys_id - local system ID
 * @param[out] master_sys_prio - local system priority
 *
 * @return TRUE when port found, otherwise FALSE
 */
int
lacp_db_master_system_id_get(unsigned long long *master_sys_id)
{
    int err = 0;
    ASSERT(master_sys_id != NULL);

    *master_sys_id = mlag_lacp_db.master_sys_id;

bail:
    return err;
}

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
                       struct mlag_lacp_data **lacp_info)
{
    int err = 0;
    cl_pool_item_t *pool_item = NULL;
    struct mlag_lacp_entry *lacp_entry = NULL;
    cl_map_item_t *map_item = NULL;

    ASSERT(lacp_info != NULL);

    map_item = cl_qmap_get(&(mlag_lacp_db.port_map),
                           (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_lacp_db.port_map))) {
        pool_item = cl_qpool_get(&(mlag_lacp_db.port_pool));
        if (pool_item == NULL) {
            err = -ENOSPC;
            MLAG_BAIL_ERROR_MSG(err, "Out of mlag ports\n");
        }
        lacp_entry =
            PARENT_STRUCT(pool_item, struct mlag_lacp_entry, pool_item);
    }
    else {
        lacp_entry = PARENT_STRUCT(map_item, struct mlag_lacp_entry, map_item);
    }

    *lacp_info = &(lacp_entry->lacp_info);
    lacp_entry->lacp_info.port_id = port_id;
    cl_qmap_insert(&(mlag_lacp_db.port_map),
                   (uint64_t)(port_id),
                   &(lacp_entry->map_item));

bail:
    return err;
}

/**
 *  This function deletes lacp info from DB
 *
 * @param[in] port_id - port ID
 *
 * @return 0 when successful, otherwise ERROR
 * @return ENOENT if port entry no found
 */
int
lacp_db_entry_delete(unsigned long port_id)
{
    cl_map_item_t *map_item = NULL;
    int err = 0;
    struct mlag_lacp_entry *lacp_entry = NULL;

    map_item = cl_qmap_remove(&(mlag_lacp_db.port_map), (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_lacp_db.port_map))) {
        err = -ENOENT;
        goto bail;
    }

    /* Clear entry */
    lacp_entry = PARENT_STRUCT(map_item, struct mlag_lacp_entry, map_item);

    /* return to pool */
    cl_qpool_put(&(mlag_lacp_db.port_pool), &(lacp_entry->pool_item));

bail:
    return err;
}

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
                            struct lacp_aggregation_message_data *data)
{
    int err = 0;
    cl_pool_item_t *pool_item = NULL;
    struct lacp_pending_entry *pending_entry;
    cl_map_item_t *map_item = NULL;

    ASSERT(data != NULL);

    map_item = cl_qmap_get(&(mlag_lacp_db.pending_map),
                           (uint64_t)port_id);
    /* Our logic does not allow for pending request
     * to override a previous one. it is expected
     * that older pending requests will be removed
     * prior to new ones added
     */
    ASSERT(map_item == cl_qmap_end(&(mlag_lacp_db.pending_map)));
    pool_item = cl_qpool_get(&(mlag_lacp_db.pending_pool));
    if (pool_item == NULL) {
        err = -ENOSPC;
        MLAG_BAIL_ERROR_MSG(err, "Out of space for pending LACP request\n");
    }
    pending_entry =
        PARENT_STRUCT(pool_item, struct lacp_pending_entry, pool_item);

    pending_entry->request = *data;
    cl_qmap_insert(&(mlag_lacp_db.pending_map), (uint64_t)(port_id),
                   &(pending_entry->map_item));

bail:
    return err;
}

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
lacp_db_pending_request_delete(unsigned long port_id)
{
    int err = 0;
    cl_map_item_t *map_item = NULL;
    struct lacp_pending_entry *pending_entry = NULL;

    map_item = cl_qmap_remove(&(mlag_lacp_db.pending_map), (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_lacp_db.pending_map))) {
        err = -ENOENT;
        goto bail;
    }

    /* Clear entry */
    pending_entry =
        PARENT_STRUCT(map_item, struct lacp_pending_entry, map_item);

    /* return to pool */
    cl_qpool_put(&(mlag_lacp_db.pending_pool), &(pending_entry->pool_item));

bail:
    return err;
}

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
                            struct lacp_aggregation_message_data **data)
{
    int err = 0;
    cl_map_item_t *map_item = NULL;
    struct lacp_pending_entry *pending_entry = NULL;

    ASSERT(data != NULL);

    map_item = cl_qmap_get(&(mlag_lacp_db.pending_map), (uint64_t)port_id);
    if (map_item == cl_qmap_end(&(mlag_lacp_db.pending_map))) {
        err = -ENOENT;
        *data = NULL;
        goto bail;
    }

    pending_entry =
        PARENT_STRUCT(map_item, struct lacp_pending_entry, map_item);
    *data = &pending_entry->request;

bail:
    return err;
}

/**
 *  Apply a function on each port item
 *
 * @param[in] apply_entry_func - function pointer
 * @param[in] data -  data for the applied function
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_port_foreach(lacp_db_pfn_t apply_entry_func, void *data)
{
    int err = 0;
    cl_map_item_t *map_item;
    struct mlag_lacp_entry *lacp_entry;
    const cl_map_item_t *map_end = NULL;
    map_item = cl_qmap_head(&(mlag_lacp_db.port_map));
    map_end = cl_qmap_end(&(mlag_lacp_db.port_map));
    while (map_item != map_end) {
        lacp_entry = PARENT_STRUCT(map_item, struct mlag_lacp_entry, map_item);
        err = apply_entry_func(&(lacp_entry->lacp_info), data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to apply func on lacp db element err [%d]\n",
                            err);
        map_item = cl_qmap_next(map_item);
    }

bail:
    return err;
}

/**
 *  clear all port DB entries
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_port_clear(void)
{
    int err = 0;
    cl_map_item_t *map_item;
    struct mlag_lacp_entry *lacp_entry;
    const cl_map_item_t *map_end = NULL;

    map_item = cl_qmap_head(&(mlag_lacp_db.port_map));
    map_end = cl_qmap_end(&(mlag_lacp_db.port_map));
    while (map_item != map_end) {
        lacp_entry = PARENT_STRUCT(map_item, struct mlag_lacp_entry, map_item);
        MAP_ITEM_DEINIT(&(mlag_lacp_db.port_pool), lacp_entry);
        map_item = cl_qmap_next(map_item);
    }
    cl_qmap_remove_all(&(mlag_lacp_db.port_map));

    return err;
}

/**
 *  Apply a function on each lacp pending request item
 *
 * @param[in] apply_entry_func - function pointer
 * @param[in] data -  data for the applied function
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_pending_foreach(lacp_db_pending_pfn_t apply_entry_func, void *data)
{
    int err = 0;
    cl_map_item_t *map_item;
    struct lacp_pending_entry *lacp_entry;
    const cl_map_item_t *map_end = NULL;
    map_item = cl_qmap_head(&(mlag_lacp_db.pending_map));
    map_end = cl_qmap_end(&(mlag_lacp_db.pending_map));
    while (map_item != map_end) {
        lacp_entry = PARENT_STRUCT(map_item, struct lacp_pending_entry,
                                   map_item);
        err = apply_entry_func(&(lacp_entry->request), data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to apply func on lacp pending db element err [%d]\n",
                            err);
        map_item = cl_qmap_next(map_item);
    }

bail:
    return err;
}

/**
 *  This function gets lacp manager counters
 *
 * @param[out] counters - mlag counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_db_counters_get(struct mlag_counters *counters)
{
    int err = 0;

    ASSERT(counters != NULL);

    counters->rx_lacp_manager = mlag_lacp_db.counters.rx_protocol_msg;
    counters->tx_lacp_manager = mlag_lacp_db.counters.tx_protocol_msg;

bail:
    return err;
}

/**
 *  This function clears lacp manager counters
 *
 * @return void
 */
void
lacp_db_counters_clear(void)
{
    SAFE_MEMSET(&(mlag_lacp_db.counters), 0);
}

/**
 *  This function gets peer operational state vector
 *
 * @param[in] counter - counter to increment
 *
 * @return void
 */
void
lacp_db_counters_inc(enum lm_counters counter)
{
    if (counter == LM_CNT_PROTOCOL_TX) {
        mlag_lacp_db.counters.tx_protocol_msg++;
    }
    else if (counter == LM_CNT_PROTOCOL_RX) {
        mlag_lacp_db.counters.rx_protocol_msg++;
    }
}

