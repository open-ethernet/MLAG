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

#ifndef MLAG_MANAGER_DB_H_
#define MLAG_MANAGER_DB_H_

#include <complib/cl_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_spinlock.h>
#include <utils/mlag_log.h>

/************************************************
 *  Global Defines
 ***********************************************/
#define INVALID_MLAG_PEER_ID    255


#ifdef MLAG_MANAGER_DB_C_

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_MANAGER
/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

struct peer_info {
    uint32_t peer_ip;
    int mlag_id;
    int ipl_id;
    unsigned long long sys_id;
};

struct mlag_manager_db {
    cl_plock_t peer_db_mutex;
    struct peer_info peer_info[MLAG_MAX_PEERS];
    uint32_t local_ip;
    unsigned long long local_sys_id;
    unsigned int reload_delay;
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

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function inits mlag manager DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_manager_db_init(void);

/**
 *  This function deinits mlag manager DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_manager_db_deinit(void);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void mlag_manager_db_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function deletes a certain peer IP, local peer not
 *  included
 *
 * @param[in] peer_ip - Peer IP
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if peer ip not found
 */
int mlag_manager_db_peer_delete(uint32_t peer_ip);

/**
 *  This function adds a new peer, indexed by its IP
 *
 * @param[in] peer_ip - Peer IP
 * @param[out] local_index - local index assigned to this peer.
 * If NULL is given than it is ignored
 *
 * @return 0 if operation completes successfully.
 * @return -ENOSPC if peer db is full
 */
int mlag_manager_db_peer_add(uint32_t peer_ip, int *local_index);

/**
 *  This function return peer local index for certain peer IP
 *
 * @param[in] peer_ip - peer IP
 * @param[out] local_index - local index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if peer ip not found
 */
int mlag_manager_db_peer_local_index_get(uint32_t peer_ip, int *local_index);

/**
 *  This function returns local index per given mlag ID
 *
 *  @param[in] mlag_id - mlag peer index
 *  @param[out] local_index - Peer local index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if not found
 */
int mlag_manager_db_local_index_from_mlag_id_get(int mlag_id,
                                                 int *local_index);

/**
 *  This function returns mlag index for given local index
 *
 *  @param[in] local_index - Peer local index
 *  @param[out] mlag_id - mlag peer index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if not found
 */
int mlag_manager_db_mlag_id_from_local_index_get(int local_index,
                                                 int *mlag_id);

/**
 *  This function sets mlag ID for certain peer IP, local peer
 *  included.
 *
 * @param[in] peer_ip - peer IP
 * @param[in] mlag_id - mlag ID assigned for this peer
 *
 * @return 0 is operation successful
 * @return -ENOENT if peer ip not found
 */
int mlag_manager_db_mlag_peer_id_set(uint32_t peer_ip, int mlag_id);

/**
 *  This function return mlag ID for certain peer IP, local peer
 *  included.
 *
 * @param[in] peer_ip - peer IP
 * @param[out] mlag_id - mlag ID assigned for this peer
 *
 * @return 0 is operation successful
 * @return -ENOENT if peer ip not found
 */
int mlag_manager_db_mlag_peer_id_get(uint32_t peer_ip, int *mlag_id);

/**
 *  This function return peer IP for certain peer ID, local peer
 *  included.
 *
 * @param[in] local_index - local peer index
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_peer_ip_from_local_index_get(int local_index,
                                                 uint32_t *peer_ip);

/**
 *  This function return peer IP for certain peer  mlag ID
 *
 * @param[in] mlag_id - mlag ID assigned for this peer
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_peer_ip_from_mlag_id_get(int mlag_id, uint32_t *peer_ip);

/**
 *  This function sets local IP in DB, use the value 0 (zero)
 *  for delete
 *
 * @param[in] peer_ip - peer IP
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_local_ip_set(uint32_t peer_ip);

/**
 *  This function sets local system ID in DB, use the value 0 (zero)
 *  for delete
 *
 * @param[in] system_id - system ID
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_local_system_id_set(unsigned long long system_id);

/**
 *  This function gets local system ID in DB
 *
 * @return local system ID
 */
unsigned long long mlag_manager_db_local_system_id_get(void);

/**
 *  This function sets  system ID for local peer index in DB
 *
 *  @param[in] local_index - Peer local index
 *  @param[in] sys_id - system ID
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_peer_system_id_set(int local_index,
                                       unsigned long long sys_id);

/**
 *  This function returns local index per given system ID
 *
 *  @param[in] sys_id - system ID
 *  @param[out] local_index - Peer local index
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_local_index_from_system_id_get(unsigned long long sys_id,
                                                   int *local_index);

/**
 *  This function gets  system ID for peer_id in DB
 *
 * @return local system ID
 */
unsigned long long mlag_manager_db_peer_system_id_get(int peer_id);

/**
 *  This function return if peer ID is local
 *
 * @param[in] local_index - peer local index
 *
 * @return 0 if operation completes successfully.
 */
int mlag_manager_db_is_local(int local_index);

/**
 *  This function returns local peer ID
 *
 *  @return int  - peer_id
 */
int mlag_manager_db_local_peer_id_get(void);

/**
 *  This function sets reload delay
 *
 * @param[in] reload_delay - reload delay value
 *
 * @return void
 */
void mlag_manager_db_reload_delay_set(unsigned int reload_delay);

/**
 *  This function returns reload delay value
 *
 *  @return unsigned long  - reload delay
 */
unsigned int mlag_manager_db_reload_delay_get(void);

/**
 *  This function locks mlag manager DB peer Add /Del operations
 *
 *  @return void
 */
void mlag_manager_db_lock(void);

/**
 *  This function unlocks mlag manager DB peer Add /Del operations
 *
 *  @return void
 */
void mlag_manager_db_unlock(void);

#endif /* MLAG_MANAGER_DB_H_ */
