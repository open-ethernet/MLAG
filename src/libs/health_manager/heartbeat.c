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
#include <complib/cl_mem.h>
#include <complib/cl_types.h>
#include <utils/mlag_defs.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include "heartbeat.h"

/************************************************
 *  Local Defines
 ***********************************************/
#undef  __MODULE__
#define __MODULE__ MLAG_HEALTH

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
struct peer_hb_info {
    struct heartbeat_peer_info peer_info;
    heartbeat_state_t peer_state;
    uint16_t sequence_num;
    uint8_t remote_defect;
    uint16_t last_rx_seq;
    unsigned long long last_rx_sys_id;
    uint16_t last_rx_tick;
    uint8_t consecutive_count;
    struct heartbeat_peer_stats peer_stats;
};

struct heartbeat_db_t {
    heartbeat_state_notifier_t state_notify_cb;
    heartbeat_msg_send_t send_cb;
    uint8_t local_defect;
    uint16_t ticks;
    struct peer_hb_info peer_data[MLAG_MAX_PEERS];
};

struct heartbeat_db_t *heartbeat_db;
static int started;

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/*
 *  This function dispatches KA interval set event
 *
 * @param[in] peer_id - peer id
 *
 * @return 0 if operation completes successfully.
 */
static int
send_heartbeat_message(int peer_id)
{
    int err = 0;
    static heartbeat_payload_t payload;

    if (started == FALSE) {
        goto bail;
    }

    payload.sequence = htons(heartbeat_db->peer_data[peer_id].sequence_num++);
    payload.local_defect = heartbeat_db->local_defect;
    payload.remote_defect = heartbeat_db->peer_data[peer_id].remote_defect;

    if (heartbeat_db->send_cb != NULL) {
        err = heartbeat_db->send_cb(peer_id, &payload);
        if (err) {
            heartbeat_db->peer_data[peer_id].peer_stats.tx_errors++;
            err = 0;
        }
        else {
            heartbeat_db->peer_data[peer_id].peer_stats.tx_heartbeat++;
        }
    }

bail:
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
heartbeat_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function inits the health module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_init(void)
{
    int err = 0;

    heartbeat_db =
        (struct heartbeat_db_t *)cl_malloc(sizeof(struct heartbeat_db_t));
    if (heartbeat_db == NULL) {
        err = -ENOMEM;
        MLAG_BAIL_NOCHECK_MSG(
            "Failed to allocate memory for heartbeat module\n");
    }

    SAFE_MEMSET(heartbeat_db, 0);

    started = FALSE;

    heartbeat_db->send_cb = NULL;
    heartbeat_db->state_notify_cb = NULL;
    heartbeat_db->ticks = 0;
    heartbeat_db->local_defect = 0;

bail:
    return err;
}

/**
 *  This function inits the health module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_deinit(void)
{
    int err = 0;
    cl_status_t cl_err;
    cl_err = cl_free(heartbeat_db);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_NOCHECK_MSG("Failed to free memory\n");
    }

bail:
    return err;
}

/**
 *  This function starts the heartbeat module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_start(void)
{
    int err = 0;
    int peer_id;

    err = heartbeat_local_defect_set(FALSE);
    MLAG_BAIL_ERROR(err);

    /* Update all the peers as down */
    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        if (heartbeat_db->peer_data[peer_id].peer_state !=
            HEARTBEAT_INACTIVE) {
            SAFE_MEMSET(&(heartbeat_db->peer_data[peer_id]), 0);
            heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_DOWN;
            heartbeat_db->peer_data[peer_id].remote_defect = TRUE;
        }
    }
    started = TRUE;

bail:
    return err;
}

/**
 *  This function stops the heartbeat module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_stop(void)
{
    int err = 0;
    int peer_id;


    err = heartbeat_local_defect_set(TRUE);
    MLAG_BAIL_ERROR(err);

    /* Update all the peers as down */
    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        /* Send faulty state to remotes before closing */
        if (heartbeat_db->peer_data[peer_id].peer_state !=
            HEARTBEAT_INACTIVE) {
            err = send_heartbeat_message(peer_id);
            if (err) {
                MLAG_LOG(MLAG_LOG_NOTICE,
                         "Failed in sending last heartbeat packet to peer [%d]\n",
                         peer_id);
                err = 0;
            }
            SAFE_MEMSET(&(heartbeat_db->peer_data[peer_id]), 0);
            heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_DOWN;
            heartbeat_db->peer_data[peer_id].remote_defect = TRUE;
        }
    }

bail:
    started = FALSE;
    return err;
}

/**
 *  This function adds a peer for heartbeat monitoring if
 *  started,  heartbeat starts sending & trying receive packets
 *  from peer. Peer is considered down when adding it.
 *
 * @param[in] peer_id - peer index
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_peer_add(int peer_id)
{
    int err = 0;
    ASSERT(peer_id < MLAG_MAX_PEERS);
    ASSERT(heartbeat_db->peer_data[peer_id].peer_state == HEARTBEAT_INACTIVE);

    SAFE_MEMSET(&(heartbeat_db->peer_data[peer_id]), 0);
    heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_DOWN;
    heartbeat_db->peer_data[peer_id].remote_defect = TRUE;

bail:
    return err;
}

/**
 *  This function stop peer heartbeat monitoring and removes
 *  peer record. no events will be sent for that peer after this
 *  function is called.
 *
 * @param[in] peer_id - peer index
 * @param[in] peer info - struct containing peer info
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_peer_remove(int peer_id)
{
    int err = 0;
    ASSERT(peer_id < MLAG_MAX_PEERS);
    heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_INACTIVE;
    SAFE_MEMSET(&heartbeat_db->peer_data[peer_id].peer_stats, 0);
bail:
    return err;
}

/**
 *  This function registers a callback for state change
 *  notification for all configured peers.
 *
 * @param[in] notify_cb - notification callback
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_register_state_cb(heartbeat_state_notifier_t notify_cb)
{
    int err = 0;

    heartbeat_db->state_notify_cb = notify_cb;

    return err;
}

/**
 *  This function registers a callback for sending heartbeat
 *  datagrams
 *
 * @param[in] send_cb - message send callback
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_register_send_cb(heartbeat_msg_send_t send_cb)
{
    int err = 0;

    heartbeat_db->send_cb = send_cb;

    return err;
}

/**
 *  This function sets local defect field value, this will cause
 *  remote side to notify our peer is down
 *
 * @param[in] local_state - local state indication
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_local_defect_set(uint8_t local_defect)
{
    int err = 0;

    heartbeat_db->local_defect = local_defect;

    return err;
}

/*
 *  This function implements sending HB message to all participating peers
 *
 * @return 0 if operation completes successfully.
 */
static int
send_to_peers(void)
{
    int err = 0;
    int peer_id;

    if (started == FALSE) {
        goto bail;
    }

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        if (heartbeat_db->peer_data[peer_id].peer_state !=
            HEARTBEAT_INACTIVE) {
            err = send_heartbeat_message(peer_id);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to send heartbeat to peer_id %d\n",
                                peer_id);
        }
    }

bail:
    return err;
}

/*
 *  This function handles stats increment
 *
 * @param[in] peer_id - peer id
 * @param[in] msg_data - received payload
 *
 * @return void
 */
static void
increment_stats_by_message(int peer_id, heartbeat_payload_t *msg_data)
{
    heartbeat_db->peer_data[peer_id].peer_stats.rx_heartbeat++;
    if (msg_data->local_defect) {
        heartbeat_db->peer_data[peer_id].peer_stats.local_defect++;
    }
    if (msg_data->remote_defect) {
        heartbeat_db->peer_data[peer_id].peer_stats.remote_defect++;
    }
}

/**
 *  This function hands a message from peer to the heartbeat
 *  module
 *
 *  @param[in] peer_id - peer id
 *  @param[in] payload - received payload
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_recv(int peer_id, heartbeat_payload_t *payload)
{
    int err = 0;

    ASSERT(peer_id < MLAG_MAX_PEERS);

    if (started == FALSE) {
        goto bail;
    }

    if (heartbeat_db->peer_data[peer_id].peer_state == HEARTBEAT_INACTIVE) {
        MLAG_LOG(MLAG_LOG_DEBUG, "Peer [%d] not active, message ignored\n",
                 peer_id);
        goto bail;
    }

    uint16_t rx_seq = ntohs(payload->sequence);
    MLAG_LOG(MLAG_LOG_DEBUG,
             "Heartbeat Peer [%d] seq [%u] remote [%u] local [%u]\n",
             peer_id, rx_seq, payload->remote_defect, payload->local_defect);

    increment_stats_by_message(peer_id, payload);

    if (heartbeat_db->peer_data[peer_id].peer_state == HEARTBEAT_DOWN) {
        if ((CALC_DISTANCE_U16(rx_seq,
                               heartbeat_db->peer_data[peer_id].last_rx_seq) ==
             1) &&
            (payload->remote_defect == 0)) {
            heartbeat_db->peer_data[peer_id].consecutive_count++;
        }
        else {
            heartbeat_db->peer_data[peer_id].consecutive_count = 1;
        }
        heartbeat_db->peer_data[peer_id].last_rx_seq = rx_seq;
        heartbeat_db->peer_data[peer_id].remote_defect = FALSE;
        SAFE_MEMCPY(&(heartbeat_db->peer_data[peer_id].last_rx_sys_id),
                    &(payload->system_id));
        heartbeat_db->peer_data[peer_id].last_rx_tick = heartbeat_db->ticks;

        MLAG_LOG(MLAG_LOG_DEBUG,
                 "Heartbeat Peer [%d] Down seq [%u] remote [%u] count [%u]\n",
                 peer_id, rx_seq, payload->remote_defect,
                 heartbeat_db->peer_data[peer_id].consecutive_count);

        if (heartbeat_db->peer_data[peer_id].consecutive_count ==
            HEARTBEAT_MESSAGE_THRESHOLD) {
            /* state change to Up */
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Peer %d changed state to heartbeat Up\n", peer_id);
            heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_UP;
            if (heartbeat_db->state_notify_cb) {
                heartbeat_db->state_notify_cb(peer_id,
                                              heartbeat_db->peer_data[peer_id].last_rx_sys_id,
                                              HEARTBEAT_UP);
            }
            heartbeat_db->peer_data[peer_id].consecutive_count = 0;
        }
    }
    else if (heartbeat_db->peer_data[peer_id].peer_state == HEARTBEAT_UP) {
        if ((payload->remote_defect != 0) || (payload->local_defect != 0) ||
            (heartbeat_db->peer_data[peer_id].last_rx_sys_id !=
             payload->system_id)) {
            heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_DOWN;
            heartbeat_db->peer_data[peer_id].remote_defect = TRUE;
            if (heartbeat_db->state_notify_cb) {
                heartbeat_db->state_notify_cb(peer_id,
                                              heartbeat_db->peer_data[peer_id].last_rx_sys_id,
                                              HEARTBEAT_DOWN);
            }
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Peer %d move to Down, remote side reported error\n",
                     peer_id);
        }

        if (CALC_DISTANCE_U16(rx_seq,
                              heartbeat_db->peer_data[peer_id].last_rx_seq) !=
            1) {
            /* we missed a packet */
            heartbeat_db->peer_data[peer_id].peer_stats.rx_miss++;
        }
        else {
            heartbeat_db->peer_data[peer_id].last_rx_tick =
                heartbeat_db->ticks;
        }
        heartbeat_db->peer_data[peer_id].last_rx_seq = rx_seq;
    }

bail:
    return err;
}

/*
 *  This function checks if timeout has occured for any peer
 *  timeout is defined when more than HEARTBEAT_MESSAGE_THRESHOLD
 *  time ticks has passed since last message received
 *
 * @param[in] buffer - event data
 *
 * @return 0 if operation completes successfully.
 */
static int
check_for_timeout(void)
{
    int err = 0;
    int peer_id;

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        if (heartbeat_db->peer_data[peer_id].peer_state == HEARTBEAT_UP) {
            if (CALC_DISTANCE_U16(heartbeat_db->ticks,
                                  heartbeat_db->peer_data[peer_id].last_rx_tick)
                >
                HEARTBEAT_MESSAGE_THRESHOLD) {
                /* Timeout on non-receiving message from peer */
                MLAG_LOG(MLAG_LOG_DEBUG,
                         "heartbeat_db->ticks [%u] last_rx_tick [%u], timeout\n", heartbeat_db->ticks,
                         heartbeat_db->peer_data[peer_id].last_rx_tick);
                MLAG_LOG(MLAG_LOG_NOTICE,
                         "Peer %d Move to heartbeat down, timeout\n", peer_id);
                heartbeat_db->peer_data[peer_id].peer_state = HEARTBEAT_DOWN;
                heartbeat_db->peer_data[peer_id].remote_defect = TRUE;
                heartbeat_db->peer_data[peer_id].peer_stats.rx_timeout++;
                if (heartbeat_db->state_notify_cb) {
                    heartbeat_db->state_notify_cb(peer_id,
                                                  heartbeat_db->peer_data[
                                                      peer_id].last_rx_sys_id,
                                                  HEARTBEAT_DOWN);
                }
            }
        }
    }

    return err;
}

/**
 *  This function is a time tick for the heartbeat module
 *  this may generate state change event for one or more peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_tick(void)
{
    int err;
    /* Happens every time unit */
    heartbeat_db->ticks++;

    /* check TO */
    err = check_for_timeout();
    MLAG_BAIL_ERROR_MSG(err, "Failed on checking peers timeout\n");

    /* Send to active peers */
    err = send_to_peers();
    MLAG_BAIL_ERROR_MSG(err, "Failed on sending to peers\n");

bail:
    return err;
}

/**
 *  This function returns states for all peers
 *
 * @param[out] states - heartbeat states for all peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_peer_states_get(heartbeat_state_t states[MLAG_MAX_PEERS])
{
    int err = 0, peer_id;

    for (peer_id = 0; peer_id < MLAG_MAX_PEERS; peer_id++) {
        states[peer_id] = heartbeat_db->peer_data[peer_id].peer_state;
    }

    return err;
}

/**
 *  This function returns statistics for the designated peer
 *
 * @param[in] peer_id - peer id
 * @param[out] stats - heartbeat statistics for all peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_peer_stats_get(int peer_id, heartbeat_peer_stats_t *stats)
{
    int err = 0;
    ASSERT(peer_id < MLAG_MAX_PEERS);
    SAFE_MEMCPY(stats, &heartbeat_db->peer_data[peer_id].peer_stats);
bail:
    return err;
}

/**
 *  This function clears statistics for the designated peer
 *
 * @param[in] peer_id - peer id
 *
 * @return 0 when successful, otherwise ERROR
 */
int
heartbeat_peer_stats_clear(int peer_id)
{
    int err = 0;
    ASSERT(peer_id < MLAG_MAX_PEERS);
    SAFE_MEMSET(&heartbeat_db->peer_data[peer_id].peer_stats, 0);
bail:
    return err;
}
