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

#ifndef MLAG_COMM_LAYER_WRAPPER_H_
#define MLAG_COMM_LAYER_WRAPPER_H_

#include <complib/cl_timer.h>
#include <pthread.h>

/************************************************
 *  Defines
 ***********************************************/
#define INVALID_TCP_SERVER_ID 0xFFFF

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
enum message_operation {
    MESSAGE_SENDING = 0,
    MESSAGE_RECEIVE = 1
};

enum message_originator {
    PEER_MANAGER = 0,
    MASTER_LOGIC = 1
};

enum stop_cause {
    PEER_STOP = 0,
    SERVER_STOP = 1
};

enum comm_fd_operation {
    COMM_FD_ADD = 0,
    COMM_FD_DEL = 1
};

enum comm_socket_protection {
    NO_SOCKET_PROTECTION = 0,
    SOCKET_PROTECTION = 1
};

typedef int (*rcv_msg_handler_t)(struct addr_info *conn_info,
                                 struct recv_payload_data *payload_data);

typedef int (*net_order_msg_handler_t)(uint8_t *payload,
                                       enum message_operation oper);

typedef int (*add_fd_handler_t)(handle_t new_handle, int state);

enum wrapper_coounters {
    TX_CNT = 0,
    RX_CNT,
    WRAPPER_LAST_COUNTER
};

struct wrapper_counters {
    int counter[WRAPPER_LAST_COUNTER];
};

struct mlag_comm_layer_wrapper_data {
    int is_started;
    enum master_election_switch_status current_switch_status;
    uint32_t dest_ip_addr;
    uint16_t tcp_port;
    uint16_t tcp_server_id;
    handle_t tcp_sock_handle[MLAG_MAX_PEERS];
    rcv_msg_handler_t rcv_msg_handler;
    add_fd_handler_t add_fd_handler;
    net_order_msg_handler_t net_order_msg_handler;
    struct wrapper_counters counters;
    cl_timer_t reconnect_timer;
    int reconnect_timer_msec;
    int reconnect_timer_started;
    enum comm_socket_protection protect_socket;
    /*cl_plock_t socket_mutex;*/
    pthread_mutex_t socket_mutex;
};

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function initializes comm layer wrapper structure
 *
 * @param comm_layer_data - module specific data
 * @param tcp_port - port
 * @param rcv_msg_handler - handler called upon receive message
 * @param add_fd_handler - handler called to add/delete fd to/from FD_SET
 *                         of select function in wrapper thread procedure
 * @param protect_socket - indicate if a mutex to protect the socket is required
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_init(
    struct mlag_comm_layer_wrapper_data *comm_layer_data, uint16_t tcp_port,
    rcv_msg_handler_t rcv_msg_handler,
    net_order_msg_handler_t net_order_msg_handler,
    add_fd_handler_t add_fd_handler,
    enum comm_socket_protection protect_socket);

/**
 *  This function deinitializes comm layer wrapper structure
 *
 * Currently it only destroies the mutex if it was initialize
 *
 * @param comm_layer_data - module specific data
 * @return 0
 */
int
mlag_comm_layer_wrapper_deinit(
    struct mlag_comm_layer_wrapper_data *comm_layer_data);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_comm_layer_wrapper_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_comm_layer_wrapper_log_verbosity_get(void);

/**
 *  This function starts TCP server for Master and
 *  opens TCP client to establish TCP connection with server
 *  for Slave.
 *
 * @param comm_layer_data - module specific data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_start(
    struct mlag_comm_layer_wrapper_data *comm_layer_data);

/**
 *  This function stops TCP session
 *
 * @param comm_layer_data - module specific data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_stop(
    struct mlag_comm_layer_wrapper_data *comm_layer_data,
    enum stop_cause cause, int peer_id);

/**
 *  This function is tcp server socket handle notification called
 *  from context of handling thread
 *
 * @param[in] new_handle - socket fd
 * @param[in] ipv4_addr - ip address of connected peer
 * @param[in] port - tcp port of connected peer
 * @param[in] data - specific comm layer wrapper data
 * @param[in,out] rc - return code defined connection status
 *                 (0 - connection established,
 *                  otherwise - failed to establish connection)
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_connection_notification(
    int new_handle, uint16_t port, uint32_t ipv4_addr, void *data, int *rc);

/**
 *  This function is receive messages handler
 *
 * @param[in] comm_layer_data - module specific data
 * @param[in] fd - socket fd
 * @param[in] data - message data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_message_dispatcher(int fd, void *data, char* msg_buf,
                                           int buf_size);

/**
 *  This function sends message to destination
 *
 * @param[in] comm_layer_data - module specific data
 * @param[in] opcode - message id
 * @param[in] payload - message data
 * @param[in] payload_len - message data length
 * @param[in] dest_peer_id - peer id to send message to
 *            used for messages originated by Master Logic only
 * @param[in] orig - message originator - master logic or peer manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_message_send(
    struct mlag_comm_layer_wrapper_data *comm_layer_data,
    enum mlag_events opcode, uint8_t* payload, uint32_t payload_len,
    uint8_t dest_peer_id, enum message_originator orig);

/**
 *  This function returns comm layer wrapper module counters
 *
 * @param[in] comm_layer_data - module specific data
 * @param[in] _counters - pointer to structure to return counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_counters_get(
    struct mlag_comm_layer_wrapper_data *comm_layer_data,
    struct wrapper_counters *_counters);

/**
 *  This function clears comm layer wrapper module counters
 *
 * @param[in] comm_layer_data - module specific data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_counters_clear(
    struct mlag_comm_layer_wrapper_data *comm_layer_data);

/**
 *  This function prints comm layer wrapper counters to log
 *
 * @param[in] comm_layer_data - module specific data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_print_counters(
    struct mlag_comm_layer_wrapper_data *comm_layer_data, void (*dump_cb)(
        const char *, ...));

/**
 *  This function writes given data to given file
 *
 * @param[in] file_name - name of file to be opened
 * @param[in] data - data to be written to file
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_write_to_proc_file(char *file_name, char *data);

/**
 *  This function handles reconnect tcp session event,
 *  called from context of handling thread
 *
 * @param[in] comm_layer_wrapper - specific wrapper data structure
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_reconnect(
    struct mlag_comm_layer_wrapper_data *comm_layer_wrapper);

#endif /* MLAG_COMM_LAYER_WRAPPER_H_ */
