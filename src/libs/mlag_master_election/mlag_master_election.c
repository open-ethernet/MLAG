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
#include <complib/cl_passivelock.h>
#include "mlag_log.h"
#include "mlag_events.h"
#include "mlag_defs.h"
#include "mlag_common.h"

#undef  __MODULE__
#define __MODULE__ MLAG_MASTER_ELECTION

#define MLAG_MASTER_ELECTION_C
#include "mlag_bail.h"
#include "mlag_master_election.h"
#include "mlag_master_election_fsm.h"
#include "health_manager.h"

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
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static int is_started;
static int is_initialized;
static struct mlag_master_election_fsm *master_election_fsm;
static struct mlag_master_election_counters counters;
static cl_plock_t mlag_master_election_mutex;

/************************************************
 *  Local function declarations
 ***********************************************/

static void mlag_master_election_fsm_trace(char* buf, int len);
static int mlag_master_election_print_counters(void (*dump_cb)(const char *,
                                                               ...));

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
mlag_master_election_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    mlag_master_election_fsm_log_verbosity_set(verbosity);
}

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_master_election_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits mlag master election module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_init(void)
{
    int err = 0;
    cl_status_t cl_err;

    if (is_initialized) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "master election init called twice\n");
    }

    is_started = 0;
    is_initialized = 1;

    /* Create fsm */
    master_election_fsm =
        (struct mlag_master_election_fsm *)cl_malloc(
            sizeof(struct mlag_master_election_fsm));
    if (master_election_fsm == NULL) {
        err = ENOMEM;
        MLAG_BAIL_NOCHECK_MSG(
            "Failed to allocate memory for master election fsm\n");
    }

    /* Create mutex to protect fsm database */
    cl_err = cl_plock_init(&mlag_master_election_mutex);
    if (cl_err != CL_SUCCESS) {
        err = EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to create mutex for master election fsm\n");
    }

    /* Init FSM and concrete attributes */
    mlag_master_election_fsm_init(master_election_fsm,
                                  mlag_master_election_fsm_trace,
                                  NULL, NULL);
    master_election_fsm->current_status = DEFAULT_SWITCH_STATUS;
    master_election_fsm->previous_status = DEFAULT_SWITCH_STATUS;
    master_election_fsm->my_ip_addr = 0;
    master_election_fsm->peer_ip_addr = 0;
    master_election_fsm->peer_status = HEALTH_PEER_UP;
    master_election_fsm->my_peer_id = 0;
    master_election_fsm->master_peer_id = 0;

    /* Clear counters on init */
    mlag_master_election_counters_clear();

bail:
    return err;
}

/**
 *  This function de-inits mlag master election module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_deinit(void)
{
    int err = 0;
    cl_status_t cl_err;

    if (!is_initialized) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "master election deinit called before init\n");
    }

    is_started = 0;
    is_initialized = 0;

    /* Destroy fsm */
    cl_err = cl_free(master_election_fsm);
    if (cl_err != CL_SUCCESS) {
        err = ENOMEM;
        MLAG_BAIL_NOCHECK_MSG("Failed to free memory\n");
    }
    master_election_fsm = NULL;

    cl_plock_destroy(&(mlag_master_election_mutex));

bail:
    return err;
}

/**
 *  This function is to print fsm trace events
 *
 * @return none
 */
static void
mlag_master_election_fsm_trace(char* buf, int len)
{
    UNUSED_PARAM(len);
    if (buf) {
        MLAG_LOG(MLAG_LOG_INFO, "FSM: %s", buf);
    }
    else {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "fsm trace called with NULL buffer pointer\n");
    }
}

/**
 *  This function starts mlag master election module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_start(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!is_initialized) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "master election start called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "master election start\n");

    is_started = 1;

    /* Call FSM with start */
    err = mlag_master_election_fsm_start_ev(master_election_fsm);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in handling of master election fsm start event, err=%d\n",
                        err);

    /* Clear Master Election counters on start */
    mlag_master_election_counters_clear();
    INC_CNT(START_EVENTS_RCVD);

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
mlag_master_election_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    INC_CNT(STOP_EVENTS_RCVD);

    if (!is_started) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "master election stop\n");

    is_started = 0;

    master_election_fsm->current_status = DEFAULT_SWITCH_STATUS;
    master_election_fsm->previous_status = DEFAULT_SWITCH_STATUS;
    master_election_fsm->peer_status = HEALTH_PEER_UP;
    master_election_fsm->my_peer_id = 0;
    master_election_fsm->master_peer_id = 0;

    /* Call FSM with stop */
    err = mlag_master_election_fsm_stop_ev(master_election_fsm);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in handling of master election fsm stop event, err=%d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles config change event
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_config_change(struct config_change_event *ev)
{
    int err = 0;

    ASSERT(ev);

    if (!is_initialized) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "Config change called before init\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Config change event: my_ip_addr_cmd=%d, my_ip_addr=0x%08x, peer_ip_addr_cmd=%d, peer_ip_addr=0x%08x\n",
             ev->my_ip_addr_cmd, ev->my_ip_addr, ev->peer_ip_addr_cmd,
             ev->peer_ip_addr);

    INC_CNT(CONFIG_CHANGE_EVENTS_RCVD);

    switch (ev->my_ip_addr_cmd) {
    case NO_CHANGE:
        break;
    case MODIFY:
        master_election_fsm->my_ip_addr = ev->my_ip_addr;
        break;
    default:
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown value of own IP address command %d\n",
                 ev->my_ip_addr_cmd);
        break;
    }
    switch (ev->peer_ip_addr_cmd) {
    case NO_CHANGE:
        break;
    case MODIFY:
        master_election_fsm->peer_ip_addr = ev->peer_ip_addr;
        break;
    default:
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown value of peer IP address command %d\n",
                 ev->peer_ip_addr_cmd);
        break;
    }

    if (!is_started) {
        goto bail;
    }

    cl_plock_excl_acquire(&mlag_master_election_mutex);

    /* Call FSM with config change */
    err = mlag_master_election_fsm_config_change_ev(master_election_fsm);
    cl_plock_release(&mlag_master_election_mutex);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in handling of master election fsm config change event, err=%d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles peer status change event
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */

int
mlag_master_election_peer_status_change(struct peer_state_change_data *data)
{
    int err = 0;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Peer status change event ignored before start: peer_id=%d\n",
                 data->mlag_id);
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_NOTICE, "Peer %d changed status to %s\n",
             data->mlag_id, health_state_str[data->state]);

    INC_CNT(PEER_STATE_CHANGE_EVENTS_RCVD);

    /* Ignore peer status change events of local peer */
    if ((data->mlag_id == master_election_fsm->my_peer_id) &&
        (master_election_fsm->current_status != STANDALONE)) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Peer status change event ignored due to belonging to local peer\n");
        goto bail;
    }

    cl_plock_excl_acquire(&mlag_master_election_mutex);

    /* Call FSM with peer status change */
    err = mlag_master_election_fsm_peer_status_change_ev(master_election_fsm,
                                                         data->mlag_id,
                                                         data->state);
    cl_plock_release(&mlag_master_election_mutex);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in handling of master election fsm peer status change event"
                        ", err=%d\n",
                        err);

bail:
    return err;
}

/**
 *  This function returns current value of switch status
 *
 * @master_election_current_status - output current parameters defined by
 *                                   master elections module
 *
 * @return  0 when successful, otherwise ERROR
 */

int
mlag_master_election_get_status(
    struct mlag_master_election_status *master_election_current_status)
{
    int err = 0;

    if (!is_initialized) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "Get status called before init\n");
    }

    cl_plock_excl_acquire(&mlag_master_election_mutex);

    master_election_current_status->current_status =
        master_election_fsm->current_status;
    master_election_current_status->previous_status =
        master_election_fsm->previous_status;
    master_election_current_status->my_ip_addr =
        master_election_fsm->my_ip_addr;
    master_election_current_status->peer_ip_addr =
        master_election_fsm->peer_ip_addr;
    master_election_current_status->my_peer_id =
        master_election_fsm->my_peer_id;
    master_election_current_status->master_peer_id =
        master_election_fsm->master_peer_id;

    cl_plock_release(&mlag_master_election_mutex);

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
mlag_master_election_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "=================\nMaster election dump\n=================\n");
        MLAG_LOG(MLAG_LOG_NOTICE, "is_initialized=%d, is_started=%d\n",
                 is_initialized, is_started);
    }
    else {
        dump_cb("=================\nMaster election dump\n=================\n");
        dump_cb("is_initialized=%d, is_started=%d\n",
                is_initialized, is_started);
    }

    if (!is_initialized) {
        goto bail;
    }

    cl_plock_excl_acquire(&mlag_master_election_mutex);

    err = mlag_master_election_fsm_print(master_election_fsm, dump_cb);
    cl_plock_release(&mlag_master_election_mutex);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in master election fsm print, err=%d\n",
                        err);

    mlag_master_election_print_counters(dump_cb);

bail:
    return err;
}

/**
 *  This function returns master election module counters
 *
 * @param[in] _counters - pointer to structure to return counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_counters_get(
    struct mlag_master_election_counters *_counters)
{
    int err = 0;
    ASSERT(_counters);
    SAFE_MEMCPY(_counters, &counters);
bail:
    return err;
}

/**
 *  This function clears master election module counters
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_counters_clear(void)
{
    SAFE_MEMSET(&counters, 0);
    return 0;
}

/*
 *  This function prints master election module counters to log
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
mlag_master_election_print_counters(void (*dump_cb)(const char *, ...))
{
    int i;
    for (i = 0; i < LAST_COUNTER; i++) {
        if (dump_cb == NULL) {
            MLAG_LOG(MLAG_LOG_NOTICE, "%s = %d\n",
                     master_election_counters_str[i],
                     counters.counter[i]);
        }
        else {
            dump_cb("%s = %d\n",
                    master_election_counters_str[i],
                    counters.counter[i]);
        }
    }
    return 0;
}

/**
 *  This function returns master election switch status as string
 *
 * @param[in] switch_status - switch status requested
 * @param[out] switch_status_str - pointer to switch status string
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_get_switch_status_str(int switch_status,
                                           char** switch_status_str)
{
    int err = 0;

    ASSERT(switch_status_str);

    if (switch_status < LAST_STATUS) {
        *switch_status_str = master_election_switch_status_str[switch_status];
    }
    else {
        err = 1;
    }
bail:
    return err;
}

/**
 *  This function increments master election counter
 *
 * @param[in] cnt - counter
 *
 * @return none
 */
void
mlag_master_election_inc_cnt(enum master_election_counters cnt)
{
    INC_CNT(cnt);
}
