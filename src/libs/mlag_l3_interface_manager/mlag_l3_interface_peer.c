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
#include "mlag_api_defs.h"

#undef  __MODULE__
#define __MODULE__ MLAG_L3_INTERFACE_PEER

#define MLAG_L3_INTERFACE_PEER_C

#include "mlag_api_defs.h"
#include "mlag_l3_interface_manager.h"
#include "mlag_l3_interface_peer.h"
#include "mlag_master_election.h"
#include "mlnx_lib/lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_dispatcher.h"
#include "service_layer.h"
#include "mlag_topology.h"

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
 *  Global variables
 ***********************************************/

/************************************************
 *  External variables
 ***********************************************/
extern char *l3_interface_vlan_local_state_str[];
extern char *l3_interface_vlan_global_state_str[];

/************************************************
 *  Local variables
 ***********************************************/
static int is_started;
static int is_inited;
static int is_peer_start;
static int is_master_sync_done;
static unsigned long ipl_ifindex;
static unsigned short ipl_vlan_id;
static uint8_t ipl_vlan_list[VLAN_N_VID];
static unsigned short sl_vlan_list[VLAN_N_VID];

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
mlag_l3_interface_peer_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return log verbosity
 */
mlag_verbosity_t
mlag_l3_interface_peer_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits mlag l3 interface peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_init(void)
{
    int err = 0;
    int i;

    if (is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "l3 interface peer init called twice\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface peer init\n");

    is_inited = 1;
    is_started = 0;
    is_peer_start = 0;
    ipl_ifindex = 0;
    ipl_vlan_id = 0;
    is_master_sync_done = 0;
    for (i = 0; i < VLAN_N_VID; i++) {
        ipl_vlan_list[i] = 0;
    }

bail:
    return err;
}

/**
 *  This function de-inits mlag l3 interface peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_deinit(void)
{
    int err = 0;

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "l3 interface peer deinit called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface peer deinit\n");

bail:
    return err;
}

/**
 *  This function starts mlag l3 interface peer sub-module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_start(uint8_t *data)
{
    int err = 0;
    int i;
    UNUSED_PARAM(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "l3 interface peer start called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface peer manager start\n");

    is_started = 1;
    is_peer_start = 0;
    is_master_sync_done = 0;
    for (i = 0; i < VLAN_N_VID; i++) {
        ipl_vlan_list[i] = 0;
    }

    /* Configure IPL as member of vlan of ipl l3 interface for control messages */
    if ((ipl_vlan_id > 0) &&
        (ipl_vlan_id < VLAN_N_VID)) {
        err = mlag_topology_ipl_port_get(0, &ipl_ifindex);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get ipl data from mlag topology database upon start, err=%d\n",
                            err);

        /* For start after disable mlag protocol vlan_id as well as
         * ipl ifindex are not relevant.
         * In that case ipl ifindex 0 will be returned.
         * Ignore it up to peer add event that should configure
         * above mentioned parameters.
         */
        if (ipl_ifindex == 0) {
            goto bail;
        }

        MLAG_LOG(MLAG_LOG_NOTICE, "Add ipl %lu to vlan %d\n",
                 ipl_ifindex, ipl_vlan_id);

        /* Add IPL to vlan */
        err = sl_api_ipl_vlan_membership_action(OES_ACCESS_CMD_ADD,
                                                ipl_ifindex,
                                                &ipl_vlan_id, 1);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to set ipl vlan membership, err=%d, ipl=%lu, vlan_id=%d\n",
                            err, ipl_ifindex, ipl_vlan_id);

        ipl_vlan_list[ipl_vlan_id] = 1;
        mlag_l3_interface_inc_cnt(ADD_IPL_TO_VLAN_EVENTS_SENT);
    }

bail:
    return err;
}

/**
 *  This function stops mlag l3 interface peer sub-module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_stop(uint8_t *data)
{
    int err = 0;
    int i;
    UNUSED_PARAM(data);
    int num_vlans_to_del = 0;

    if (!is_started) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "l3 interface peer manager stop\n");

    /* Check on ipl ifindex validity */
    err = mlag_topology_ipl_port_get(0, &ipl_ifindex);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get ipl data from mlag topology database upon stop, err=%d\n",
                        err);

    if (ipl_ifindex == 0) {
        goto bail;
    }
    /* Remove IPL from all vlans */
    for (i = 1; i < VLAN_N_VID; i++) {
        if (ipl_vlan_list[i]) {
            /* Remove IPL from vlan */
            ipl_vlan_list[i] = 0;
            sl_vlan_list[num_vlans_to_del++] = i;
            mlag_l3_interface_inc_cnt(DEL_IPL_FROM_VLAN_EVENTS_SENT);
        }
    }

    if (num_vlans_to_del) {
        err = sl_api_ipl_vlan_membership_action(OES_ACCESS_CMD_DELETE,
                                                ipl_ifindex, sl_vlan_list,
                                                num_vlans_to_del);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to delete ipl vlan membership, err=%d, ipl_ifindex=%lu, num_vlans_to_del=%d\n",
                            err, ipl_ifindex, num_vlans_to_del);
    }

bail:
    is_started = 0;
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
mlag_l3_interface_peer_peer_start(struct peer_state_change_data *data)
{
    int err = 0;
    struct sync_event_data ev;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Sending MLAG_L3_SYNC_START_EVENT to master logic\n");

    is_peer_start = 1;

    /* Send MLAG_SYNC_START_EVENT event to Master Logic */
    ev.peer_id = data->mlag_id;
    ev.state = 0;
    err = mlag_dispatcher_message_send(MLAG_L3_SYNC_START_EVENT, &ev,
                                       sizeof(ev), MASTER_PEER_ID,
                                       PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending MLAG_L3_SYNC_START_EVENT, err=%d\n",
                        err);

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
mlag_l3_interface_peer_ipl_port_set(struct ipl_port_set_event_data *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "ipl port set called before init\n");
    }

    mlag_l3_interface_inc_cnt(L3_INTERFACE_IPL_PORT_SET_EVENTS_RCVD);

    if (data->ifindex == 0) {
        /* Remove ipl port from ipl vlan due to disconnect of IPL from port-channel */
        /* Check if IPL is a member of the vlan */
        if ((ipl_vlan_id != 0) &&
            (ipl_vlan_list[ipl_vlan_id] == 1)) {
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "mlag internal delete vlan %d from ipl vlan list\n",
                     ipl_vlan_id);
            ipl_vlan_list[ipl_vlan_id] = 0;
        }
    }
    else {
        if (!is_started) {
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "ipl port set event ignored because it called before start: ipl=%d, ipl_vlan=%d\n",
                     data->ifindex, ipl_vlan_id);
            goto bail;
        }
        /* Connect port-channel to ipl */
        /* If ipl vlan is not configured on ipl yet configure it here */
        if ((ipl_vlan_id != 0) &&
            (ipl_vlan_list[ipl_vlan_id] == 0)) {
            MLAG_LOG(MLAG_LOG_NOTICE, "Add ipl=%d to ipl vlan=%d\n",
                     data->ifindex, ipl_vlan_id);

            /* Add IPL to vlan */
            err = sl_api_ipl_vlan_membership_action(OES_ACCESS_CMD_ADD,
                                                    data->ifindex,
                                                    &ipl_vlan_id, 1);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to add ipl vlan membership: ipl=%d, ipl_vlan=%d, err=%d\n",
                                data->ifindex, ipl_vlan_id, err);
            ipl_vlan_list[ipl_vlan_id] = 1;
            mlag_l3_interface_inc_cnt(ADD_IPL_TO_VLAN_EVENTS_SENT);
        }
        else {
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "Vlan is not configured on ipl vlan interface: ipl=%d, ipl_vlan=%d\n",
                     data->ifindex, ipl_vlan_id);
        }
    }

bail:
    ipl_ifindex = data->ifindex;
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
mlag_l3_interface_peer_vlan_interface_add(struct peer_conf_event_data *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_inited) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Add ipl to vlan interface called before init\n");
    }

    /* vlan_id can be 0 when delete peer event accepted */
    if (!(data->vlan_id < VLAN_N_VID)) {
        MLAG_BAIL_ERROR_MSG(err, "vlan id %d is not valid\n", data->vlan_id);
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "Add ipl to vlan interface %d\n", data->vlan_id);

    ipl_vlan_id = data->vlan_id;

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Add ipl to vlan interface %d ignored because called before start\n",
                 ipl_vlan_id);
        goto bail;
    }

    /* Check if IPL is not a member of the vlan */
    if ((ipl_vlan_id != 0) &&
        (ipl_vlan_list[ipl_vlan_id] == 0)) {
        /* Add IPL to vlan */
        err = mlag_topology_ipl_port_get(0, &ipl_ifindex);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get ipl port data from mlag topology: ipl vlan id=%d, ipl ifindex=%lu, err=%d\n",
                            ipl_vlan_id, ipl_ifindex, err);

        if (ipl_ifindex == 0) {
            goto bail;
        }
        err = sl_api_ipl_vlan_membership_action(OES_ACCESS_CMD_ADD,
                                                ipl_ifindex,
                                                &ipl_vlan_id, 1);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to add ipl %lu to vlan interface %d, err %d\n",
                            ipl_ifindex, ipl_vlan_id, err);

        ipl_vlan_list[ipl_vlan_id] = 1;
        mlag_l3_interface_inc_cnt(ADD_IPL_TO_VLAN_EVENTS_SENT);
    }

bail:
    return err;
}

/**
 *  This function removes IPL from vlan of l3 interface
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_vlan_interface_del()
{
    int err = 0;

    if (!is_inited) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Delete ipl from vlan interface called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Delete ipl from vlan interface: ipl_vlan_id=%d\n",
             ipl_vlan_id);

    /* Check if IPL is not a member of the vlan */
    if ((ipl_vlan_id != 0) &&
        (ipl_vlan_list[ipl_vlan_id] == 1)) {
        ipl_vlan_list[ipl_vlan_id] = 0;
    }

bail:
    return err;
}

/**
 *  This function handles local sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_local_sync_done(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);
    struct sync_event_data ev;
    struct mlag_master_election_status master_election_current_status;

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Peer local sync done event accepted before start of module\n");
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE,
             "send MLAG_L3_SYNC_FINISH_EVENT to master logic\n");

    err = mlag_master_election_get_status(&master_election_current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get status from master election in handling of local sync done event, err=%d\n",
                        err);

    /* Send MLAG_L3_SYNC_FINISH_EVENT to Master Logic */
    ev.peer_id = master_election_current_status.my_peer_id;
    ev.state = 1;
    err = mlag_dispatcher_message_send(MLAG_L3_SYNC_FINISH_EVENT,
                                       &ev, sizeof(ev), MASTER_PEER_ID,
                                       PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending MLAG_L3_SYNC_FINISH_EVENT, err=%d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles master sync done event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_master_sync_done(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    is_master_sync_done = 1;

    MLAG_LOG(MLAG_LOG_NOTICE, "send local vlan oper status trigger\n");

    /* Send query to update local vlans operational states */
    err = sl_api_vlan_oper_status_trigger_get();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in local vlan oper status trigger, err=%d\n",
                        err);

bail:
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
mlag_l3_interface_peer_vlan_local_state_change(
    struct vlan_state_change_event_data *data)
{
    int err = 0;
    int ev_len = 0;
    struct mlag_master_election_status master_election_current_status;
    int i;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Vlan local state change event accepted before start of module\n");
        goto bail;
    }

    if (!is_master_sync_done) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Vlan local state change event accepted before master sync done\n");
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_INFO, "Vlan local state change event: number vlans %d\n",
             data->vlans_arr_cnt);

    if ((!data->vlans_arr_cnt) ||
        (data->vlans_arr_cnt >= VLAN_N_VID)) {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(EINVAL, "Invalid vlans array_counter %d",
                            data->vlans_arr_cnt);
    }

    for (i = 0; i < data->vlans_arr_cnt; i++) {
        if (!(((data->vlan_data[i].vlan_id > 0) &&
               (data->vlan_data[i].vlan_id < VLAN_N_VID)) &&
              ((data->vlan_data[i].vlan_state == VLAN_DOWN) ||
               (data->vlan_data[i].vlan_state == VLAN_UP)))) {
            MLAG_BAIL_ERROR_MSG(EINVAL,
                                "Invalid vlan parameters: vlan id=%d, vlan state=%s",
                                data->vlan_data[i].vlan_id,
                                (data->vlan_data[i].vlan_state ==
                                 VLAN_UP) ? "up" : "down");
        }
    }

    mlag_l3_interface_inc_cnt(VLAN_LOCAL_STATE_EVENTS_RCVD);

    /* Send MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT
     * to Master Logic */
    err = mlag_master_election_get_status(&master_election_current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get status from master election in handling of vlan local state change event, err=%d\n",
                        err);

    ev_len = sizeof(struct vlan_state_change_base_event_data) +
             sizeof(data->vlan_data[0]) * data->vlans_arr_cnt;

    data->peer_id = master_election_current_status.my_peer_id;

    err = mlag_dispatcher_message_send(
        MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT,
        data, ev_len, MASTER_PEER_ID, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT, err=%d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles vlan global state change event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_l3_interface_peer_vlan_global_state_change(
    struct vlan_state_change_event_data *data)
{
    int err = 0;
    int i;
    unsigned short vlan_id;
    int num_vlans_to_add = 0;
    int num_vlans_to_del = 0;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Vlan global state change event accepted before start of module\n");
        goto bail;
    }

    if (!is_peer_start) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Vlan global state change event accepted before peer start event\n");
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "Vlan global state change event: number vlans %d\n",
             data->vlans_arr_cnt);

    mlag_l3_interface_inc_cnt(VLAN_GLOBAL_STATE_EVENTS_RCVD);

    for (i = 0; i < data->vlans_arr_cnt; i++) {
        vlan_id = data->vlan_data[i].vlan_id;
        if (data->vlan_data[i].vlan_state == VLAN_GLOBAL_DOWN) {
            /* Check if IPL is a member of the vlan */
            if (ipl_vlan_list[vlan_id] == 1) {
                /* Remove IPL from vlan */
                /* Do not remove vlan of mlag control messages interface from ipl */
                if (vlan_id != ipl_vlan_id) {
                    ipl_vlan_list[vlan_id] = 0;
                    sl_vlan_list[num_vlans_to_del++] = vlan_id;
                    mlag_l3_interface_inc_cnt(DEL_IPL_FROM_VLAN_EVENTS_SENT);
                }
                else {
                    MLAG_LOG(MLAG_LOG_NOTICE,
                             "ignored ipl vlan %d, state=%s\n",
                             vlan_id,
                             l3_interface_vlan_global_state_str[data->vlan_data
                                                                [i].
                                                                vlan_state]);
                }
            }
        }
    }
    if (num_vlans_to_del) {
        err = sl_api_ipl_vlan_membership_action(OES_ACCESS_CMD_DELETE,
                                                ipl_ifindex, sl_vlan_list,
                                                num_vlans_to_del);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to delete ipl vlan membership, err=%d\n",
                            err);
    }

    for (i = 0; i < data->vlans_arr_cnt; i++) {
        vlan_id = data->vlan_data[i].vlan_id;
        if (data->vlan_data[i].vlan_state == VLAN_GLOBAL_UP) {
            /* Check if IPL is not a member of the vlan */
            if (ipl_vlan_list[vlan_id] == 0) {
                /* Add IPL to vlan */
                ipl_vlan_list[vlan_id] = 1;
                sl_vlan_list[num_vlans_to_add++] = vlan_id;
                mlag_l3_interface_inc_cnt(ADD_IPL_TO_VLAN_EVENTS_SENT);
            }
        }
    }
    if (num_vlans_to_add) {
        err = sl_api_ipl_vlan_membership_action(OES_ACCESS_CMD_ADD,
                                                ipl_ifindex, sl_vlan_list,
                                                num_vlans_to_add);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to add ipl vlan membership, err=%d\n",
                            err);
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
mlag_l3_interface_peer_print(void (*dump_cb)(const char *, ...))
{
    int i, cnt, total_cnt, tmp;

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "L3 interface local peer dump:\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "is_initialized=%d, is_started=%d, "
                 "is_peer_start=%d, is_master_sync_done=%d\n",
                 is_inited, is_started, is_peer_start, is_master_sync_done);
    }
    else {
        dump_cb("L3 interface local peer dump:\n");
        dump_cb("is_initialized=%d, is_started=%d, is_peer_start=%d, "
                "is_master_sync_done=%d\n",
                is_inited, is_started, is_peer_start, is_master_sync_done);
    }

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "ipl_ifindex=%lu, ipl_vlan_id=%d\n",
                 ipl_ifindex, ipl_vlan_id);
    }
    else {
        dump_cb("ipl_ifindex=%lu, ipl_vlan_id=%d\n",
                ipl_ifindex, ipl_vlan_id);
    }

    if (!is_inited) {
        goto bail;
    }

    /* Print IPL vlan list */
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "ipl vlan list:\n");
    }
    else {
        dump_cb("ipl vlan list:\n");
    }

    for (i = 1, total_cnt = 0; i < VLAN_N_VID; i++) {
        if (ipl_vlan_list[i]) {
            total_cnt++;
        }
    }
    for (i = 1, cnt = 0, tmp = 0; i < VLAN_N_VID; i++) {
        if (ipl_vlan_list[i]) {
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
        MLAG_LOG(MLAG_LOG_NOTICE, "\ntotal vlans in ipl_vlan_list: %d\n",
                 total_cnt);
        MLAG_LOG(MLAG_LOG_NOTICE, "\n");
    }
    else {
        dump_cb("\ntotal vlans in ipl_vlan_list: %d\n", total_cnt);
        dump_cb("\n");
    }

bail:
    return 0;
}

/**
 *  This function returns ipl_vlan_id
 *
 * @return ipl_vlan_id
 */
uint16_t
mlag_l3_interface_peer_get_ipl_vlan_id(void)
{
    return ipl_vlan_id;
}
