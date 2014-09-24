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
#include <complib/cl_timer.h>
#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_defs.h"
#include "mlag_events.h"
#include "lib_commu.h"
#include "mlag_common.h"
#include "mlag_master_election.h"
#include "mlag_manager_db.h"
#include "mlag_comm_layer_wrapper.h"

#undef  __MODULE__
#define __MODULE__ MLAG_COMM_LAYER_WRAPPER

#define MLAG_COMM_LAYER_WRAPPER_C

/************************************************
 *  Local Defines
 ***********************************************/
#define WRAPPER_INC_CNT(wrapper, cnt){if (cnt < WRAPPER_LAST_COUNTER) {       \
                                          (wrapper->counters.counter[cnt])++; \
                                      } }
#define WRAPPER_CLR_CNT(wrapper, cnt){if (cnt < WRAPPER_LAST_COUNTER) {       \
                                          wrapper->counters.counter[cnt] = 0; \
                                      } }
#define DEFAULT_RECONNECT_MSEC 500

#define SOCKET_LOCK(comm_layer_data)                                  \
    if (comm_layer_data->protect_socket == SOCKET_PROTECTION) {       \
    	 err = pthread_mutex_lock(&(comm_layer_data->socket_mutex));  \
         MLAG_BAIL_ERROR_MSG(err, "Failed to lock socket mutex\n");   \
        /*cl_plock_acquire(&(comm_layer_data->socket_mutex));*/       \
    }

#define SOCKET_UNLOCK(comm_layer_data)                                \
    if (comm_layer_data->protect_socket == SOCKET_PROTECTION) {       \
        err = pthread_mutex_unlock(&(comm_layer_data->socket_mutex)); \
        MLAG_BAIL_ERROR_MSG(err, "Failed to unlock socket mutex\n");  \
        /*cl_plock_release(&(comm_layer_data->socket_mutex));*/       \
    }

/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static char *wrapper_counters_str[WRAPPER_LAST_COUNTER] = {
    "MSG_TX_CNT",
    "MSG_RX_CNT"
};

/************************************************
 *  Local function declarations
 ***********************************************/
static int message_send(
    struct mlag_comm_layer_wrapper_data *comm_layer_data,
    enum mlag_events opcode, uint8_t dest_peer_id, uint8_t *payload,
    uint32_t payload_len);
static int tcp_conn_stop(
    struct mlag_comm_layer_wrapper_data *comm_layer_data, int peer_id);
static int tcp_conn_start(
    struct mlag_comm_layer_wrapper_data *comm_layer_data);
static int tcp_server_handle_notification(
    handle_t new_handle, struct addr_info peer_addr_st, void *data, int rc);
static void mlag_comm_layer_wrapper_reconnect_timer_cb(
    void* data);

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
mlag_comm_layer_wrapper_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_comm_layer_wrapper_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

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
    struct mlag_comm_layer_wrapper_data *comm_layer_data,
    uint16_t tcp_port, rcv_msg_handler_t rcv_msg_handler,
    net_order_msg_handler_t net_order_msg_handler,
    add_fd_handler_t add_fd_handler,
    enum comm_socket_protection protect_socket)
{
    int err = 0;
    int i;

    /* Initialize structure of comm_layer_wrapper */
    comm_layer_data->is_started = 0;
    comm_layer_data->current_switch_status = DEFAULT_SWITCH_STATUS;
    comm_layer_data->rcv_msg_handler = rcv_msg_handler;
    comm_layer_data->add_fd_handler = add_fd_handler;
    comm_layer_data->net_order_msg_handler = net_order_msg_handler;
    comm_layer_data->tcp_port = tcp_port;
    comm_layer_data->tcp_server_id = INVALID_TCP_SERVER_ID;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        comm_layer_data->tcp_sock_handle[i] = 0;
    }
    mlag_comm_layer_wrapper_counters_clear(comm_layer_data);
    comm_layer_data->reconnect_timer_msec = DEFAULT_RECONNECT_MSEC;
    comm_layer_data->reconnect_timer_started = 0;
    comm_layer_data->protect_socket = protect_socket;
    if (comm_layer_data->protect_socket == SOCKET_PROTECTION) {
        if (pthread_mutex_init(&(comm_layer_data->socket_mutex), NULL) < 0) {
            err = -EIO;
            MLAG_BAIL_ERROR_MSG(err, "Failed to init mutex\n");
        }
    }

bail:
    return err;
}

/**
 *  This function deinitializes comm layer wrapper structure
 *
 * Currently it only destroies the mutex if it was initialize
 *
 * @param comm_layer_data - module specific data
 *
 * @return 0
 */
int
mlag_comm_layer_wrapper_deinit(
    struct mlag_comm_layer_wrapper_data *comm_layer_data)
{
    int err = 0;

    if (comm_layer_data->protect_socket == SOCKET_PROTECTION) {
        /*cl_plock_destroy(&(comm_layer_data->socket_mutex));*/
        err = pthread_mutex_destroy(&(comm_layer_data->socket_mutex));
        if (err != 0) {
            MLAG_LOG(MLAG_LOG_ERROR,
                    "Failed to destroy socket mutex, err(%d): %s\n",
                    errno, strerror(errno));
        }
    }

    return err;
}

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
    struct mlag_comm_layer_wrapper_data *comm_layer_data)
{
    int err = 0;
    struct session_params params;
    struct register_to_new_handle clbk_st;
    struct connection_info conn_info;
    uint32_t peer_ip_addr = 0;
    cl_status_t cl_err;

    clbk_st.clbk_notify_func = tcp_server_handle_notification;
    clbk_st.data = (void*)comm_layer_data;

    if (comm_layer_data->current_switch_status == MASTER) {
        /* Open TCP server for L3 interface management messages */
        params.s_ipv4_addr = INADDR_ANY;
        params.port = htons(comm_layer_data->tcp_port);
        params.msg_type = 0;

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "TCP server session start for tcp port %d\n",
                 comm_layer_data->tcp_port);

        SOCKET_LOCK(comm_layer_data);
        err = comm_lib_tcp_server_session_start(params, &clbk_st,
                                                &comm_layer_data->tcp_server_id);
        SOCKET_UNLOCK(comm_layer_data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to start TCP server session on tcp port %d and dest ip 0x%08x\n",
                            htons(comm_layer_data->tcp_port),
                            htonl(comm_layer_data->dest_ip_addr));
    }
    else if (comm_layer_data->current_switch_status == SLAVE) {
        /* Retrieve Master peer ip addr by Master peer id */
        err = mlag_manager_db_peer_ip_from_mlag_id_get(
            MASTER_PEER_ID, &peer_ip_addr);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get ip address of master peer from mlag manager database\n");

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "TCP client session start with master for ip address 0x%08x and tcp port %d\n",
                 peer_ip_addr, comm_layer_data->tcp_port);

        /* Open TCP client */
        conn_info.s_ipv4_addr = 0;
        conn_info.d_ipv4_addr = htonl(peer_ip_addr);
        conn_info.d_port = htons(comm_layer_data->tcp_port);
        conn_info.msg_type = 0;

        SOCKET_LOCK(comm_layer_data);
        err =
            comm_lib_tcp_client_non_blocking_start(&conn_info, &clbk_st, NULL);
        SOCKET_UNLOCK(comm_layer_data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to start TCP client session with tcp port %d dest ip 0x%08x\n",
                            htons(comm_layer_data->tcp_port),
                            htonl(comm_layer_data->dest_ip_addr));
    }

    mlag_comm_layer_wrapper_counters_clear(comm_layer_data);

    cl_err = cl_timer_init(&comm_layer_data->reconnect_timer,
                           mlag_comm_layer_wrapper_reconnect_timer_cb,
                           comm_layer_data);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init reconnect timer\n");
    }
    comm_layer_data->reconnect_timer_started = 0;
    comm_layer_data->is_started = 1;

bail:
    return err;
}

/*
 *  This function stops single tcp connection
 *
 * @param[in] comm_layer_data - module specific data
 * @param[in] peer_id - peer id with which connection established
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
tcp_conn_stop(struct mlag_comm_layer_wrapper_data *comm_layer_data,
              int peer_id)
{
    int err = 0;
    int is_locked = 0;

    if (comm_layer_data->tcp_sock_handle[peer_id]) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "TCP client session stop for handle %d\n",
                 comm_layer_data->tcp_sock_handle[peer_id]);

        SOCKET_LOCK(comm_layer_data);
        is_locked = 1;

        err = comm_lib_tcp_peer_stop(
            comm_layer_data->tcp_sock_handle[peer_id]);
        MLAG_BAIL_ERROR_MSG(err, "Failed to stop TCP client session\n");

        /* Delete fd from message dispatcher */
        if (comm_layer_data->add_fd_handler) {
            comm_layer_data->add_fd_handler(
                comm_layer_data->tcp_sock_handle[peer_id], COMM_FD_DEL);
        }
        comm_layer_data->tcp_sock_handle[peer_id] = 0;

        SOCKET_UNLOCK(comm_layer_data);
        is_locked = 0;
    }

bail:
    if (is_locked) {
    	SOCKET_UNLOCK(comm_layer_data);
    }
    return err;
}

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
    enum stop_cause cause,
    int peer_id)
{
    int err = 0;
    int i;
    handle_t handle_array[MAX_CONNECTION_NUM];

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Communication layer wrapper stop for tcp port %d , role %d, cause %d\n",
             comm_layer_data->tcp_port,
             comm_layer_data->current_switch_status,
             cause);

    if (comm_layer_data->current_switch_status == MASTER) {
        if ((cause == SERVER_STOP) &&
            (comm_layer_data->tcp_server_id != INVALID_TCP_SERVER_ID)) {
            for (i = 0; i < MLAG_MAX_PEERS; i++) {
                if (comm_layer_data->tcp_sock_handle[i]) {
                    /* Delete fd from message dispatcher */
                    if (comm_layer_data->add_fd_handler) {
                        comm_layer_data->add_fd_handler(
                            comm_layer_data->tcp_sock_handle[i], COMM_FD_DEL);
                    }
                    comm_layer_data->tcp_sock_handle[i] = 0;
                }
            }
            SOCKET_LOCK(comm_layer_data);
            err = comm_lib_tcp_server_session_stop(
                comm_layer_data->tcp_server_id,
                handle_array,
                MAX_CONNECTION_NUM);
            SOCKET_UNLOCK(comm_layer_data);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed TCP server stop for server id %d, err=%d\n",
                                comm_layer_data->tcp_server_id, err);

            comm_layer_data->tcp_server_id = INVALID_TCP_SERVER_ID;
            comm_layer_data->is_started = 0;
        }
        else if (cause == PEER_STOP) {
            err = tcp_conn_stop(comm_layer_data, peer_id);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in peer stop on master, err=%d\n",
                                err);
        }
    }
    else {
        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            err = tcp_conn_stop(comm_layer_data, i);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in peer stop on slave, err=%d\n",
                                err);
        }
        if (comm_layer_data->is_started) {
            if (comm_layer_data->reconnect_timer_started) {
                cl_timer_stop(&comm_layer_data->reconnect_timer);
                comm_layer_data->reconnect_timer_started = 0;
            }
            cl_timer_destroy(&comm_layer_data->reconnect_timer);
        }
        comm_layer_data->is_started = 0;
    }

bail:
    return err;
}

/*
 *  This function starts single tcp connection
 *
 * @param[in] comm_layer_data - module specific data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
tcp_conn_start(struct mlag_comm_layer_wrapper_data *comm_layer_data)
{
    int err = 0;
    struct connection_info conn_info;
    struct register_to_new_handle clbk_st;

    conn_info.s_ipv4_addr = 0;
    conn_info.d_ipv4_addr = htonl(comm_layer_data->dest_ip_addr);
    conn_info.d_port = htons(comm_layer_data->tcp_port);
    conn_info.msg_type = 0;
    clbk_st.clbk_notify_func = tcp_server_handle_notification;
    clbk_st.data = (void*)comm_layer_data;

    /* Open TCP client */
    SOCKET_LOCK(comm_layer_data);
    err = comm_lib_tcp_client_non_blocking_start(&conn_info, &clbk_st, NULL);
    SOCKET_UNLOCK(comm_layer_data);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in start TCP client on tcp port %d and dest ip 0x%08x\n",
                        htons(comm_layer_data->tcp_port),
                        htonl(comm_layer_data->dest_ip_addr));

bail:
    return err;
}

/**
 *  This function is mlag comm layer interface peer messages handler
 *
 * @param[in] fd - socket fd
 * @param[in] data - message data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_message_dispatcher(int fd,
                                           void *data,
                                           char *msg_buf,
                                           int buf_size)
{
    int err = 0;
    UNUSED_PARAM(msg_buf);
    UNUSED_PARAM(buf_size);
    struct mlag_comm_layer_wrapper_data *comm_layer_data =
        (struct mlag_comm_layer_wrapper_data *) data;
    struct addr_info ad_info;
    struct recv_payload_data payload_data;
    int peer_id;
    int is_locked = 0;

    MLAG_LOG(MLAG_LOG_INFO,
             "Received message from communication library\n");

    memset(&payload_data, 0, sizeof(payload_data));
    memset(&ad_info, 0, sizeof(ad_info));

    SOCKET_LOCK(comm_layer_data);
    err = comm_lib_tcp_recv_blocking(fd, &ad_info, &payload_data, 1);
    SOCKET_UNLOCK(comm_layer_data);

    /* Check on connection failure */
    if (err) {
        if ((err == -ECONNRESET) || (err == -ENOTCONN) ||
        		(err == -ETIMEDOUT)) {
            /* Connection failure, ECONNRESET=104 ENOTCONN=107 ETIMEDOUT=110*/
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Connection failure on tcp receive blocking: handle %d, tcp port %d, ip address 0x%08x, err %d\n",
                     fd, ntohs(ad_info.port), ntohl(ad_info.ipv4_addr), err);

            /* Retrieve peer id by peer ip addr */
            err = mlag_manager_db_mlag_peer_id_get(
                ntohl(ad_info.ipv4_addr), &peer_id);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to get peer id by peer ip address 0x%08x from mlag manager database\n",
                                ntohl(ad_info.ipv4_addr));

            SOCKET_LOCK(comm_layer_data);
            is_locked = 1;

            err = comm_lib_tcp_peer_stop(fd);
            MLAG_BAIL_ERROR_MSG(err, "Failed to stop TCP session\n");

            comm_layer_data->tcp_sock_handle[peer_id] = 0;

            /* Delete fd from message dispatcher */
            if (comm_layer_data->add_fd_handler) {
                comm_layer_data->add_fd_handler(fd, COMM_FD_DEL);
            }

            SOCKET_UNLOCK(comm_layer_data);
            is_locked = 0;

            /* Try to re-connect from the Slave side */
            if (comm_layer_data->current_switch_status == SLAVE &&
            	err != -ETIMEDOUT) {
                tcp_conn_start(comm_layer_data);
            }
        }
        MLAG_BAIL_ERROR_MSG(
                err,
                "Failed in communication library TCP receive blocking: handle %d, tcp port %d, ip address 0x%08x, err %d\n",
                fd, ntohs(ad_info.port), ntohl(ad_info.ipv4_addr), err);
        goto bail;
    }

    /* Handle receive message */
    if (payload_data.msg_num_recv != 1) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "communication library TCP receive blocking returned number packets not equal to 1: handle %d tcp port %d ip address 0x%08x\n",
                 fd, ntohs(ad_info.port), ntohl(ad_info.ipv4_addr));
        goto bail;
    }

    if (payload_data.payload_len[0] > 0) {
        if (comm_layer_data->net_order_msg_handler) {
            comm_layer_data->net_order_msg_handler(
                (void*)payload_data.payload[0], MESSAGE_RECEIVE);
        }
    }

    if (payload_data.jumbo_payload_len > 0) {
        if (comm_layer_data->net_order_msg_handler) {
            comm_layer_data->net_order_msg_handler(
                (void*)payload_data.jumbo_payload, MESSAGE_RECEIVE);
        }
    }

    WRAPPER_INC_CNT(comm_layer_data, RX_CNT);

    if (comm_layer_data->rcv_msg_handler) {
        comm_layer_data->rcv_msg_handler(&ad_info, &payload_data);
    }

bail:
    if (is_locked) {
    	SOCKET_UNLOCK(comm_layer_data);
    }
    return 0;
}

/*
 *  This function is tcp server socket handle notification called
 *  upon connection established with client from context of comm library
 *
 * @param[in] new_handle - socket fd
 * @param[in] peer_addr_st - used to retrieve ip address of connected peer
 * @param[in] data - pointer to wrapper data structure
 * @param[in] rc - return code defined connection status
 *                 (0 - connection established,
 *                  otherwise - failed to establish connection)
 * @return 0 when successful, otherwise ERROR
 */
static int
tcp_server_handle_notification(handle_t new_handle,
                               struct addr_info peer_addr_st,
                               void *data,
                               int rc)
{
    int err = 0;
    struct tcp_conn_notification_event_data ev;

    ev.new_handle = new_handle;
    ev.port = peer_addr_st.port;
    ev.ipv4_addr = peer_addr_st.ipv4_addr;
    ev.data = data;
    ev.rc = rc;

    /* Send event to all dispatchers to change context.
       Only one thread dispatcher should handle this event. */
    err = send_system_event(MLAG_CONN_NOTIFY_EVENT, &ev, sizeof(ev));
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending connection notification event\n");

bail:
    return err;
}

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
    handle_t new_handle, uint16_t port,
    uint32_t ipv4_addr, void *data,
    int *rc)
{
    int err = 0;
    struct mlag_comm_layer_wrapper_data *comm_layer_data =
        (struct mlag_comm_layer_wrapper_data*) data;
    int peer_id;
    cl_status_t cl_err;

    if (comm_layer_data->reconnect_timer_started) {
        cl_timer_stop(&comm_layer_data->reconnect_timer);
        comm_layer_data->reconnect_timer_started = 0;
    }

    /* Check on wrapper stop */
    if ((comm_layer_data->is_started == 0) && (*rc == 0)) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "TCP connection established but wrapper on tcp port %d and ip address 0x%08x already stopped, delete new handle\n",
                 comm_layer_data->tcp_port,
                 comm_layer_data->dest_ip_addr);
        SOCKET_LOCK(comm_layer_data);
        err = comm_lib_tcp_peer_stop(new_handle);
        SOCKET_UNLOCK(comm_layer_data);
        MLAG_BAIL_ERROR(err);
        goto bail;
    }

    if ((comm_layer_data->is_started == 0) && (*rc)) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "TCP connection is not established and wrapper on port %d dest ip 0x%08x already stopped\n",
                 comm_layer_data->tcp_port,
                 comm_layer_data->dest_ip_addr);
        goto bail;
    }

    /* *rc != 0 if connection is not established */
    if (*rc) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Failed to establish connection on tcp port %d and ip address 0x%08x\n",
                 comm_layer_data->tcp_port,
                 comm_layer_data->dest_ip_addr);

        /* Start timer to reconnect next time */
        cl_err = cl_timer_start(&comm_layer_data->reconnect_timer,
                                comm_layer_data->reconnect_timer_msec);
        if (cl_err != CL_SUCCESS) {
            err = -EIO;
            MLAG_BAIL_ERROR(err);
        }
        comm_layer_data->reconnect_timer_started = 1;
    }
    else {
        /* Connection established */
        /* Retrieve peer id by peer ip addr */
        err = mlag_manager_db_mlag_peer_id_get(ntohl(ipv4_addr), &peer_id);
        if (err) {
            *rc = 1;
            /* Connection established via another ip (not peer ip).
             * Delete new handle and start reconnection timer.
             */
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "connection established via another l3 interface: stopping new handle %d\n",
                     new_handle);

            SOCKET_LOCK(comm_layer_data);
            err = comm_lib_tcp_peer_stop(new_handle);
            SOCKET_UNLOCK(comm_layer_data);
            MLAG_BAIL_ERROR(err);

            if (comm_layer_data->current_switch_status == SLAVE) {
                /* Start timer to reconnect upon it expiration */
                cl_err = cl_timer_start(&comm_layer_data->reconnect_timer,
                                        comm_layer_data->reconnect_timer_msec);
                if (cl_err != CL_SUCCESS) {
                    err = -EIO;
                    MLAG_BAIL_ERROR(err);
                }
                comm_layer_data->reconnect_timer_started = 1;
            }
            goto bail;
        }

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "TCP connection established on ip address 0x%08x, tcp port %d, peer_id %d, new_handle %d\n",
                 ntohl(ipv4_addr), ntohs(port), peer_id, new_handle);

        comm_layer_data->tcp_sock_handle[peer_id] = new_handle;

        /* Save ip adrress of accepted client to comm_layer_data wrapper */
        comm_layer_data->dest_ip_addr = ntohl(ipv4_addr);

        /* Add new fd to message dispatcher for receive messages handling */
        if (comm_layer_data->add_fd_handler) {
            comm_layer_data->add_fd_handler(new_handle, COMM_FD_ADD);
        }
    }

bail:
    return err;
}

/*
 *  This function sends message by using communication library
 *
 * @param[in] comm_layer_data - module specific data
 * @param[in] opcode - message id
 * @param[in] dest_peer_id - peer id to send message to
 * @param[in] payload - message data
 * @param[in] payload_len - message data length
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
message_send(struct mlag_comm_layer_wrapper_data *comm_layer_data,
             enum mlag_events opcode, uint8_t dest_peer_id,
             uint8_t *payload, uint32_t payload_len)
{
    int err = 0;
    uint32_t payload_len_sent = payload_len;
    handle_t conn_handle = -1;
    int is_locked = 0;

    MLAG_LOG(MLAG_LOG_INFO,
             "Send tcp message with opcode %d, destination peer id %d, length %d, handle %d\n",
             opcode, dest_peer_id, payload_len,
             comm_layer_data->tcp_sock_handle[dest_peer_id]);

    /* Set opcode to the message body */
    *((uint16_t*)payload) = (uint16_t)opcode;

    SOCKET_LOCK(comm_layer_data);
    is_locked = 1;

    conn_handle = comm_layer_data->tcp_sock_handle[dest_peer_id];

    /* Send via communication library interface */
    if (conn_handle) {
    	if (comm_layer_data->net_order_msg_handler) {
            comm_layer_data->net_order_msg_handler((void*)payload,
                                                   MESSAGE_SENDING);
        }

        err = comm_lib_tcp_send_blocking(
        		conn_handle, payload, &payload_len_sent);

        SOCKET_UNLOCK(comm_layer_data);
        is_locked = 0;

        /* Check on connection failure */
        if (err) {
            if ((err == -ECONNRESET) || (err == -EPIPE) ||
            		(err == -ETIMEDOUT)) {
                /* Connection failure, EPIPE=32 ECONNRESET=104 ETIMEDOUT=110 */
                MLAG_LOG(MLAG_LOG_NOTICE,
                         "Connection failure on tcp send blocking: handle %d, tcp port %d, ip address 0x%08x, err %d\n",
                         conn_handle, comm_layer_data->tcp_port,
                         comm_layer_data->dest_ip_addr, err);

                SOCKET_LOCK(comm_layer_data);
                is_locked = 1;

                err = comm_lib_tcp_peer_stop(conn_handle);
                MLAG_BAIL_ERROR_MSG(err, "Failed to stop TCP session\n");

                comm_layer_data->tcp_sock_handle[dest_peer_id] = 0;

                /* Delete fd from message dispatcher */
                if (comm_layer_data->add_fd_handler) {
                    comm_layer_data->add_fd_handler(conn_handle, COMM_FD_DEL);
                }

                SOCKET_UNLOCK(comm_layer_data);
                is_locked = 0;

                /* Try to re-connect from the Slave side */
                if (comm_layer_data->current_switch_status == SLAVE &&
                	err != -ETIMEDOUT) {
                	tcp_conn_start(comm_layer_data);
                }
            }
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to send tcp message with opcode %d, destination peer id %d, length %d, handle %d, err %d\n",
                                opcode, dest_peer_id, payload_len, conn_handle, err);
            goto bail;
        }

        /* return the pointer back to previous state */
        if (comm_layer_data->net_order_msg_handler) {
            comm_layer_data->net_order_msg_handler((void*)payload,
                                                   MESSAGE_RECEIVE);
        }

        WRAPPER_INC_CNT(comm_layer_data, TX_CNT);
    }
    else {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Failed to send message because of handle is zero, opcode %d destination peer id %d\n",
                 opcode, dest_peer_id);
        goto bail;
    }

bail:
	if (is_locked) {
		SOCKET_UNLOCK(comm_layer_data);
	}
    return err;
}

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
    enum mlag_events opcode, uint8_t* payload,
    uint32_t payload_len, uint8_t dest_peer_id,
    enum message_originator orig)
{
    int err = 0;
    struct mlag_master_election_status master_election_current_status;

    err = mlag_master_election_get_status(&master_election_current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get status from master election in comm layer wrapper send message\n");

    if (master_election_current_status.current_status == MASTER) {
        if (orig == PEER_MANAGER) {
            MLAG_LOG(MLAG_LOG_INFO,
                     "sending system event with opcode %d from peer manager on master\n",
                     opcode);

            err = send_system_event(opcode, payload, payload_len);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in sending peer manager system event on master\n");
        }
        else if (dest_peer_id == master_election_current_status.my_peer_id) {
            MLAG_LOG(MLAG_LOG_INFO,
                     "sending system event with opcode %d from master logic\n",
                     opcode);

            err = send_system_event(opcode, payload, payload_len);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in sending master logic system event\n");
        }
        else {
            err = message_send(comm_layer_data, opcode, dest_peer_id, payload,
                               payload_len);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in sending tcp message on master\n");
        }
    }
    else if (master_election_current_status.current_status == SLAVE) {
        err = message_send(comm_layer_data, opcode,
                           master_election_current_status.master_peer_id,
                           payload, payload_len);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending tcp message on slave\n");
    }
    else if (master_election_current_status.current_status == STANDALONE) {
        if (dest_peer_id == master_election_current_status.my_peer_id) {
            MLAG_LOG(MLAG_LOG_INFO,
                     "sending system event with opcode %d on standalone\n",
                     opcode);

            err = send_system_event(opcode, payload, payload_len);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in sending system event on standalone\n");
        }
        else {
            err = -ENOENT;
            MLAG_LOG(MLAG_LOG_NOTICE, "Standalone peer, can not send message\n");

        }
    }

bail:
    return err;
}

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
    struct wrapper_counters *_counters)
{
    SAFE_MEMCPY(_counters, &comm_layer_data->counters);
    return 0;
}

/**
 *  This function clears comm layer wrapper module counters
 *
 * @param[in] comm_layer_data - module specific data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_counters_clear(
    struct mlag_comm_layer_wrapper_data *comm_layer_data)
{
    SAFE_MEMSET(&comm_layer_data->counters, 0);
    return 0;
}

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
        const char *, ...))
{
    int i;
    for (i = 0; i < WRAPPER_LAST_COUNTER; i++) {
        if (dump_cb == NULL) {
            MLAG_LOG(MLAG_LOG_NOTICE, "%s = %d\n",
                     wrapper_counters_str[i],
                     comm_layer_data->counters.counter[i]);
        }
        else {
            dump_cb("%s = %d\n",
                    wrapper_counters_str[i],
                    comm_layer_data->counters.counter[i]);
        }
    }
    return 0;
}

/**
 *  This function writes given data to given file
 *
 * @param[in] file_name - name of file to be opened
 * @param[in] data - data to be written to file
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_write_to_proc_file(char *file_name, char *data)
{
    int err = 0;
    FILE *fp = NULL;

    ASSERT(file_name);
    ASSERT(data);

    fp = fopen(file_name, "w");
    if (!fp) {
        err = 1;
        MLAG_LOG(MLAG_LOG_ERROR, "can't open file %s, errno=%d", file_name,
                 errno);
        goto bail;
    }

    if (fputs(data, fp) < 0) {
        err = 1;
        MLAG_LOG(MLAG_LOG_ERROR, "couldn't write data %s to file %s", data,
                 file_name);
        goto bail;
    }

bail:
    if (fp) {
        fclose(fp);
    }
    return err;
}

/**
 *  This function handles timer event to reconnect tcp session
 *
 * @param[in] data - pointer to wrapper data structure
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
mlag_comm_layer_wrapper_reconnect_timer_cb(void* data)
{
    int err = 0;
    struct reconnect_event_data ev;

    ev.data = data;
    err = send_system_event(MLAG_RECONNECT_EVENT, &ev, sizeof(ev));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending reconnect event\n");

bail:
    return;
}

/**
 *  This function handles reconnect tcp session event,
 *  called from context of handling thread
 *
 * @param[in] comm_layer_wrapper - specific wrapper data structure
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_comm_layer_wrapper_reconnect(
    struct mlag_comm_layer_wrapper_data *comm_layer_data)
{
    int err = 0;

    /* Check on reconnection process completion */
    if ((comm_layer_data->is_started == 0) ||
        (comm_layer_data->reconnect_timer_started == 0)) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Reconnect on tcp port %d and destination ip address 0x%08x\n",
             comm_layer_data->tcp_port,
             comm_layer_data->dest_ip_addr);

    tcp_conn_start(comm_layer_data);

bail:
    return err;
}
