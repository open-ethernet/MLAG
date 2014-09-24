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
#include "mlag_defs.h"
#include "mlag_events.h"
#include "mlag_common.h"
#include "mlag_api_defs.h"

#undef  __MODULE__
#define __MODULE__ MLAG_L3_INTERFACE_MASTER_LOGIC

#define MLAG_L3_INTERFACE_MASTER_LOGIC_C

#include "mlag_api_defs.h"
#include "mlag_l3_interface_manager.h"
#include "mlag_l3_interface_master_logic.h"
#include "mlag_l3_interface_peer.h"
#include "health_manager.h"
#include "mlag_manager.h"
#include "mlag_master_election.h"
#include "mlnx_lib/lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_dispatcher.h"

/************************************************
 *  Local Defines
 ***********************************************/

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  External variables
 ***********************************************/
extern char *l3_interface_vlan_local_state_str[];

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static int is_started;
static int is_inited;
static enum mlag_manager_peer_state peer_state[MLAG_MAX_PEERS];
static enum vlan_state vlan_peer_state[VLAN_N_VID][MLAG_MAX_PEERS];
static enum l3_interface_vlan_global_state vlan_global_state[VLAN_N_VID];
static struct vlan_state_change_event_data vlan_state_change_event;

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

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
mlag_l3_interface_master_logic_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return log verbosity
 */
mlag_verbosity_t
mlag_l3_interface_master_logic_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits mlag l3 interface master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_init(void)
{
    int err = 0;
    int i, j;

    if (is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "l3 interface master logic init called twice\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface master logic init\n");

    is_started = 0;
    is_inited = 1;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        peer_state[i] = PEER_DOWN;
    }
    for (i = 0; i < VLAN_N_VID; i++) {
        vlan_global_state[i] = VLAN_GLOBAL_DOWN;
    }
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        for (j = 0; j < VLAN_N_VID; j++) {
            vlan_peer_state[j][i] = VLAN_DOWN;
        }
    }

bail:
    return err;
}

/**
 *  This function de-inits mlag l3 interface master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_deinit(void)
{
    int err = 0;

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "l3 interface master logic deinit called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface master logic deinit\n");

    is_started = 0;
    is_inited = 0;

bail:
    return err;
}

/**
 *  This function starts mlag l3 interface master_logic sub-module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_start(uint8_t *data)
{
    int err = 0;
    int i, j;
    UNUSED_PARAM(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "l3 interface master logic start called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface master logic start\n");

    is_started = 1;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        peer_state[i] = PEER_DOWN;
    }
    for (i = 0; i < VLAN_N_VID; i++) {
        vlan_global_state[i] = VLAN_GLOBAL_DOWN;
    }
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        for (j = 0; j < VLAN_N_VID; j++) {
            vlan_peer_state[j][i] = VLAN_DOWN;
        }
    }

bail:
    return err;
}

/**
 *  This function stops mlag master election module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!is_started) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface master logic stop\n");

    is_started = 0;

bail:
    return err;
}

/**
 *  This function calculates global vlan state and
 *  set it into master logic data base
 *
 * @param[in] vlan_id - vlan id
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_calc_global_state(
    unsigned short vlan_id, int *is_changed)
{
    int i;

    *is_changed = 0;

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if ((vlan_peer_state[vlan_id][i] == VLAN_UP) &&
            (peer_state[i] == PEER_ENABLE)) {
            /* Global vlan up */
            if (vlan_global_state[vlan_id] == VLAN_GLOBAL_DOWN) {
                /* changed */
                vlan_global_state[vlan_id] = VLAN_GLOBAL_UP;
                *is_changed = 1;
            }
            break;
        }
    }
    if (i == MLAG_MAX_PEERS) {
        /* Global vlan down */
        if (vlan_global_state[vlan_id] == VLAN_GLOBAL_UP) {
            /* changed */
            vlan_global_state[vlan_id] = VLAN_GLOBAL_DOWN;
            *is_changed = 1;
        }
    }
    return 0;
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
mlag_l3_interface_master_logic_vlan_state_change(
    struct vlan_state_change_event_data *data)
{
    int err = 0;
    int i, j;
    int is_global_vlan_state_changed;
    int ev_len = 0;
    unsigned short vlan_id;

    ASSERT(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "vlan state change called before start\n");
    }

    MLAG_LOG(MLAG_LOG_INFO, "vlan state change with number vlans %d\n",
             data->vlans_arr_cnt);

    mlag_l3_interface_inc_cnt(VLAN_LOCAL_STATE_EVENTS_RCVD_FROM_PEER);

    for (i = 0, j = 0; i < data->vlans_arr_cnt; i++) {
        vlan_id = data->vlan_data[i].vlan_id;
        /* Save to DB and do not change global state if peer is not enabled */
        vlan_peer_state[vlan_id][data->peer_id] =
            data->vlan_data[i].vlan_state;

        /* If peer enabled check on global state change for vlan */
        if (peer_state[data->peer_id] == PEER_ENABLE) {
            mlag_l3_interface_master_logic_calc_global_state(
                vlan_id, &is_global_vlan_state_changed);

            if (is_global_vlan_state_changed) {
                /* Send MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT
                 * to all peers in PEER_ENABLE and PEER_TX_ENABLE states
                 */
                vlan_state_change_event.vlan_data[j].vlan_id = vlan_id;
                vlan_state_change_event.vlan_data[j].vlan_state =
                    vlan_global_state[vlan_id];
                j++;
            }
        }
    }

    if (j > 0) {
        ev_len = sizeof(struct vlan_state_change_base_event_data) +
                 sizeof(vlan_state_change_event.vlan_data[0]) * j;
        vlan_state_change_event.opcode =
            MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT;
        vlan_state_change_event.peer_id = data->peer_id;
        vlan_state_change_event.vlans_arr_cnt = j;
        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            if (peer_state[i] != PEER_DOWN) {
                err = mlag_dispatcher_message_send(
                    MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
                    &vlan_state_change_event, ev_len, i, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in sending MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT, err %d\n",
                                    err);
            }
        }
    }

bail:
    return err;
}

/**
 *  This function handles sync start event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_sync_start(struct sync_event_data *data)
{
    int err = 0;
    int i, j;
    struct sync_event_data ev1;
    int ev_len = 0;

    ASSERT(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "sync start called before start\n");
    }
    if (!(((data->peer_id >= 0) &&
           (data->peer_id < MLAG_MAX_PEERS)) &&
          (data->state == 0))) {
        MLAG_BAIL_ERROR_MSG(EINVAL,
                            "Invalid parameters in master logic peer start: peer id=%d, vlan state=%s",
                            data->peer_id,
                            (data->state == 0) ? "start" : "finish");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "sync start for peer %d\n",
             data->peer_id);

    /* Traverse master logic vlan database.
     * For each vlan in global up state send
     * MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT to given peer only
     */
    for (i = 1, j = 0; i < VLAN_N_VID; i++) {
        if (vlan_global_state[i] == VLAN_GLOBAL_UP) {
            /* Send MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT
             * to the peer in sync */
            vlan_state_change_event.vlan_data[j].vlan_id = i;
            vlan_state_change_event.vlan_data[j].vlan_state = VLAN_GLOBAL_UP;
            j++;
        }
    }

    if (j > 0) {
        ev_len = sizeof(struct vlan_state_change_base_event_data) +
                 sizeof(vlan_state_change_event.vlan_data[0]) * j;
        vlan_state_change_event.opcode =
            MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT;
        vlan_state_change_event.peer_id = data->peer_id;
        vlan_state_change_event.vlans_arr_cnt = j;

        /* Send MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT
         * to the peer in sync */
        err = mlag_dispatcher_message_send(
            MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
            &vlan_state_change_event, ev_len, data->peer_id, MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT, err %d\n",
                            err);
    }

    /* Set peer state to tx enable in order to enable sending
     * of global state changes to this peer
     * because master sync to the peer finished
     */
    peer_state[data->peer_id] = PEER_TX_ENABLE;

    /* Send MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT to the peer in sync */
    ev1.peer_id = data->peer_id;
    err = mlag_dispatcher_message_send(
        MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT, &ev1, sizeof(ev1),
        data->peer_id, MASTER_LOGIC);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT, err %d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles sync finish event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_sync_finish(struct sync_event_data *data)
{
    int err = 0;
    struct sync_event_data sync_done;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "sync finish ignored because of master logic is not started\n");
        goto bail;
    }
    if (!(((data->peer_id >= 0) &&
           (data->peer_id < MLAG_MAX_PEERS)) &&
          (data->state == 1))) {
        MLAG_BAIL_ERROR_MSG(EINVAL,
                            "Invalid parameters in master logic sync finish: peer id=%d, vlan state=%s",
                            data->peer_id,
                            (data->state == 0) ? "start" : "finish");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "sync finish for peer %d\n",
             data->peer_id);

    /* Send MLAG_PEER_SYNC_DONE to MLag protocol manager */
    sync_done.peer_id = data->peer_id;
    sync_done.state = TRUE;
    sync_done.sync_type = L3_PEER_SYNC;
    err = send_system_event(MLAG_PEER_SYNC_DONE,
                            &sync_done, sizeof(sync_done));
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in send_system_event for MLAG_PEER_SYNC_DONE, err %d\n",
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
mlag_l3_interface_master_logic_peer_enable(struct peer_state_change_data *data)
{
    int err = 0;
    int i, j;
    int is_global_vlan_state_changed;
    int ev_len = 0;

    ASSERT(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "peer enable called before start\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "peer enable for peer %d\n",
             data->mlag_id);

    if (!(((data->mlag_id >= 0) &&
           (data->mlag_id < MLAG_MAX_PEERS)) &&
          (data->state == PEER_ENABLE))) {
        MLAG_BAIL_ERROR_MSG(EINVAL,
                            "Invalid parameters in peer enable: mlag id=%d, vlan state=%d",
                            data->mlag_id, data->state);
    }

    mlag_l3_interface_inc_cnt(L3_INTERFACE_PEER_ENABLE_EVENTS_RCVD);

    peer_state[data->mlag_id] = PEER_ENABLE;

    /* Traverse master logic vlan database.
     * For each vlan calculate global state and if changed send
     * MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT to all peers
     */
    for (i = 1, j = 0; i < VLAN_N_VID; i++) {
        mlag_l3_interface_master_logic_calc_global_state(
            i, &is_global_vlan_state_changed);

        if (is_global_vlan_state_changed) {
            /* Send MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT
             * to all peers in PEER_ENABLE and PEER_TX_ENABLE states
             */
            vlan_state_change_event.vlan_data[j].vlan_id = i;
            vlan_state_change_event.vlan_data[j].vlan_state =
                vlan_global_state[i];
            j++;
        }
    }

    if (j > 0) {
        ev_len = sizeof(struct vlan_state_change_base_event_data) +
                 sizeof(vlan_state_change_event.vlan_data[0]) * j;
        vlan_state_change_event.opcode =
            MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT;
        vlan_state_change_event.peer_id = data->mlag_id;
        vlan_state_change_event.vlans_arr_cnt = j;

        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            if (peer_state[i] != PEER_DOWN) {
                err = mlag_dispatcher_message_send(
                    MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
                    &vlan_state_change_event, ev_len, i, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in sending MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT, err %d\n",
                                    err);
            }
        }
    }

bail:
    return err;
}

/**
 *  This function handles peer status change event:
 *  only down state is interesting
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_peer_status_change(
    struct peer_state_change_data *data)
{
    int err = 0;
    int i, j;
    int is_global_vlan_state_changed;
    uint16_t ipl_vlan_id = mlag_l3_interface_peer_get_ipl_vlan_id();
    int ev_len;

    /* Master Logic updates peer status to down.
     * In vlan database set peer status to default value (down) for each vlan.
     * Sends each global state change to other peers.
     */
    peer_state[data->mlag_id] = PEER_DOWN;

    for (i = 1, j = 0; i < VLAN_N_VID; i++) {
        if (i == ipl_vlan_id) {
            vlan_peer_state[i][data->mlag_id] = VLAN_DOWN;
            continue;
        }

        vlan_peer_state[i][data->mlag_id] = VLAN_DOWN;

        mlag_l3_interface_master_logic_calc_global_state(
            i, &is_global_vlan_state_changed);

        if (is_global_vlan_state_changed) {
            /* Send MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT
             * to all peers in PEER_ENABLE or PEER_TX_ENABLE states
             */
            vlan_state_change_event.vlan_data[j].vlan_id = i;
            vlan_state_change_event.vlan_data[j].vlan_state =
                vlan_global_state[i];
            j++;
        }
    }

    if (j > 0) {
        ev_len = sizeof(struct vlan_state_change_base_event_data) +
                 sizeof(vlan_state_change_event.vlan_data[0]) * j;
        vlan_state_change_event.opcode =
            MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT;
        vlan_state_change_event.peer_id = data->mlag_id;
        vlan_state_change_event.vlans_arr_cnt = j;

        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            if (peer_state[i] != PEER_DOWN) {
                err = mlag_dispatcher_message_send(
                    MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
                    &vlan_state_change_event, ev_len, i, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in sending MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT, err %d\n",
                                    err);
            }
        }
    }

bail:
    return err;
}

/**
 *  This function dumps sub-module's internal attributes
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_master_logic_print(void (*dump_cb)(const char *, ...))
{
    int i, j, cnt, total_cnt, tmp;

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "L3 interface master logic dump:\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "is_initialized=%d, is_started=%d\n",
                 is_inited, is_started);
    }
    else {
        dump_cb("L3 interface master logic dump:\n");
        dump_cb("is_initialized=%d, is_started=%d\n",
                is_inited, is_started);
    }

    if (!is_inited) {
        goto bail;
    }

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "peer states: \n");
    }
    else {
        dump_cb("peer states: \n");
    }
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (dump_cb == NULL) {
            MLAG_LOG(MLAG_LOG_NOTICE, "peer %d. %s\n", i,
                     (peer_state[i] == PEER_DOWN) ? "DOWN" :
                     (peer_state[i] ==
                      PEER_TX_ENABLE) ? "TX ENABLE" : "ENABLE");
        }
        else {
            dump_cb("peer %d. %s\n", i,
                    (peer_state[i] == PEER_DOWN) ? "DOWN" :
                    (peer_state[i] ==
                     PEER_TX_ENABLE) ? "TX ENABLE" : "ENABLE");
        }
    }
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "vlan peer states: \n");
    }
    else {
        dump_cb("\n");
        dump_cb("vlan peer states: \n");
    }
    for (i = 1; i <= 10; i++) {
        if (dump_cb == NULL) {
            MLAG_LOG(MLAG_LOG_NOTICE, "vlan %d:\n", i);
        }
        else {
            dump_cb("vlan %d:\n", i);
        }
        for (j = 0; j < MLAG_MAX_PEERS; j++) {
            if (dump_cb == NULL) {
                MLAG_LOG(MLAG_LOG_NOTICE,
                         "peer %d. %s\n", j,
                         (vlan_peer_state[i][j] == VLAN_DOWN) ? "DOWN" : "UP");
            }
            else {
                dump_cb("peer %d. %s\n", j,
                        (vlan_peer_state[i][j] == VLAN_DOWN) ? "DOWN" : "UP");
            }
        }
    }
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "vlans in global up state: \n");
    }
    else {
        dump_cb("\n");
        dump_cb("vlans in global up state: \n");
    }
    for (i = 1, total_cnt = 0; i < VLAN_N_VID; i++) {
        if (vlan_global_state[i] == VLAN_GLOBAL_UP) {
            total_cnt++;
        }
    }
    for (i = 1, cnt = 0, tmp = 0; i < VLAN_N_VID; i++) {
        if (vlan_global_state[i] == VLAN_GLOBAL_UP) {
            if ((++cnt > 100) &&
                (cnt < (total_cnt - 100))) {
                continue;
            }
            tmp++;
            if (dump_cb == NULL) {
                MLAG_LOG(MLAG_LOG_NOTICE, "%d  ", i);
            }
            else {
                dump_cb("%d  ", i);
            }
            if (tmp >= 10) {
                if (dump_cb == NULL) {
                    MLAG_LOG(MLAG_LOG_NOTICE, "\n");
                }
                else {
                    dump_cb("\n");
                }
                tmp = 0;
            }
        }
    }
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "\ntotal vlans in global up state: %d\n",
                 total_cnt);
        MLAG_LOG(MLAG_LOG_NOTICE, "\n");
    }
    else {
        dump_cb("\ntotal vlans in global up state: %d\n", total_cnt);
        dump_cb("\n");
    }

bail:
    return 0;
}
