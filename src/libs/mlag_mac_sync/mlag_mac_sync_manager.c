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
#define MLAG_MAC_SYNC_MANAGER_C

#include <errno.h>
#include <complib/cl_init.h>
#include <complib/cl_mem.h>
#include <complib/cl_timer.h>
#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_events.h"
#include "mac_sync_events.h"
#include "mlag_common.h"
#include "health_manager.h"
#include "lib_ctrl_learn.h"
#include "mlag_api_defs.h"
#include "mlag_mac_sync_manager.h"
#include "mlag_mac_sync_peer_manager.h"
#include "mlag_mac_sync_master_logic.h"
#include "mlag_mac_sync_flush_fsm.h"
#include "mlag_mac_sync_router_mac_db.h"
#include "mlag_master_election.h"
#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_mac_sync_dispatcher.h"
#include "mlag_manager.h"

#undef  __MODULE__
#define __MODULE__ MLAG_MAC_SYNC_MANAGER

/************************************************
 *  Local Defines
 ***********************************************/

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/
static int
rcv_msg_handler(uint8_t *payload_data);
static int
net_order_msg_handler(uint8_t *data, int oper);

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static int is_started = 0;
static int is_inited = 0;
static struct mac_sync_counters counters;
static enum master_election_switch_status current_switch_status;

static handler_command_t mac_sync_ibc_msgs[] = {
    {MLAG_MAC_SYNC_ALL_FDB_GET_EVENT,     "FDB get event", rcv_msg_handler,
     net_order_msg_handler},
    {MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT,  "FDB export event", rcv_msg_handler,
     net_order_msg_handler},
    {MLAG_MAC_SYNC_SYNC_FINISH_EVENT,     "Mac sync Finish", rcv_msg_handler,
     net_order_msg_handler},
    {MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT, "Mac sync Master Done to peer",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT,   "Mac sync Local learn",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_LOCAL_AGED_EVENT,      "Mac sync Local age",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT,  "Mac sync Global learn",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_GLOBAL_AGED_EVENT,     "Mac sync Global age",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT, "Mac sync Global rejected",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT,
     "Mac Sync Start Global flush from peer",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT,
     "Mac Sync Start Global flush from master",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT,
     "Mac Sync Start Global flush ACK from peer",
     rcv_msg_handler, net_order_msg_handler},

    {0, "", NULL, NULL}
};

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/
static int mlag_mac_sync_print_counters(void (*dump_cb)(const char *, ...));

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
mlag_mac_sync_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    mlag_mac_sync_peer_mngr_log_verbosity_set(verbosity);
    mlag_mac_sync_master_logic_log_verbosity_set(verbosity);
    mlag_mac_sync_flush_fsm_log_verbosity_set(verbosity);
    mlag_mac_sync_router_mac_db_log_verbosity_set(verbosity);
}

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_mac_sync_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits mlag mac sync module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_init(insert_to_command_db_func insert_msgs)
{
    int err = 0;

    if (is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "mac sync init called twice\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "mac_sync init\n" );

    is_started = 0;
    is_inited = 1;
    current_switch_status = DEFAULT_SWITCH_STATUS;

    err = insert_msgs(mac_sync_ibc_msgs);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_mac_sync_master_logic_init();
    MLAG_BAIL_ERROR(err);

    err = mlag_mac_sync_peer_mngr_init();
    MLAG_BAIL_ERROR(err);

    /* Clear counters on init */
    mlag_mac_sync_counters_clear();

bail:
    return err;
}

/**
 *  This function de-inits mlag mac sync module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_deinit(void)
{
    int err = 0;

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "mac sync deinit called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "mac sync_deinit\n");

    is_started = 0;
    is_inited = 0;

bail:
    return err;
}

/**
 *  This function starts mlag mac sync module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_start(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "mac sync start called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "mac_sync start\n" );

    is_started = 1;

    /* Clear counters on start */
    mlag_mac_sync_counters_clear();

    MAC_SYNC_INC_CNT(MAC_SYNC_START_EVENTS_RCVD);

    current_switch_status = DEFAULT_SWITCH_STATUS;

    mlag_mac_sync_peer_mngr_start(NULL);

bail:
    return err;
}

/**
 *  This function stops mlag mac sync module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    MLAG_LOG(MLAG_LOG_NOTICE, "mac_sync stop\n" );

    MAC_SYNC_INC_CNT(MAC_SYNC_STOP_EVENTS_RCVD);

    if (!is_started) {
        goto bail;
    }

    is_started = 0;

    mlag_mac_sync_peer_mngr_stop(NULL);

    if (current_switch_status == MASTER) {
        err = mlag_mac_sync_master_logic_stop(NULL);
        MLAG_BAIL_ERROR(err);
    }
    current_switch_status = DEFAULT_SWITCH_STATUS;

bail:
    return err;
}

/**
 *  This function handles ipl port set event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_ipl_port_set(struct ipl_port_set_event_data *data)
{
    return mlag_mac_sync_peer_mngr_ipl_port_set(data);
}

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_sync_start(struct sync_event_data *data)
{
    return mlag_mac_sync_master_logic_sync_start(data);
}

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_sync_finish(struct sync_event_data *data)
{
    return mlag_mac_sync_master_logic_sync_finish(data);
}

/**
 *  This function handles sync done event to peer
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_done_to_peer( struct sync_event_data * data)
{
    return mlag_mac_sync_peer_mngr_sync_done((void *)data);
}

/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_local_learn(void *data)
{
    return mlag_mac_sync_master_logic_local_learn((void *)data);
}

/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_global_learn(void *data)
{
    int err = 0;

    err = mlag_mac_sync_peer_mngr_global_learned(data, 1);
    if (err == -EXFULL) {
        err = 0;
    }
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_local_age(void *data)
{
    return mlag_mac_sync_master_logic_local_aged((void *)data);
}

/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_global_age(void *data)
{
    return mlag_mac_sync_peer_mngr_global_aged((void *)data);
}

/**
 *  This function handles
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_global_reject(void *data)
{
    MLAG_LOG(MLAG_LOG_NOTICE, "Global reject in Peer Manager IBC flow :\n");
    return mlag_mac_sync_peer_mngr_global_reject((void *)data);
}


/*
 *  This function returns whether port is non mlag port
 *
 * @param[in] ifindex_num -ifindex
 *
 * @return 0 when successful, otherwise ERROR
 */
int
PORT_MNGR_IS_NON_MLAG_PORT(int ifindex_num)
{
    if ((ifindex_num > FIRST_MLAG_IFINDEX)
        && (ifindex_num <= LAST_MLAG_IFINDEX)) {
        return false;
    }
    else {
        return true;
    }
}

/*
 *  This function is called to handle IBC message
 *
 * @param[in] payload_data - message body
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
rcv_msg_handler(uint8_t *data)
{
    int err = 0;
    ASSERT(data);
    struct recv_payload_data *payload_data = (struct recv_payload_data*) data;
    uint8_t *msg_data = NULL;
    uint16_t opcode;

    if (payload_data->payload_len[0]) {
        msg_data = payload_data->payload[0];
    }
    else if (payload_data->jumbo_payload_len) {
        msg_data = payload_data->jumbo_payload;
    }

    opcode = *(uint16_t*)msg_data;

    MLAG_LOG(MLAG_LOG_INFO, "TCP message %d received\n", opcode);

    switch (opcode) {
    case MLAG_MAC_SYNC_ALL_FDB_GET_EVENT:
        err = mlag_mac_sync_fdb_get_event(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT:
        err = mlag_mac_sync_fdb_export_event(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_SYNC_FINISH_EVENT:
        err = mlag_mac_sync_sync_finish((struct sync_event_data *)msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT:
        err = mlag_mac_sync_done_to_peer((struct sync_event_data *)msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    /* LOCAL, GLOBAL MAC SYNC EVENTS*/
    case MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT:
        err = mlag_mac_sync_local_learn(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_LOCAL_AGED_EVENT:
        err = mlag_mac_sync_local_age(
            (struct mac_sync_age_event_data *)msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT:
        err = mlag_mac_sync_global_learn( msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_GLOBAL_AGED_EVENT:
        err = mlag_mac_sync_global_age(
            (struct mac_sync_age_event_data *)msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT:
        err = mlag_mac_sync_global_reject(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    /* Global flush related IBC messages*/
    case MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT:
        err = mlag_mac_sync_master_logic_flush_start(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT:
        err = mlag_mac_sync_peer_mngr_flush_start(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    case MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT:
        err = mlag_mac_sync_master_logic_flush_ack(msg_data);
        MLAG_BAIL_ERROR(err);
        break;
    default:
        /* Unknown opcode */
        break;
    }

bail:
    return err;
}

/*
 *  This function is called to handle network order for IBC message
 *
 * @param[in] payload_data - message body
 * @param[in] oper - on send or receive
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
net_order_msg_handler(uint8_t *data, int oper)
{
    int err = 0;
    uint16_t opcode;
    ASSERT(data);
    if (oper == MESSAGE_SENDING) {
        opcode = *(uint16_t*)data;
        *(uint16_t*)data = htons(opcode);
    }
    else {
        opcode = ntohs(*(uint16_t*)data);
        *(uint16_t*)data = opcode;
    }

    switch (opcode) {
    case MLAG_MAC_SYNC_ALL_FDB_GET_EVENT:
        break;
    case MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT:
        break;
    case MLAG_MAC_SYNC_SYNC_FINISH_EVENT:
        break;
    case MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT:
        break;
    /* LOCAL, GLOBAL MAC SYNC EVENTS*/
    case MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT:
        break;
    case MLAG_MAC_SYNC_LOCAL_AGED_EVENT:
        break;
    case MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT:
        break;
    case MLAG_MAC_SYNC_GLOBAL_AGED_EVENT:
        break;
    case MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT:
        break;
    /* Global flush related IBC messages*/
    case MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT:
        break;
    case MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT:
        break;
    case MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT:
        break;
    default:
        /* Unknown opcode */
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown event %d in network ordering handler\n",
                 opcode);
        break;
    }
bail:
    return err;
}

/**
 *  This function handles peer status change event
 *
 *  @param data - event data
 *
 * @return int as error code.
 */
int
mlag_mac_sync_peer_status_change(struct peer_state_change_data *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "mlag_mac_sync_peer_status_change event ignored before start: mlag_id=%d, state=%d\n",
                 data->mlag_id, data->state);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Peer %d changed status to %s\n",
             data->mlag_id, health_state_str[data->state]);

    if (!((data->mlag_id >= 0) && (data->mlag_id < MLAG_MAX_PEERS))) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameters in peer status change event: mlag_id=%d\n",
                            data->mlag_id);
    }

    MAC_SYNC_INC_CNT(MAC_SYNC_PEER_STATE_CHANGE_EVENTS_RCVD);

    if ((current_switch_status == MASTER) &&
        ((data->state == HEALTH_PEER_DOWN) ||
         (data->state == HEALTH_PEER_COMM_DOWN))) {
        err = mlag_mac_sync_master_logic_peer_status_change(data);
        MLAG_BAIL_ERROR_MSG(err,
                            "mlag_mac_sync_master_logic_peer_status_change returned error %d\n",
                            err);
    }
    else if ((current_switch_status == SLAVE) &&
             ((data->state == HEALTH_PEER_DOWN) ||
              (data->state == HEALTH_PEER_COMM_DOWN))) {
        err = mlag_mac_sync_peer_mngr_stop(NULL);
        MLAG_BAIL_ERROR_MSG(err,
                            "mlag_mac_sync_peer_mngr_stop returned error %d\n",
                            err);
    }

bail:
    return err;
}

/**
 *  This function handles peer start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_start_event(struct peer_state_change_data *data)
{
    int err = 0;
    struct sync_event_data sync_done;

    ASSERT(data);

    if (!is_started) {
        goto bail;
    }

    if (current_switch_status == STANDALONE) {
        /* Ignore this event in Standalone mode */
        /* Send MLAG_PEER_SYNC_DONE to MLag protocol manager */
        sync_done.peer_id = data->mlag_id;
        sync_done.state = TRUE;
        sync_done.sync_type = MAC_PEER_SYNC;
        err =
            send_system_event(MLAG_PEER_SYNC_DONE, &sync_done,
                              sizeof(sync_done));
        MLAG_BAIL_ERROR(err);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "mlag_mac_sync_peer_start_event: mlag_id=%d\n",
             data->mlag_id);

    MAC_SYNC_INC_CNT(MAC_SYNC_PEER_START_EVENTS_RCVD);

    err = mlag_mac_sync_peer_mngr_peer_start(data);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}


/**
 *  This function handles fdb get event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_fdb_get_event(uint8_t *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "fdb get event accepted before start\n");
    }

    MAC_SYNC_INC_CNT(MAC_SYNC_FDB_GET_EVENTS_RCVD);

    err = mlag_mac_sync_master_logic_fdb_export(data);
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_NOTICE, "processed successfully FDB get to master\n");

bail:
    return err;
}

/**
 *  This function handles fdb export event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_fdb_export_event(uint8_t *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "fdb export event accepted before start\n");
    }

    MAC_SYNC_INC_CNT(MAC_SYNC_FDB_EXPORT_EVENTS_RCVD);

    err = mlag_mac_sync_peer_mngr_process_fdb_export(data);
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_NOTICE, "peer manager processed fdb export\n");

bail:
    return err;
}

/**
 *  This function handles peer enable event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_enable(struct peer_state_change_data *data)
{
    int err = 0;
    if (current_switch_status == MASTER) {
        err = mlag_mac_sync_master_logic_peer_enable(data);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 *  This function handles switch state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_switch_status_change(struct switch_status_change_event_data *data)
{
    int err = 0;
    enum master_election_switch_status previous_switch_status =
        current_switch_status;
    char *current_status_str;
    char *previous_status_str;
    char *mac_sync_current_status_str;

    ASSERT(data);

    err = mlag_master_election_get_switch_status_str(data->current_status,
                                                     &current_status_str);
    MLAG_BAIL_ERROR(err);
    err = mlag_master_election_get_switch_status_str(data->previous_status,
                                                     &previous_status_str);
    MLAG_BAIL_ERROR(err);
    err = mlag_master_election_get_switch_status_str(current_switch_status,
                                                     &mac_sync_current_status_str);
    MLAG_BAIL_ERROR(err);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "mlag_mac_sync_switch_status_change event accepted before start: msg_current_status=%s, msg_previous_status=%s, mac_sync_current_status=%s\n",
                 current_status_str, previous_status_str,
                 mac_sync_current_status_str);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "mlag_mac_sync_switch_status_change event: msg_current_status=%s, msg_previous_status=%s, mac_sync_current_status=%s\n",
             current_status_str, previous_status_str,
             mac_sync_current_status_str);

    MAC_SYNC_INC_CNT(MAC_SYNC_SWITCH_STATUS_CHANGE_EVENTS_RCVD);

    current_switch_status = data->current_status;

    if ((previous_switch_status == DEFAULT_SWITCH_STATUS) ||
        (previous_switch_status == STANDALONE)) {
        /* Switch status changed from default status
         * that means after start.
         * Start TCP server and Master Logic  for MASTER only.
         * For SLAVE TCP client will be opened upon peer start event.
         * For STANDALONE no need to open TCP connection.
         */
        if (current_switch_status == MASTER) {
            err = mlag_mac_sync_master_logic_start(NULL);
            MLAG_BAIL_ERROR(err);
        }
    }
    else {
        if (current_switch_status != previous_switch_status) {
            /* Switch status changed from non-default status
             * that means during real work, not after start.
             * Call stop of this module and after that start it again. */

            /* Need previous status to handle stop procedure in right way */
            current_switch_status = previous_switch_status;

            err = mlag_mac_sync_stop(NULL);
            MLAG_BAIL_ERROR(err);

            err = mlag_mac_sync_start(NULL);
            MLAG_BAIL_ERROR(err);

            current_switch_status = data->current_status;
        }
    }
bail:
    return err;
}

/**
 *  This function prints module's internal attributes
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "=================\nMAC Sync dump\n=================\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "is_inited=%d, is_started=%d\n",
                 is_inited, is_started);
    }
    else {
        dump_cb("=================\nMAC Sync dump\n=================\n");
        dump_cb("is_inited=%d, is_started=%d\n",
                is_inited, is_started);
    }

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "mac sync dump called before init\n");
    }

    mlag_mac_sync_print_counters(dump_cb);

    mlag_mac_sync_peer_mngr_print(dump_cb);

    /* if (current_switch_status == MASTER) {
         mlag_mac_sync_master_logic_print(dump_cb, 1);
       } */


bail:
    return err;
}

/**
 *  This function clears mac sync module counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_counters_clear(void)
{
    SAFE_MEMSET(&counters, 0);
    return 0;
}


/**
 * Populates mac sync relevant counters.
 *
 * @param[out] counters - Mlag protocol counters. Pointer to an already
 *                        allocated memory structure. Only the fdb
 *                        counters are relevant to this function
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 */
int
mlag_mac_sync_counters_get(struct mlag_counters *mlag_counters)
{
    if (current_switch_status == MASTER) {
        mlag_counters->tx_fdb_sync = counters.counter[MASTER_TX];
        mlag_counters->rx_fdb_sync = counters.counter[MASTER_RX];
    }
    else if (current_switch_status == SLAVE) {
        mlag_counters->tx_fdb_sync = counters.counter[SLAVE_TX];
        mlag_counters->rx_fdb_sync = counters.counter[SLAVE_RX];
    }
    else {
        mlag_counters->tx_fdb_sync = 0;
        mlag_counters->rx_fdb_sync = 0;
    }

    return 0;
}

/**
 *  This function prints mac sync module counters to log
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
mlag_mac_sync_print_counters(void (*dump_cb)(const char *, ...))
{
    int i;
    for (i = 0; i < MAC_SYNC_LAST_COUNTER; i++) {
        if (dump_cb == NULL) {
            MLAG_LOG(MLAG_LOG_NOTICE, "%s = %d\n",
                     mac_sync_counters_str[i],
                     counters.counter[i]);
        }
        else {
            dump_cb("%s = %d\n",
                    mac_sync_counters_str[i],
                    counters.counter[i]);
        }
    }
    return 0;
}

/**
 *  This function increments mac sync module's counter
 *
 * @return 0 when successful, otherwise ERROR
 */
void
mlag_mac_sync_inc_cnt(enum mac_sync_counts cnt)
{
    MAC_SYNC_INC_CNT(cnt);
}


/**
 *  This function increments mac sync module's counter
 *
 * @return 0 when successful, otherwise ERROR
 */
void
mlag_mac_sync_inc_cnt_num(enum mac_sync_counts cnt, int num)
{
    MAC_SYNC_INC_CNT_NUM(cnt, num);
}



/**
 *  This function returns switch current status
 *
 * @return switch current status
 */
enum master_election_switch_status
mlag_mac_sync_get_current_status(void)
{
    return current_switch_status;
}
