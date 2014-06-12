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

#define MLAG_MANAGER_DB_C_

#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_defs.h>
#include <mlag_manager_db.h>
#include "mlag_manager.h"

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static struct mlag_manager_db mlag_db;
static cl_spinlock_t peer_lock;

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function inits mlag manager DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_db_init(void)
{
    cl_status_t cl_err;
    int err = 0;
    int idx = 0;

    SAFE_MEMSET(mlag_db.peer_info, 0);
    SAFE_MEMSET(&mlag_db.local_ip, 0);
    SAFE_MEMSET(&mlag_db.local_sys_id, 0);
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        mlag_db.peer_info[idx].mlag_id = INVALID_MLAG_PEER_ID;
    }
    mlag_db.reload_delay = RELOAD_DELAY_DEFAULT_MSEC;

    cl_err = cl_plock_init(&(mlag_db.peer_db_mutex));
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR(err);
    }

    cl_err = cl_spinlock_init(&peer_lock);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 *  This function deinits mlag manager DB
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_manager_db_deinit(void)
{
    int err = 0;

    cl_plock_destroy(&(mlag_db.peer_db_mutex));
    cl_spinlock_destroy(&peer_lock);

    return err;
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_manager_db_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

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
int
mlag_manager_db_peer_add(uint32_t peer_ip, int *local_index)
{
    int err = 0;
    int idx = 0;
    int vacant = -1;

    cl_plock_excl_acquire(&(mlag_db.peer_db_mutex));
    /* local peer is always index 0 */
    if (mlag_db.local_ip == peer_ip) {
        goto bail;
    }
    for (idx = 1; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].peer_ip == peer_ip) {
            goto bail;
        }
        if ((vacant == -1) && (mlag_db.peer_info[idx].peer_ip == 0)) {
            vacant = idx;
        }
    }
    idx = vacant;
    if (idx == -1) {
        err = -ENOSPC;
        goto bail;
    }
    mlag_db.peer_info[idx].peer_ip = peer_ip;
bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    if (local_index != NULL) {
        *local_index = idx;
    }
    return err;
}

/**
 *  This function deletes a certain peer IP, local peer not
 *  included
 *
 * @param[in] peer_ip - Peer IP
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if peer ip not found
 */
int
mlag_manager_db_peer_delete(uint32_t peer_ip)
{
    int i;
    int err = -ENOENT;

    cl_plock_excl_acquire(&(mlag_db.peer_db_mutex));

    for (i = 1; i < MLAG_MAX_PEERS; i++) {
        if (mlag_db.peer_info[i].peer_ip == peer_ip) {
            SAFE_MEMSET(&(mlag_db.peer_info[i]), 0);
            err = 0;
            goto bail;
        }
    }

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function return peer local index for certain peer IP
 *
 * @param[in] peer_ip - peer IP
 * @param[out] local_index - local index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if peer ip not found
 */
int
mlag_manager_db_peer_local_index_get(uint32_t peer_ip, int *local_index)
{
    int err = -ENOENT;
    int idx = 0;

    ASSERT(local_index != NULL);
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    if (mlag_db.local_ip == peer_ip) {
        *local_index = 0;
        err = 0;
        goto bail;
    }
    for (idx = 1; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].peer_ip == peer_ip) {
            *local_index = idx;
            err = 0;
            goto bail;
        }
    }
bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

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
int
mlag_manager_db_mlag_peer_id_set(uint32_t peer_ip, int mlag_id)
{
    int idx;
    int err = -ENOENT;

    cl_plock_excl_acquire(&(mlag_db.peer_db_mutex));
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].peer_ip == peer_ip) {
            mlag_db.peer_info[idx].mlag_id = mlag_id;
            err = 0;
            goto bail;
        }
    }

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

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
int
mlag_manager_db_mlag_peer_id_get(uint32_t peer_ip, int *mlag_id)
{
    int idx;
    int err = -ENOENT;
    ASSERT(mlag_id != NULL);
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].peer_ip == peer_ip) {
            *mlag_id = mlag_db.peer_info[idx].mlag_id;
            err = 0;
            goto bail;
        }
    }

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function return peer IP for certain peer ID, local peer
 *  included.
 *
 * @param[in] local_index - local peer index
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_db_peer_ip_from_local_index_get(int local_index,
                                             uint32_t *peer_ip)
{
    int err = 0;

    ASSERT(peer_ip != NULL);
    ASSERT(local_index < MLAG_MAX_PEERS);
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    *peer_ip = mlag_db.peer_info[local_index].peer_ip;
    cl_plock_release(&(mlag_db.peer_db_mutex));

bail:
    return err;
}

/**
 *  This function return peer IP for certain peer  mlag ID
 *
 * @param[in] mlag_id - mlag ID assigned for this peer
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_db_peer_ip_from_mlag_id_get(int mlag_id, uint32_t *peer_ip)
{
    int err = -ENOENT;
    int idx;

    ASSERT(peer_ip != NULL);
    ASSERT(mlag_id < MLAG_MAX_PEERS);
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].mlag_id == mlag_id) {
            *peer_ip = mlag_db.peer_info[idx].peer_ip;
            err = 0;
            goto bail;
        }
    }

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function sets local IP in DB, use the value 0 (zero)
 *  for delete
 *
 * @param[in] peer_ip - peer IP
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_db_local_ip_set(uint32_t peer_ip)
{
    int err = 0;
    cl_plock_excl_acquire(&(mlag_db.peer_db_mutex));
    mlag_db.local_ip = peer_ip;
    mlag_db.peer_info[0].peer_ip = peer_ip;
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function sets local system ID in DB, use the value 0 (zero)
 *  for delete
 *
 * @param[in] system_id - system ID
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_db_local_system_id_set(unsigned long long system_id)
{
    int err = 0;
    cl_plock_excl_acquire(&(mlag_db.peer_db_mutex));
    mlag_db.local_sys_id = system_id;
    mlag_db.peer_info[0].sys_id = system_id;
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function gets local system ID in DB
 *
 * @return local system ID
 */
unsigned long long
mlag_manager_db_local_system_id_get(void)
{
    unsigned long long id;
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    id = mlag_db.local_sys_id;
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return id;
}

/**
 *  This function sets  system ID for local peer index in DB
 *
 *  @param[in] local_index - Peer local index
 *  @param[in] sys_id - system ID
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_db_peer_system_id_set(int local_index, unsigned long long sys_id)
{
    int err = 0;

    ASSERT(local_index < MLAG_MAX_PEERS);
    cl_plock_excl_acquire(&(mlag_db.peer_db_mutex));
    mlag_db.peer_info[local_index].sys_id = sys_id;
    cl_plock_release(&(mlag_db.peer_db_mutex));
bail:
    return err;
}

/**
 *  This function returns local index per given system ID
 *
 *  @param[in] sys_id - system ID
 *  @param[out] local_index - Peer local index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if not found
 */
int
mlag_manager_db_local_index_from_system_id_get(unsigned long long sys_id,
                                               int *local_index)
{
    int idx;
    int err = -ENOENT;
    ASSERT(local_index != NULL);
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].sys_id == sys_id) {
            *local_index = idx;
            err = 0;
            goto bail;
        }
    }

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function gets  system ID for peer_id in DB
 *
 *  @param[in] local_index - Peer local index
 *
 * @return local system ID
 */
unsigned long long
mlag_manager_db_peer_system_id_get(int local_index)
{
    unsigned long long val = 0;
    if (local_index >= MLAG_MAX_PEERS) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "System ID get with local index %d which is over the max (%d)\n",
                 local_index, MLAG_MAX_PEERS);
        goto bail;
    }
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    val = mlag_db.peer_info[local_index].sys_id;
    cl_plock_release(&(mlag_db.peer_db_mutex));
bail:
    return val;
}

/**
 *   This function returns local index per given mlag ID
 *
 *  @param[in] mlag_id - mlag peer index
 *  @param[out] local_index - Peer local index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if not found
 */
int
mlag_manager_db_local_index_from_mlag_id_get(int mlag_id, int *local_index)
{
    int idx;
    int err = -ENOENT;
    ASSERT(local_index != NULL);
    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    for (idx = 0; idx < MLAG_MAX_PEERS; idx++) {
        if (mlag_db.peer_info[idx].mlag_id == mlag_id) {
            *local_index = idx;
            err = 0;
            goto bail;
        }
    }

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function returns mlag index for given local index
 *
 *  @param[in] local_index - Peer local index
 *  @param[out] mlag_id - mlag peer index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT if not found
 */
int
mlag_manager_db_mlag_id_from_local_index_get(int local_index, int *mlag_id)
{
    int err = 0;

    ASSERT(mlag_id != NULL);
    ASSERT(local_index < MLAG_MAX_PEERS);

    cl_plock_acquire(&(mlag_db.peer_db_mutex));
    if (mlag_db.peer_info[local_index].mlag_id == INVALID_MLAG_PEER_ID) {
        err = -ENOENT;
        MLAG_BAIL_CHECK_NO_MSG(err);
    }
    *mlag_id = mlag_db.peer_info[local_index].mlag_id;

bail:
    cl_plock_release(&(mlag_db.peer_db_mutex));
    return err;
}

/**
 *  This function return if peer ID is local
 *
 * @param[in] local_index - peer local index
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_manager_db_is_local(int local_index)
{
    int err = 0;
    ASSERT(local_index < MLAG_MAX_PEERS);
    return (local_index == LOCAL_PEER_ID);
bail:
    return err;
}

/**
 *  This function returns local peer ID
 *
 *  @return int  - peer_id
 */
int
mlag_manager_db_local_peer_id_get(void)
{
    return LOCAL_PEER_ID;
}

/**
 *  This function sets reload delay
 *
 * @param[in] reload_delay - reload delay value
 *
 * @return void
 */
void
mlag_manager_db_reload_delay_set(unsigned int reload_delay)
{
    mlag_db.reload_delay = reload_delay;
}

/**
 *  This function returns reload delay value
 *
 *  @return unsigned long  - reload delay
 */
unsigned int
mlag_manager_db_reload_delay_get(void)
{
    return mlag_db.reload_delay;
}

/**
 *  This function locks mlag manager DB peer Add /Del operations
 *
 *  @return void
 */
void
mlag_manager_db_lock(void)
{
    cl_spinlock_acquire(&peer_lock);
}

/**
 *  This function unlocks mlag manager DB peer Add /Del operations
 *
 *  @return void
 */
void
mlag_manager_db_unlock(void)
{
    cl_spinlock_release(&peer_lock);
}
