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
#include <errno.h>
#include <complib/cl_init.h>
#include <complib/cl_mem.h>
#include <complib/cl_timer.h>
#include <complib/cl_pool.h>

#include <complib/cl_qmap.h>
#include <complib/cl_thread.h>

#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_defs.h"
#include "mlag_events.h"
#include "mac_sync_events.h"
#include <mlnx_lib/lib_event_disp.h>
#include "mlag_common.h"
#include "mlag_master_election.h"
#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_mac_sync_dispatcher.h"
#include "mlag_mac_sync_manager.h"
#include "mlag_manager.h"
#include "mlag_mac_sync_router_mac_db.h"
#include "mlag_mac_sync_master_logic.h"
#include "mlag_mac_sync_peer_manager.h"
#include "mlag_dispatcher.h"


#undef  __MODULE__
#define __MODULE__ MLAG_MAC_SYNC_ROUTER_MAC_DB

/************************************************
 *  Local Defines
 ***********************************************/
/************************************************
 *  Local Macros
 ***********************************************/
#define ROUTER_MAC_SYNC_MAC_VLAN_TO_KEY(mac_addr, vid)  \
    (uint64_t)((MAC_TO_U64(mac_addr)) | ((uint64_t)(vid) << 48))

#define CHECK_ROUTER_DB_INIT_DONE  if (!is_initialized) {err = -EPERM; \
                                                         goto bail; }

/************************************************
 *  Local Type definitions
 ***********************************************/



/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static cl_pool_t db_pool;   /* pool for allocations of router mac entries */
static cl_qmap_t db_map;   /* qmap used for store allocated router macs */
static int is_initialized = 0;
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

static router_mac_cookie_init_deinit_func init_deinit_cb
    = NULL;

/************************************************
 *  Local function declarations
 ***********************************************/
static int add_record(uint64_t mac_key, struct router_db_entry * entry_p);
static int delete_record(struct router_db_entry * entry_p);
static int get_record_by_key(uint64_t mac_key,
                             struct router_db_entry **entry_p);
static int db_destroy(void);

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
mlag_mac_sync_router_mac_db_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function registers init_deinit callback function
 * @param[in]  func - callback to init_deinit function
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_register_cookie_func(
    router_mac_cookie_init_deinit_func func)
{
    init_deinit_cb = func;
    return 0;
}

/**
 *  This function initializes router mac database
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_init(void)
{
    int err = 0;

    if (is_initialized == 0) {
        cl_pool_construct(&db_pool);
        cl_qmap_init(&db_map);

        err = cl_pool_init(&db_pool, MAX_ROUTER_MAC_ENTRIES,
                           MAX_ROUTER_MAC_ENTRIES,
                           0, sizeof(struct router_db_entry), NULL, NULL,
                           NULL);
        MLAG_BAIL_ERROR(err);

        is_initialized = 1;
    }
bail:
    MLAG_LOG(MLAG_LOG_NOTICE,  "Init router mac db : err = %d\n", err);
    return err;
}

/**
 *  This function registers destroys  router mac database
 *
 * @return 0 when successful, otherwise ERROR
 */

static int
db_destroy(void)
{
    struct router_db_entry *entry_item_p = NULL;
    int err = 0;

    CHECK_ROUTER_DB_INIT_DONE;

    err = mlag_mac_sync_router_mac_db_first_record(&entry_item_p);
    if (err) {
        MLAG_LOG(MLAG_LOG_NOTICE, "db_destroy get first record: err = %d\n",
                 err);
        goto bail;
    }
    while (entry_item_p) {
        cl_qmap_remove_item(&db_map, &entry_item_p->map_item);
        cl_pool_put(&db_pool, entry_item_p);
        err = mlag_mac_sync_router_mac_db_first_record(&entry_item_p);
        if (err) {
            MLAG_LOG(MLAG_LOG_NOTICE, "db_destroy err = %d\n", err);
            goto bail;
        }
    }
bail:
    return err;
}

/**
 *  This function registers de-initializes  router mac database
 *
 * @return 0 when successful, otherwise ERROR
 */

int
mlag_mac_sync_router_mac_db_deinit(void)
{
    int err = 0;

    CHECK_ROUTER_DB_INIT_DONE;

    err = db_destroy();
    MLAG_BAIL_ERROR(err);

    cl_pool_destroy(&db_pool);

    is_initialized = 0;
bail:
    return err;
}

/**
 *  This function configures router mac
 * @param[in]  data - pointer to the structure
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_conf(uint8_t *data)
{
    int err = 0;
    int i = 0;
    struct router_mac_cfg_data *event = NULL;
    struct ether_addr mac_addr = {{0}};

    CHECK_ROUTER_DB_INIT_DONE;

    MLAG_BAIL_CHECK(data != NULL, -EINVAL);

    event = (struct router_mac_cfg_data *)data;

    if (!event->del_command) {
        for (i = 0; i < event->router_macs_list_cnt; i++) {
            memcpy(&mac_addr, &event->mac_router_list[i].mac,
                   sizeof(mac_addr));
            err = mlag_mac_sync_router_mac_db_add(mac_addr,
                                                  event->mac_router_list[i].vlan_id);
            MLAG_BAIL_ERROR(err);
        }
    }
    else {
        for (i = 0; i < event->router_macs_list_cnt; i++) {
            memcpy(&mac_addr, &event->mac_router_list[i].mac,
                   sizeof(mac_addr));
            err = mlag_mac_sync_router_mac_db_remove(mac_addr,
                                                     event->mac_router_list[i].vlan_id);
            MLAG_BAIL_ERROR(err);
        }
    }

bail:
    return err;
}

/**
 *  This function adds router mac to the database and launches mac sync process
 *  for this mac
 * @param[in]  mac    - mac address
 * @param[in]  vid    - vlan id
 *
 * @return 0 when successful, otherwise ERROR
 */

int
mlag_mac_sync_router_mac_db_add( struct ether_addr mac,
                                 unsigned short vid)
{
    int err = 0;
    uint64_t key;
    struct router_db_entry new_entry;
    struct router_db_entry *old_entry = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    key = ROUTER_MAC_SYNC_MAC_VLAN_TO_KEY(mac, vid);
    err = get_record_by_key(key, &old_entry);
    if (err == -ENOENT) {
        err = 0;
    }
    MLAG_BAIL_ERROR(err);

    if (old_entry == NULL) { /*not exist - new entry added*/
        new_entry.vid = vid;
        new_entry.last_action = ADD_ROUTER_MAC;
        new_entry.sync_status = 0;
        new_entry.cookie = NULL;
        memcpy(&new_entry.mac_addr, &mac, sizeof(new_entry.mac_addr));
        err = add_record( key, &new_entry);
        MLAG_BAIL_ERROR(err);
    }
    else { /* this mac and vlan already exist in the db*/
        if (old_entry->last_action == ADD_ROUTER_MAC) {
            /* No need to sync again */
            goto bail;
        }
        old_entry->sync_status = 0;
        old_entry->last_action = ADD_ROUTER_MAC;
    }

    /*Synchronize*/
    err = mlag_mac_sync_peer_mngr_sync_router_mac(
        mac, vid, 1);
    if (err == -EPERM) { /* peer is not ready for sync for example it is down*/
        err = 0;
        MLAG_LOG(MLAG_LOG_INFO,
                 " router mac add: peer is not ready to sync MAC\n");
        goto bail;
    }
    MLAG_BAIL_ERROR(err);


bail:
    return err;
}

/**
 *  This function removes router mac from the database and launches mac sync process
 *  for this mac
 *  @param[in]  mac    - mac address
 *  @param[in]  vid    - vlan id
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_remove( struct ether_addr mac,
                                    unsigned short vid)
{
    int err = 0;
    uint64_t key;
    struct router_db_entry *old_entry = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    key = ROUTER_MAC_SYNC_MAC_VLAN_TO_KEY(mac, vid);
    err = get_record_by_key(key, &old_entry);
    if ((err == -ENOENT) || (old_entry == NULL)) {
        err = 0;
        MLAG_LOG(MLAG_LOG_NOTICE,
                 " router mac remove :no router mac configured\n");
        goto bail;
    }

    MLAG_BAIL_ERROR(err);

    if (old_entry) {
        if (old_entry->last_action != REMOVE_ROUTER_MAC) {
            /* Synchronize*/
            err = mlag_mac_sync_peer_mngr_sync_router_mac(
                mac, vid, 0);
            if (err == -EPERM) {
                MLAG_LOG(MLAG_LOG_INFO,
                         " router mac remove: peer is not ready to sync MAC\n");
                err = delete_record(old_entry);
                MLAG_BAIL_ERROR(err);
                goto bail;
            }
            MLAG_BAIL_ERROR(err);
            old_entry->sync_status = 0;
        }
        old_entry->last_action = REMOVE_ROUTER_MAC;
    }

bail:
    return err;
}


/**
 *  This function fetches all router macs from the router mac db
 *  and sends Local learn message for them to the master
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_sync_router_macs(void)
{
    int err = 0;
    struct router_db_entry *item_p;
    struct router_db_entry *prev_item_p;

    CHECK_ROUTER_DB_INIT_DONE;

    err = mlag_mac_sync_router_mac_db_first_record(&item_p);
    MLAG_BAIL_ERROR(err);
    while (item_p) {
        if ((item_p->last_action == ADD_ROUTER_MAC) &&
            (item_p->sync_status == 0)) {
            err = mlag_mac_sync_peer_mngr_sync_router_mac(
                item_p->mac_addr,
                item_p->vid,
                1);
            if (err == -EPERM) {
                err = 0;
                MLAG_LOG(MLAG_LOG_INFO,
                         " router mac sync: peer is not ready to sync MAC\n");
                goto bail;
            }
            MLAG_BAIL_ERROR(err);
        }
        prev_item_p = item_p;
        err = mlag_mac_sync_router_mac_db_next_record(
            prev_item_p, &item_p);
        MLAG_BAIL_ERROR(err);
    }
bail:
    return err;
}


/**
 *  This function passes all router macs from the router mac db
 *  and sets sync flag to "not sync"
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_set_not_sync(void)
{
    int err = 0;
    struct router_db_entry *item_p;
    struct router_db_entry *prev_item_p;

    CHECK_ROUTER_DB_INIT_DONE;

    err = mlag_mac_sync_router_mac_db_first_record(&item_p);
    MLAG_BAIL_ERROR(err);
    while (item_p) {
        item_p->sync_status = 0;
        prev_item_p = item_p;
        err = mlag_mac_sync_router_mac_db_next_record(
            prev_item_p, &item_p);
        MLAG_BAIL_ERROR(err);

        if (prev_item_p->last_action == REMOVE_ROUTER_MAC) {
            err = delete_record(prev_item_p);
            MLAG_BAIL_ERROR(err);
        }
    }
bail:
    return err;
}


/**
 * This function returns entry from the database regarding
 *   specific router MAC . if entry not exists - returns NULL
 *
 * @param[in]  mac    - mac address
 * @param[in]  vid    - vlan id
 * @param[out] old_entry - pointer to pointer on the entry from the database
 *
 * @return 0 operation completes successfully
 * @return   -EPERM  in case general error
 */
int
mlag_mac_sync_router_mac_db_get( struct ether_addr mac,
                                 unsigned short vid,
                                 struct router_db_entry **old_entry
                                 )
{
    int err = 0;
    uint64_t key;

    CHECK_ROUTER_DB_INIT_DONE;

    key = ROUTER_MAC_SYNC_MAC_VLAN_TO_KEY(mac, vid);
    err = get_record_by_key(key,  old_entry);
bail:
    return err;
}

/**
 * This function processes  response of Mac sync regarding
 *   specific router MAC
 *
 * @param[in] mac    - mac address
 * @param[in] vid    - vlan id
 * @param[in] action - Add or Remove router MAC
 *
 * @return 0 operation completes successfully
 * @return   -EPERM  in case general error
 */
int
mlag_mac_sync_router_mac_db_sync_response(struct ether_addr mac,
                                          unsigned short vid,
                                          enum router_mac_action action)
{
    int err = 0;
    uint64_t key;
    struct router_db_entry *old_entry = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    key = ROUTER_MAC_SYNC_MAC_VLAN_TO_KEY(mac, vid);
    err = get_record_by_key(key, &old_entry);
    MLAG_BAIL_ERROR(err);


    if (old_entry->last_action == action) {
        if (action == ADD_ROUTER_MAC) {
            old_entry->sync_status = 1;
            if (init_deinit_cb && (old_entry->cookie == NULL)) {
                err = init_deinit_cb(COOKIE_OP_INIT, &old_entry->cookie);
                MLAG_BAIL_ERROR(err);
            }
        }
        else if (action == REMOVE_ROUTER_MAC) {
            if (init_deinit_cb && (old_entry->cookie != NULL)) {
                err = init_deinit_cb(COOKIE_OP_DEINIT, &old_entry->cookie);
                MLAG_BAIL_ERROR(err);
            }
            err = delete_record(old_entry);
            MLAG_BAIL_ERROR(err);
        }
    }
bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "mlag_mac_sync_router_mac_db_sync_response completed. err %d\n",
             err );
    return err;
}

/**
 *  This function gets router mac from the database by the key
 * @param[in]  mac_key       - key for the database
 * @param[out] entry_item_pp - pointer to pointer of the entry from the database
 *
 * @return 0 when successful, otherwise ERROR
 */
int
get_record_by_key(uint64_t mac_key, struct router_db_entry **entry_item_pp)
{
    int err = 0;
    cl_map_item_t *map_item_p = NULL;
    *entry_item_pp = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    if (CL_QMAP_KEY_EXISTS(&db_map, mac_key, map_item_p)) {
        *entry_item_pp = CL_QMAP_PARENT_STRUCT(struct router_db_entry);
    }
    else {
        err = -ENOENT;
        MLAG_LOG(MLAG_LOG_INFO,
                 "get record by key [%" PRIx64 "] Entry not found\n", mac_key);
        goto bail;
    }
bail:
    return err;
}


/**
 *  This function adds router mac to the database
 * @param[in]  mac_key       - key for the database
 * @param[in]  entry_item_pp - pointer to the entry that should be added to the database
 *
 * @return 0 when successful, otherwise ERROR
 */
int
add_record(    uint64_t mac_key,
               struct router_db_entry *entry_p)
{
    int err = 0;
    struct router_db_entry *new_entry_p = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    new_entry_p = (struct router_db_entry *) cl_pool_get(&db_pool);
    if (!(new_entry_p)) {
        err = -ENOMEM;
        MLAG_LOG(MLAG_LOG_NOTICE, " mem. alloc. err = %d\n", err);
        goto bail;
    }

    MEM_CPY_P(new_entry_p, entry_p);
    cl_qmap_insert(&db_map, mac_key, &((new_entry_p)->map_item));

bail:
    return err;
}

/**
 *  This function deletes router mac to the database
 * @param[in]  mac_key       - key for the database
 * @param[in]  entry_item_pp - pointer to the entry that should be deleted from the database
 *
 * @return 0 when successful, otherwise ERROR
 */
int
delete_record(struct router_db_entry *entry_p)
{
    int err = 0;

    CHECK_ROUTER_DB_INIT_DONE;
    if (!entry_p) {
        MLAG_LOG(MLAG_LOG_NOTICE, " entry is null \n" );
        err = -EPERM;
        goto bail;
    }

    cl_qmap_remove_item(&db_map, &entry_p->map_item);
    cl_pool_put(&db_pool, entry_p);
bail:
    return err;
}


/**
 * This function returns the pointer for first entry in DB
 *
 * @param[out] item_pp - pointer to entry
 *
 * @return 0 operation completes successfully
 * @return   -EPERM  in case general error
 */
int
mlag_mac_sync_router_mac_db_first_record(struct router_db_entry **item_pp)
{
    int err = 0;
    cl_map_item_t *map_item_p = NULL;
    *item_pp = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    if (CL_QMAP_HEAD(map_item_p, &db_map)) {
        /*LOG(LOG_DEBUG, "map_item_p key :0x%" PRIx64 "]\n", map_item_p->key);*/
        if (!CL_QMAP_END(map_item_p, &db_map)) {
            *item_pp = CL_QMAP_PARENT_STRUCT(struct router_db_entry);
        }
    }
bail:
    return err;
}

/**
 * This function returns the pointer to next entry that comes
 * after the given one
 * @param[in]  input_item_p - input entry
 * @param[out] return_item_pp - pointer to entry
 *
 * @return 0  operation completes successfully
 * @return -EPERM   general error , ENOENT - no next entry found
 */
int
mlag_mac_sync_router_mac_db_next_record(
    struct router_db_entry * input_item_p,
    struct router_db_entry **return_item_p)
{
    int err = 0;
    cl_map_item_t *map_item_p = NULL;

    CHECK_ROUTER_DB_INIT_DONE;

    if (!input_item_p) {
        err = -EPERM;
        MLAG_LOG(MLAG_LOG_NOTICE, " pointer null err = %d\n", err);
        goto bail;
    }
    map_item_p = cl_qmap_next(&(input_item_p->map_item));
    if (CL_QMAP_END(map_item_p, &db_map)) {
        err = -ENOENT;
        *return_item_p = NULL;
    }
    else {
        *return_item_p = CL_QMAP_PARENT_STRUCT(struct router_db_entry);
    }
    err = 0;

bail:
    return err;
}


/**
 * This function prints router mac database
 * @param[in]  dump_cb - callback to dump function
 * @return 0  operation completes successfully, otherwise ERROR
 *
 */

int
mlag_mac_sync_router_mac_db_print(void (*dump_cb)(const char *, ...))
{
    int err = 0;
    struct router_db_entry *item_p;
    struct router_db_entry *prev_item_p;
    uint32_t count;
    CHECK_ROUTER_DB_INIT_DONE;

    err = mlag_mac_sync_router_mac_db_first_record(&item_p);
    MLAG_BAIL_ERROR(err);
    dump_cb("== Router MAC DB ==\n");
    while (item_p) {
        /*dump  entry*/

        DUMP_OR_LOG(
            "mac %02x:%02x:%02x:%02x:%02x:%02x, vid %03d, add %d, sync %d cookie %p\n",
            PRINT_MAC_OES((*item_p)), item_p->vid,
            item_p->last_action, item_p->sync_status,
            item_p->cookie);

        if (item_p->cookie) {
            mlag_mac_sync_master_logic_print_master(item_p->cookie,
                                                    dump_cb);
        }
        prev_item_p = item_p;
        mlag_mac_sync_router_mac_db_next_record(
            prev_item_p, &item_p);
    }

    mlag_mac_sync_router_mac_db_get_free_pool_count(&count);
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "\n number free pools: %d\n", count);
    }
    else {
        dump_cb("\n number free pools: %d\n", count);
    }


bail:
    return err;
}

/**
 * This function prints router mac db free pools
 *
 * @return 0  operation completes successfully, otherwise ERROR
 *
 */
int
mlag_mac_sync_router_mac_db_get_free_pool_count(uint32_t * count)
{
    int err = 0;

    CHECK_ROUTER_DB_INIT_DONE;

    *count = cl_pool_count(&db_pool);
bail:
    return err;
}

