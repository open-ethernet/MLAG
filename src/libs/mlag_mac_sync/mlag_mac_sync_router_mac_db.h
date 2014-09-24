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

#ifndef MLAG_MAC_SYNC_ROUTER_MAC_DB_H_
#define MLAG_MAC_SYNC_ROUTER_MAC_DB_H_

/************************************************
 *  Defines
 ***********************************************/
#define MAX_ROUTER_MAC_ENTRIES  400
/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
enum router_mac_action {
    ADD_ROUTER_MAC = 0,
    REMOVE_ROUTER_MAC
};


struct router_db_entry {
    cl_map_item_t map_item;             /* map overhead */
    unsigned short vid;                 /*  Vlan id */
    struct ether_addr mac_addr;         /* mac address */
    int sync_status;                    /* synchronization status */
    enum router_mac_action last_action; /* add or remove action */
    void                   *cookie;     /* pointer to master logic instance */
};


typedef int (*router_mac_cookie_init_deinit_func)( enum cookie_op,
                                                   void** cookie);


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
mlag_mac_sync_router_mac_db_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function registers init_deinit callback function
 * @param[in]  func - callback to init_deinit function
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_router_mac_db_register_cookie_func(
    router_mac_cookie_init_deinit_func func);
/**
 *  This function initializes router mac database
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_router_mac_db_init(void);

/**
 *  This function registers de-initializes  router mac database
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_router_mac_db_deinit(void);


/**
 *  This function configures router mac
 * @param[in]  data - pointer to the structure
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_router_mac_db_conf(uint8_t *data);

/**
 *  This function adds router mac to the database and launches mac sync process
 *  for this mac
 * @param[in]  mac    - mac address
 * @param[in]  vid    - vlan id
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_router_mac_db_add( struct ether_addr mac,
                                     unsigned short vid);

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
int mlag_mac_sync_router_mac_db_remove( struct ether_addr mac,
                                        unsigned short vid);

/**
 *  This function fetches all router macs from the router mac db
 *  and sends Local learn message for them to the master
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_router_mac_db_sync_router_macs(void);


/**
 *  This function passes all router macs from the router mac db
 *  and sets sync flag to "not sync"
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_router_mac_db_set_not_sync(void);

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
int mlag_mac_sync_router_mac_db_sync_response( struct ether_addr mac,
                                               unsigned short vid,
                                               enum router_mac_action action);


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
int mlag_mac_sync_router_mac_db_get( struct ether_addr mac, unsigned short vid, struct router_db_entry **old_entry
                                     );

/**
 * This function returns the pointer for first entry in DB
 *
 * @param[out] item_pp - pointer to entry
 *
 * @return 0 operation completes successfully
 * @return   -EPERM  in case general error
 */
int mlag_mac_sync_router_mac_db_first_record(struct router_db_entry **item_pp);

/**
 * This function returns the pointer to next entry that comes
 * after the given one
 * @param[in]  input_item_p - input entry
 * @param[out] return_item_pp - pointer to entry
 *
 * @return 0  operation completes successfully
 * @return -EPERM   general error
 */
int mlag_mac_sync_router_mac_db_next_record(
    struct router_db_entry * input_item_p,
    struct router_db_entry **return_item_p);


/**
 * This function prints router mac db free pools
 *
 * @return 0  operation completes successfully, otherwise ERROR
 *
 */
int mlag_mac_sync_router_mac_db_get_free_pool_count(uint32_t * count);

/**
 * This function prints router mac database
 * @param[in]  dump_cb - callback to dump function
 * @return 0  operation completes successfully, otherwise ERROR
 *
 */
int mlag_mac_sync_router_mac_db_print(void (*dump_cb)(const char *, ...));



#endif
