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

#define MLAG_INIT_C_

#include <complib/sx_rpc.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <mlag_api_defs.h>
#include <complib/cl_thread.h>
#include <complib/cl_init.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include <mlnx_lib/lib_event_disp.h>
#include <mlnx_lib/lib_commu.h>
#include <libs/health_manager/health_dispatcher.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include "libs/mlag_l3_interface_manager/mlag_l3_interface_manager.h"
#include <libs/mlag_common/mlag_comm_layer_wrapper.h>
#include <libs/mlag_manager/mlag_dispatcher.h>
#include <libs/mlag_mac_sync/mlag_mac_sync_dispatcher.h>
#include <libs/port_manager/port_manager.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/mlag_manager/mlag_manager.h>
#include <libs/service_layer/service_layer.h>
#include <libs/mlag_mac_sync/mlag_mac_sync_manager.h>
#include <libs/mlag_tunneling/mlag_tunneling.h>
#include <libs/lacp_manager/lacp_manager.h>
#include "mlag_init.h"
#include "mlag_internal_api.h"
#include "mlag_main.h"

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
EXTERN int mlag_init_state;
EXTERN int sl_start_state = FALSE;
/************************************************
 *  Local function declarations
 ***********************************************/
static int mlag_set_socket_params(void);
/************************************************
 *  Function implementations
 ***********************************************/

/*
 *  This function inits all libraries needed in mlag
 *
 * @return ERROR if operation failed.
 *
 */
static int
libs_init(mlag_log_cb_t log_cb)
{
    event_disp_status_t event_status = EVENT_DISP_STATUS_SUCCESS;
    int err = 0;

    err = sl_api_init(NULL);
    MLAG_BAIL_ERROR(err);

    complib_init();

    event_status = event_disp_api_init();
    if (event_status != EVENT_DISP_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to init event dispatcher library [%u]\n",
                            event_status);
    }

    err = comm_lib_init(log_cb);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 *  This function de-Inits Mlag required libraries
 *
 * @return int as error code.
 */
static int
libs_deinit()
{
    event_disp_status_t event_status = EVENT_DISP_STATUS_SUCCESS;
    int err = 0;

    err = comm_lib_deinit();
    MLAG_BAIL_ERROR(err);

    event_status = event_disp_api_deinit();
    if (event_status != EVENT_DISP_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to deinit event dispatcher library [%u]\n",
                            event_status);
    }

    complib_exit();

    err = sl_api_deinit();
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function inits all mlag modules
 *
 * @return ERROR if operation failed.
 *
 */
int
mlag_init(mlag_log_cb_t log_cb)
{
    int err = 0;

    err = libs_init(log_cb);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_set_socket_params();
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* Init other dispatchers */
    err = mlag_health_dispatcher_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init MLAG health dispatcher\n");

    /* Call mlag manager init */
    err = mlag_dispatcher_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init MLAG manager dispatcher\n");

    err = mlag_mac_sync_dispatcher_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init MLAG Mac Sync dispatcher\n");

    /* Init mlag tunneling */
    err = mlag_tunneling_init();
    MLAG_BAIL_ERROR_MSG(err, "Failed to init MLAG tunneling\n");

    /* Safety for backward compatibility */
    ASSERT(MLAG_TUNNELING_IGMP_MESSAGE == 52);

    mlag_init_state = TRUE;

bail:
    return err;
}

/**
 * De-Initialize the mlag protocol.
 * *
 * @return 0 - Operation completed successfully.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_deinit(void)
{
    int err = 0;

    if (mlag_init_state == FALSE) {
        goto bail;
    }

    /* stop mlag if running */
    err = mlag_stop();
    MLAG_BAIL_ERROR_MSG(err, "Failed to stop mlag protocol\n");

    mlag_init_state = FALSE;

    /* Stop inserting API requests to the system */
    sx_rpc_change_main_exit_signal_state(TRUE);

    /* De-init rpc */
    err = mlag_rpc_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit mlag rpc\n");

    /* Stop handle oes events */
    err = ctrl_learn_stop();
    MLAG_BAIL_ERROR_MSG(err, "Failed to stop control learn\n");

    /* send deinit event */
    err = send_system_event(MLAG_DEINIT_EVENT, NULL, 0);
    MLAG_BAIL_ERROR(err);

    /* deinit mlag tunneling */
    err = mlag_tunneling_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit MLAG tunneling\n");

    MLAG_LOG(MLAG_LOG_NOTICE,
             "mlag tunneling deinit done\n");

    err = mlag_health_dispatcher_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit MLAG health dispatcher\n");

    MLAG_LOG(MLAG_LOG_NOTICE,
             "mlag health dispatcher deinit done\n");

    err = mlag_mac_sync_dispatcher_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit MLAG Mac Sync dispatcher\n");

    MLAG_LOG(MLAG_LOG_NOTICE,
             "mlag mac sync dispatcher deinit done\n");

    err = mlag_dispatcher_deinit();
    MLAG_BAIL_ERROR_MSG(err, "Failed to deinit MLAG dispatcher\n");

    MLAG_LOG(MLAG_LOG_NOTICE,
             "mlag dispatcher deinit done\n");

    /* stop ctrl learn */
    err = ctrl_learn_stop();
    MLAG_BAIL_ERROR_MSG(err, "ctrl learn stop done\n");

    /* deinit ctrl learn */
    err = ctrl_learn_deinit();
    MLAG_BAIL_ERROR_MSG(err, "ctrl learn deinit done\n");

    /* stop the service layer */
    if (sl_start_state) {
        err = sl_api_stop();
        MLAG_BAIL_ERROR(err);
        sl_start_state = FALSE;
    }

    /* De-init all modules */
    err = libs_deinit();
    MLAG_BAIL_CHECK_NO_MSG(err);

    /* clean the mlag main resources */
    mlag_main_deinit();

bail:
    exit(0);
    return err;
}

/**
 * Enable the mlag protocol.
 * Start is called in order to enable the protocol. From this stage on, the protocol
 * is running and thus exchanges messages with peers, triggers configuration toward
 * HAL (Hardware Abstraction Layer) and events toward the system.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] system_id - The system identification of the local machine.
 * @param[in] params - Mlag features. Disable (=FALSE) a specific feature or enable (=TRUE) it.
 *                     Pointer to an already allocated memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_start(const unsigned long long system_id,
           const struct mlag_start_params *params)
{
    int err = 0;
    struct start_event_data start_data;

    BAIL_MLAG_NOT_INIT();

    err = mlag_manager_db_local_system_id_set(system_id);
    MLAG_BAIL_ERROR(err);

    if (sl_start_state == FALSE) {
        err = sl_api_start();
        MLAG_BAIL_ERROR(err);
        sl_start_state = TRUE;
    }

    start_data.start_params = *params;

    err = send_system_event(MLAG_START_EVENT, (void *)&start_data,
                            sizeof(struct start_event_data));
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 * Disable the mlag protocol.
 * Stop means stopping the mlag capability of the switch. No messages or
 * configurations should be sent from any mlag module after handling stop request.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_stop(void)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = send_system_event(MLAG_STOP_EVENT, NULL, 0);
    MLAG_BAIL_ERROR(err);

    err = mlag_manager_wait_for_stop_done();
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_NOTICE, "Mlag protocol stop done\n");


bail:
    return err;
}

/**
 * Set module log verbosity level.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] module - The module his log verbosity is going to change.
 * @param[in] verbosity - New log verbosity level.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 */
int
mlag_log_verbosity_set(enum mlag_log_module module,
                       const mlag_verbosity_t verbosity)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    switch (module) {
    case MLAG_LOG_ALL:
        port_manager_log_verbosity_set(verbosity);
        mlag_l3_interface_log_verbosity_set(verbosity);
        mlag_master_election_log_verbosity_set(verbosity);
        mlag_mac_sync_log_verbosity_set(verbosity);
        mlag_tunneling_log_verbosity_set(verbosity);
        mlag_manager_log_verbosity_set(verbosity);
        mlag_dispatcher_log_verbosity_set(verbosity);
        mlag_mac_sync_dispatcher_log_verbosity_set(verbosity);
        mlag_comm_layer_wrapper_log_verbosity_set(verbosity);
        lacp_manager_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_PORT_MANAGER:
        port_manager_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_L3_INTERFACE_MANAGER:
        mlag_l3_interface_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_MASTER_ELECTION:
        mlag_master_election_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_MAC_SYNC:
        mlag_mac_sync_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_TUNNELING:
        mlag_tunneling_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_MLAG_MANAGER:
        mlag_manager_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_MLAG_DISPATCHER:
        mlag_dispatcher_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_MAC_SYNC_DISPATCHER:
        mlag_mac_sync_dispatcher_log_verbosity_set(verbosity);
        break;
    case MLAG_LOG_MODULE_LACP_MANAGER:
        lacp_manager_log_verbosity_set(verbosity);
        break;
    default:
        /* Unknown module */
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown module %d in mlag_log_verbosity_set", module);
        break;
    }

bail:
    return err;
}

/**
 * Get module log verbosity level.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] module - The module his log verbosity is going to change.
 * @param[out] verbosity - The log verbosity level.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 */
int
mlag_log_verbosity_get(const enum mlag_log_module module,
                       mlag_verbosity_t *verbosity)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    switch (module) {
    case MLAG_LOG_MODULE_L3_INTERFACE_MANAGER:
        *verbosity = mlag_l3_interface_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_PORT_MANAGER:
        *verbosity = port_manager_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_MASTER_ELECTION:
        *verbosity = mlag_master_election_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_MAC_SYNC:
        *verbosity = mlag_mac_sync_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_TUNNELING:
        *verbosity = mlag_tunneling_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_MLAG_MANAGER:
        *verbosity = mlag_manager_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_MLAG_DISPATCHER:
        *verbosity = mlag_dispatcher_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_MAC_SYNC_DISPATCHER:
        *verbosity = mlag_mac_sync_dispatcher_log_verbosity_get();
        break;
    case MLAG_LOG_MODULE_LACP_MANAGER:
        *verbosity = lacp_manager_log_verbosity_get();
        break;
    default:
        /* Unknown module */
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Unknown module %d in mlag_log_verbosity_get", module);
        *verbosity = MLAG_VERBOSITY_LEVEL_NONE;
        break;
    }

bail:
    return err;
}

/*
 * Set socket parameters.
 *
 * @return 0 - Operation completed successfully,
 *             otherwise operation failure.
 */
static int
mlag_set_socket_params(void)
{
    int err = 0;
    char data[100];
    char file_name[100];

    snprintf(data, sizeof(data), "%d", RMEM_MAX_SOCK_BUF_SIZE);
    snprintf(file_name, sizeof(file_name), "/proc/sys/net/core/rmem_max");
    err = mlag_comm_layer_wrapper_write_to_proc_file(file_name, data);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "failed to set RMEM_MAX_SOCK_BUF_SIZE parameter, errno=%d",
                 errno);
    }

    snprintf(data, sizeof(data), "%d", WMEM_MAX_SOCK_BUF_SIZE);
    snprintf(file_name, sizeof(file_name), "/proc/sys/net/core/wmem_max");
    err = mlag_comm_layer_wrapper_write_to_proc_file(file_name, data);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "failed to set WMEM_MAX_SOCK_BUF_SIZE parameter, errno=%d",
                 errno);
    }

    snprintf(data, sizeof(data), "%d", MAX_DGRAM_QLEN);
    snprintf(file_name, sizeof(file_name),
             "/proc/sys/net/unix/max_dgram_qlen");
    err = mlag_comm_layer_wrapper_write_to_proc_file(file_name, data);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "failed to set WMEM_MAX_SOCK_BUF_SIZE parameter, errno=%d",
                 errno);
    }
    return err;
}
