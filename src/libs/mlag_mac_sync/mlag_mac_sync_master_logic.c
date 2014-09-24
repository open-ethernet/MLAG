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
#include <complib/cl_pool.h>

#include <complib/cl_list.h>
#include <complib/cl_qmap.h>

#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_events.h"
#include "mac_sync_events.h"
#include "mlag_common.h"
#include "mlag_manager.h"
#include <sys/time.h>
#include <unistd.h>
#include "lib_ctrl_learn_defs.h"
#include "lib_ctrl_learn.h"
#include "mlag_mac_sync_flush_fsm.h"
#include "mlag_master_election.h"
#include "mlag_mac_sync_peer_manager.h"

#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_mac_sync_dispatcher.h"
#include "mlag_mac_sync_manager.h"
#include "mlag_mac_sync_master_logic.h"
#include "mlag_mac_sync_router_mac_db.h"
#include "port_manager.h"
#include "stdlib.h"



#undef  __MODULE__
#define __MODULE__ MLAG_MAC_SYNC_MASTER_LOGIC

#define MLAG_MAC_SYNC_MASTER_LOGIC_C

/************************************************
 *  Local Defines
 ***********************************************/
enum global_learn_send_type {
    SEND_TO_ALL_PEERS,
    SEND_TO_ORIGINATOR_PEER,
    SEND_REJECT_TO_ORIGINATOR_PEER,
    SEND_TO_REMOTE_PEERS
};

#define MAX_ENTRIES_IN_TRY 300

/* num of Vid/Port/System Flush Fsm */
/* 4094 vid + 64 * 2 (port + port channel) + 1(global flush */
#define NUM_OF_VID_PORT_SYSTEM_FLUSH_FSM 8 * (4094 + 64 * 2) + 1


/* num of Vid+Port Flush Fsm */
#define NUM_OF_VID_PORT_FLUSH_FSM 10000

/* definitions for the qmap key*/
#define KEY_PORT_SHIFT  32 /* 4*8 */
#define NON_MLAG_PART_SHIFT (KEY_PORT_SHIFT + 16) /* port + vid shift  */
#define NON_MLAG_BIT  0x8
/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/
/* Master logic entry per MAC+vid*/
struct master_logic_data {
    uint32_t port;   /* port MLAG or 0xffffffff for non mlag ports*/
    uint32_t timestamp;  /* timestamp  when mac added or modified in the DB*/
    enum fdb_uc_mac_entry_type entry_type; /* static, dynamic_ageable, dynamic_non_ageable */
    uint16_t peer_bmap;  /*  states per peer for 16 peers : local_learned bit =1 , else  0*/
    /* DEBUG data*/
};

/* internal struct for fsm map and list(free pool) */
struct flush_fsm_item {
    cl_list_item_t list_item;
    cl_map_item_t map_item;
    uint64_t filter;
    mlag_mac_sync_flush_fsm fsm;
};

/************************************************
 *  External variables
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static int is_started;
static int is_inited;
static enum mlag_manager_peer_state peer_state[MLAG_MAX_PEERS];

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;


/* map of flush fsm for port+vlan , vlan,port and system(global flush) , key is the filter,value is fsm */
static cl_qmap_t vlan_ports_system_flush_fsm_map;

/* free pool of flush fsm for vlan,port and system(global flush) */
static cl_list_t vlan_port_system_flush_fsm_pool;

/* free pool of flush fsm for ports */
static cl_list_t vlan_port_flush_fsm_pool;

/* counter for flash fsm in qmap */
static signed int flash_fsm_in_qmap_cnt = 0;

static unsigned int non_available_memory_fsm_cnt = 0;

static void * master_cookie = NULL;

static cl_pool_t cookie_pool;   /* pool for allocations of cookies */

#define FDB_EXPORT_DATA_BLOCK_SIZE   \
    sizeof(struct mac_sync_master_fdb_export_event_data) + 100 + \
    sizeof(struct mac_sync_learn_event_data) * MAX_FDB_ENTRIES

static uint8_t fdb_export_data_block[FDB_EXPORT_DATA_BLOCK_SIZE];


static const struct fdb_uc_key_filter empty_filter =
{FDB_KEY_FILTER_FIELD_NOT_VALID, 0, FDB_KEY_FILTER_FIELD_NOT_VALID, 0};


static struct mac_sync_multiple_learn_buffer gl_originator_buff;  /*global learn buffers*/
static struct mac_sync_multiple_learn_buffer gl_all_buff;
static struct mac_sync_multiple_learn_buffer gl_remote_buff;
/* global age buffer*/
static struct mac_sync_multiple_age_buffer global_age_buffer;

/************************************************
 *  Local function declarations
 ***********************************************/
static int generic_get_entry(struct fdb_uc_mac_addr_params *mac_entry);

static int process_new_macs_set_to_peer( struct
                                         mac_sync_multiple_learn_buffer * gl_buff);

static int process_local_learn_new_mac(void * cookie,
                                       struct mac_sync_learn_event_data * data);

static int process_local_learn_existed_mac(void * cookie,
                                           struct mac_sync_learn_event_data * data);

static int send_global_learn_buffers();

static int send_global_age_buffer();


static int send_global_age_buffer();


static int process_local_aged(void * cookie,
                              struct mac_sync_age_event_data * data);

/* callbacks in db_access method*/
static int _process_fdb_export_request_cb(void * data);

static int fdb_export_add_router_macs(uint8_t peer_id);

static int _master_process_local_learn_cb(void* user_data);

static int _master_process_peer_down_cb(void* user_data);

static int _master_process_local_aged_cb(void* user_data);

int _local_learn_update_master_and_peers(struct master_logic_data *master_data,
                                         struct mac_sync_learn_event_data * data,
                                         enum   global_learn_send_type type);

static int is_flush_busy(struct mac_sync_learn_event_data * msg_data,
                         int *flush_busy);

/* flush fsm callbacks*/
/*static void flush_fsm_user_trace(char *buf, int len);*/
/**
 *  This function
 *
 * @return 0 when successful, otherwise ERROR
 */
static void flush_fsm_timer_cb(void *data);
/*
 * This function is a timer scheduling service for FSM
 *
 * @param[in] timeout - msec timeout
 * @param[in] data - data to be used when timer expires
 * @param[out] timer_handler - timer handler returned by timer service
 *
 * @return 0 if operation completes successfully.
 */
static int flush_sched_func(int timeout, void *data, void ** timer_handler);

static int flush_unsched_func(void ** timer_handler);

static int mlag_mac_sync_master_logic_get_available_flush_fsm(
    struct mac_sync_flush_generic_data * flush_data, int allocate,
    mlag_mac_sync_flush_fsm **fsm);

static int mlag_mac_sync_master_logic_get_fsm(cl_qmap_t * const p_map,
                                              IN cl_list_t * const free_pool,
                                              uint64_t key, int allocate,
                                              mlag_mac_sync_flush_fsm **fsm);

static int mlag_mac_sync_master_logic_is_flush_busy(
    struct mac_sync_uc_mac_addr_params * mac_params, uint8_t peer_originator,
    int *busy);

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function sets module log verbosity level
 *
 *  @param[in]  verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_mac_sync_master_logic_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return log verbosity
 */
mlag_verbosity_t
mlag_mac_sync_master_logic_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits mlag mac sync master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_init(void)
{
    int err = 0;
    int i;
    cl_status_t cl_status = 0;

    if (is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "mac sync master logic init called twice\n");
    }

    cl_pool_construct(&cookie_pool);

    err = cl_pool_init(&cookie_pool, MIN_FDB_ENTRIES, MAX_FDB_ENTRIES,
                       0, sizeof(struct master_logic_data), NULL, NULL,
                       NULL);
    is_started = 0;
    is_inited = 1;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        peer_state[i] = PEER_DOWN;
    }

    int num_of_vlan_port_system_flush_fsm = NUM_OF_VID_PORT_SYSTEM_FLUSH_FSM;
    int num_of_port_vlan_flush_fsm = NUM_OF_VID_PORT_FLUSH_FSM;
    cl_status = cl_list_init(&vlan_port_system_flush_fsm_pool,
                             num_of_vlan_port_system_flush_fsm);
    if (cl_status != 0) {
        MLAG_LOG(MLAG_LOG_ERROR, "Fail to allocate fsm pool\n");
        MLAG_BAIL_ERROR(cl_status);
    }

    cl_status =
        cl_list_init(&vlan_port_flush_fsm_pool,  num_of_port_vlan_flush_fsm);
    if (cl_status != 0) {
        MLAG_LOG(MLAG_LOG_ERROR, "Fail to allocate fsm pool\n");
        MLAG_BAIL_ERROR(cl_status);
    }

    for (i = 0; i < num_of_vlan_port_system_flush_fsm; i++) {
        struct flush_fsm_item  *entry = NULL;
        /* Allocate FSM and insert it to the free pool*/
        entry =
            (struct flush_fsm_item  *)cl_malloc(sizeof(struct flush_fsm_item));
        if (entry == NULL) {
            /* Allocate fsm state machine memory error */
            MLAG_LOG(MLAG_LOG_ERROR,
                     "Allocate fsm state machine memory error\n");
            MLAG_BAIL_ERROR(cl_status);
        }

        entry->filter = 0;
        cl_list_insert_tail(&vlan_port_system_flush_fsm_pool, entry);
    }

    for (i = 0; i < num_of_port_vlan_flush_fsm; i++) {
        struct flush_fsm_item  *entry = NULL;
        /* Allocate FSM and insert it to the free pool*/
        entry =
            (struct flush_fsm_item  *)cl_malloc(sizeof(struct flush_fsm_item));
        if (entry == NULL) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "Allocate fsm state machine memory error\n");
            MLAG_BAIL_ERROR(cl_status);
        }
        entry->filter = 0;
        cl_list_insert_tail(&vlan_port_flush_fsm_pool, entry);
    }

    /* Allocate the Maps */
    cl_qmap_init(&vlan_ports_system_flush_fsm_map);


    MLAG_LOG(MLAG_LOG_NOTICE, "Master logic initialized\n");

bail:
    return err;
}

/**
 *  This function de-inits mlag mac sync master_logic sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_deinit(void)
{
    int err = 0;
    cl_list_iterator_t itor;
    cl_map_item_t *map_item = NULL;
    const cl_map_item_t *map_end = NULL;

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync master logic deinit called before init\n");
    }
    err = mlag_mac_sync_master_logic_stop(NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed to stop master logic, err %d\n", err);
    cl_pool_destroy(&cookie_pool);

    /* move all flush fsm to idle and free the memory*/
    itor = cl_list_head(&vlan_port_system_flush_fsm_pool);
    while (itor != cl_list_end(&vlan_port_system_flush_fsm_pool)) {
        struct flush_fsm_item *flush_fsm =
            (struct flush_fsm_item * )(cl_list_obj(itor));

        /* Stop  Flush FSM*/
        err = mlag_mac_sync_flush_fsm_stop_ev(&flush_fsm->fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "master logic stop: Failed to stop flash process of pool 1, err %d\n",
                            err);


        cl_free(flush_fsm);
        itor = cl_list_next(itor);
    }

    itor = cl_list_head(&vlan_port_flush_fsm_pool);
    while (itor != cl_list_end(&vlan_port_flush_fsm_pool)) {
        struct flush_fsm_item *flush_fsm =
            (struct flush_fsm_item * )(cl_list_obj(itor));

        /* Stop Flush FSM*/
        err = mlag_mac_sync_flush_fsm_stop_ev(&flush_fsm->fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "master logic stop: Failed to stop flash process of pool 2, err %d",
                            err);


        cl_free(flush_fsm);
        itor = cl_list_next(itor);
    }

    map_item = cl_qmap_head(&vlan_ports_system_flush_fsm_map);
    map_end = cl_qmap_end(&vlan_ports_system_flush_fsm_map);
    while (map_item != map_end) {
        struct flush_fsm_item *flush_fsm = PARENT_STRUCT(map_item,
                                                         struct flush_fsm_item,
                                                         map_item);

        /* Stop Flush FSM*/
        err = mlag_mac_sync_flush_fsm_stop_ev(&flush_fsm->fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "master logic stop: Failed to stop flash process in map, err %d",
                            err);


        cl_free(flush_fsm);

        map_item = cl_qmap_next(map_item);
    }

    is_started = 0;
    is_inited = 0;

bail:
    return err;
}

/**
 *  This function
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
flush_fsm_timer_cb(void *data)
{
    int err = 0;
    struct timer_event_data timer_data;

    timer_data.data = data;
    err = send_system_event( MLAG_FLUSH_FSM_TIMER, &timer_data,
                             sizeof(timer_data));
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending timer event for flush process\n");

bail:
    return;
}
/*
 * This function is a timer scheduling service for FSM
 *
 * @param[in] timeout - msec timeout
 * @param[in] data - data to be used when timer expires
 * @param[out] timer_handler - timer handler returned by timer service
 *
 * @return 0 if operation completes successfully.
 */
static int
flush_sched_func(int timeout, void *data, void ** timer_handler)
{
    int err = 0;
    cl_status_t cl_err;
    cl_timer_t *concrete_timer_handler = NULL;
    ASSERT(timer_handler != NULL);

    concrete_timer_handler = (cl_timer_t *)cl_malloc(sizeof(cl_timer_t));
    if (concrete_timer_handler == NULL) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err,
                            "no memory upon allocation timer for flush sm, err %d\n",
                            err);
    }

    /* Init timer */
    cl_err = cl_timer_init(concrete_timer_handler, flush_fsm_timer_cb, data);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to init timer for flush sm, err %d\n",
                            err);
    }

    cl_err = cl_timer_start(concrete_timer_handler, timeout);
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to start timer for flush sm, err %d\n",
                            err);
    }

bail:
    *timer_handler = concrete_timer_handler;
    return err;
}

/*
 * This function is a timer unscheduling service for FSM
 *
 * @param[in] timer_handler - timer handler as returned by timer service
 *
 * @return 0 if operation completes successfully.
 */
static int
flush_unsched_func(void ** timer_handler)
{
    int err = 0;
    cl_status_t cl_err;
    ASSERT(*timer_handler != NULL);
    cl_timer_stop(*timer_handler);

    /* Destroy timer */
    cl_timer_destroy(*timer_handler);

    cl_err = cl_free(*timer_handler);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR_MSG(err, "Failed to un-schedule timer, err %d\n", err);
    }

bail:
    return err;
}
/**
 *  This function starts mlag mac sync master_logic sub-module
 *
 *  @param[in]  data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_start(uint8_t *data)
{
    int err = 0;
    int i;
    cl_list_iterator_t itor;
    UNUSED_PARAM(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync master logic start called before init\n");
    }
    if (is_started) {
        goto bail;
    }


    is_started = 1;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        peer_state[i] = PEER_DOWN;
    }

    itor = cl_list_head(&vlan_port_system_flush_fsm_pool);
    while (itor != cl_list_end(&vlan_port_system_flush_fsm_pool)) {
        struct flush_fsm_item *flush_fsm =
            (struct flush_fsm_item * )(cl_list_obj(itor));

        /* Init Flush FSM*/
        err = mlag_mac_sync_flush_fsm_init(&flush_fsm->fsm,
                                           NULL /*flush_fsm_user_trace*/,
                                           flush_sched_func,
                                           flush_unsched_func);

        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to init flush sm from 1 pool, err %d\n",
                            err);

        memset(&flush_fsm->filter, 0xF, sizeof(flush_fsm->filter));
        itor = cl_list_next(itor);
    }

    itor = cl_list_head(&vlan_port_flush_fsm_pool);
    while (itor != cl_list_end(&vlan_port_flush_fsm_pool)) {
        struct flush_fsm_item *flush_fsm =
            (struct flush_fsm_item * )(cl_list_obj(itor));

        /* Init Flush FSM*/
        err = mlag_mac_sync_flush_fsm_init(&flush_fsm->fsm,
                                           NULL /*flush_fsm_user_trace*/,
                                           flush_sched_func,
                                           flush_unsched_func);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to init flush sm from 2 pool, err %d\n",
                            err);

        memset(&flush_fsm->filter, 0xF, sizeof(flush_fsm->filter));
        itor = cl_list_next(itor);
    }

bail:
    MLAG_LOG(MLAG_LOG_INFO, "Master logic started err %d", err);

    return err;
}

/**
 *  This function stops mlag master election module
 *
 *  @param[in]  data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync master logic stop called before start\n");
    }

    err = mlag_mac_sync_master_logic_flush_stop(NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed to stop flush sm, err %d\n", err);

    is_started = 0;

    MLAG_LOG(MLAG_LOG_NOTICE, "Master logic stopped");

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
mlag_mac_sync_master_logic_sync_start(struct sync_event_data *data)
{
    int err = 0;
    ASSERT(data);

    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "sync start event accepted before start\n");
    }
    if (!(((data->peer_id >= 0) && (data->peer_id < MLAG_MAX_PEERS)) &&
          (data->state == 0))) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameters in sync start event: peer_id=%d, state=%d\n",
                            data->peer_id, data->state);
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "mlag_mac_sync_master_logic_sync_start: peer_id=%d\n",
             data->peer_id);

bail:
    return err;
}

/**
 *  This function handles sync finish event from the peer
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_sync_finish(struct sync_event_data *data)
{
    int err = 0;
    struct sync_event_data sync_done;

    ASSERT(data);

    if (!is_started) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "mlag_mac_sync_master_logic_sync_finish: peer_id=%d, state=%d, ignore event because of master logic not started\n",
                 data->peer_id, data->state);
        goto bail;
    }
    if (!(((data->peer_id >= 0) && (data->peer_id < MLAG_MAX_PEERS))
          )) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameters in sync finish event: peer_id=%d\n",
                            data->peer_id);
    }

    peer_state[data->peer_id] = PEER_ENABLE; /* TODO : PEER_TX_ENABLE; */

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Send sync done for mac sync peer_id=%d, state=%d\n",
             data->peer_id, data->state);

    /* Send MLAG_PEER_SYNC_DONE to MLag protocol manager */
    sync_done.peer_id = data->peer_id;
    sync_done.state = TRUE;
    sync_done.sync_type = MAC_PEER_SYNC;
    err =
        send_system_event(MLAG_PEER_SYNC_DONE, &sync_done, sizeof(sync_done));

    MLAG_BAIL_ERROR_MSG(err, "Failed to send event peer_done, err %d\n", err);

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
mlag_mac_sync_master_logic_peer_enable(struct peer_state_change_data *data)
{
    int err = 0;

    ASSERT(data);
    struct sync_event_data ev;
    if (!is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "peer enable event accepted before start\n");
    }
    if (!(((data->mlag_id >= 0) && (data->mlag_id < MLAG_MAX_PEERS)) &&
          (data->state == PEER_ENABLE))) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameters in sync enable event: mlag_id=%d, state=%d\n",
                            data->mlag_id, data->state);
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "mlag_mac_sync_master_logic_peer_enable event: mlag_id=%d\n",
             data->mlag_id);

    mlag_mac_sync_inc_cnt(MAC_SYNC_PEER_ENABLE_EVENTS_RCVD);

    peer_state[data->mlag_id] = PEER_ENABLE;
    ev.peer_id = data->mlag_id;
    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT, (void *)&ev, sizeof(ev),
        data->mlag_id, MASTER_LOGIC);
    MLAG_BAIL_ERROR_MSG(err, "Failed to send event master_sync_done, err %d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles peer status change event: only down state is interesting
 *
 *  @param[in]  data - event data
 *
 * @return int as error code.
 */
int
mlag_mac_sync_master_logic_peer_status_change(
    struct peer_state_change_data *data)
{
    int err = 0;
    ASSERT(data);
    /* Master Logic updates peer status to down */
    peer_state[data->mlag_id] = PEER_DOWN;
    err = ctrl_learn_api_get_uc_db_access( _master_process_peer_down_cb, data);

    MLAG_BAIL_ERROR_MSG(err, "Failed to process peer down in master err %d\n",
                        err);

bail:
    return err;
}


/**
 *  This function handles global flush cased by MLAG port global state changed to down
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */

int
mlag_mac_sync_master_logic_global_flush_start(uint8_t *data)
{
    int err = 0, i;
    mlag_mac_sync_flush_fsm *fsm = NULL;

    ASSERT(data);
    struct port_global_state_event_data *inc_msg =
        (struct port_global_state_event_data *)data;

    struct mac_sync_flush_peer_sends_start_event_data msg;

    if (!is_started) {
        goto bail;
    }

    if (inc_msg->port_num > MLAG_MAX_PORTS) {
        err = -EPERM;
        MLAG_BAIL_ERROR_MSG(err, "Global flush start: illegal port, err %d \n",
                            err);
    }

    for (i = 0; i < inc_msg->port_num; i++) {
        if (inc_msg->state[i] == MLAG_PORT_GLOBAL_DOWN) {
            msg.gen_data.filter.filter_by_log_port =
                FDB_KEY_FILTER_FIELD_VALID;
            msg.gen_data.filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;
            msg.gen_data.filter.log_port = inc_msg->port_id[i];
            msg.gen_data.non_mlag_port_flush = 0;
            msg.gen_data.peer_originator = 0;
            msg.number_mac_params = 0;


            MLAG_LOG(MLAG_LOG_INFO,
                     "Master logic global flush start vid [%s] [%u] log_port [%s] [%lu]\n",
                     (msg.gen_data.filter.filter_by_vid ==
                      FDB_KEY_FILTER_FIELD_VALID) ? "Valid" : "Not Valid",
                     msg.gen_data.filter.vid,
                     (msg.gen_data.filter.filter_by_log_port ==
                      FDB_KEY_FILTER_FIELD_VALID) ? "Valid" : "Not Valid",
                     msg.gen_data.filter.log_port);

            /* get available flush fsm */
            err = mlag_mac_sync_master_logic_get_available_flush_fsm(
                &msg.gen_data, 1, &fsm);
            if (err == -ENOMEM) {
                non_available_memory_fsm_cnt++;
                MLAG_LOG(MLAG_LOG_NOTICE,
                         "Global flush - No available fsm non_available_memory_fsm_cnt [%du]\n",
                         non_available_memory_fsm_cnt);
                err = 0;
                continue;
            }
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in get available sm for global flush process, err %d \n",
                                err);

            if (FALSE == mlag_mac_sync_flush_fsm_idle_in(fsm)) {
                MLAG_LOG(MLAG_LOG_INFO,
                         "Global MLAG port state is already processed by Master\n");
            }
            else {
                MLAG_LOG(MLAG_LOG_INFO,
                         "Global MLAG port state processed by Master\n");

                err = mlag_mac_sync_flush_fsm_start_ev(fsm, (void *)&msg);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to start sm for global flush, err %d \n",
                                    err);
            }
        }
    }

bail:
    return err;
}

/**
 *  This function get fsm from the internal map . If a fsm is not exis it and allocate a new one from the pool
 *
 *  @param[in]  p_map - event data
 * @param[in] allocate - allocate new fsm
 *
 *  @param[in]  free_pool - if pool is NULL then no new fsm will be retrived if case its not exist
 *  @param[in]  key
 *  @param[out]  fsm
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_get_fsm(cl_qmap_t * const p_map,
                                   IN cl_list_t * const free_pool,
                                   uint64_t key, int allocate,
                                   mlag_mac_sync_flush_fsm **fsm)
{
    int err = 0;

    cl_map_item_t *map_item = NULL;
    const cl_map_item_t *map_end = NULL;

    struct flush_fsm_item  *fsm_item = NULL;

    /* 1. Is Region Found for requested priority? */

    map_item = cl_qmap_get(p_map, key);
    map_end = cl_qmap_end(p_map);
    if (map_item != map_end) {
        /* Found */
        fsm_item = PARENT_STRUCT(map_item, struct flush_fsm_item, map_item);
        err = 0;
        *fsm = &fsm_item->fsm;
    }
    else {
        *fsm = NULL;

        if ((free_pool != NULL) && (allocate == 1)) {
            /* get from free pool */
            fsm_item = cl_list_remove_head(free_pool);
            if (NULL == fsm_item) {
                err = -ENOMEM;
                goto out;
            }

            fsm_item->filter = key;

            cl_qmap_insert(p_map, fsm_item->filter, &fsm_item->map_item);
            flash_fsm_in_qmap_cnt++;

            *fsm = &fsm_item->fsm;
            fsm_item->fsm.key = key;
        }
        else {
            err = -ENOENT;
        }
    }

out:
    return err;
}

/**
 *  This function handles global flush initiated by peer manager
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_flush_start(uint8_t *data)
{
    int err = 0;
    struct mac_sync_flush_peer_sends_start_event_data * msg = NULL;
    mlag_mac_sync_flush_fsm *fsm = NULL;

    ASSERT(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "flush start called before init\n");
    }
    if (!is_started) {
        goto bail;
    }

    msg = (struct mac_sync_flush_peer_sends_start_event_data *)data;

    MLAG_LOG(MLAG_LOG_INFO,
             "Master logic Flush start vid [%s] [%u] log_port [%s] [%lu]\n",
             (msg->gen_data.filter.filter_by_vid == FDB_KEY_FILTER_FIELD_VALID) ? "Valid" : "Not Valid",
             msg->gen_data.filter.vid,
             (msg->gen_data.filter.filter_by_log_port ==
              FDB_KEY_FILTER_FIELD_VALID) ? "Valid" : "Not Valid",
             msg->gen_data.filter.log_port);

    /* get available flush fsm */
    err = mlag_mac_sync_master_logic_get_available_flush_fsm(&msg->gen_data, 1,
                                                             &fsm);
    if (err == -ENOMEM) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Flush - No available sm for flush, cnt [%d]\n",
                 non_available_memory_fsm_cnt);
        err = 0;
        goto bail;
    }

    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get available sm for peer initiated flush, err %d \n",
                        err);
    if (FALSE == mlag_mac_sync_flush_fsm_idle_in(fsm)) {
        MLAG_LOG(MLAG_LOG_INFO, "Flush is already processed\n");
    }
    else {
        MLAG_LOG(MLAG_LOG_INFO, "Flush start ,num macs %d\n",
                 msg->number_mac_params);

        err = mlag_mac_sync_flush_fsm_start_ev(fsm,  (void *)msg);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to start sm for peer initiated flush, err %d \n",
                            err);
    }

bail:
    return err;
}


/**
 *  This function handles flush stop
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_flush_stop(uint8_t *data)
{
    int err = 0;
    cl_list_iterator_t itor;
    cl_map_item_t *map_item = NULL;

    const cl_map_item_t *map_end = NULL;

    UNUSED_PARAM(data);

    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "flush stop called before init\n");
    }
    if (!is_started) {
        goto bail;
    }
    /* move all flush fsm to idle */
    itor = cl_list_head(&vlan_port_system_flush_fsm_pool);
    while (itor != cl_list_end(&vlan_port_system_flush_fsm_pool)) {
        struct flush_fsm_item *flush_fsm =
            (struct flush_fsm_item * )(cl_list_obj(itor));

        /* Stop  Flush FSM*/
        err = mlag_mac_sync_flush_fsm_stop_ev(&flush_fsm->fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to stop flush sm in pool 1, err %d \n",
                            err);
        itor = cl_list_next(itor);
    }

    itor = cl_list_head(&vlan_port_flush_fsm_pool);
    while (itor != cl_list_end(&vlan_port_flush_fsm_pool)) {
        struct flush_fsm_item *flush_fsm =
            (struct flush_fsm_item * )(cl_list_obj(itor));

        /* Stop Flush FSM*/
        err = mlag_mac_sync_flush_fsm_stop_ev(&flush_fsm->fsm);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to stop flush sm in pool 2, err %d \n",
                            err);

        itor = cl_list_next(itor);
    }

    map_item = cl_qmap_head(&vlan_ports_system_flush_fsm_map);
    map_end = cl_qmap_end(&vlan_ports_system_flush_fsm_map);
    while (map_item != map_end) {
        struct flush_fsm_item *flush_fsm = PARENT_STRUCT(map_item,
                                                         struct flush_fsm_item,
                                                         map_item);
        /* Stop Flush FSM*/
        err = mlag_mac_sync_flush_fsm_stop_ev(&flush_fsm->fsm);
        MLAG_BAIL_ERROR_MSG(err, "Failed to stop flush sm in map, err %d \n",
                            err);

        map_item = cl_qmap_next(map_item);
    }

bail:
    return err;
}

/**
 *  This function handles flush ACK from Peer
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_flush_ack(uint8_t *data)
{
    int err = 0;
    struct mac_sync_flush_peer_ack_event_data * msg = NULL;
    struct mlag_mac_sync_flush_fsm  * flush_fsm = NULL;

    ASSERT(data);


    if (!is_started) {
        goto bail;
    }


    msg = (struct mac_sync_flush_peer_ack_event_data *)data;

    /* get relevant fsm */
    err = mlag_mac_sync_master_logic_get_available_flush_fsm(&msg->gen_data, 0,
                                                             &flush_fsm);
    if (err == -ENOENT) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "no available flush sm for Ack %d %d %lu, %d\n",
                 msg->gen_data.filter.filter_by_log_port,
                 msg->gen_data.filter.filter_by_vid,
                 msg->gen_data.filter.log_port,
                 msg->gen_data.filter.vid);
        err = 0;
        goto bail;
    }
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get flush sm for peer's Ack %lu, err %d \n",
                        msg->gen_data.filter.log_port,
                        err);


    err = mlag_mac_sync_flush_fsm_peer_ack(flush_fsm,  msg->peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sm processing peer's Ack, err %d \n",
                        err);

bail:
    return err;
}


/**
 *  This function handles timers in flush FSM
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_flush_fsm_timer(uint8_t *data)
{
    int err = 0;
    ASSERT(data);
    struct timer_event_data * timer_data = ( struct timer_event_data *) data;
    if (!is_started) {
        mlag_mac_sync_inc_cnt(MAC_SYNC_TIMER_IN_WRONG_STATE_EVENT);
        goto bail;
    }

    err = fsm_timer_trigger_operation(timer_data->data );
    MLAG_BAIL_ERROR_MSG(err,
                        "Error in flush sm upon process of timer operation %d \n",
                        err);

bail:
    return err;
}

/**
 *  This function handles local learn message from the Peer
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */


int
mlag_mac_sync_master_logic_local_learn(void *data)
{
    int err = 0;
    ASSERT(data);
    struct mac_sync_multiple_learn_event_data *msg =
        (struct mac_sync_multiple_learn_event_data *)data;

    if (!is_started) {
        goto bail;
    }

    gl_originator_buff.num_msg = 0;
    gl_originator_buff.opcode = MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT;

    gl_all_buff.num_msg = 0;
    gl_all_buff.opcode = MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT;

    gl_remote_buff.num_msg = 0;
    gl_remote_buff.opcode = MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT;

    if (peer_state[msg->msg.originator_peer_id] == PEER_ENABLE) {
        err = ctrl_learn_api_get_uc_db_access( _master_process_local_learn_cb,
                                               data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed  in master in process local learn callback, err %d \n",
                            err);
    }
    else {
        MLAG_LOG(MLAG_LOG_INFO, "master ignores LL :peer %d disabled\n",
                 msg->msg.originator_peer_id);
    }
    /* process all global learn buffers - send them to appropriate peers*/
    err = send_global_learn_buffers();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to send global learn messages from buffers, err %d \n",
                        err);
bail:
    return err;
}

/*
 *  This function handles local learn callback
 *
 *  @param[in]  user_data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
static int
_master_process_local_learn_cb(void* user_data)
{
    int err = 0;
    int i;
    int flush_busy = 0;

    ASSERT(user_data);

    struct mac_sync_multiple_learn_buffer *msg =
        (struct mac_sync_multiple_learn_buffer *)user_data;

    struct fdb_uc_mac_addr_params mac_entry;
    static struct mac_sync_multiple_learn_buffer gl_buff;

    gl_buff.num_msg = 0;
    gl_buff.opcode = MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT;


    MLAG_LOG(MLAG_LOG_INFO,
             "master receives local learn %d messages, orig %d \n",
             msg->num_msg, msg->msg[0].originator_peer_id );

    for (i = 0; i < msg->num_msg; i++) {
        memcpy(&mac_entry.mac_addr_params, &msg->msg[i].mac_params,
               sizeof(struct oes_fdb_uc_mac_addr_params));
        mac_entry.cookie = 0;

        err = is_flush_busy(&msg->msg[i], &flush_busy);
        MLAG_BAIL_ERROR_MSG(err, "Failed in func is_flush_busy, err %d \n",
                            err);

        if (flush_busy) {
            /* goto bail; */
            continue; /* for this mac flush is busy*/
        }
        mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_EVENT);
        mlag_mac_sync_inc_cnt(MASTER_RX);
        err = generic_get_entry(&mac_entry);

        if ((err == 0) && mac_entry.cookie) {  /* Learning existed mac*/
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_MIGRATE_EVENT);
            err = process_local_learn_existed_mac(mac_entry.cookie,
                                                  &msg->msg[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in process local learn message for existed mac, err %d \n",
                                err);
        }
        else if ((err != 0) && (err != -ENOENT)) {
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in processing local learn message for existed mac, err %d \n",
                                err);
        }
        else { /* (err = -ENOENT <= new mac ) ||
                     (err == 0 but cookie == null )<= static mac from Auto learn mode  */
               /* now build array of newly learned macs  that would be written at once by peer*/
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_NEW_EVENT);
            gl_buff.msg[gl_buff.num_msg].originator_peer_id =
                msg->msg[i].originator_peer_id;
            gl_buff.msg[gl_buff.num_msg].port_cookie = msg->msg[i].port_cookie;

            memcpy(&gl_buff.msg[gl_buff.num_msg].mac_params,
                   &msg->msg[i].mac_params,
                   sizeof(struct oes_fdb_uc_mac_addr_params));

            gl_buff.num_msg++;
        }
    } /* end of for loop*/

    err = process_new_macs_set_to_peer(&gl_buff);
    MLAG_BAIL_ERROR_MSG(err,
                        "master failed in processing local learn message for new Macs, err %d \n",
                        err);


bail:
    return err;
}
/*
 *  This function tries to get entry from control learning DB and from Router MAC DB
 *
 *  @param[in/out]  mac_entry
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
generic_get_entry(struct fdb_uc_mac_addr_params *mac_entry)
{
    int err = 0;
    unsigned short data_cnt = 1;
    struct fdb_uc_key_filter filter;
    struct router_db_entry *router_mac_entry = NULL;

    filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
    filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;
    err = ctrl_learn_api_uc_mac_addr_get(
        OES_ACCESS_CMD_GET,
        &filter,
        mac_entry,
        &data_cnt,
        0
        );
    if (err) {
        if (err == -ENOENT) { /* try to search from router mac DB*/
            err = mlag_mac_sync_router_mac_db_get(
                mac_entry->mac_addr_params.mac_addr,
                mac_entry->mac_addr_params.vid,
                &router_mac_entry
                );
            if (err) {
                goto bail;
            }
            mac_entry->entry_type = FDB_UC_STATIC;
            mac_entry->cookie = router_mac_entry->cookie;
        }
        else {
            goto bail;
        }
    }

bail:
    return err;
}


int
process_new_macs_set_to_peer( struct mac_sync_multiple_learn_buffer * gl_buff)
{
    int i, num_ok = 0;
    int err = 0;

    struct fdb_uc_mac_addr_params mac_entry;


    struct mac_sync_multiple_learn_buffer dup_buff; /* peer manager modifies buffer that  sent to remote peer*/
    if (gl_buff->num_msg == 0) {
        goto bail;
    }
    memcpy(&dup_buff, gl_buff, sizeof(struct mac_sync_multiple_learn_buffer));
    /*TODO copy only relevant data*/

    err = mlag_mac_sync_peer_mngr_global_learned((void*)&dup_buff, 0);
    MLAG_LOG(MLAG_LOG_INFO, "master wrote bulk %d macs ,err = %d \n",
             gl_buff->num_msg, err);
    /* continue even if the error is not 0*/

    /*iterate all written to further  process successfully written mac entries */
    for (i = 0; i < gl_buff->num_msg; i++) {
        memcpy(&mac_entry.mac_addr_params, &gl_buff->msg[i].mac_params,
               sizeof(struct oes_fdb_uc_mac_addr_params));

        err = generic_get_entry(&mac_entry);

        if ((err == 0) && mac_entry.cookie /*&& (data_cnt == 1)*/) {
            num_ok++;
            mlag_mac_sync_inc_cnt(MASTER_TX);
            err = process_local_learn_new_mac
                      (mac_entry.cookie, &gl_buff->msg[i] );
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in low level processing of local learn for new mac, err %d \n",
                                err);
        }
        else {
            MLAG_LOG(MLAG_LOG_INFO,
                     "master denied new mac (%d): %02x:%02x:%02x:%02x:%02x:%02x \n", i,
                     PRINT_MAC(mac_entry));
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARN_REGECTED_BY_MASTER);
        }
    }
    err = 0; /* return err =0 on the process to ignore faults on some  specific mac*/

bail:
    if (gl_buff->num_msg) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "Process new mac :in message  %d macs , conf %d, err = %d \n",
                 gl_buff->num_msg, num_ok, err);
    }
    return err;
}


/*
 *  This function handles local age callback
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
static int
_master_process_local_aged_cb(void* user_data)
{
    int err = 0;
    int i = 0, num_ok = 0;
    struct mac_sync_multiple_age_buffer *msg = NULL;

    static struct fdb_uc_mac_addr_params
        mac_entry;
    ASSERT(user_data);
    global_age_buffer.num_msg = 0;

    msg = (struct mac_sync_multiple_age_buffer *)user_data;

    mlag_mac_sync_inc_cnt_num(MAC_SYNC_LOCAL_AGED_EVENT, msg->num_msg);
    mlag_mac_sync_inc_cnt_num(MASTER_RX, msg->num_msg);


    for (i = 0; i < msg->num_msg; i++) {
        memcpy(&mac_entry.mac_addr_params, &msg->msg[i].mac_params,
               sizeof(struct oes_fdb_uc_mac_addr_params));

        err = generic_get_entry(&mac_entry);

        if (err == -ENOENT) {
            mlag_mac_sync_inc_cnt(MAC_SYNC_WRONG_1_LOCAL_AGED_EVENT);
            MLAG_LOG(MLAG_LOG_INFO,
                     "master wrong mac : %02x:%02x:%02x:%02x:%02x:%02x \n",
                     PRINT_MAC(mac_entry));
            err = 0;
            continue;
        }
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in processing of local age: error get entry from the DB, err %d \n",
                            err);
        if (mac_entry.cookie) {
            num_ok++;
            err = process_local_aged(mac_entry.cookie,  &msg->msg[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in low level processing of local aged, err %d \n",
                                err);
        }
        else {
            /* MLAG_LOG(MLAG_LOG_ERROR, "master:  local aged MAC not found in FDB ");*/
            mlag_mac_sync_inc_cnt(MAC_SYNC_WRONG_2_LOCAL_AGED_EVENT);
            continue;
        }
    }
    err = send_global_age_buffer();
    MLAG_BAIL_ERROR_MSG(err, "Failed in send_global_age_buffer, err %d \n",
                        err);

bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "Process Local Age in message  %d macs , OK %d, err = %d \n",
             msg->num_msg, num_ok, err);
    return err;
}





/**
 *  This function handles local age message from the Peer
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_local_aged(void *data)
{
    int err = 0;
    struct mac_sync_fixed_age_event_data *msg = NULL;

    ASSERT(data);
    msg = (struct mac_sync_fixed_age_event_data *)data;

    if (!is_started) {
        goto bail;
    }

    if (peer_state[msg->msg.originator_peer_id] == PEER_ENABLE) {
        err = ctrl_learn_api_get_uc_db_access( _master_process_local_aged_cb,
                                               data);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in processing of local age message, err %d \n",
                            err);
    }
bail:
    return err;
}


/**
 *  This function handles FDB export message from the Peer
 *
 *  @param[in]  data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_fdb_export(uint8_t *data)
{
    int err = 0;
    int size = 0;
    struct mac_sync_master_fdb_export_event_data  *resp_msg = NULL;
    ASSERT(data);
    struct mac_sync_mac_sync_master_fdb_get_event_data *rx_msg =
        (struct mac_sync_mac_sync_master_fdb_get_event_data *)data;
    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "fdb export called before init\n");
    }

    resp_msg =
        (struct mac_sync_master_fdb_export_event_data *)fdb_export_data_block;
    resp_msg->num_entries = 0; /* init fdb export message */

    err =
        ctrl_learn_api_get_uc_db_access( _process_fdb_export_request_cb, data);
    MLAG_BAIL_ERROR_MSG(err, "Failed in process fdb_export , err %d \n",
                        err)

    /* access built message , calculate size and send it*/
    err = fdb_export_add_router_macs(rx_msg->peer_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed in fdb_export for router macs, err %d \n",
                        err);
    resp_msg =
        (struct mac_sync_master_fdb_export_event_data *)fdb_export_data_block;


    size = sizeof(struct mac_sync_master_fdb_export_event_data) -
           sizeof(struct mac_sync_learn_event_data);
    if (resp_msg->num_entries > 0) {
        size += sizeof(struct mac_sync_learn_event_data) *
                (resp_msg->num_entries);
    }
    MLAG_LOG(MLAG_LOG_NOTICE,
             "Send FDB EXPORT(opcode %d): %d messages to peer %d ,size = %d\n",
             resp_msg->opcode, resp_msg->num_entries, rx_msg->peer_id, size);
    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT, (void *)resp_msg, size,
        rx_msg->peer_id, MASTER_LOGIC);
    MLAG_BAIL_ERROR_MSG(err, "Failed in send fdb_export message , err %d \n"
                        , err);

bail:
    return err;
}


/**
 *  This function performs actions upon cookie allocated/deallocated
 * @param[in] oper - operation :allocate or de-allocate
 * @param[in/out] cookie
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_cookie_func( int oper,   void ** cookie )
{
    int err = 0;
    ASSERT(cookie);
    if (oper == COOKIE_OP_INIT) {
        /* copy cookie to the static variable*/

        *cookie = cl_pool_get(&cookie_pool);
        if (*cookie == NULL) {
            err = -ENOMEM;
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to allocate  master instance from the pool, err %d \n",
                                err);
        }
        master_cookie = *cookie;
    }
    else if (oper == COOKIE_OP_DEINIT) {
        if (*cookie == NULL) {
            goto bail;
        }
        cl_pool_put(&cookie_pool, *cookie);
        *cookie = NULL;
    }
bail:
    return err;
}

/*
 *  This function called in case MAC successfully configured to the local Peer first time
 *  Function initializes cookie structure and notifies remote peers about global learn
 *
 *  @param[in/out]  cookie    - cookie of the Master
 *  @param[in]  msg_data  - Local learn message from the Peer
 *
 * @return current status value
 */
int
process_local_learn_new_mac(void *cookie,
                            struct mac_sync_learn_event_data *msg_data)
{
    int err = 0;
    struct master_logic_data *master_data;

    ASSERT(cookie);
    ASSERT(msg_data);

    master_data = (struct master_logic_data *)cookie;
    err = _local_learn_update_master_and_peers(master_data, msg_data,
                                               SEND_TO_REMOTE_PEERS);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in updating master DB for local learn of the new Mac, err %d\n",
                        err);
    MLAG_LOG(MLAG_LOG_INFO,
             "master: learn new Mac completed type %d, port %d\n",
             msg_data->mac_params.entry_type, msg_data->port_cookie);

bail:
    return err;
}

int
is_flush_busy(struct mac_sync_learn_event_data * msg_data, int *flush_busy)
{
    int err = 0;

    if (msg_data->mac_params.entry_type != FDB_UC_STATIC) {
        err = mlag_mac_sync_master_logic_is_flush_busy( &msg_data->mac_params,
                                                        msg_data->originator_peer_id,
                                                        flush_busy);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in getting flush_busy status, err %d \n",
                            err);
    }
bail:
    return err;
}




/**
 *  This function handles Local learn  from the Peer
 *  on the MAC that already exists in the Master DB
 *
 *  @param[in/out]  cookie    - cookie of the Master
 *  @param[in] msg_data  - Local learn message from the Peer
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
process_local_learn_existed_mac(void * cookie,
                                struct mac_sync_learn_event_data * msg_data)
{
    int err = 0;
    struct master_logic_data *master_data;
    struct timeval tv_start;


    ASSERT(cookie);
    ASSERT(msg_data);

    master_data = (struct master_logic_data *)cookie;

    if ((master_data->entry_type !=
         msg_data->mac_params.entry_type)
        &&
        (msg_data->mac_params.entry_type == FDB_UC_STATIC)
        ) {
        /* was previously configured dynamic and user configures static instead*/
        err = _local_learn_update_master_and_peers(master_data, msg_data,
                                                   SEND_TO_ALL_PEERS );
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in updating master DB for local learn of Mac that changed type to static , err %d \n",
                            err);
    }
    else if ((master_data->entry_type != msg_data->mac_params.entry_type)
             &&
             (master_data->entry_type == FDB_UC_STATIC)
             ) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "master: learn not allowed : transition from static to dynamic mac is not allowed ");
        goto bail;
        /* static-> dynamic not allowed */
    }
    else { /* entry type not modified */
        if (((master_data->peer_bmap & (1 << msg_data->originator_peer_id)) ==
             0)
            /* check whether locally learned MAC is from another  peer_id */
            &&
            (master_data->port == msg_data->mac_params.log_port)
            &&
            (master_data->port != NON_MLAG_PORT)
            ) { /* MAC locally learned on the other MLAG port - traffic*/
            err = _local_learn_update_master_and_peers(master_data, msg_data,
                                                       SEND_TO_ORIGINATOR_PEER);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in updating master DB for local learn of existed Mac , err %d \n",
                                err);
        }
        else if (((master_data->peer_bmap &
                   (1 << msg_data->originator_peer_id)))
                 &&
                 (master_data->port == msg_data->mac_params.log_port)
                 &&
                 (master_data->port != NON_MLAG_PORT)
                 ) {
            /*
               currently ignore this case  - it can be explained by slowness of writing to HW.
               and meantime next packet of same macs accepted in control learning block from the HW
             */
        }
        else {  /*MAC migration*/
            gettimeofday(&tv_start, NULL);
            if ((tv_start.tv_sec - master_data->timestamp) == 0) { /*simple de-bouncing logic*/
                MLAG_LOG(MLAG_LOG_INFO,
                         "Migration not allowed for peer %d port %lu type %d",
                         msg_data->originator_peer_id,
                         msg_data->mac_params.log_port,
                         msg_data->mac_params.entry_type);
                goto bail;
            }
            else { /* migration approved */
                err = _local_learn_update_master_and_peers(master_data,
                                                           msg_data,
                                                           SEND_TO_ALL_PEERS);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in updating master DB for local learn of migrated Mac , err %d \n",
                                    err);
            }
        }
    }
bail:
    return err;
}


/*
 *  This function handles Local Age  from the Peer
 *  on the MAC that already exists in the Master DB
 *
 *  @param[in/out]  cookie    - cookie of the Master
 *  @param[in]  msg_data  - Local age message from the Peer
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
process_local_aged(void * cookie, struct mac_sync_age_event_data * msg_data)
{
    int err = 0;
    struct master_logic_data *master_data;

    if (cookie == NULL) {
        if (msg_data) {
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "aged mac with null cookie: %02x:%02x:%02x:%02x:%02x:%02x \n",
                     PRINT_MAC_OES(msg_data->mac_params));
            goto bail;
        }
    }
    ASSERT(msg_data);
    mlag_mac_sync_inc_cnt(MAC_SYNC_PROCESS_LOCAL_AGED_EVENT);
    /*if (msg_data->mac_params.entry_type != FDB_UC_STATIC) {}*/

    master_data = (struct master_logic_data *)cookie;

    if (msg_data->originator_peer_id >= MLAG_MAX_PEERS) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameters in process of local aged event: originator_peer_id=%d\n",
                            msg_data->originator_peer_id);
    }


    master_data->peer_bmap &= ~(1 << msg_data->originator_peer_id);
    if (master_data->peer_bmap == 0) {
        memcpy(&global_age_buffer.msg[global_age_buffer.num_msg],
               msg_data, sizeof(*msg_data));
        global_age_buffer.num_msg++;
    }

bail:
    return err;
}

/*
 *  This function handles FDB export callback for router macs configured on the Master
 *  @param [in] peer_id   the id of a peer
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
fdb_export_add_router_macs(uint8_t peer_id)
{
    struct  mlag_master_election_status current_status;
    struct mac_sync_learn_event_data *glob_learn_msg = NULL;
    uint8_t my_peer_id = 0;
    int err = 0;
    struct mac_sync_master_fdb_export_event_data  *resp_msg = NULL;
    struct router_db_entry *item_p;
    struct router_db_entry *prev_item_p;

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in get status from master election  for router macs in fdb_export, err %d \n",
                        err);
    my_peer_id = current_status.my_peer_id;

    if (peer_id == my_peer_id) { /* no need to send this message to the local peer*/
        goto bail;
    }
    resp_msg =
        (struct mac_sync_master_fdb_export_event_data *)fdb_export_data_block;
    /* pass all router database and add Global learn message to the buffer
     * for each route found with Add operation*/

    err = mlag_mac_sync_router_mac_db_first_record(&item_p);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in getting first router mac record for fdb_export, err %d \n",
                        err);
    while (item_p) {
        if (item_p->last_action == ADD_ROUTER_MAC) {
            glob_learn_msg =
                &(resp_msg->entry) + resp_msg->num_entries;
            glob_learn_msg->port_cookie = 0;
            glob_learn_msg->mac_params.entry_type = FDB_UC_STATIC;
            glob_learn_msg->mac_params.log_port = NON_MLAG_PORT;
            memcpy(        &glob_learn_msg->mac_params.mac_addr,
                           &item_p->mac_addr,
                           sizeof(item_p->mac_addr));
            glob_learn_msg->mac_params.vid = item_p->vid;
            glob_learn_msg->originator_peer_id = my_peer_id;

            resp_msg->num_entries++;
        }
        prev_item_p = item_p;
        mlag_mac_sync_router_mac_db_next_record(
            prev_item_p, &item_p);
    }

bail:
    return err;
}

/*
 *  This function handles FDB export callback
 *  builds response message to the peer
 *  @param [in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
static int
_process_fdb_export_request_cb(void * data)
{
    /* get all FDB and build long message (all actions performed in callback under the mutex of FDB)
     * at the end return from the function(release mutex also) and send one long message to the peer
     * */
    int err = 0, i, cnt = 0;

    unsigned short data_cnt = 1;
    uint8_t my_peer_id = 0;
    struct mlag_master_election_status current_status;

    struct master_logic_data *master_data;
    struct mac_sync_master_fdb_export_event_data  *resp_msg;
    struct mac_sync_learn_event_data *glob_learn_msg = NULL;
    enum oes_access_cmd access_cmd;
    ASSERT(data);

    struct mac_sync_mac_sync_master_fdb_get_event_data *msg =
        (struct mac_sync_mac_sync_master_fdb_get_event_data *)data;


    struct fdb_uc_key_filter key_filter;
    struct fdb_uc_mac_addr_params mac_param_list[MAX_ENTRIES_IN_TRY];

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in get status from master election  for dynamic macs in fdb_export, err %d \n",
                        err);
    my_peer_id = current_status.my_peer_id;

    if (msg->peer_id == my_peer_id) { /* no need to send this message to the local peer*/
        goto bail;
    }
    resp_msg =
        (struct mac_sync_master_fdb_export_event_data *)&fdb_export_data_block[
            0];
    resp_msg->num_entries = 0;
    resp_msg->opcode = MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT;
    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;
    access_cmd = OES_ACCESS_CMD_GET_FIRST;
    data_cnt = 1;

    while (cnt < MAX_FDB_ENTRIES) {
        err = ctrl_learn_api_uc_mac_addr_get(
            access_cmd,
            &key_filter,
            mac_param_list,
            &data_cnt,
            0);

        if (err == -ENOENT) { /* the FDB is empty*/
            err = 0;
            goto bail;
        }
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in fdb_export callback :get mac from the control learning lib , err %d \n",
                            err);
        /*other errors are critical*/
        /*to Process received entries */
        cnt += data_cnt;
        if (data_cnt <= MAX_ENTRIES_IN_TRY) {
            for (i = 0; i < data_cnt; i++) {
                master_data =
                    (struct master_logic_data *)mac_param_list[i].cookie;
                if (master_data) {
                    /* if( !( master_data->peer_bmap &(1<<(msg->peer_id))) )*/
                    /* this is entry learned by other peer*/
                    {
                        glob_learn_msg =
                            &(resp_msg->entry) + resp_msg->num_entries;
                        /* glob_learn_msg->opcode  =  MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT;*/
                        glob_learn_msg->originator_peer_id = my_peer_id;
                        /*put some peer id  that not equal msg->peer_id */
                        memcpy(&glob_learn_msg->mac_params,
                               &mac_param_list[i].mac_addr_params,
                               sizeof(mac_param_list[0]));
                        glob_learn_msg->mac_params.entry_type =
                            mac_param_list[i].entry_type;
                        err = _correct_port_on_tx(
                            glob_learn_msg,
                            &mac_param_list[i].mac_addr_params );

                        resp_msg->num_entries++;
                    }
                }
            }
        }
        /* prepare to next cycle : copy last mac from current cycle at first place of array*/
        memcpy( &mac_param_list[0], &mac_param_list[data_cnt - 1],
                sizeof(mac_param_list[0]));
        access_cmd = OES_ACCESS_CMD_GET_NEXT;
        data_cnt = MAX_ENTRIES_IN_TRY;
    }

bail:
    return err;
}


/*
 *  This function handles Local learn  from the Peer
 *  on the MAC that already exists in the Master DB
 *
 *  @param[in] data - event data
 *
 *  @return 0 when successful, otherwise ERROR
 */
static int
_master_process_peer_down_cb(void* data)
{
    int err = 0;
    int i, cnt = 0;
    unsigned short data_cnt = 1;
    uint8_t my_peer_id = 0;
    struct mlag_master_election_status current_status;
    enum oes_access_cmd access_cmd;
    struct mac_sync_age_event_data msg_data;
    struct fdb_uc_key_filter key_filter;
    struct fdb_uc_mac_addr_params mac_param_list[MAX_ENTRIES_IN_TRY];

    ASSERT(data);
    struct peer_state_change_data *msg =
        (struct peer_state_change_data *)data;


    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in get status from master election  for processing of peer_down, err %d \n",
                        err);
    MLAG_LOG(MLAG_LOG_NOTICE, "Peer down event in Master\n");

    my_peer_id = current_status.my_peer_id;
    if (msg->mlag_id == my_peer_id) {
        /* no need to send this message to the local peer*/
        goto bail;
    }

    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;

    access_cmd = OES_ACCESS_CMD_GET_FIRST;
    data_cnt = 1;
    msg_data.originator_peer_id = msg->mlag_id;
    global_age_buffer.num_msg = 0;

    err = mlag_mac_sync_peer_check_init_delete_list();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in initialization of delete_list for processing of peer_down , err %d \n",
                        err);
    while (data_cnt != 0) {
        err = ctrl_learn_api_uc_mac_addr_get(
            access_cmd,
            &key_filter,
            mac_param_list,
            &data_cnt,
            0);
        if (err == -ENOENT) {
            /* the FDB is empty */
            err = 0;
            goto bail;
        }
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in peer_down procesing :get mac from the control learning lib , err %d \n",
                            err);
        /*other errors are critical*/
        /*to Process received entries */
        for (i = 0; i < data_cnt; i++) {
            cnt++;

            memcpy(&msg_data.mac_params, &mac_param_list[i].mac_addr_params,
                   sizeof(msg_data.mac_params));
            if (mac_param_list[i].entry_type != FDB_UC_STATIC) {
                err = process_local_aged(mac_param_list[i].cookie, &msg_data);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in process local aged upon peer_down, err %d \n",
                                    err);

                if (global_age_buffer.num_msg ==
                    (CTRL_LEARN_FDB_NOTIFY_SIZE_MAX)) {
                    err = send_global_age_buffer();
                    MLAG_BAIL_ERROR_MSG(err,
                                        "Failed to send global age buffer upon peer_down , err %d \n",
                                        err);
                }
            }
            else { /* 2. if entry == static and port == ipl*/
                err = mlag_mac_sync_peer_mngr_addto_delete_list_ipl_static_mac(
                    &mac_param_list[i].mac_addr_params);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed add macs to delete list upon peer_down , err %d \n",
                                    err);
            }
        }
        /* prepare to next cycle : copy last mac
         * from current cycle at first place of array*/
        memcpy(&mac_param_list[0], &mac_param_list[data_cnt - 1],
               sizeof(mac_param_list[0]));
        access_cmd = OES_ACCESS_CMD_GET_NEXT;
        data_cnt = MAX_ENTRIES_IN_TRY;
    }
bail:
    if (err == 0) {
        err = mlag_mac_sync_peer_process_delete_list();
        if (err) {
            MLAG_LOG(MLAG_LOG_ERROR, "error process delete list %d \n", err);
        }
        err = send_global_age_buffer();
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to send global age buffer upon peer_down , err %d \n",
                            err);
    }
    mlag_mac_sync_peer_init_delete_list();
    MLAG_LOG(MLAG_LOG_NOTICE, "completed . processed %d macs, err  %d\n", cnt,
             err );
    return err;
}



/*
 *  This function builds and sends message to the Peer(s)
 *  and updates Master Logic Database
 *  @param[in/out]   master_data - pointer to the Master database entry
 *  @param[in]       msg_data    - local learn message from the Peer
 *  @param[in]       type        - parameter explains to what peers to send the message
 *  @return 0 when successful, otherwise ERROR
 */
int
_local_learn_update_master_and_peers(struct master_logic_data *master_data,
                                     struct mac_sync_learn_event_data * msg_data,
                                     enum   global_learn_send_type type)
{
    int err = 0;
    struct timeval tv_start;




    ASSERT(msg_data);


    /* TODO  send global reject*/
#ifdef q1
    if (type == SEND_REJECT_TO_ORIGINATOR_PEER) {
        /* Send Global reject message to originator peer */
        struct mac_sync_global_reject_event_data *rej_msg =
            (struct mac_sync_global_reject_event_data *)msg_data;
        /*rej_msg->learn_msg->opcode = MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT;*/

        err = mlag_mac_sync_dispatcher_message_send(
            MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT, (void *)rej_msg,
            sizeof(*rej_msg),
            msg_data->originator_peer_id, MASTER_LOGIC);
        MLAG_BAIL_ERROR(err);
        goto bail;
    }
#endif

    ASSERT(master_data);
    if (type == SEND_TO_ORIGINATOR_PEER) {
        master_data->peer_bmap |= (1 << msg_data->originator_peer_id);

        memcpy(&gl_originator_buff.msg[gl_originator_buff.num_msg],
               msg_data, sizeof(struct mac_sync_learn_event_data));
        gl_originator_buff.num_msg++;
        goto bail;
    }

    master_data->peer_bmap = 0;
    master_data->entry_type = msg_data->mac_params.entry_type;
    master_data->port = msg_data->mac_params.log_port;
    master_data->peer_bmap |= (1 << msg_data->originator_peer_id);
    gettimeofday(&tv_start, NULL);
    master_data->timestamp = tv_start.tv_sec;

    if (type == SEND_TO_ALL_PEERS) {
        memcpy(&gl_all_buff.msg[gl_all_buff.num_msg],
               msg_data, sizeof(struct mac_sync_learn_event_data));
        gl_all_buff.num_msg++;
    }
    else if (type == SEND_TO_REMOTE_PEERS) {
        memcpy(&gl_remote_buff.msg[gl_remote_buff.num_msg],
               msg_data, sizeof(struct mac_sync_learn_event_data));
        gl_remote_buff.num_msg++;
    }
bail:
    return err;
}

/*
 *  This function sends to peers global age buffer
 *  @return 0 when successful, otherwise ERROR
 */
int
send_global_age_buffer()
{
    int err = 0;
    int i = 0;
    int sizeof_msg = 0;
    if (global_age_buffer.num_msg) {
        sizeof_msg = sizeof(struct mac_sync_multiple_age_event_data) +
                     global_age_buffer.num_msg *
                     sizeof(struct mac_sync_age_event_data);

        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            if (peer_state[i] != PEER_DOWN) {
                /* Send Global age message to all peer(s) - msg_data*/
                mlag_mac_sync_inc_cnt(MASTER_TX);
                err = mlag_mac_sync_dispatcher_message_send(
                    MLAG_MAC_SYNC_GLOBAL_AGED_EVENT,
                    (void *)&global_age_buffer,
                    sizeof_msg,
                    i, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to send global_aged event from the buffer , err %d \n",
                                    err);
            }
        }
        MLAG_LOG(MLAG_LOG_INFO, "Master sends Gl Age buff %d  messages\n",
                 global_age_buffer.num_msg);
        global_age_buffer.num_msg = 0;
    }
bail:
    return err;
}


/*
 *  This function sends to peers global learn buffers
 *  @return 0 when successful, otherwise ERROR
 */
int
send_global_learn_buffers()
{
    int err = 0;
    int i = 0;
    int sizeof_msg = 0;
    struct mlag_master_election_status current_status;

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in get status from master election  for processing of learn buffers, err %d \n",
                        err);

    if (gl_all_buff.num_msg) {
        sizeof_msg = sizeof(struct mac_sync_multiple_learn_event_data);
        if (gl_all_buff.num_msg > 1) {
            sizeof_msg += ((gl_all_buff.num_msg - 1) *
                           sizeof(struct mac_sync_learn_event_data));
        }
        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            if (peer_state[i] != PEER_DOWN) {
                /* Send Global learn message to all peer(s) */
                mlag_mac_sync_inc_cnt(MASTER_TX);
                err = mlag_mac_sync_dispatcher_message_send(
                    MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT, (void *)&gl_all_buff,
                    sizeof_msg,
                    i, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to send global_learned event to all peers, err %d \n",
                                    err);
            }
        }
        MLAG_LOG(MLAG_LOG_INFO, "Master sends GL all buff %d  messages\n",
                 gl_all_buff.num_msg);
    }
    if (gl_remote_buff.num_msg) {
        sizeof_msg = sizeof(struct mac_sync_multiple_learn_event_data);
        if (gl_remote_buff.num_msg > 1) {
            sizeof_msg += ((gl_remote_buff.num_msg - 1) *
                           sizeof(struct mac_sync_learn_event_data));
        }
        mlag_mac_sync_inc_cnt_num(MAC_SYNC_LOCAL_LEARNED_NEW_EVENT_PROCESSED,
                                  gl_remote_buff.num_msg );
        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            if ((peer_state[i] != PEER_DOWN) &&
                (i != current_status.my_peer_id)) {
                /* Send Global learn message to all peer(s) */
                mlag_mac_sync_inc_cnt(MASTER_TX);
                err = mlag_mac_sync_dispatcher_message_send(
                    MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT,
                    (void *)&gl_remote_buff,
                    sizeof_msg,
                    i, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to send global learned event to remote peers, err %d \n",
                                    err);
            }
        }
        MLAG_LOG(MLAG_LOG_INFO,
                 "Master sends GL remote buff %d  messages origin %d\n",
                 gl_remote_buff.num_msg,
                 gl_remote_buff.msg[0].originator_peer_id);
    }

    if (gl_originator_buff.num_msg) {
        sizeof_msg = sizeof(struct mac_sync_multiple_learn_event_data);
        if (gl_originator_buff.num_msg > 1) {
            sizeof_msg += ((gl_originator_buff.num_msg - 1) *
                           sizeof(struct mac_sync_learn_event_data));
        }
        mlag_mac_sync_inc_cnt(MASTER_TX);
        err = mlag_mac_sync_dispatcher_message_send(
            MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT,
            (void *)&gl_originator_buff,
            sizeof_msg,
            gl_originator_buff.msg[0].originator_peer_id
            , MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to send global learned event to originator peer, err %d \n",
                            err);

        MLAG_LOG(MLAG_LOG_INFO,
                 "Master sends GL originator buff %d  messages\n",
                 gl_originator_buff.num_msg);
    }
    gl_all_buff.num_msg = 0;
    gl_originator_buff.num_msg = 0;
    gl_remote_buff.num_msg = 0;

bail:
    return err;
}


/**
 *  This function checkes whether peer is enabled
 * @param[in]  peer_id - ID of the peer
 * @param[out] res -result
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_is_peer_enabled(int peer_id, int *res)
{
    int err = 0;
    ASSERT(res);
    if (peer_id >= MLAG_MAX_PEERS) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "Invalid parameters in master logic is peer enabled: peer_id=%d\n",
                            peer_id);
    }
    *res = (peer_state[peer_id] != PEER_DOWN);

bail:
    return err;
}


/**
 *  This function remove fsm from the internal map and insert it back to pool
 *
 *  @param[in]  p_map - event data
 *  @param[in] free_pool - allocate new fsm
 *  @param[in]  key
 *
 *  @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_inset_fsm_to_pool(cl_qmap_t * const p_map,
                                             IN cl_list_t * const free_pool,
                                             uint64_t key)
{
    int err = 0;
    cl_map_item_t *map_item = NULL;
    const cl_map_item_t *map_end = NULL;
    struct flush_fsm_item  *fsm_item = NULL;

    map_item = cl_qmap_get(p_map, key);
    map_end = cl_qmap_end(p_map);
    if (map_item != map_end) {
        /* Found */
        fsm_item = PARENT_STRUCT(map_item, struct flush_fsm_item, map_item);
        cl_qmap_remove_item(p_map, map_item);

        /* insert back to pool */
        fsm_item->filter = 0;
        cl_list_insert_tail(free_pool, fsm_item);
        flash_fsm_in_qmap_cnt--;

        MLAG_LOG(MLAG_LOG_INFO,
                 "mlag_mac_sync_master_logic_insert_fsm_to_pool key %" PRIx64 " remove from map and insert to pool\n",
                 key);

        goto bail;
    }
    else {
        err = -ENOENT;
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "mlag_mac_sync_master_logic_insert_fsm_to_pool key [%" PRIx64 "] err [%d]\n", key,
             err);

bail:
    return err;
}

/**
 *  This function is called when the fsm is moved to idle state
 *
 * @param[in]  key - fsm key id

 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_notify_fsm_idle(uint64_t key, int timeout)
{
    int err = 0;
    unsigned short vid = 0;
    unsigned long log_port = 0;


    /* check the key to identify which pool to use */
    /* extract the vid */
    vid = (key >> KEY_PORT_SHIFT) & 0xFFFF;
    log_port = (key & 0xFFFFFFFF);

    MLAG_LOG(MLAG_LOG_INFO,
             "mlag_mac_sync_master_logic_notify_fsm_idle(%d) key %" PRIx64 " vid [%u] log_port [%lu]\n",
             timeout, key, vid, log_port);


    /* Check if fsm exist in vlan port system flush fsm map */
    if ((vid != 0) && (log_port != 0)) {
        /* port+vid flush , use vlan_port_flush_fsm_pool */
        err = mlag_mac_sync_master_logic_inset_fsm_to_pool(
            &vlan_ports_system_flush_fsm_map, &vlan_port_flush_fsm_pool, key);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to insert flush sm to pool 1 upon idle, err %d \n",
                            err);

        MLAG_LOG(MLAG_LOG_INFO,
                 "fsm returned back to vlan port flush pool\n");
    }
    else {
        /* global/port/vlanflush , use vlan_port_system_flush_fsm_pool */
        err = mlag_mac_sync_master_logic_inset_fsm_to_pool(
            &vlan_ports_system_flush_fsm_map, &vlan_port_system_flush_fsm_pool,
            key);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to insert flush sm to pool 2 upon idle, err %d \n",
                            err);

        MLAG_LOG(MLAG_LOG_INFO,
                 "fsm returned back to vlan port system flush pool\n");
    }

bail:
    return err;
}


/**
 *  This function prints specific mac entry from FDB + master data
 * @param[in] data oes mac entry with filled mac and vlan fields
 * @return
 */
int
master_print_mac_params(void * data,  void (*dump_cb)(const char *, ...))
{
    int err = 0;
    ASSERT(data);
    struct fdb_uc_mac_addr_params mac_params;
    struct master_logic_data  * master_data = NULL;
    unsigned short data_cnt = 1;

    memcpy(&mac_params.mac_addr_params, data,
           sizeof(struct oes_fdb_uc_mac_addr_params));

    err = ctrl_learn_api_uc_mac_addr_get(
        OES_ACCESS_CMD_GET,
        &empty_filter,
        &mac_params,
        &data_cnt, 1 );

    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in get mac info :get mac from the control learning lib , err %d \n",
                        err);
    if (mac_params.cookie) {
        master_data =
            (struct master_logic_data *)mac_params.cookie;
    }

    DUMP_OR_LOG("\ntype %d, mac %02x:%02x:%02x:%02x:%02x:%02x, "
                "port %d, vlan %d - timestamp %d, peer_bmap %d \n",
                mac_params.entry_type,
                PRINT_MAC(mac_params),
                (int) mac_params.mac_addr_params.log_port,
                mac_params.mac_addr_params.vid,
                (mac_params.cookie) ? master_data->timestamp : 0,
                (mac_params.cookie) ? master_data->peer_bmap : 0
                );

bail:
    return err;
}



/**
 *  This function prints module's internal attributes
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 * @param[in]  verbosity
 *
 * @return 0 when successful, otherwise ERROR
 */

int
mlag_mac_sync_master_logic_print(void (*dump_cb)(const char *,
                                                 ...), int verbosity)
{
    int err = 0, i;
    unsigned short data_cnt = 1;
    int cnt = 0;
    enum oes_access_cmd access_cmd;
    struct master_logic_data  * master_data = NULL;

    struct fdb_uc_mac_addr_params mac_param_list[100];

    DUMP_OR_LOG("is_initialized=%d, is_started=%d\n",
                is_inited, is_started);


    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync master logic print called before init\n");
    }

    DUMP_OR_LOG("peer states: \n");

    for (i = 0; i < 2; i++) {
        DUMP_OR_LOG("peer %d. %s\n", i,
                    (peer_state[i] == PEER_DOWN) ? "DOWN" :
                    (peer_state[i] ==
                     PEER_TX_ENABLE) ? "TX ENABLE" : "ENABLE");
    }


    /* Get all macs from FDB filter statics on Non Mlag   ports*/
    access_cmd = OES_ACCESS_CMD_GET_FIRST;
    data_cnt = 1;
    while (data_cnt != 0) {
        err = ctrl_learn_api_uc_mac_addr_get(
            access_cmd,
            &empty_filter,
            mac_param_list,
            &data_cnt,
            1);
        if (err == -ENOENT) {
            goto bail;
        }

        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in ctrl_learn_api_uc_mac_addr_get , err %d \n",
                            err);

        /*to Process received entries */

        for (i = 0; i < data_cnt; i++) {
            if (mac_param_list[i].cookie) {
                master_data =
                    (struct master_logic_data *)mac_param_list[i].cookie;
            }
            if ((cnt < 1000) && verbosity) {
                DUMP_OR_LOG("\ntype %d, mac %02x:%02x:%02x:%02x:%02x:%02x, "
                            "port %d, vlan %d - timestamp %d, peer_bmap %d \n",
                            mac_param_list[i].entry_type,
                            PRINT_MAC(
                                mac_param_list[i]),
                            (int) mac_param_list[i].mac_addr_params.log_port,
                            mac_param_list[i].mac_addr_params.vid,
                            (mac_param_list[i].cookie) ? master_data->timestamp : 0,
                            (mac_param_list[i].cookie) ? master_data->peer_bmap : 0
                            );
            }
            cnt++;
        }
        /* prepare to next cycle : copy last mac from current cycle at first place of array*/
        memcpy( &mac_param_list[0], &mac_param_list[data_cnt - 1],
                sizeof(mac_param_list[0]));
        access_cmd = OES_ACCESS_CMD_GET_NEXT;
        data_cnt = 100;
    }


bail:
    DUMP_OR_LOG("\n=== total macs =%d === active flush fsms %d\n",
                cnt, flash_fsm_in_qmap_cnt);
    return err;
}

/**
 *  This function prints master pointer
 *
 * @param[in]  data  - pointer to the master instance
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_master_logic_print_master(void *data,  void (*dump_cb)(
                                            const char *,
                                            ...))
{
    int err = 0;
    struct master_logic_data  * master_data = NULL;
    ASSERT(data);

    master_data = (struct master_logic_data  * )data;
    DUMP_OR_LOG("     timestamp %d, peer_bmap %d \n",
                master_data->timestamp,
                master_data->peer_bmap );

bail:
    return err;
}


/**
 *  This function prints free pool count
 *
 * @return
 */
void
master_print_free_cookie_pool_cnt(void (*dump_cb)(const char *, ...))
{
    int count = 0;

    if (!is_inited) {
        MLAG_BAIL_ERROR_MSG(ECANCELED,
                            "print cookie pool called before init\n");
    }

    count = cl_pool_count(&cookie_pool);

    DUMP_OR_LOG(
        "\n Master cookie pool: alloc count %d, free blocks count %d, act. flush fsm %d, pool1 %d pool2 %d\n",
        (MAX_FDB_ENTRIES - count),
        count,
        flash_fsm_in_qmap_cnt,
        (int)cl_list_count(&vlan_port_system_flush_fsm_pool),
        (int)cl_list_count(&vlan_port_flush_fsm_pool));

bail:
    return;
}

/**
 *  This function returns free pool count
 *  @param[out]  cnt  - pointer to the returned number of free master pools
 *  @return
 */
int
mlag_mac_sync_master_logic_get_free_cookie_cnt(uint32_t *cnt)
{
	int err = 0;
	if (!is_inited) {
	        MLAG_BAIL_ERROR_MSG(ECANCELED,
	                            " get_free_cookie_cnt called before init\n");
	}
	ASSERT(cnt);

    *cnt =   cl_pool_count(&cookie_pool) ;

 bail:
    return err;
}


/*
 *  This function verifies whether FSM is "busy" with flush
 *
 * @param[in] fsm   - pointer to the flush FSM instance
 * @param[out] busy - pointer to the timer event
 *
 * @return 0 when successful, otherwise ERROR
 *
 */
int
mlag_mac_sync_master_logic_is_flush_busy(
    struct mac_sync_uc_mac_addr_params * mac_params, uint8_t peer_originator,
    int *busy)
{
    int err = 0;

    UNUSED_PARAM(mac_params);
    UNUSED_PARAM(busy);

    uint64_t key = 0;
    uint64_t non_mlag_key = 0;
    struct mlag_mac_sync_flush_fsm *fsm = NULL;
    int flush_busy = 0;


    if (!is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "is flush busy called before init\n");
    }

    if (flash_fsm_in_qmap_cnt == 0) {
        goto bail;
    }

    if (mac_params->log_port == NON_MLAG) {
        non_mlag_key = peer_originator | NON_MLAG_BIT;  /*0x8 = non mlag flush indicator */
    }

    /* is global system flush is performed */
    key = 0;
    err = mlag_mac_sync_master_logic_get_fsm(&vlan_ports_system_flush_fsm_map,
                                             NULL, key, 0, &fsm);
    if (err == 0) {
        err = mlag_mac_sync_flush_is_busy(fsm, &flush_busy);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in getting flush_busy  status for fsm of system flush type , err %d \n",
                            err);

        if (flush_busy) {
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_DURING_FLUSH_EVENT);
            goto bail;
        }
    }
    else if (err == -ENOENT) {
        err = 0;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in get pointer to system flush  sm upon checking flush busy, err %d \n",
                            err);
    }
    /* is flush per vid is performed */

    key = (mac_params->vid & 0xFFFF);
    key = (key << KEY_PORT_SHIFT) | (non_mlag_key << NON_MLAG_PART_SHIFT);
    err = mlag_mac_sync_master_logic_get_fsm(&vlan_ports_system_flush_fsm_map,
                                             NULL, key, 0, &fsm);
    if (err == 0) {
        err = mlag_mac_sync_flush_is_busy(fsm, &flush_busy);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in getting flush_busy  status for fsm of vid flush type , err %d \n",
                            err);
        if (flush_busy) {
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_DURING_FLUSH_EVENT);
            goto bail;
        }
    }
    else if (err == -ENOENT) {
        err = 0;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in get pointer to vid flush sm upon checking flush busy, err %d \n",
                            err);
    }

    /* is flush per port is performed */
    key =
        ((mac_params->log_port &
          0xFFFFFFFF)) | (non_mlag_key << NON_MLAG_PART_SHIFT);
    err = mlag_mac_sync_master_logic_get_fsm(&vlan_ports_system_flush_fsm_map,
                                             NULL, key, 0, &fsm);
    if (err == 0) {
        err = mlag_mac_sync_flush_is_busy(fsm, &flush_busy);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in getting flush_busy  status for fsm of port flush type , err %d \n",
                            err);
        if (flush_busy) {
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_DURING_FLUSH_EVENT);
            goto bail;
        }
    }
    else if (err == -ENOENT) {
        err = 0;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in get pointer to port flush sm upon checking flush busy, err %d \n",
                            err);
    }

    /* is flush per port+vid is performed */
    key = (mac_params->vid & 0xFFFF);
    key =
        (key <<
         KEY_PORT_SHIFT) |
        ((mac_params->log_port &
          0xFFFFFFFF)) | (non_mlag_key << NON_MLAG_PART_SHIFT);

    err = mlag_mac_sync_master_logic_get_fsm(&vlan_ports_system_flush_fsm_map,
                                             NULL,
                                             key, 0, &fsm);
    if (err == 0) {
        err = mlag_mac_sync_flush_is_busy(fsm, &flush_busy);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in getting flush_busy  status for fsm of port+vid flush type , err %d \n",
                            err);
        if (flush_busy) {
            mlag_mac_sync_inc_cnt(MAC_SYNC_LOCAL_LEARNED_DURING_FLUSH_EVENT);
            goto bail;
        }
    }
    else if (err == -ENOENT) {
        err = 0;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in get pointer to port+vid flush sm upon checking flush busy, err %d \n",
                            err);
    }

bail:
    *busy = flush_busy;

    return err;
}


/**
 *  This function get available flush fsm
 *
 * @param[in] allocate - allocate new fsm
 * @param[out] fsm - pointer to available flush FSM instance
 *
 *  @return 0 when successful, otherwise ERROR
 */


int
mlag_mac_sync_master_logic_get_available_flush_fsm(
    struct mac_sync_flush_generic_data *flush_data,
    int allocate,
    mlag_mac_sync_flush_fsm **fsm)
{
    int err = 0;
    uint64_t key = 0;
    uint64_t non_mlag_key = 0;
    fdb_uc_key_filter_t *key_filter = NULL;
    ASSERT(fsm);
    ASSERT(flush_data);

    key_filter = &flush_data->filter;
    ASSERT(key_filter);

    if (flush_data->non_mlag_port_flush) {
        non_mlag_key = flush_data->peer_originator | NON_MLAG_BIT;  /*0x8 = non mlag flush indicator */
    }

    if ((key_filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) &&
        (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
        /* vid is 2 bytes . log_port is 4 bytes = total of 6 bytes */
        key = (key_filter->vid & 0xFFFF);
        key =
            (key <<
             KEY_PORT_SHIFT) |
            ((key_filter->log_port &
              0xFFFFFFFF)) | (non_mlag_key << NON_MLAG_PART_SHIFT);
        /* lookup in vlan port system map pool */
        err = mlag_mac_sync_master_logic_get_fsm(
            &vlan_ports_system_flush_fsm_map,
            &vlan_port_flush_fsm_pool,
            key,
            allocate, fsm);
        if (err != 0) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "master: no available fsm resource log_port [%lu] - vid [%u] %d\n", key_filter->log_port,
                     key_filter->vid, err);
            goto bail;
        }
    }
    else if ((key_filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) &&
             (key_filter->filter_by_vid != FDB_KEY_FILTER_FIELD_VALID)) {
        key =
            ((key_filter->log_port &
              0xFFFFFFFF)) | (non_mlag_key << NON_MLAG_PART_SHIFT);
        err = mlag_mac_sync_master_logic_get_fsm(
            &vlan_ports_system_flush_fsm_map, &vlan_port_system_flush_fsm_pool,
            key, allocate, fsm);
        if (err != 0) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "master: no available fsm resource log_port [%lu] %d\n",
                     key_filter->log_port, err);
            goto bail;
        }
    }
    else if ((key_filter->filter_by_log_port != FDB_KEY_FILTER_FIELD_VALID) &&
             (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
        key = (key_filter->vid & 0xFFFF);
        key = (key << KEY_PORT_SHIFT) | (non_mlag_key << NON_MLAG_PART_SHIFT);

        err = mlag_mac_sync_master_logic_get_fsm(
            &vlan_ports_system_flush_fsm_map, &vlan_port_system_flush_fsm_pool,
            key, allocate, fsm);
        if (err != 0) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "master: no available fsm resource vid [%u] %d\n",
                     key_filter->vid, err);
            goto bail;
        }
    }
    else if ((key_filter->filter_by_log_port != FDB_KEY_FILTER_FIELD_VALID) &&
             (key_filter->filter_by_vid != FDB_KEY_FILTER_FIELD_VALID)) {
        /* global flush */
        key = 0;
        err = mlag_mac_sync_master_logic_get_fsm(
            &vlan_ports_system_flush_fsm_map, &vlan_port_system_flush_fsm_pool,
            key, allocate, fsm);
        if (err != 0) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "master: no available fsm resource system flush %d\n",
                     err);
            goto bail;
        }
    }

bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "search by key alloc(%d) : %" PRIx64 ", %" PRIx64 " , err %d\n",
             allocate, key, non_mlag_key, err);
    return err;
}

