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
#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_events.h"
#include "mlag_common.h"
#include "health_manager.h"
#include "mlnx_lib/lib_commu.h"

#undef  __MODULE__
#define __MODULE__ MLAG_L3_INTERFACE_MANAGER

#define MLAG_L3_INTERFACE_MANAGER_C

#include "mlag_api_defs.h"
#include "mlag_l3_interface_manager.h"
#include "mlag_l3_interface_peer.h"
#include "mlag_l3_interface_master_logic.h"
#include "mlag_master_election.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_manager.h"

/************************************************
 *  Local Defines
 ***********************************************/

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/
static int rcv_msg_handler(uint8_t *data);
static int net_order_msg_handler(uint8_t *data, int oper);

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static int is_started = 0;
static int is_inited = 0;
static struct mlag_l3_interface_counters counters;
static enum master_election_switch_status current_switch_status;

static handler_command_t mlag_l3_interface_ibc_msgs[] = {
    {MLAG_L3_SYNC_START_EVENT, "L3 sync start event",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_L3_SYNC_FINISH_EVENT, "L3 sync finish event",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT, "L3 master sync done event",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT,
     "Vlan local state change event from peer",
     rcv_msg_handler, net_order_msg_handler},
    {MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
     "Vlan global state change event",
     rcv_msg_handler, net_order_msg_handler},
    {0, "", NULL, NULL}
};

/************************************************
 *  Local function declarations
 ***********************************************/
static int
mlag_l3_interface_print_counters(void (*dump_cb)(const char *, ...));

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
mlag_l3_interface_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    mlag_l3_interface_peer_log_verbosity_set(verbosity);
    mlag_l3_interface_master_logic_log_verbosity_set(verbosity);
}

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_l3_interface_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function initializes mlag l3 interface module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_init(insert_to_command_db_func insert_msgs)
{
    int err = 0;

    if (is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "l3 interface init called twice\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface init\n");

    is_started = 0;
    is_inited = 1;
    current_switch_status = DEFAULT_SWITCH_STATUS;

    err = insert_msgs(mlag_l3_interface_ibc_msgs);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_l3_interface_peer_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed in peer init, err=%d\n", err);

    err = mlag_l3_interface_master_logic_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed in master logic init, err=%d\n", err);

    /* Clear counters */
    mlag_l3_interface_counters_clear();

bail:
    return err;
}

/**
 *  This function de-inits mlag l3 interface module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_deinit(void)
{
    int err = 0;

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "l3 interface deinit called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface deinit\n");

    is_inited = 0;

bail:
    return err;
}

/**
 *  This function starts mlag l3 interface module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_start(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "l3 interface start called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface start\n");

    is_started = 1;
    current_switch_status = DEFAULT_SWITCH_STATUS;

    /* Clear counters on start */
    mlag_l3_interface_counters_clear();

    L3_INTERFACE_INC_CNT(L3_INTERFACE_START_EVENTS_RCVD);

    err = mlag_l3_interface_peer_start(NULL);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to start l3 interface peer, err=%d\n", err);

bail:
    return err;
}

/**
 *  This function stops mlag l3 interface module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    L3_INTERFACE_INC_CNT(L3_INTERFACE_STOP_EVENTS_RCVD);

    if (!is_started) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface stop\n");

    is_started = 0;

    err = mlag_l3_interface_peer_stop(NULL);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to stop peer, err=%d\n",
                        err);

    if (current_switch_status == MASTER) {
        err = mlag_l3_interface_master_logic_stop(NULL);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to stop master logic, err=%d\n",
                            err);
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
mlag_l3_interface_ipl_port_set(struct ipl_port_set_event_data *data)
{
    return mlag_l3_interface_peer_ipl_port_set(data);
}

/**
 *  This function handles local sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_local_sync_done(uint8_t *data)
{
    return mlag_l3_interface_peer_local_sync_done(data);
}

/**
 *  This function handles master sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_sync_done(uint8_t *data)
{
    return mlag_l3_interface_peer_master_sync_done(data);
}

/**
 *  This function handles vlan local state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_vlan_local_state_change(
    struct vlan_state_change_event_data *data)
{
    return mlag_l3_interface_peer_vlan_local_state_change(data);
}

/**
 *  This function handles peer vlan local state change event
 *  that means vlan local event received from peer and destined to master logic
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_vlan_state_change_from_peer(
    struct vlan_state_change_event_data *data)
{
    return mlag_l3_interface_master_logic_vlan_state_change(data);
}

/**
 *  This function handles vlan global state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_vlan_global_state_change(
    struct vlan_state_change_event_data *data)
{
    return mlag_l3_interface_peer_vlan_global_state_change(data);
}

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_sync_start(struct sync_event_data *data)
{
    return mlag_l3_interface_master_logic_sync_start(data);
}

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_sync_finish(struct sync_event_data *data)
{
    return mlag_l3_interface_master_logic_sync_finish(data);
}

/**
 *  This function handles peer status change event
 *
 *  @param data - event data
 *
 * @return int as error code.
 */
int
mlag_l3_interface_peer_status_change(struct peer_state_change_data *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "Peer status change event ignored before start: mlag_id=%d, state=%d\n",
                 data->mlag_id, data->state);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "Peer %d changed status to %s\n",
             data->mlag_id, health_state_str[data->state]);

    if (!(((data->mlag_id >= 0) &&
           (data->mlag_id < MLAG_MAX_PEERS)))) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameter in peer status change event\n");
    }

    L3_INTERFACE_INC_CNT(L3_INTERFACE_PEER_STATE_CHANGE_EVENTS_RCVD);

    if ((current_switch_status == MASTER) &&
        ((data->state == HEALTH_PEER_DOWN) ||
         (data->state == HEALTH_PEER_COMM_DOWN))) {
        err = mlag_l3_interface_master_logic_peer_status_change(data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in master logic peer status change, err=%d\n",
                            err);
    }
    else if ((current_switch_status == SLAVE) &&
             ((data->state == HEALTH_PEER_DOWN) ||
              (data->state == HEALTH_PEER_COMM_DOWN))) {
        err = mlag_l3_interface_peer_stop(NULL);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to stop peer, err=%d\n",
                            err);
        err = mlag_l3_interface_peer_start(NULL);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to start peer, err=%d\n",
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
mlag_l3_interface_peer_start_event(struct peer_state_change_data *data)
{
    int err = 0;
    struct sync_event_data sync_done;

    ASSERT(data);

    if (!is_started) {
        goto bail;
    }

    if (current_switch_status == STANDALONE) {
        /* Ignore this event in stand alone mode */
        /* Send MLAG_PEER_SYNC_DONE to MLag protocol manager */
        sync_done.peer_id = data->mlag_id;
        sync_done.state = TRUE;
        sync_done.sync_type = L3_PEER_SYNC;

        err = send_system_event(MLAG_PEER_SYNC_DONE, &sync_done,
                                sizeof(sync_done));
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to send peer sync done event, err=%d\n",
                            err);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "Peer start handle : mlag_id=%d\n",
             data->mlag_id);

    L3_INTERFACE_INC_CNT(L3_INTERFACE_PEER_START_EVENTS_RCVD);

    err = mlag_l3_interface_peer_peer_start(data);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to start peer, err=%d\n",
                        err);
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
mlag_l3_interface_peer_enable(struct peer_state_change_data *data)
{
    int err = 0;

    if (current_switch_status == MASTER) {
        err = mlag_l3_interface_master_logic_peer_enable(data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in master logic peer enable, err=%d\n",
                            err);
    }

bail:
    return err;
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
    case MLAG_L3_SYNC_START_EVENT: {
        struct sync_event_data *ev =
            (struct sync_event_data*)msg_data;
        err = mlag_l3_interface_sync_start(ev);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in handling of sync start event, err=%d\n",
                            err);
    }
    break;
    case MLAG_L3_SYNC_FINISH_EVENT: {
        struct sync_event_data *ev =
            (struct sync_event_data*)msg_data;
        err = mlag_l3_interface_sync_finish(ev);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in handling of sync finish event, err=%d\n",
                            err);
    }
    break;
    case MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT:
        err = mlag_l3_interface_master_sync_done(NULL);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in handling of master sync done event, err=%d\n",
                            err);
        break;
    case MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT: {
        struct vlan_state_change_event_data *ev =
            (struct vlan_state_change_event_data*)msg_data;
        err = mlag_l3_interface_vlan_state_change_from_peer(ev);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in handling of vlan state change event from peer, err=%d\n",
                            err);
    }
    break;
    case MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT: {
        struct vlan_state_change_event_data *ev =
            (struct vlan_state_change_event_data*)msg_data;
        err = mlag_l3_interface_vlan_global_state_change(ev);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in handling of vlan global state change event, err=%d\n",
                            err);
    }
    break;
    default:
        /* Unknown opcode */
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown event %d\n",
                 opcode);
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
    int i;

    if (oper == MESSAGE_SENDING) {
        opcode = *(uint16_t*)data;
        *(uint16_t*)data = htons(opcode);
    }
    else {
        opcode = ntohs(*(uint16_t*)data);
        *(uint16_t*)data = opcode;
    }

    switch (opcode) {
    case MLAG_L3_SYNC_START_EVENT: {
        struct sync_event_data *ev =
            (struct sync_event_data*)data;
        if (oper == MESSAGE_SENDING) {
            ev->peer_id = htons(ev->peer_id);
            ev->state = htons(ev->state);
        }
        else {
            ev->peer_id = ntohs(ev->peer_id);
            ev->state = ntohs(ev->state);
        }
    }
    break;
    case MLAG_L3_SYNC_FINISH_EVENT: {
        struct sync_event_data *ev =
            (struct sync_event_data*)data;
        if (oper == MESSAGE_SENDING) {
            ev->peer_id = htons(ev->peer_id);
            ev->state = htons(ev->state);
        }
        else {
            ev->peer_id = ntohs(ev->peer_id);
            ev->state = ntohs(ev->state);
        }
    }
    break;
    case MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT:
        /* nothing to do */
        break;
    case MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT: {
        struct vlan_state_change_event_data *ev =
            (struct vlan_state_change_event_data*)data;
        if (oper == MESSAGE_SENDING) {
            ev->peer_id = htons(ev->peer_id);
            for (i = 0; i < ev->vlans_arr_cnt; i++) {
                ev->vlan_data[i].vlan_id = htons(ev->vlan_data[i].vlan_id);
            }
        }
        else {
            ev->peer_id = ntohs(ev->peer_id);
            for (i = 0; i < ev->vlans_arr_cnt; i++) {
                ev->vlan_data[i].vlan_id = ntohs(ev->vlan_data[i].vlan_id);
            }
        }
    }
    break;
    case MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT: {
        struct vlan_state_change_event_data *ev =
            (struct vlan_state_change_event_data*)data;
        if (oper == MESSAGE_SENDING) {
            ev->peer_id = htons(ev->peer_id);
            for (i = 0; i < ev->vlans_arr_cnt; i++) {
                ev->vlan_data[i].vlan_id = htons(ev->vlan_data[i].vlan_id);
            }
        }
        else {
            ev->peer_id = ntohs(ev->peer_id);
            for (i = 0; i < ev->vlans_arr_cnt; i++) {
                ev->vlan_data[i].vlan_id = ntohs(ev->vlan_data[i].vlan_id);
            }
        }
    }
    break;
    default:
        /* Unknown opcode */
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown event %d in network ordering handler\n",
                 opcode);
        break;
    }

    return err;
}

/**
 *  This function handles vlan local state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_switch_status_change(
    struct switch_status_change_event_data *data)
{
    int err = 0;
    enum master_election_switch_status previous_switch_status =
        current_switch_status;
    char *current_status_str;
    char *previous_status_str;
    char *l3_current_status_str;

    ASSERT(data);

    err = mlag_master_election_get_switch_status_str(data->current_status,
                                                     &current_status_str);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get master election current switch role string, err=%d\n",
                        err);
    err = mlag_master_election_get_switch_status_str(data->previous_status,
                                                     &previous_status_str);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get master election previous switch role string, err=%d\n",
                        err);
    err = mlag_master_election_get_switch_status_str(current_switch_status,
                                                     &l3_current_status_str);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get l3 interface current switch role string, err=%d\n",
                        err);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "Role change event accepted before start: current=%s, previous=%s\n",
                 current_status_str, previous_status_str);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_INFO, "Role changed from %s to %s\n",
             previous_status_str, current_status_str);

    L3_INTERFACE_INC_CNT(L3_INTERFACE_SWITCH_STATUS_CHANGE_EVENTS_RCVD);

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
            err = mlag_l3_interface_master_logic_start(NULL);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to start master logic, err=%d\n",
                                err);
        }
    }
    else {
        if (current_switch_status != previous_switch_status) {
            /* Switch status changed from non-default status
             * that means during real work, not after start.
             * Call stop of this module and after that start it again. */

            /* Need previous status to handle stop procedure in right way */
            current_switch_status = previous_switch_status;

            err = mlag_l3_interface_stop(NULL);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to stop module, err=%d\n",
                                err);

            err = mlag_l3_interface_start(NULL);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to start module, err=%d\n",
                                err);

            current_switch_status = data->current_status;

            if (current_switch_status == MASTER) {
                err = mlag_l3_interface_master_logic_start(NULL);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to start master logic, err=%d\n",
                                    err);
            }
        }
    }

bail:
    return err;
}

/**
 *  This function adds IPL to vlan of l3 interface
 *  to exchange control messages
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_add(struct peer_conf_event_data *data)
{
    return mlag_l3_interface_peer_vlan_interface_add(data);
}

/**
 *  This function removes IPL from vlan of l3 interface
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_del()
{
    return mlag_l3_interface_peer_vlan_interface_del();
}

/**
 *  This function outputs module's dump, either using print_cb
 *  or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_l3_interface_dump(void (*dump_cb)(const char *, ...))
{
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "=================\nL3 interface manager dump\n=================\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "is_initialized=%d, is_started=%d\n",
                 is_inited, is_started);
    }
    else {
        dump_cb(
            "=================\nL3 interface manager dump\n=================\n");
        dump_cb("is_initializes=%d, is_started=%d\n",
                is_inited, is_started);
    }

    if (!is_inited) {
        goto bail;
    }

    mlag_l3_interface_peer_print(dump_cb);
    if (current_switch_status == MASTER) {
        mlag_l3_interface_master_logic_print(dump_cb);
    }

    mlag_l3_interface_print_counters(dump_cb);

bail:
    return 0;
}

/**
 *  This function returns l3 interface module counters
 *
 * @param[in] _counters - pointer to structure to return counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_counters_get(struct mlag_l3_interface_counters *_counters)
{
    SAFE_MEMCPY(_counters, &counters);
    return 0;
}

/**
 *  This function clears l3 interface module counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_counters_clear(void)
{
    SAFE_MEMSET(&counters, 0);
    return 0;
}

/*
 *  This function prints l3 interface module counters to log
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
mlag_l3_interface_print_counters(void (*dump_cb)(const char *, ...))
{
    int i;
    for (i = 0; i < L3_INTERFACE_LAST_COUNTER; i++) {
        if (dump_cb == NULL) {
            MLAG_LOG(MLAG_LOG_NOTICE, "%s = %" PRIu64 " \n",
                     l3_interface_counters_str[i],
                     counters.counter[i]);
        }
        else {
            dump_cb("%s = %" PRIu64 " \n",
                    l3_interface_counters_str[i],
                    counters.counter[i]);
        }
    }
    return 0;
}

/**
 *  This function increments mlag_l3_interface counter
 *
 * @param[in] cnt - counter
 *
 * @return none
 */
void
mlag_l3_interface_inc_cnt(enum l3_interface_counters cnt)
{
    L3_INTERFACE_INC_CNT(cnt);
}
