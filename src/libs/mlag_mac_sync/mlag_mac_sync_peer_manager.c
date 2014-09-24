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
#include "mac_sync_events.h"
#include "mlag_common.h"
#include "mlag_mac_sync_manager.h"
#include "lib_ctrl_learn_defs.h"
#include "lib_ctrl_learn.h"
#include "mlag_master_election.h"
#include "mlag_mac_sync_master_logic.h"
#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include "mlag_mac_sync_dispatcher.h"
#include "mlag_mac_sync_peer_manager.h"
#include "mlag_mac_sync_manager.h"
#include "mlag_topology.h"
#include "mlag_manager.h"
#include "mlag_mac_sync_router_mac_db.h"
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

#undef  __MODULE__
#define __MODULE__ MLAG_MAC_SYNC_PEER_MANAGER

#define MLAG_MAC_SYNC_PEER_MANAGER_C



/************************************************
 *  Local Defines
 ***********************************************/
#define MAX_NUM_GET_ENTRIES  100

/* below used for delete operations*/
#define num_macs_in_msg  100
/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/
struct peer_flags {
    int is_started;
    int is_inited;
    int is_peer_start;
    int is_master_sync_done;
    int is_stop_begun;
};


struct mac_sync_internal_age_buffer {
    uint16_t opcode;
    uint16_t num_msg;
    struct fdb_uc_mac_addr_params mac_params[CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
};

struct mac_sync_internal_age_msg {
    uint16_t opcode;
    uint16_t num_msg;
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  External variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static struct peer_flags flags;

static unsigned long ipl_ifindex = 0;
static const struct fdb_uc_key_filter empty_filter =
{FDB_KEY_FILTER_FIELD_NOT_VALID, 0, FDB_KEY_FILTER_FIELD_NOT_VALID, 0};

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

static int delete_list_num_entries = 0;
static struct oes_fdb_uc_mac_addr_params delete_list[MAX_FDB_ENTRIES];

#define NON_MLAG_FLUSH_DATA_BLOCK_SIZE   sizeof(struct \
                                                mac_sync_flush_peer_sends_start_event_data) \
    + (sizeof(struct mac_sync_mac_params)) * MAX_FDB_ENTRIES

static uint8_t non_mlag_flush_data_block[NON_MLAG_FLUSH_DATA_BLOCK_SIZE];




/************************************************
 *  Local function declarations
 ***********************************************/
static int peer_mngr_notification_func(
    struct ctrl_learn_fdb_notify_data* notif_records,
    const void* originator_cookie);

static void _init_flags(void);
static int _fetch_static_non_mlag_macs_cb(void* user_data);
static int _delete_static_macs_from_ipl_cb(void* user_data);

static int _set_mac_list_to_fdb(uint16_t  *num_macs, enum oes_access_cmd cmnd,
                                struct fdb_uc_mac_addr_params * mac_list,
                                int need_lock);

static int _correct_port_on_rx(struct mac_sync_learn_event_data   *msg,
                               struct mlag_master_election_status *current_status);

static int _correct_learned_mac_entry_type(
    struct mac_sync_learn_event_data * msg, int my_peer_id,
    enum fdb_uc_mac_entry_type *entry_type);

static int process_flush_non_mlag_port(
    struct mac_sync_flush_peer_sends_start_event_data* msg,
    struct mlag_master_election_status *current_status);

static int _fill_mac_params_for_non_mlag_port_cb(
    void * msg);

int _cookie_init_deinit( enum cookie_op oper, void** cookie);

static int build_approved_list(struct ctrl_learn_fdb_notify_data*
		                               notif_records);

static int process_local_learn_buffer();

static int process_internal_age_buffer();
/************************************************
 *  Function implementations
 ***********************************************/

void trace(sx_log_severity_t severity, const char *module_name, char *msg);

void
trace(sx_log_severity_t severity, const char *module_name, char *msg)
{
    if (severity != CL_LOG_WARN) {
        sx_log(severity, module_name, msg);
    }
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_mac_sync_peer_mngr_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return log verbosity
 */
mlag_verbosity_t
mlag_mac_sync_peer_mngr_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits control learning library
 *
 * @return 0 when successful, otherwise ERROR
 */

int
mlag_mac_sync_peer_manager_init_ctrl_learn_lib()
{
    int err = 0;
    err = ctrl_learn_init(0, 1,  NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed init control learning lib, err %d\n",
                        err);
    err = ctrl_learn_start();
    MLAG_BAIL_ERROR_MSG(err, "Failed start control learning lib, err %d\n",
                        err);
bail:
    return err;
}

/**
 *  This function inits mlag mac sync peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_init(void)
{
    int err = 0;

    if (flags.is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "mac sync peer manager init called twice\n");
    }

    _init_flags();
    flags.is_inited = 1;
    ipl_ifindex = 0;
    delete_list_num_entries = 0;
    err = mlag_mac_sync_router_mac_db_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init router mac database, err %d\n",
                        err);

    MLAG_LOG(MLAG_LOG_NOTICE, "Peer manager initialized\n");

bail:
    return err;
}


/**
 *  This function de-inits mlag mac sync peer sub-module
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_deinit(void)
{
    int err = 0;

    if (!flags.is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync peer manager deinit called before init\n");
    }
    err = mlag_mac_sync_router_mac_db_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to de-init router mac database err %d\n",
                        err);
    if (flags.is_started) {
        mlag_mac_sync_peer_mngr_stop(NULL);
    }
    _init_flags();

bail:
    return err;
}

/**
 *  This function starts mlag mac sync peer sub-module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_start(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!flags.is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync peer manager start called before init\n");
    }

    flags.is_started = 1;

    MLAG_LOG(MLAG_LOG_NOTICE, "Peer manager started\n");

bail:
    return err;
}


/**
 *  This function stops mlag mac sync peer sub-module
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_stop(uint8_t *data)
{
    int err = 0;
    UNUSED_PARAM(data);

    if (!flags.is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err,
                            "mac sync peer manager stop called before start\n");
    }
    MLAG_LOG(MLAG_LOG_NOTICE, "Peer manager stopped\n");

    flags.is_stop_begun = 1;
    err = ctrl_learn_api_fdb_uc_flush_set((void *)MAC_SYNC_ORIGINATOR);
    MLAG_BAIL_ERROR_MSG(err,
                        " Failed to configure flush upon peer_stop, err %d\n",
                        err);
    /*Remove also all static entries from IPL */
    err = ctrl_learn_api_get_uc_db_access( _delete_static_macs_from_ipl_cb,
                                           NULL);
    MLAG_BAIL_ERROR_MSG(err, "Failed to process delete static macs, err %d\n",
                        err);

    err = ctrl_learn_unregister_notification_cb();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to unregister control learning lib upon peer_stop, err %d\n",
                        err);

    err = mlag_mac_sync_router_mac_db_set_not_sync();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to configure router mac db upon peer_stop, err %d\n",
                        err);

    flags.is_peer_start = 0;
    flags.is_stop_begun = 0;
    flags.is_master_sync_done = 0;

bail:
    return err;
}


/**
 *  This function notifies peer that Master completed initial sync wih peer
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_sync_done(uint8_t *data)
{
    int err = 0;
    ASSERT(data);
    struct sync_event_data * msg = (struct sync_event_data *)data;

    if (!flags.is_started) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "sync done event accepted before start\n");
    }
    MLAG_LOG(MLAG_LOG_NOTICE, "Master sync done accepted at %d peer\n",
             msg->peer_id);
    /* Fetch static MACs and send to master - upon  receiving MASTER SYNC_DONE event*/
    err = ctrl_learn_api_get_uc_db_access( _fetch_static_non_mlag_macs_cb,
                                           &msg->peer_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to fetch static macs upon receiving sync done from master, err %d\n",
                        err);

    err = mlag_mac_sync_router_mac_db_sync_router_macs();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to sync router macs  upon receiving sync done from master, err %d\n",
                        err);

bail:
    return err;
}


/**
 *  This function notifies peer that Control learning lib allocates
 *  or deallocates cookie upon ad new entry or delete entry
 *
 *  @param data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
_cookie_init_deinit( enum cookie_op oper, void** cookie)
{
    int err = 0;
    ASSERT(cookie);

    if (mlag_mac_sync_get_current_status() == MASTER) {
        err = mlag_mac_sync_master_logic_cookie_func((int)oper, cookie);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in de-allocation of master logic instance, err %d\n",
                            err);
    }
    else {
        *cookie = NULL;
    }
bail:
    return 0;
}






/**
 *  This function called when control learning lib needs to notify registered clients
 *
 * @param[in,out] notif_records
 * @param[in]     originator_cookie
 *
 * @return 0 when successful, otherwise ERROR
 */
static struct mac_sync_multiple_learn_buffer ll_buff;
/* static struct mac_sync_multiple_age_buffer   ma_buff; */
static struct mac_sync_internal_age_buffer ia_buff;

static struct mac_sync_internal_age_buffer ia_buff;





int
peer_mngr_notification_func(struct ctrl_learn_fdb_notify_data* notif_records,
                            const void* originator_cookie)
{
    int err = 0;
    uint32_t i = 0;
    int origin;
    struct mlag_master_election_status current_status;
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);

    ASSERT(notif_records);

    if (!flags.is_peer_start) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "control learning notice appears in wrong state of peer\n");
        goto bail;
    }

    if (flags.is_stop_begun) {
        goto bail;
    }

    origin = *((int*)&originator_cookie);

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to get switch status in notification callback, err %d\n",
                        err);

    if (origin == MAC_SYNC_ORIGINATOR) {

    	err = build_approved_list(notif_records);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to build approve list, err=%d\n",
                            err);
        goto bail;
    }
    else { /* originator is some other actor - need to process all
              records and perform proper actions*/
        if (notif_records->records_num > 1) {
            MLAG_LOG(MLAG_LOG_INFO,
                     "Cntrl learn Notify: (first rec =%d) ,%d records, time: %lu . %lu sec \n",
                     notif_records->records_arr[0].event_type,
                     notif_records->records_num,
                     tv_start.tv_sec, tv_start.tv_usec);
        }

        ll_buff.num_msg = 0;
        ia_buff.num_msg = 0;

        ll_buff.opcode = MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT;
        ia_buff.opcode = MLAG_MAC_SYNC_AGE_INTERNAL_EVENT;

        for (i = 0; i < notif_records->records_num; i++) {
            switch (notif_records->records_arr[i].event_type) {
            case OES_FDB_EVENT_LEARN:

                /* Static MACs on Mlag ports are approved and configured to FDB*/
                if ((notif_records->records_arr[i].entry_type == FDB_UC_STATIC)
                    && (!PORT_MNGR_IS_NON_MLAG_PORT(notif_records->records_arr[
                                                        i].
                                                    oes_event_fdb.
                                                    fdb_event_data.fdb_entry.
                                                    fdb_entry.log_port)
                        )) {
                    notif_records->records_arr[i].decision =
                        CTRL_LEARN_NOTIFY_DECISION_APPROVE;
                    break;
                }
                mlag_mac_sync_inc_cnt(MAC_SYNC_NOTIFY_LEARNED_EVENT);
                mlag_mac_sync_inc_cnt(SLAVE_TX);

                ll_buff.msg[ll_buff.num_msg].originator_peer_id =
                    current_status.my_peer_id;
                memcpy(&ll_buff.msg[ll_buff.num_msg].mac_params,
                       &notif_records->records_arr[i].oes_event_fdb.
                       fdb_event_data.fdb_entry.fdb_entry,
                       sizeof(struct oes_fdb_uc_mac_addr_params));
                ll_buff.msg[ll_buff.num_msg].mac_params.entry_type =
                    notif_records->records_arr[i].entry_type;

                err = _correct_port_on_tx(&ll_buff.msg[ll_buff.num_msg],
                                          &notif_records->records_arr[i].oes_event_fdb.
                                          fdb_event_data.fdb_entry.fdb_entry);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to correct port value in local_learn message to master, err %d\n",
                                    err);

                notif_records->records_arr[i].decision =
                    CTRL_LEARN_NOTIFY_DECISION_DENY;
                ll_buff.num_msg++;
                break;

            case OES_FDB_EVENT_AGE:

                memcpy(&ia_buff.mac_params[ia_buff.num_msg].mac_addr_params,
                       &notif_records->records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry,
                       sizeof(struct oes_fdb_uc_mac_addr_params));
                ia_buff.mac_params[ia_buff.num_msg].entry_type =
                    FDB_UC_NONAGEABLE;
                ia_buff.num_msg++;
                mlag_mac_sync_inc_cnt(MAC_SYNC_NOTIFY_AGED_EVENT);

                notif_records->records_arr[i].decision =
                    CTRL_LEARN_NOTIFY_DECISION_DENY;
                break;

            case    OES_FDB_EVENT_FLUSH_ALL:
            case    OES_FDB_EVENT_FLUSH_VID:
            case    OES_FDB_EVENT_FLUSH_PORT:
            case    OES_FDB_EVENT_FLUSH_PORT_VID:
            {
                struct mac_sync_flush_peer_sends_start_event_data msg;

                msg.gen_data.filter.filter_by_log_port =
                    FDB_KEY_FILTER_FIELD_NOT_VALID;
                msg.gen_data.filter.filter_by_vid =
                    FDB_KEY_FILTER_FIELD_NOT_VALID;
                msg.number_mac_params = 0;
                msg.gen_data.non_mlag_port_flush = 0;

                if (notif_records->records_arr[i].event_type ==
                    OES_FDB_EVENT_FLUSH_PORT) {
                    msg.gen_data.filter.log_port =
                        notif_records->records_arr[i].oes_event_fdb.
                        fdb_event_data.fdb_port.port;

                    MLAG_LOG(MLAG_LOG_INFO,
                             "Peer performs port Flush on port %lu\n",
                             msg.gen_data.filter.log_port);
                    msg.gen_data.filter.filter_by_log_port =
                        FDB_KEY_FILTER_FIELD_VALID;
                }
                else if (notif_records->records_arr[i].event_type ==
                         OES_FDB_EVENT_FLUSH_VID) {
                    msg.gen_data.filter.filter_by_vid =
                        FDB_KEY_FILTER_FIELD_VALID;
                    msg.gen_data.filter.vid =
                        notif_records->records_arr[i].oes_event_fdb.
                        fdb_event_data.fdb_vid.vid;
                }
                else if (notif_records->records_arr[i].event_type ==
                         OES_FDB_EVENT_FLUSH_PORT_VID) {
                    msg.gen_data.filter.filter_by_log_port =
                        FDB_KEY_FILTER_FIELD_VALID;
                    msg.gen_data.filter.filter_by_vid =
                        FDB_KEY_FILTER_FIELD_VALID;
                    msg.gen_data.filter.vid =
                        notif_records->records_arr[i].oes_event_fdb.
                        fdb_event_data.fdb_port_vid.vid;
                    msg.gen_data.filter.log_port =
                        notif_records->records_arr[i].oes_event_fdb.
                        fdb_event_data.fdb_port_vid.port;
                }
                /*  send flush message to Master */
                msg.opcode = MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT;
                if (msg.gen_data.filter.filter_by_log_port ==
                    FDB_KEY_FILTER_FIELD_VALID) {
                    msg.gen_data.non_mlag_port_flush =
                        PORT_MNGR_IS_NON_MLAG_PORT(msg.gen_data.filter.log_port);
                }
                if (!msg.gen_data.non_mlag_port_flush) { /* flush on MLAG ports */
                    err = mlag_mac_sync_dispatcher_message_send(
                        MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT,
                        (void *)&msg, sizeof(msg),
                        current_status.master_peer_id, PEER_MANAGER);
                    MLAG_BAIL_ERROR_MSG(err,
                                        "Failed sending global_flush_start to master, err %d\n",
                                        err);
                }
                else {
                    err = process_flush_non_mlag_port(&msg, &current_status);
                    MLAG_BAIL_ERROR_MSG(err,
                                        "Flush on non-mlag port failed, err %d\n",
                                        err);
                }
                notif_records->records_arr[i].decision =
                    CTRL_LEARN_NOTIFY_DECISION_DENY;
            }
            break;
            default:
                err = ECANCELED;
                MLAG_BAIL_ERROR_MSG(err,
                                    "Invalid parameters in peer manager notification\n");
            }
        }
    }
    err = process_local_learn_buffer();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed sending local_learn buffer to master ,err %d\n",
                        err);
    err = process_internal_age_buffer();
    MLAG_BAIL_ERROR_MSG(err, "Failed sending internal_age buffer ,err %d\n",
                        err);
bail:
    return err;
}

/*
 *  This function builds list of approved for configure MAC entries.
 *  Verified data base limitations
 *  @param[in,out] notif_records - notification records that are modified
 */
static int
build_approved_list(struct ctrl_learn_fdb_notify_data* notif_records)
{
    int err = 0;
    uint32_t i,  cnt;
    uint32_t num_approved = notif_records->records_num;
    uint32_t processed_approved =0;

    ASSERT(notif_records);

    if (mlag_mac_sync_get_current_status() == MASTER) {

        err = mlag_mac_sync_master_logic_get_free_cookie_cnt(&cnt);
        MLAG_BAIL_ERROR_MSG(err,
          "Failed getting counter of free blocks ,err %d\n",err);

        if(cnt < notif_records->records_num){
            num_approved = cnt;

            /* after passing "num_approved" elements mark as "denied"
             * all entries with LEARN TYPE  */
            for (i = 0; i < notif_records->records_num; i++) {

                if(notif_records->records_arr[i].event_type == OES_FDB_EVENT_LEARN){

                    if(processed_approved >= num_approved){
                         notif_records->records_arr[i].decision =
                           CTRL_LEARN_NOTIFY_DECISION_DENY;
                    }
                    processed_approved ++;
                }
            }
        }
    }

bail:
	return err;
}

/**
 *  This function handles processing buffer with internal age messges
 *  need to send internal event to peer manager
 * @return 0 when successful, otherwise ERROR
 */
int
process_internal_age_buffer()
{
    int err = 0;
    int sizeof_msg;
    struct mac_sync_internal_age_msg *msg = NULL;
    if (ia_buff.num_msg == 0) {
        goto bail;
    }
    msg = (struct mac_sync_internal_age_msg *)&ia_buff;

    sizeof_msg = sizeof(struct mac_sync_internal_age_msg)
                 + msg->num_msg * sizeof(struct fdb_uc_mac_addr_params);

    err = send_system_event(MLAG_MAC_SYNC_AGE_INTERNAL_EVENT,
                            (void *)msg, sizeof_msg);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed sending local_age buffer to master ,err %d\n",
                        err);

bail:
    return err;
}

/**
 *  This function handles processing buffer with local learn messages
 *  need to send it to master
 * @return 0 when successful, otherwise ERROR
 */

int
process_local_learn_buffer()
{
    int err = 0;
    int sizeof_msg;
    struct mac_sync_multiple_learn_event_data * msg =
        (struct mac_sync_multiple_learn_event_data *)&ll_buff;
    if (ll_buff.num_msg == 0) {
        goto bail;
    }

    sizeof_msg = sizeof(struct mac_sync_multiple_learn_event_data);
    if (msg->num_msg > 1) {
        sizeof_msg += ((msg->num_msg - 1) *
                       sizeof(struct mac_sync_learn_event_data));
    }

    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT,
        (void *)msg, sizeof_msg, 0, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in sending MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT event, err %d\n",
                        err);

    MLAG_LOG(MLAG_LOG_INFO,
             "Sent local learn message: %d bytes, %d messages\n",
             sizeof_msg, msg->num_msg);

bail:
    ll_buff.num_msg = 0;
    return err;
}



/**
 *  This function handles processing non mlag port flush
 *
 * @param[in] msg - message filled with filter parameters
 *
 * @return 0 when successful, otherwise ERROR
 */

int
process_flush_non_mlag_port(
    struct mac_sync_flush_peer_sends_start_event_data* msg,
    struct mlag_master_election_status *current_status)
{
    int err = 0, sizeof_msg = 0;
    struct mac_sync_flush_peer_sends_start_event_data* send_msg =
        (struct mac_sync_flush_peer_sends_start_event_data *)
        non_mlag_flush_data_block;

    memcpy(&send_msg->gen_data.filter, &msg->gen_data.filter,
           sizeof(msg->gen_data.filter));
    send_msg->gen_data.peer_originator = current_status->my_peer_id;
    send_msg->opcode = msg->opcode;
    send_msg->gen_data.non_mlag_port_flush = 1;
    send_msg->number_mac_params = 0;

    err = ctrl_learn_api_get_uc_db_access(
        _fill_mac_params_for_non_mlag_port_cb,
        send_msg);
    MLAG_BAIL_ERROR_MSG(err, "Failed in flush non mlag port, err %d\n", err);

    if (send_msg->number_mac_params == 0) {
        goto bail;
    }

    sizeof_msg = sizeof(*send_msg) +
                 send_msg->number_mac_params * (sizeof(send_msg->mac_params));

    MLAG_LOG(MLAG_LOG_INFO,
             "Peer built non mlag port %lu flush msg .num %d, size = %d\n",
             send_msg->gen_data.filter.log_port, send_msg->number_mac_params,
             sizeof_msg);

    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT,
        (void *)send_msg, sizeof_msg,
        current_status->master_peer_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err, "Failed in global flush send to master %d\n",
                        err);

bail:
    return err;
}



/**
 *  This function handles processing non mlag port flush under the semaphores of
 *  control learning lib
 *
 * @param[in/out] msg - built message
 *
 * @return 0 when successful, otherwise ERROR
 */

int
_fill_mac_params_for_non_mlag_port_cb(void * msg)
{
    int err = 0, i;
    struct mac_sync_flush_peer_sends_start_event_data* send_msg =
        (struct mac_sync_flush_peer_sends_start_event_data *)msg;

    unsigned short data_cnt = 1;
    enum oes_access_cmd access_cmd;
    struct mac_sync_mac_params  *mac_params = NULL;
    int number_mac_params = 0;
    static struct fdb_uc_mac_addr_params mac_param_list[MAX_NUM_GET_ENTRIES];


    MLAG_LOG(MLAG_LOG_INFO, "Peer builds non mlag port %lu flush  msg\n",
             send_msg->gen_data.filter.log_port);
    access_cmd = OES_ACCESS_CMD_GET_FIRST;
    data_cnt = 1;
    while (data_cnt != 0) {
        err = ctrl_learn_api_uc_mac_addr_get(
            access_cmd,
            &send_msg->gen_data.filter,
            mac_param_list,
            &data_cnt,
            0);
        if (err == -ENOENT) {
            /* the FDB is empty */
            err = 0;
            goto bail;
        }
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in get mac enry from control learning lib upon non mlag flush, err %d\n",
                            err);                                                                                      /*other errors are critical*/

        for (i = 0; i < data_cnt; i++) {
            if (mac_param_list[i].entry_type != FDB_UC_STATIC) {
                mac_params = &send_msg->mac_params + number_mac_params;
                memcpy(&mac_params->mac_addr,
                       &mac_param_list[i].mac_addr_params.mac_addr,
                       sizeof(struct ether_addr));
                mac_params->vid = mac_param_list[i].mac_addr_params.vid;
                number_mac_params++;
            }
        }
        /* prepare to next cycle : copy last mac from current cycle at first place of array*/
        memcpy( &mac_param_list[0], &mac_param_list[data_cnt - 1],
                sizeof(mac_param_list[0]));
        access_cmd = OES_ACCESS_CMD_GET_NEXT;
        data_cnt = MAX_NUM_GET_ENTRIES;
    }

bail:
    send_msg->number_mac_params = number_mac_params;
    MLAG_LOG(MLAG_LOG_INFO,
             "Peer built non mlag port %lu flush  mac list %d entries , err = %d\n",
             send_msg->gen_data.filter.log_port, number_mac_params, err);

    return err;
}



/**
 *  This function checkes whether found at least one dynamic entry in the FDB
 *  applying specific filter . If found - master performs Global flush process
 *
 *
 * @param[in] filter - input filter
 * @param[out] need_flush -  result value
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_check_need_flush( void *fltr, int *need_flush)
{
    int err = 0;
    unsigned short data_cnt = 1;
    fdb_uc_key_filter_t * filter = (fdb_uc_key_filter_t *)fltr;
    static struct fdb_uc_mac_addr_params mac_param;

    data_cnt = 1;
    *need_flush = 1;

    err = ctrl_learn_api_uc_mac_addr_get(
        OES_ACCESS_CMD_GET_FIRST,
        filter,
        &mac_param,
        &data_cnt,
        0);
    if (err == -ENOENT) {
        /* the FDB is empty */
        *need_flush = 0;
        err = 0;
        goto bail;
    }
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in get mac entry from control learning lib upon check whether flush need, err %d\n",
                        err);                                                                                                 /*other errors are critical*/

bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "Peer check need flush: result %d , err = %d\n", *need_flush,
             err);
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
mlag_mac_sync_peer_mngr_peer_start(struct peer_state_change_data *data)
{
    int err = 0;
    UNUSED_PARAM(data);
    struct mac_sync_mac_sync_master_fdb_get_event_data ev;
    struct mlag_master_election_status current_status;
    struct ctrl_learn_notify_params notify_params;

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting switch status upon peer_start, err %d\n",
                        err);

    MLAG_LOG(MLAG_LOG_NOTICE, "My peer is started\n");

    if (flags.is_peer_start) {
        MLAG_LOG(MLAG_LOG_INFO, "My peer is already started - do nothing\n");
        goto bail;
    }
    flags.is_peer_start = 1;

    /* get ipl*/
    err = mlag_topology_ipl_port_get(0, &ipl_ifindex);
    MLAG_BAIL_ERROR_MSG(err, "Failed get ipl port index, err %d\n", err);

    /*1. set notification callback*/
    notify_params.interval_units = 3; /*peer manager is able to receive notifications each 0.3*2 seconds*/
    notify_params.size_threshold = CTRL_LEARN_FDB_NOTIFY_SIZE_MAX / 2 - 100;

    err = ctrl_learn_register_notification(&notify_params,
                                           peer_mngr_notification_func);
    MLAG_BAIL_ERROR_MSG(err, "Failed register notification function, err %d\n",
                        err);
    err = ctrl_learn_register_init_deinit_mac_addr_cookie_cb(
        _cookie_init_deinit );
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed register init cookie function in control learning lib, err %d\n",
                        err);

    err = mlag_mac_sync_router_mac_db_register_cookie_func(
        _cookie_init_deinit );
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed register init cookie function in router mac database, err %d\n",
                        err);

    /*2.flush fdb all*/
    err = ctrl_learn_api_fdb_uc_flush_set((void *)MAC_SYNC_ORIGINATOR);
    MLAG_BAIL_ERROR_MSG(err, "Failed full flush upon peer_start, err %d\n",
                        err);

    /*3. Send MLAG_SYNC_START_EVENT event to Master Logic */
    ev.opcode = MLAG_MAC_SYNC_ALL_FDB_GET_EVENT;
    ev.peer_id = data->mlag_id;

    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_ALL_FDB_GET_EVENT, (void *)&ev, sizeof(ev),
        current_status.master_peer_id, PEER_MANAGER);

    MLAG_BAIL_ERROR_MSG(err,
                        "Failed send message fdb_get to the master, err %d\n",
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
mlag_mac_sync_peer_mngr_ipl_port_set(struct ipl_port_set_event_data *data)
{
    int err = 0;

    ASSERT(data);

    if (!flags.is_inited) {
        err = ECANCELED;
        MLAG_BAIL_ERROR_MSG(err, "ipl port set called before init\n");
    }

    mlag_mac_sync_inc_cnt(MAC_SYNC_IPL_PORT_SET_EVENTS_RCVD);

    ipl_ifindex = data->ifindex;

bail:
    return err;
}


/**
 *  This function handles sync finish event that peer sends to Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_master_sync_finish(uint8_t *data)
{
    struct sync_event_data ev;
    int err = 0;
    UNUSED_PARAM(data);
    struct mlag_master_election_status current_status;

    if (!flags.is_peer_start) {
        err = -EPERM;
        MLAG_BAIL_ERROR_MSG(err,
                            "sync finish message while peer is not started, err %d\n",
                            err);
    }
    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed get switch status, err %d\n", err);
    flags.is_master_sync_done = 1;

    ev.opcode = MLAG_MAC_SYNC_SYNC_FINISH_EVENT;
    ev.peer_id = current_status.my_peer_id;
    ev.state = 1;
    /* Send message to Master*/
    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_SYNC_FINISH_EVENT, (void *)&ev, sizeof(ev),
        current_status.master_peer_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed sending sync_finish event to master, err %d\n",
                        err);

bail:
    return err;
}


/**
 *  This function handles Flush command from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_flush_start(void *data)
{
    int err = 0;
    uint32_t i;
    struct mac_sync_flush_peer_ack_event_data resp_msg;
    int my_peer_id;
    ASSERT(data);
    struct mlag_master_election_status current_status;
    struct mac_sync_flush_master_sends_start_event_data *msg =
        (struct mac_sync_flush_master_sends_start_event_data *)data;

    if (!flags.is_peer_start) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "flush start event from master received while peer is down\n");
        goto bail;
    }

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting switch status upon flush start, err %d\n",
                        err);

    my_peer_id = current_status.my_peer_id;
    MLAG_LOG(MLAG_LOG_INFO,
             "Peer receives  Flush from Master: filter %d %d ,port %lu, non_mlag_flush = %d\n",
             msg->gen_data.filter.filter_by_log_port,
             msg->gen_data.filter.filter_by_vid,
             msg->gen_data.filter.log_port,
             msg->gen_data.non_mlag_port_flush
             );

    if (msg->gen_data.non_mlag_port_flush &&
        (current_status.my_peer_id != msg->gen_data.peer_originator)) { /* Flush on non mlag port */
        uint16_t num_macs_in_curr_msg = 0;
        struct fdb_uc_mac_addr_params mac_list[num_macs_in_msg];

        /* parse message from the master and perform delete for each entry*/
        MLAG_LOG(MLAG_LOG_INFO,
                 "Perform non mlag flush on port %lu , origin %d, num %d \n",
                 msg->gen_data.filter.log_port, msg->gen_data.peer_originator,
                 msg->number_mac_params);
        for (i = 0; i < msg->number_mac_params; i++) {
            memcpy(&mac_list[num_macs_in_curr_msg].mac_addr_params.mac_addr,
                   &((&msg->mac_params + i)->mac_addr),
                   sizeof(mac_list[0].mac_addr_params.mac_addr));
            mac_list[num_macs_in_curr_msg].mac_addr_params.vid =
                (&msg->mac_params + i)->vid;

            num_macs_in_curr_msg++;
            if ((num_macs_in_curr_msg == num_macs_in_msg) ||
                (i == (msg->number_mac_params - 1))) {
                /*Send Message*/
                err = _set_mac_list_to_fdb(&num_macs_in_curr_msg,
                                           OES_ACCESS_CMD_DELETE,
                                           mac_list, 1);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to delete macs in processing non mlag flush, err %d\n",
                                    err);
                num_macs_in_curr_msg = 0;
            }
        }
    }
    else {
        if ((msg->gen_data.filter.filter_by_log_port ==
             FDB_KEY_FILTER_FIELD_NOT_VALID)
            &&
            (msg->gen_data.filter.filter_by_vid ==
             FDB_KEY_FILTER_FIELD_NOT_VALID)) {
            MLAG_LOG(MLAG_LOG_INFO, "Peer performs full Flush\n");
            err = ctrl_learn_api_fdb_uc_flush_set((void *)MAC_SYNC_ORIGINATOR);
            MLAG_BAIL_ERROR_MSG(err, "Failed to perform full flush, err %d\n",
                                err);
        }
        else if (msg->gen_data.filter.filter_by_vid ==
                 FDB_KEY_FILTER_FIELD_VALID) {
            MLAG_LOG(MLAG_LOG_INFO, "Peer performs vid Flush \n");
            err = ctrl_learn_api_fdb_uc_flush_vid_set(msg->gen_data.filter.vid,
                                                      (void *)MAC_SYNC_ORIGINATOR);
            MLAG_BAIL_ERROR_MSG(err, "Failed to perform vid flush, err %d\n",
                                err);
        }
        else if (msg->gen_data.filter.filter_by_log_port ==
                 FDB_KEY_FILTER_FIELD_VALID) {
            MLAG_LOG(MLAG_LOG_INFO,
                     "Peer performs port Flush on port %lu \n",
                     msg->gen_data.filter.log_port);
            err = ctrl_learn_api_fdb_uc_flush_port_set(
                msg->gen_data.filter.log_port,
                (void *)MAC_SYNC_ORIGINATOR);
            MLAG_BAIL_ERROR_MSG(err, "Failed to perform port flush, err %d\n",
                                err);
        }
        else {
            err = ctrl_learn_api_fdb_uc_flush_port_vid_set(
                msg->gen_data.filter.vid,
                msg->gen_data.filter.log_port,
                (void *)MAC_SYNC_ORIGINATOR);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to perform port+vid flush, err %d\n",
                                err);
        }
    }

    /* When completed send Ack to MASTER*/
    resp_msg.opcode = MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT;
    resp_msg.peer_id = my_peer_id;
    resp_msg.gen_data.filter = msg->gen_data.filter;
    resp_msg.gen_data.non_mlag_port_flush = msg->gen_data.non_mlag_port_flush;
    resp_msg.gen_data.peer_originator = msg->gen_data.peer_originator;
    /* Send message to the master*/
    err = mlag_mac_sync_dispatcher_message_send(
        MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT,
        (void *)&resp_msg, sizeof(resp_msg),
        current_status.master_peer_id, PEER_MANAGER);
    MLAG_BAIL_ERROR_MSG(err, "Failed to send flush ack to master, err %d\n",
                        err);

bail:
    return err;
}

/**
 *  This function performs local flush on mlag port
 *
 * @param[in] port id
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_manager_port_local_flush(unsigned long port)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_INFO,
             "Performing flush on port %lu\n",
             port);

    err = ctrl_learn_api_fdb_uc_flush_port_set(port,
                                               (void *)MAC_SYNC_ORIGINATOR);
    MLAG_BAIL_ERROR_MSG(err, "Failed to flush port %lu, err %d\n",
                        port, err);

bail:
    return err;
}

/**
 *  This function handles internal age event
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_internal_aged(void *data)
{
    int err = 0;
    int i = 0;
    uint16_t cnt = 0, dup_cnt = 0;
    struct mac_sync_internal_age_buffer * msg = NULL;
    static struct mac_sync_multiple_age_buffer snd_buff;
    static struct fdb_uc_mac_addr_params mac_entry[
        CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
    unsigned short data_cnt = 1;
    int sizeof_msg = 0;
    ASSERT(data);
    msg = (struct mac_sync_internal_age_buffer *)data;
    struct mlag_master_election_status current_status;

    if (!flags.is_peer_start) {
        MLAG_LOG(MLAG_LOG_INFO,
                 "internal_age event received while peer is down\n");
        goto bail;
    }

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting switch status for processing age message, err %d\n",
                        err);

    /* 1. set non-ageable to the HW for all age messages*/

    /* verify that mac entry configured in FDB - filter not existed entries */

    snd_buff.num_msg = 0;

    for (i = 0; i < msg->num_msg; i++) {
        data_cnt = 1;
        err = ctrl_learn_api_uc_mac_addr_get(
            OES_ACCESS_CMD_GET,
            &empty_filter,
            &msg->mac_params[i],
            &data_cnt,
            1 );
        if (err == -ENOENT) {
            MLAG_LOG(MLAG_LOG_INFO,
                     "age notif: Mac not in FDB %02x:%02x:%02x:%02x:%02x:%02x \n",
                     PRINT_MAC(msg->mac_params[i]));
            continue;
        }
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get mac entry from the control learning lib upon processing aging message, err %d\n",
                            err);

        if (msg->mac_params[i].entry_type == FDB_UC_NONAGEABLE) {
            MLAG_LOG(MLAG_LOG_INFO, "Mac is already non ageable \n" );
            continue;
        }
        /* add to the new filtered list  */
        memcpy(&mac_entry[cnt].mac_addr_params,
               &msg->mac_params[i].mac_addr_params,
               sizeof(struct oes_fdb_uc_mac_addr_params));

        mac_entry[cnt].entry_type = FDB_UC_NONAGEABLE; /*msg->mac_params[i].entry_type;*/


        memcpy(&snd_buff.msg[cnt].mac_params,
               &msg->mac_params[i].mac_addr_params,
               sizeof(struct oes_fdb_uc_mac_addr_params));
        snd_buff.msg[cnt].originator_peer_id = current_status.my_peer_id;
        cnt++;
    }
    dup_cnt = cnt;
    if (cnt) {
        err = _set_mac_list_to_fdb(&cnt, OES_ACCESS_CMD_ADD, mac_entry, 1);

        if (err && (err != EXFULL)) {
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to configure mac to control learning lib during aging processing, err %d\n",
                                err);
        }
        if (cnt) {
            MLAG_LOG(MLAG_LOG_ERROR, "%d mac errors upon set nonageable \n",
                     cnt );
        }

        /* 2. send local age buffer to the  master*/
        snd_buff.num_msg = dup_cnt;
        snd_buff.opcode = MLAG_MAC_SYNC_LOCAL_AGED_EVENT;
        sizeof_msg = sizeof(struct mac_sync_multiple_age_event_data) +
                     snd_buff.num_msg *
                     sizeof(struct mac_sync_age_event_data);

        err = mlag_mac_sync_dispatcher_message_send(
            MLAG_MAC_SYNC_LOCAL_AGED_EVENT, (void *)&snd_buff, sizeof_msg,
            0, PEER_MANAGER);
        mlag_mac_sync_inc_cnt_num(SLAVE_TX, snd_buff.num_msg);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending MLAG_MAC_SYNC_LOCAL_AGED event, err = %d\n",
                            err);
    }

bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "accepted internal message : %d mac age messages, set to %d macs, size %d , err = %d",
             msg->num_msg, dup_cnt, sizeof_msg, err );
    return err;
}


/**
 *  This function handles Global learn command from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_global_learned(void *data, int need_lock)
{
    int err = 0;
    int i = 0;
    struct mac_sync_multiple_learn_buffer * mesg =
        (struct mac_sync_multiple_learn_buffer *)data;
    static struct fdb_uc_mac_addr_params mac_entry[
        CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
    struct mlag_master_election_status current_status;
    struct timeval tv_stop;
    uint16_t num_macs = 0;

    ASSERT(data);

    gettimeofday(&tv_stop, NULL);

    if (!flags.is_peer_start) {
        err = -EPERM;
        MLAG_BAIL_ERROR_MSG(err,
                            "Accepted global learn message when peer is not started, err %d\n",
                            err);
    }
    /*printf("GLobal learn: sec %d usec %d\n",(int)tv_stop.tv_sec , (int)tv_stop.tv_usec);*/
    for (i = 0; i < mesg->num_msg; i++) {
        _correct_port_on_rx(&mesg->msg[i], &current_status);

        mlag_mac_sync_inc_cnt(MAC_SYNC_GLOBAL_LEARNED_EVENT);
        mlag_mac_sync_inc_cnt(SLAVE_RX);
        if ((mesg->msg[i].mac_params.log_port == 0) &&

            (mesg->msg[i].mac_params.entry_type == FDB_UC_STATIC)) {
            MLAG_LOG(MLAG_LOG_INFO,
                     " ROUTER MAC  set:   %02x:%02x:%02x:%02x:%02x:%02x \n",
                     PRINT_MAC_OES(mesg->msg[i].mac_params));

            mlag_mac_sync_router_mac_db_sync_response(
                mesg->msg[i].mac_params.mac_addr,
                mesg->msg[i].mac_params.vid,
                ADD_ROUTER_MAC);
            /* allocate master cookie on master*/
            continue; /* Global learned for originator of router mac - not to write to FW*/
        }

        memcpy(&mac_entry[num_macs].mac_addr_params, &mesg->msg[i].mac_params,
               sizeof(mesg->msg[0].mac_params));

        _correct_learned_mac_entry_type(&mesg->msg[i],
                                        current_status.my_peer_id,
                                        &mac_entry[num_macs].entry_type);
        num_macs++;
    }

    /* mac_entry[i].cookie = NULL; */

    if (num_macs == 0) {
        goto bail;
    }
    err = _set_mac_list_to_fdb(&num_macs, OES_ACCESS_CMD_ADD, mac_entry,
                               need_lock);
    if (num_macs) {
        mlag_mac_sync_inc_cnt_num(MAC_SYNC_ERROR_FDB_SET, num_macs);
        int i;
        for (i = 0; i < num_macs; i++) {
            MLAG_LOG(MLAG_LOG_INFO,
                     " mac wasn't set: %d  %02x:%02x:%02x:%02x:%02x:%02x \n",
                     i,
                     PRINT_MAC(mac_entry[i]));
        }
    }
    if (err && (err != -EXFULL)) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to set Mac list to SDK while processing global learn message, err %d\n",
                            err);
    }
bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "Received Global learn message :  %d messages , my %d , port %lu , err %d\n",
             mesg->num_msg, current_status.my_peer_id,
             mesg->msg[0].mac_params.log_port, err
             );
    return err;
}


/**
 *  This function handles Global age command from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_global_aged(void *data)
{
    int err = 0;
    int i = 0, dup_num_msg = 0;
    int router_mac_age = 0;
    struct mac_sync_multiple_age_buffer *msg =
        (struct mac_sync_multiple_age_buffer *)data;
    static struct fdb_uc_mac_addr_params mac_entry[
        CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
    struct mlag_master_election_status current_status;

    ASSERT(data);
    if (!flags.is_peer_start) {
        err = -EPERM;
        MLAG_BAIL_ERROR_MSG(err,
                            "Accepted global aged message when peer is not started, err %d\n",
                            err);
    }
    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting switch status for processing global age message, err %d\n",
                        err);


    mlag_mac_sync_inc_cnt_num(MAC_SYNC_GLOBAL_AGED_EVENT, msg->num_msg );
    for (i = 0; i < msg->num_msg; i++) {
        mlag_mac_sync_inc_cnt(SLAVE_RX);
        if ((msg->msg[i].originator_peer_id == current_status.my_peer_id) &&
            (msg->msg[i].mac_params.log_port == 0)
            ) {
            /**/
            router_mac_age = 1;
            err = mlag_mac_sync_router_mac_db_sync_response(
                msg->msg[i].mac_params.mac_addr,
                msg->msg[i].mac_params.vid,
                REMOVE_ROUTER_MAC);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to write aging response to router database err %d\n",
                                err);
            continue;
        }
        memcpy(&mac_entry[i].mac_addr_params, &msg->msg[i].mac_params,
               sizeof(struct oes_fdb_uc_mac_addr_params));
        mac_entry[i].cookie = NULL;
    }
    if (router_mac_age) {
        goto bail;
    }
    dup_num_msg = msg->num_msg;
    err = _set_mac_list_to_fdb(&msg->num_msg, OES_ACCESS_CMD_DELETE, mac_entry,
                               1);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to set Mac list to SDK while processing global age message, err %d\n",
                        err);

bail:
    MLAG_LOG(MLAG_LOG_INFO,
             "Received Global Age message :  %d messages ,   port %lu , err %d\n",
             dup_num_msg,
             msg->msg[0].mac_params.log_port, err
             );
    return err;
}


/**
 *  This function handles Global reject from Master
 *
 * @param[in] data - event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_global_reject(void *data)
{
    int err = 0;
    ASSERT(data);
    struct mac_sync_global_reject_event_data *msg =
        (struct mac_sync_global_reject_event_data *)data;

    if (!flags.is_peer_start) {
        err = -EPERM;
        MLAG_BAIL_ERROR_MSG(err,
                            "Accepted global reject message when peer is not started, err %d\n",
                            err);
    }


    MLAG_LOG(MLAG_LOG_NOTICE, "on static MAC :type %d, port %d, cause %d \n",
             msg->learn_msg->mac_params.entry_type,
             (int)msg->learn_msg->mac_params.log_port,
             msg->error_cause);
bail:
    return err;
}


/**
 *  This function fetches static non mlag MACs from FBD
 *
 * @param[in] data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
_fetch_static_non_mlag_macs_cb(void* user_data)
{
    int err = 0, i, cnt = 0;
    /* get all macs from FDB filter statics on Non Mlag   ports*/
    unsigned short data_cnt = 1;
    struct mac_sync_multiple_learn_event_data msg;
    enum oes_access_cmd access_cmd;

    struct fdb_uc_key_filter key_filter;
    static struct fdb_uc_mac_addr_params mac_param_list[MAX_NUM_GET_ENTRIES];
    struct mlag_master_election_status current_status;
    UNUSED_PARAM(user_data);

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting switch status for fetching static macs process, err %d\n",
                        err);


    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;

    msg.opcode = MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT;
    msg.num_msg = 1;
    msg.msg.originator_peer_id = current_status.my_peer_id;

    MLAG_LOG(MLAG_LOG_NOTICE, "fetch static non mlag started\n");
    access_cmd = OES_ACCESS_CMD_GET_FIRST;
    data_cnt = 1;
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
                            "Failed to get mac entry from the control learn lib while fetching static macs, err %d\n",
                            err); /*other errors are critical*/

        for (i = 0; i < data_cnt; i++) {
            if (((PORT_MNGR_IS_NON_MLAG_PORT(
                      mac_param_list[i].mac_addr_params.log_port)))
                &&
                (mac_param_list[i].mac_addr_params.log_port != ipl_ifindex)
                /* no need to send to master macs on ipl */
                &&
                (mac_param_list[i].entry_type == FDB_UC_STATIC)
                ) {
                memcpy(&msg.msg.mac_params, &mac_param_list[i].mac_addr_params,
                       sizeof(msg.msg.mac_params));
                msg.msg.mac_params.log_port = NON_MLAG;
                msg.msg.mac_params.entry_type = FDB_UC_STATIC;

                msg.msg.port_cookie =
                    mac_param_list[i].mac_addr_params.log_port;
                /* Send message to Master*/
                err = mlag_mac_sync_dispatcher_message_send(
                    MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT,
                    (void *)&msg, sizeof(msg),
                    current_status.master_peer_id, PEER_MANAGER);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed in sending local learn event, err = %d\n",
                                    err);
                cnt++;
            }
        }
        /* prepare to next cycle : copy last mac from current cycle at first place of array*/
        memcpy( &mac_param_list[0], &mac_param_list[data_cnt - 1],
                sizeof(mac_param_list[0]));
        access_cmd = OES_ACCESS_CMD_GET_NEXT;
        data_cnt = MAX_NUM_GET_ENTRIES;
    }
bail:
    MLAG_LOG(MLAG_LOG_NOTICE,
             "fetch static non mlag completed for %d entries \n", cnt);
    return err;
}


/**
 *  This function processes FDB export message from the Master.
 *  Parsed Global learn messages are set to FBD
 *
 * @param[in] message from the Master
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_mngr_process_fdb_export(void *data)
{
    int err = 0;
    int i;
    uint16_t num_macs_in_curr_msg = 0;
    static struct   fdb_uc_mac_addr_params mac_list[num_macs_in_msg];
    ASSERT(data);
    struct mac_sync_master_fdb_export_event_data *msg =
        (struct mac_sync_master_fdb_export_event_data *)data;

    /* Parse long message FDB export*/
    struct mlag_master_election_status current_status;


    for (i = 0; i < (int)msg->num_entries; i++) {
        _correct_port_on_rx(&msg->entry + i, &current_status);

        memcpy(&mac_list[num_macs_in_curr_msg].mac_addr_params,
               &(&msg->entry + i)->mac_params,
               sizeof(mac_list[0].mac_addr_params));

        _correct_learned_mac_entry_type(
            (&msg->entry + i),
            current_status.my_peer_id,
            &(mac_list[num_macs_in_curr_msg].entry_type));
        num_macs_in_curr_msg++;
        if ((num_macs_in_curr_msg == num_macs_in_msg) ||
            (i == ((int)msg->num_entries - 1))) {
            /*Send Message*/
            err = _set_mac_list_to_fdb(&num_macs_in_curr_msg,
                                       OES_ACCESS_CMD_ADD,
                                       mac_list, 1);
            if (err == -EXFULL) {
                err = 0;
                MLAG_LOG(MLAG_LOG_NOTICE, "Hash bin full occurred \n");
            }
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to configure macs to SDK, err %d\n",
                                err);
            num_macs_in_curr_msg = 0;
        }
    }
    MLAG_LOG(MLAG_LOG_NOTICE,
             "Parsed FDB_export message consisting of %d entries\n",
             msg->num_entries );

    err = mlag_mac_sync_peer_mngr_master_sync_finish(NULL);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to send sync finish to the master, err %d\n",
                        err);

bail:
    return err;
}

/**
 *  This function deletes static non Mlag MACs from the IPL.
 *  Called upon Peer down
 * @param[in] data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
_delete_static_macs_from_ipl_cb(void* user_data)
{
    UNUSED_PARAM(user_data);
    int err = 0;
    int i;
    unsigned short data_cnt = 1;
    enum oes_access_cmd access_cmd;
    struct fdb_uc_key_filter key_filter;
    struct fdb_uc_mac_addr_params mac_param_list[MAX_NUM_GET_ENTRIES];

    MLAG_LOG(MLAG_LOG_NOTICE, "delete_static_macs started\n");

    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;

    access_cmd = OES_ACCESS_CMD_GET_FIRST;
    data_cnt = 1;
    err = mlag_mac_sync_peer_check_init_delete_list();
    MLAG_BAIL_ERROR_MSG(err,
                        " Data base inconsistent while delete static macs from ipl, err %d\n",
                        err);
    while (data_cnt != 0) {
        err = ctrl_learn_api_uc_mac_addr_get(
            access_cmd,
            &key_filter,
            mac_param_list,
            &data_cnt, 0);
        if (err == -ENOENT) {
            /* the FDB is empty */
            err = 0;
            goto bail;
        }
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to get mac entry from control learn lib while delete static macs from ipl, err %d\n",
                            err);                                                                              /*other errors are critical*/
        /*to Process received entries */
        for (i = 0; i < data_cnt; i++) {
            if (mac_param_list[i].entry_type == FDB_UC_STATIC) {
                err = mlag_mac_sync_peer_mngr_addto_delete_list_ipl_static_mac(
                    &mac_param_list[i].mac_addr_params);
                MLAG_BAIL_ERROR_MSG(err,
                                    "Failed to update internal database while delete static macs, err %d\n",
                                    err);
            }
        }
        /* prepare to next cycle : copy last mac
         * from current cycle at first place of array*/
        memcpy(&mac_param_list[0], &mac_param_list[data_cnt - 1],
               sizeof(mac_param_list[0]));
        access_cmd = OES_ACCESS_CMD_GET_NEXT;
        data_cnt = MAX_NUM_GET_ENTRIES;
    }

bail:
    if (err == 0) {
        err = mlag_mac_sync_peer_process_delete_list();
        if (err) {
            MLAG_LOG(MLAG_LOG_ERROR, "error process delete list %d \n", err);
        }
    }
    err = mlag_mac_sync_peer_init_delete_list();


    MLAG_LOG(MLAG_LOG_NOTICE, "delete_static_macs completed. err %d\n", err );
    return err;
}



/**
 *  This function synchronizes router mac .
 *
 * @param[in]  mac
 * @param[in]  vid
 * @param[in]  add   =1 for add mac and 0 for remove mac
 *
 * @return 0 when successful, otherwise ERROR
 */

int
mlag_mac_sync_peer_mngr_sync_router_mac(
    struct ether_addr mac,
    unsigned short vid,
    int add)
{
    int err = 0;
    struct mac_sync_multiple_learn_event_data learn_msg;
    struct mac_sync_fixed_age_event_data age_msg;
    struct mlag_master_election_status current_status;


    if (!flags.is_peer_start) {
        err = -EPERM;
        goto bail;
    }

    err = mlag_master_election_get_status(&current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed getting switch status for sync router macs process, err %d\n",
                        err);
    if (add) {
        learn_msg.num_msg = 1;
        learn_msg.opcode = MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT;
        learn_msg.msg.port_cookie = 0;           /* indication of router mac*/
        learn_msg.msg.originator_peer_id = current_status.my_peer_id;
        learn_msg.msg.mac_params.vid = vid;
        learn_msg.msg.mac_params.log_port = NON_MLAG_PORT;
        learn_msg.msg.mac_params.entry_type = FDB_UC_STATIC;
        memcpy(&learn_msg.msg.mac_params.mac_addr, &mac,
               sizeof(struct ether_addr));
        err = mlag_mac_sync_dispatcher_message_send(
            MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT,
            (void *)&learn_msg, sizeof(learn_msg), 0, PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending router mac LEARNED_EVENT, err %d\n",
                            err);
    }
    else {
        age_msg.num_msg = 1;
        age_msg.opcode = MLAG_MAC_SYNC_LOCAL_AGED_EVENT;
        age_msg.msg.originator_peer_id = current_status.my_peer_id;
        age_msg.msg.mac_params.vid = vid;

        age_msg.msg.mac_params.log_port = 0;   /*indication of Router mac age*/
        memcpy(&age_msg.msg.mac_params.mac_addr, &mac,
               sizeof(struct ether_addr));
        err = mlag_mac_sync_dispatcher_message_send(
            MLAG_MAC_SYNC_LOCAL_AGED_EVENT,
            (void *)&age_msg, sizeof(age_msg), 0, PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending router mac AGED_EVENT  \n");
    }

bail:
    MLAG_LOG(MLAG_LOG_NOTICE,
             "mlag_mac_sync_peer_mngr_sync_router_mac . command %d, err %d\n",
             add,  err );
    return err;
}


/**
 *  This function corrects port on received Global learn message.
 *
 * @param[in/out] msg  - Global learn message from Master
 * @param[in]  current_status - current master election status
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
_correct_port_on_rx(struct mac_sync_learn_event_data   *msg,
                    struct mlag_master_election_status *current_status)
{
    int err = 0;
    int my_peer_id = 0;

    err = mlag_master_election_get_status(current_status);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to correct port in received global learn message, err %d\n",
                        err);
    my_peer_id = current_status->my_peer_id;

    if (msg->mac_params.log_port == NON_MLAG) {
        if (msg->originator_peer_id == my_peer_id) {
            msg->mac_params.log_port = msg->port_cookie;
        }
        else {
            msg->mac_params.log_port = ipl_ifindex; /* configure MAC on IPL port*/
        }
    }
bail:
    return err;
}

/**
 *  This function corrects port on transmitted Local learn message.
 *
 * @param[in]     mac_addr_params - MAC address parameters
 * @param[in/out] data            - pointer to Local learn message
 *
 * @return 0 when successful, otherwise ERROR
 */
int
_correct_port_on_tx(
    void  *data,
    struct oes_fdb_uc_mac_addr_params *mac_addr_params
    )
{
    /* int err = 0; */
    struct mac_sync_learn_event_data  *msg =
        (struct mac_sync_learn_event_data  *)data;

    if (PORT_MNGR_IS_NON_MLAG_PORT(mac_addr_params->log_port)) {
        msg->mac_params.log_port = NON_MLAG;
    }
    else {
        msg->mac_params.log_port = mac_addr_params->log_port;
    }
    msg->port_cookie = mac_addr_params->log_port;

/* bail: */
    return 0;
}



/**
 *  This function corrects MAC entry type on received
 *  Global learn message upon configuring in to FDB.
 *
 * @param[in]     msg  - received Global learn message
 * @param[in]     my_peer_id
 * @param[in/out] entry_type  - entry type of the message
 *
 * @return 0 when successful, otherwise ERROR
 */
int
_correct_learned_mac_entry_type(struct mac_sync_learn_event_data * msg,
                                int my_peer_id,
                                enum fdb_uc_mac_entry_type *entry_type )
{
    if (msg->mac_params.entry_type == FDB_UC_STATIC) {
        *entry_type = FDB_UC_STATIC;
    }
    else if (msg->originator_peer_id == my_peer_id) {
        *entry_type = FDB_UC_AGEABLE;
    }
    else {
        *entry_type = FDB_UC_NONAGEABLE;  /* remotely learned*/
    }
    return 0;
}


/**
 *  This function sets MAC parameters to the FDB
 *
 * @param[in]     num_macs - number MACs to set
 * @param[in]     cmnd     - control learning lib. command
 * @param[in]     mac_list - list of MACs
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
_set_mac_list_to_fdb(uint16_t *num_macs,
                     enum oes_access_cmd cmnd,
                     struct fdb_uc_mac_addr_params * mac_list,
                     int need_lock)
{
    int err = 0;

    err = ctrl_learn_api_fdb_uc_mac_addr_set(
        cmnd,
        mac_list,
        num_macs,
        (void *)MAC_SYNC_ORIGINATOR,
        need_lock
        );
    return err;
}



/*======== Delete list operations (not re- entry  type ) ==========*/


/**
 *  This function initializes delete list
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_init_delete_list(void)
{
    delete_list_num_entries = 0;

    return 0;
}

/**
 *  This function  checks whether delete list is initialized
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_check_init_delete_list(void)
{
    if (delete_list_num_entries != 0) {
        return -1;
    }
    else {
        return 0;
    }
}

/**
 *  This function  passes through delete list and
 *  deletes all MAC entries in the list from the FDB
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_mac_sync_peer_process_delete_list(void)
{
    int i, err = 0;
    struct fdb_uc_mac_addr_params params;
    uint16_t num_macs = 1;
    for (i = 0; i < delete_list_num_entries; i++) {
        memcpy(&params.mac_addr_params, &delete_list[i],
               sizeof(params.mac_addr_params));

        num_macs = 1;
        err = _set_mac_list_to_fdb(&num_macs, OES_ACCESS_CMD_DELETE,
                                   &params, 0);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to delete mac from the SDK while processing delete list, err %d\n",
                            err);
    }
bail:
    mlag_mac_sync_peer_init_delete_list();
    return err;
}

/**
 *  This function adds to delete list STATIC mac on IPL from FDB
 *
 * @return current status value
 */
int
mlag_mac_sync_peer_mngr_addto_delete_list_ipl_static_mac(
    struct oes_fdb_uc_mac_addr_params *mac_addr_params)
{
    int err = 0;

    if (mac_addr_params->log_port == ipl_ifindex) {
        /*add to delete list*/
        memcpy(&delete_list[delete_list_num_entries],
               mac_addr_params, sizeof(*mac_addr_params));
        delete_list_num_entries++;
    }

    return err;
}


/**
 *  This function prints module's internal attributes
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return current status value
 */
int
mlag_mac_sync_peer_mngr_print(void (*dump_cb)(const char *, ...))
{
    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "is_initialized=%d, is_started=%d, is_peer_start=%d, is_master_sync_done=%d\n",
                 flags.is_inited, flags.is_started, flags.is_peer_start,
                 flags.is_master_sync_done);
        MLAG_LOG(MLAG_LOG_NOTICE, "ipl_ifindex=%lu \n", ipl_ifindex);
    }
    else {
        dump_cb(
            "is_initialized=%d, is_started=%d, is_peer_start=%d, is_master_sync_done=%d\n",
            flags.is_inited, flags.is_started, flags.is_peer_start,
            flags.is_master_sync_done);
        dump_cb("ipl_ifindex=%d\n", ipl_ifindex);
    }


    return 0;
}


/*
 *  This function initializes all static flags of the peer manager
 *
 * @return
 */
void
_init_flags(void)
{
    flags.is_started = 0;
    flags.is_inited = 0;
    flags.is_peer_start = 0;
    flags.is_stop_begun = 0;
    flags.is_master_sync_done = 0;
    return;
}


/*======== Debug functions ===========*/

static int global_vid = 30;
uint8_t int_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

int
simulate_notify_local_learn(int mac, int port)
{
    struct ctrl_learn_fdb_notify_data notif;

    memcpy(int_mac, &mac, sizeof(mac));
    notif.records_num = 1;
    notif.records_arr[0].event_type = OES_FDB_EVENT_LEARN;  /* CTRL_LEARN_NOTIFY_TYPE_NEW_MAC_LAG; */
    notif.records_arr[0].entry_type = FDB_UC_AGEABLE;
    notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.
    log_port = port;
    notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.vid =
        global_vid;
    memcpy(
        &notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.mac_addr, int_mac,
        6);
    peer_mngr_notification_func(&notif, (void *)1);
    return 0;
}



static struct ether_addr g_mac_list[] = {
    {{0x28, 0xd7, 0xac, 0x19, 0x0, 0x01}},
    {{0x16, 0xa7, 0x45, 0x34, 0x0, 0x01}},
    {{0x18, 0xeb, 0xf3, 0x10, 0x0, 0x01}},
    {{0x22, 0xbe, 0xc2, 0x3c, 0x0, 0x01}},
    {{0x24, 0x82, 0x19, 0x78, 0x0, 0x01}},
    {{0x36, 0xe5, 0xce, 0x75, 0x0, 0x01}},
    {{0x38, 0x13, 0xdd, 0x7d, 0x0, 0x01}},
    {{0x38, 0x30, 0x36, 0x62, 0x0, 0x01}},
    {{0x38, 0xda, 0x7f, 0x12, 0x0, 0x01}},
    {{0x40, 0x23, 0x9e, 0x31, 0x0, 0x01}},
    {{0x44, 0x70, 0x6e, 0x6f, 0x0, 0x01}},
    {{0x46, 0xbe, 0xdf, 0x03, 0x0, 0x01}},
    {{0x46, 0xd4, 0x90, 0x79, 0x0, 0x01}},
    {{0x4a, 0x31, 0xd5, 0x59, 0x0, 0x01}},
    {{0x4a, 0x76, 0xe8, 0x21, 0x0, 0x01}},
    {{0x4c, 0xd6, 0x3f, 0x4a, 0x0, 0x01}},
    {{0x58, 0x39, 0xcc, 0x64, 0x0, 0x01}},
    {{0x5a, 0x7a, 0x55, 0x0f, 0x0, 0x01}},
    {{0x5e, 0xd2, 0x66, 0x07, 0x0, 0x01}},
    {{0x74, 0x4a, 0x91, 0x3c, 0x0, 0x01}}
};


#define  MAX_LRN_SIZE 30 //(CTRL_LEARN_FDB_NOTIFY_SIZE_MAX - 200)
int
simulate_notify_multiple_local_learn(int num, int first_mac, int port,
                                     int astatic )
{
    int i = 0, full_cnt = 0;
    struct ctrl_learn_fdb_notify_data notif;
    int mode = 1;
    int index = 0;
    if (first_mac == 0) {
        num = NUM_ELEMS(g_mac_list);
        mode = 0;
    }

    for (full_cnt = 0;; full_cnt++) {
        if (mode == 0) { /* simulated macs are hardcoded */
            memcpy(
                &notif.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.
                fdb_entry.mac_addr, &g_mac_list[index],
                6);
            index++;
        }
        else {
            first_mac = rand();
            memcpy(int_mac, &first_mac, sizeof(first_mac));
            memcpy(
                &notif.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.
                fdb_entry.mac_addr, int_mac,
                6);
            /* until legal UC mac*/
            notif.records_arr[i].oes_event_fdb.fdb_event_data.
            fdb_entry.fdb_entry.mac_addr.ether_addr_octet[0] &= 0xFE;
        }
        notif.records_arr[i].event_type = OES_FDB_EVENT_LEARN;  /* CTRL_LEARN_NOTIFY_TYPE_NEW_MAC_LAG; */
        notif.records_arr[i].entry_type = FDB_UC_AGEABLE;
        if (astatic) {
            notif.records_arr[i].entry_type = FDB_UC_STATIC;
        }
        notif.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.
        log_port = port;
        notif.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.
        vid = global_vid;
        notif.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.
        entry_type = 0;

        i++;
        if ((full_cnt == num) || (i == MAX_LRN_SIZE)) {
            notif.records_num =
                ((full_cnt == num)) ? full_cnt % MAX_LRN_SIZE
                : MAX_LRN_SIZE;

            peer_mngr_notification_func(&notif, (void *)1);
            usleep(2000 );
            i = 0;
            if (full_cnt == num) {
                break;
            }
        }
    }
    return 0;
}



int
simulate_notify_local_age(int mac, int port)
{
    struct ctrl_learn_fdb_notify_data notif;

    memcpy(int_mac, &mac, sizeof(mac));

    notif.records_num = 1;
    notif.records_arr[0].event_type = OES_FDB_EVENT_AGE;  /* CTRL_LEARN_NOTIFY_TYPE_NEW_MAC_LAG; */
    notif.records_arr[0].entry_type = FDB_UC_AGEABLE;
    notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.
    log_port = port;
    notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.vid =
        global_vid;
    memcpy(
        &notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.mac_addr, int_mac,
        6);
    peer_mngr_notification_func(&notif, (void *)1);
    return 0;
}


int
simulate_notify_local_flush(int port)
{
    struct ctrl_learn_fdb_notify_data notif;
    notif.records_num = 1;
    notif.records_arr[0].event_type = OES_FDB_EVENT_FLUSH_ALL;  /* CTRL_LEARN_NOTIFY_TYPE_NEW_MAC_LAG; */
    notif.records_arr[0].entry_type = FDB_UC_AGEABLE;
    if (port != 0) {
        notif.records_arr[0].event_type = OES_FDB_EVENT_FLUSH_PORT;
        notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_port.port = port;
    }
    /*notif.records_arr[0].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.vid  = global_vid;*/

    peer_mngr_notification_func(&notif, (void *)1);
    return 0;
}



int
peer_simulate_local_flush(void)
{
    int err = 0;

    err = ctrl_learn_api_fdb_uc_flush_set(
        (void *)MAC_SYNC_ORIGINATOR);
    MLAG_BAIL_ERROR_MSG(err, " err %d\n", err);
bail:
    return err;
}
