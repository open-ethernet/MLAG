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

#ifndef MLAG_HEARTBEAT_H_
#define MLAG_HEARTBEAT_H_


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <utils/mlag_defs.h>
#include <utils/mlag_log.h>

/************************************************
 *  Defines
 ***********************************************/

#define DEFAULT_HEARTBEAT_MSEC  1000
#define HEARTBEAT_MESSAGE_THRESHOLD 3

/************************************************
 *  Macros
 ***********************************************/
#define CALC_DISTANCE_U16(a, b) (((a) - (b)) % UINT16_MAX)
#define PORT_STATE_UP   1

/************************************************
 *  Type definitions
 ***********************************************/

typedef struct __attribute__((__packed__)) heartbeat_payload {
    unsigned long long system_id;
    uint16_t sequence;
    uint8_t local_defect;
    uint8_t remote_defect;
} heartbeat_payload_t;

typedef enum heartbeat_state {
    HEARTBEAT_INACTIVE = 0,
    HEARTBEAT_DOWN = 1,
    HEARTBEAT_UP = 2,
}heartbeat_state_t;

struct heartbeat_peer_info {
    int peer_id;
};

typedef struct heartbeat_peer_stats {
    uint64_t rx_heartbeat;
    uint64_t tx_heartbeat;
    uint64_t tx_errors;
    uint64_t rx_miss;
    uint64_t rx_timeout;
    uint64_t remote_defect;
    uint64_t local_defect;
}heartbeat_peer_stats_t;

/**
 * Callbcak for state changes
 */
typedef void (*heartbeat_state_notifier_t)( int peer_id,
                                            unsigned long long system_id,
                                            heartbeat_state_t heartbeat_state );

/**
 * Callback for packet sending
 * expected to return 0 on success
 */
typedef int (*heartbeat_msg_send_t) ( int peer_id,
                                      heartbeat_payload_t *payload);
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
void heartbeat_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits the heartbeat module
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_init(void);

/**
 *  This function inits the heartbeat module
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_deinit(void);

/**
 *  This function starts the heartbeat module
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_start(void);

/**
 *  This function stops the heartbeat module
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_stop(void);

/**
 *  This function adds a peer for heartbeat monitoring if
 *  started,  heartbeat starts sending & trying receive packets
 *  from peer. Peer is considered down when adding it.
 *
 * @param[in] peer_id - peer index
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_peer_add(int peer_id);

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
int heartbeat_peer_remove(int peer_id);


/**
 *  This function is a time tick for the heartbeat module
 *  this may generate state change event for one or more peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_tick(void);

/**
 *  This function hands a message from peer to the heartbeat
 *  module
 *
 *  @param[in] peer_id - peer id
 *  @param[in] payload - received payload
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_recv(int peer_id, heartbeat_payload_t *payload);

/**
 *  This function sets local defect field value, this will cause
 *  remote side to notify our peer is down
 *
 * @param[in] local_state - local state indication
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_local_defect_set(uint8_t local_defect);

/**
 *  This function registers a callback for state change
 *  notification for all configured peers.
 *
 * @param[in] notify_cb - notification callback
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_register_state_cb(heartbeat_state_notifier_t notify_cb);

/**
 *  This function registers a callback for sending heartbeat
 *  datagrams
 *
 * @param[in] send_cb - message send callback
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_register_send_cb(heartbeat_msg_send_t send_cb);

/**
 *  This function returns states for all peers
 *
 * @param[out] states - heartbeat states for all peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_peer_states_get(heartbeat_state_t states[MLAG_MAX_PEERS]);

/**
 *  This function returns statistics for the designated peer
 *
 * @param[in] peer_id - peer id
 * @param[out] stats - heartbeat statistics for all peers
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_peer_stats_get(int peer_id, heartbeat_peer_stats_t *stats);

/**
 *  This function clears statistics for the designated peer
 *
 * @param[in] peer_id - peer id
 *
 * @return 0 when successful, otherwise ERROR
 */
int heartbeat_peer_stats_clear(int peer_id);

#endif /* MLAG_HEARTBEAT_H_ */
