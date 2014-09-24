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

#include <unistd.h>
#include <complib/sx_rpc.h>
#include "mlag_internal_api.h"
#include "mlag_bail.h"
#include "mlag_api_rpc.h"
#include <errno.h>
#include <arpa/inet.h>
#include <mlnx_lib/lib_ctrl_learn.h>
#include <signal.h>

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_INTERNAL_API

/************************************************
 *  Local Macros
 ***********************************************/
#define INIT_STRUCT_AND_CHECK(var) INIT_RPC_POINTER_PARAM_AND_CHECK( \
        struct mlag_api_ ## var, var)

#define INIT_RPC_POINTER_PARAM_AND_CHECK(param_type, var) \
    param_type * var = NULL; \
    err = check_message_size_equal(rcv_len, sizeof(param_type)); \
    MLAG_BAIL_ERROR(err); \
    var = (param_type *)rcv_msg_body; \

#define RETURN_EMPTY_REPLY(snd_body, snd_len) \
    do { \
        (*snd_body) = NULL; \
        (*snd_len) = 0; \
    } while (0) \

#define COMMAND_ID_REPLICA(cmd) cmd, #cmd

#define CONVERT_CTRL_LEARN_ERR_TO_API(err) err = (err == -EPERM) ? -EIO : err;
/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/
EXTERN int mlag_init_state;

sx_rpc_api_command_t mlag_api_command_id[] = {
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_START),
      mlag_internal_api_start, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_STOP),
      mlag_internal_api_stop, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_STOP),
      mlag_internal_api_stop, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_INIT),
      mlag_internal_api_fdb_init, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_DEINIT),
      mlag_internal_api_fdb_deinit, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_START),
      mlag_internal_api_fdb_start, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_STOP),
      mlag_internal_api_fdb_stop, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_GET),
      mlag_internal_api_fdb_uc_mac_addr_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_SET),
      mlag_internal_api_fdb_uc_mac_addr_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_SET),
      mlag_internal_api_fdb_uc_flush_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_PORT_SET),
      mlag_internal_api_fdb_uc_flush_port_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_GET),
      mlag_internal_api_fdb_uc_mac_addr_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_VID_SET),
      mlag_internal_api_fdb_uc_flush_vid_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_PORT_VID_SET),
      mlag_internal_api_fdb_uc_flush_port_vid_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_RELOAD_DELAY_SET),
      mlag_internal_api_reload_delay_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_RELOAD_DELAY_GET),
      mlag_internal_api_reload_delay_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_KEEPALIVE_INTERVAL_SET),
      mlag_internal_api_keepalive_interval_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_KEEPALIVE_INTERVAL_GET),
      mlag_internal_api_keepalive_interval_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PORTS_STATE_CHANGE_NOTIFY),
      mlag_internal_api_ports_state_change_notify, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_VLANS_STATE_CHANGE_NOTIFY),
      mlag_internal_api_vlans_state_change_notify, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_MGMT_PEER_STATE_NOTIFY),
      mlag_internal_api_mgmt_peer_state_notify, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_COUNTERS_GET),
      mlag_internal_api_counters_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_COUNTERS_CLEAR),
      mlag_internal_api_counters_clear, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PORT_SET),
      mlag_internal_api_port_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_IPL_SET),
      mlag_internal_api_ipl_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_SET),
      mlag_internal_api_log_verbosity_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_GET),
      mlag_internal_api_log_verbosity_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_IPL_IDS_GET),
      mlag_internal_api_ipl_ids_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_IPL_PORT_SET),
      mlag_internal_api_ipl_port_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_IPL_PORT_GET),
      mlag_internal_api_ipl_port_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_IPL_IP_SET),
      mlag_internal_api_ipl_ip_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(
          MLAG_INTERNAL_API_CMD_VLANS_STATE_CHANGE_SYNC_FINISH_NOTIFY),
      mlag_internal_api_vlans_state_change_sync_finish_notify,
      SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PROTOCOL_OPER_STATE_GET),
      mlag_internal_api_protocol_oper_state_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_SYSTEM_ROLE_STATE_GET),
      mlag_internal_api_system_role_state_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PEERS_STATE_LIST_GET),
      mlag_internal_api_peers_state_list_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_DUMP),
      mlag_internal_api_dump, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PORTS_STATE_GET),
      mlag_internal_api_ports_state_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PORT_STATE_GET),
      mlag_internal_api_port_state_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_IPL_IP_GET),
      mlag_internal_api_ipl_ip_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_ROUTER_MAC_SET),
      mlag_internal_api_router_mac_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PORT_MODE_SET),
      mlag_internal_api_port_mode_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_PORT_MODE_GET),
      mlag_internal_api_port_mode_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_LACP_SYS_ID_SET),
      mlag_internal_api_lacp_local_sys_id_set, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_LACP_ACTOR_PARAMS_GET),
      mlag_internal_api_lacp_actor_parameters_get, SX_RPC_API_CMD_PRIO_HIGH },
    { COMMAND_ID_REPLICA(MLAG_INTERNAL_API_CMD_LACP_SELECT_REQUEST),
      mlag_internal_api_lacp_selection_request, SX_RPC_API_CMD_PRIO_HIGH },
};
/************************************************
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) =
    MLAG_VERBOSITY_LEVEL_NOTICE;

static const char *access_command_str[ACCESS_CMD_LAST] = {
    "Add",
    "Delete",
    "Create",
    "Get",
    "Get first",
    "Get next"
};
/************************************************
 *  Local function declarations
 ***********************************************/
/*
 * Converts MLAG access cmd to OES.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] command - Access command.
 *
 * @return oes_access_cmd - OES access command.
 */
static enum oes_access_cmd
convert_mlag_access_cmd_to_oes(enum access_cmd command)
{
    enum oes_access_cmd oes_command = OES_ACCESS_CMD_ADD;

    switch (command) {
    case ACCESS_CMD_ADD:
        oes_command = OES_ACCESS_CMD_ADD;
        break;
    case ACCESS_CMD_DELETE:
        oes_command = OES_ACCESS_CMD_DELETE;
        break;
    case ACCESS_CMD_CREATE:
        oes_command = OES_ACCESS_CMD_CREATE;
        break;
    case ACCESS_CMD_GET:
        oes_command = OES_ACCESS_CMD_GET;
        break;
    case ACCESS_CMD_GET_FIRST:
        oes_command = OES_ACCESS_CMD_GET_FIRST;
        break;
    case ACCESS_CMD_GET_NEXT:
        oes_command = OES_ACCESS_CMD_GET_NEXT;
        break;
    default:
        return oes_command;
    }

    return oes_command;
}

/*
 * Checks that message size is equal to the provided parameters.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_len - Receive bytes.
 * @param[in] expected_len - Expected bytes.
 *
 * @return 0 - Operation completed successfully, equal size.
 * @return -EIO - Operation failure; expected bytes not received.
 */
static int
check_message_size_equal(unsigned int rcv_len,
                         unsigned int expected_len)
{
    int err = 0;

    if (rcv_len != expected_len) {
        err = -EIO;
        MLAG_LOG(MLAG_LOG_ERROR,
                 "I/O error, didn't receive the entire message. Received [%u], expected [%u]\n",
                 rcv_len,
                 expected_len);
    }

    return err;
}

/*
 * Checks that message size is less to the provided parameters.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_len - Receive bytes.
 * @param[in] expected_len - Expected bytes.
 *
 * @return 0 - Operation completed successfully, equal size.
 * @return -EIO - Operation failure; expected bytes not received.
 */
static int
check_message_size_less(unsigned int rcv_len,
                        unsigned int expected_len)
{
    int err = 0;

    if (rcv_len < expected_len) {
        err = -EIO;
        MLAG_LOG(MLAG_LOG_ERROR,
                 "I/O error, didn't receive the entire message. Received [%u], expected at least [%u]\n",
                 rcv_len,
                 expected_len);
    }

    return err;
}

/************************************************
 *  Function implementations
 ***********************************************/

/**
 * Initializes the RPC layer.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] log_cb - Logging callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation failure.
 */
int
mlag_rpc_init_procedure(sx_log_cb_t log_cb)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "Mlag rpc init procedure\n");

    sx_rpc_register_log_function(log_cb);

    MLAG_LOG(MLAG_LOG_DEBUG, "Call to sx_api_rpc_command_db.\n");

    err = sx_rpc_api_rpc_command_db_init(mlag_api_command_id,
                                         (sizeof(mlag_api_command_id) /
                                          sizeof(sx_rpc_api_command_t)));
    if (err != SX_RPC_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to call sx_api_rpc_command_db_init. Error code %d returned\n",
                            err);
    }

    MLAG_LOG(MLAG_LOG_DEBUG, "Call to rpc_sx_core_td_init\n");

    err = sx_rpc_core_td_init(MLAG_COMMCHNL_ADDRESS,
                              MLAG_RPC_API_MESSAGE_SIZE_LIMIT);
    if (err != SX_RPC_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to call sx_core_td_init. Error code %d returned\n",
                            err);
    }

bail:
    return err;
}

/**
 * De-initializes the RPC layer.
 * Deinit is called when MLAG process exits.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation failure.
 */
int
mlag_rpc_deinit()
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "Mlag rpc deinit\n");

    err = sx_rpc_api_rpc_command_db_deinit();
    if (err != SX_RPC_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to call sx_api_rpc_command_db_deinit. Error code %d returned\n",
                            err);
    }

bail:
    return err;
}

/**
 * Infinate loop. Wait on rpc_select_event.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_rpc_start_loop(void)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "Start loop\n");

    sx_rpc_core_td_start_loop();

    return err;
}

/**
 * Notifies on-ports operational state changes.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ports_state_change_notify(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len)
{
    int err = 0;
    struct mlag_api_ports_state_change_notify_params *
    ports_state_change_notify_params = NULL;
    err = check_message_size_less(rcv_len,
                                  sizeof(struct
                                         mlag_api_ports_state_change_notify_params));
    MLAG_BAIL_ERROR(err);
    ports_state_change_notify_params =
        (struct mlag_api_ports_state_change_notify_params *)rcv_msg_body;

    /* validate parameters */
    MLAG_BAIL_CHECK(ports_state_change_notify_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK(ports_state_change_notify_params->ports_arr_cnt > 0,
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Notify on ports operational state changes. Ports number is [%u], first port is [%lu], first port state is [%s]\n",
             ports_state_change_notify_params->ports_arr_cnt,
             ports_state_change_notify_params->ports_arr[0].port_id,
             (ports_state_change_notify_params->ports_arr[0].port_state ==
              OES_PORT_DOWN) ? "DOWN" : "UP");

    err = mlag_ports_state_change_notify(
        ports_state_change_notify_params->ports_arr,
        ports_state_change_notify_params->ports_arr_cnt);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Notifies on-VLAN state changes.
 * Invoke on all VLANs.
 * VLAN is considered as up if one member port is up, excluding IPL.
 * VLAN is considered as down if all member ports of the VLAN are down, excluding IPL.
 * This function is temporary and next release it becomes considered as obsolete.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_vlans_state_change_notify(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len)
{
    int err = 0;
    struct mlag_api_vlans_state_change_notify_params *
    vlans_state_change_notify_params = NULL;
    err = check_message_size_less(rcv_len,
                                  sizeof(struct
                                         mlag_api_vlans_state_change_notify_params));
    MLAG_BAIL_ERROR(err);
    vlans_state_change_notify_params =
        (struct mlag_api_vlans_state_change_notify_params *)rcv_msg_body;

    /* validate parameters */
    MLAG_BAIL_CHECK(vlans_state_change_notify_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK(vlans_state_change_notify_params->vlans_arr_cnt > 0,
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Notify on vlans state changes. Vlans number is [%u], first vlan is [%hu], first vlan state is [%s]\n",
             vlans_state_change_notify_params->vlans_arr_cnt,
             vlans_state_change_notify_params->vlans_arr[0].vlan_id,
             (vlans_state_change_notify_params->vlans_arr[0].vlan_state ==
              VLAN_DOWN) ? "DOWN" : "UP");

    err = mlag_vlans_state_change_notify(
        vlans_state_change_notify_params->vlans_arr,
        vlans_state_change_notify_params->vlans_arr_cnt);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Notifies on-VLAN state sync change.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_vlans_state_change_sync_finish_notify(uint8_t *rcv_msg_body,
                                                        uint32_t rcv_len,
                                                        uint8_t **snd_body,
                                                        uint32_t *snd_len)
{
    int err = 0;
    UNUSED_PARAM(rcv_msg_body);

    err = check_message_size_equal(rcv_len, sizeof(unsigned int));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Vlans state change sync done\n");

    err = mlag_vlans_state_change_sync_finish_notify();
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Notifies on-peer management connectivity state changes.
 * The system identification refers to the peer.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_mgmt_peer_state_notify(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len,
                                         uint8_t **snd_body,
                                         uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(mgmt_peer_state_notify_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(mgmt_peer_state_notify_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Management peer operational state changes. Peer system id is [%llu], the mgmt state is [%s]\n",
             mgmt_peer_state_notify_params->peer_system_id,
             (mgmt_peer_state_notify_params->mgmt_state ==
              MGMT_DOWN) ? "DOWN" : "UP");

    err = mlag_mgmt_peer_state_notify(
        mgmt_peer_state_notify_params->peer_system_id,
        mgmt_peer_state_notify_params->mgmt_state);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets all the current MLAG ports state information.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ports_state_get(uint8_t *rcv_msg_body,
                                  uint32_t rcv_len,
                                  uint8_t **snd_body,
                                  uint32_t *snd_len)
{
    int err = 0;
    struct mlag_api_ports_state_get_params *ports_state_get_params = NULL;
    err = check_message_size_less(rcv_len,
                                  sizeof(struct mlag_api_ports_state_get_params));
    MLAG_BAIL_ERROR(err);
    ports_state_get_params =
        (struct mlag_api_ports_state_get_params *)rcv_msg_body;

    /* validate parameters */
    MLAG_BAIL_CHECK(ports_state_get_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK(ports_state_get_params->mlag_ports_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Get mlag ports state. Mlag ports number is [%u]\n",
             ports_state_get_params->mlag_ports_cnt);

    err = mlag_ports_state_get(
        ports_state_get_params->mlag_ports_information,
        &(ports_state_get_params->mlag_ports_cnt));
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(ports_state_get_params);
    (*snd_len) = sizeof(struct mlag_api_ports_state_get_params) +
                 (ports_state_get_params->mlag_ports_cnt *
                  sizeof(struct mlag_port_info));

bail:
    return err;
}

/**
 * Populates mlag_port_information with the port information of the given port ID.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_port_state_get(uint8_t *rcv_msg_body,
                                 uint32_t rcv_len,
                                 uint8_t **snd_body,
                                 uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(port_state_get_params);

    /* validate parameter */
    MLAG_BAIL_CHECK(port_state_get_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get mlag port state. Port id is [%lu]\n",
             port_state_get_params->port_id);

    err = mlag_port_state_get(port_state_get_params->port_id,
                              &(port_state_get_params->mlag_port_information));
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(port_state_get_params);
    (*snd_len) = sizeof(*port_state_get_params);

bail:
    return err;
}

/**
 * Gets the current protocol operational state.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_protocol_oper_state_get(uint8_t *rcv_msg_body,
                                          uint32_t rcv_len,
                                          uint8_t **snd_body,
                                          uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(enum protocol_oper_state, rpc_param);

    /* validate parameter */
    MLAG_BAIL_CHECK(rpc_param != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get mlag protocol operational state\n");

    err = mlag_protocol_oper_state_get(rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(rpc_param);
    (*snd_len) = sizeof(*rpc_param);

bail:
    return err;
}

/**
 * Gets the current system role state.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_system_role_state_get(uint8_t *rcv_msg_body,
                                        uint32_t rcv_len,
                                        uint8_t **snd_body,
                                        uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(enum system_role_state, rpc_param);

    /* validate parameter */
    MLAG_BAIL_CHECK(rpc_param != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get system role state\n");

    err = mlag_system_role_state_get(rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(rpc_param);
    (*snd_len) = sizeof(*rpc_param);

bail:
    return err;
}

/**
 * Gets the current peers state.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_peers_state_list_get(uint8_t *rcv_msg_body,
                                       uint32_t rcv_len,
                                       uint8_t **snd_body,
                                       uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(peers_state_list_get_params);

    /* validate parameter */
    MLAG_BAIL_CHECK(peers_state_list_get_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get peers state list. Peers list count is %u\n",
             peers_state_list_get_params->peers_list_cnt);

    err = mlag_peers_state_list_get(peers_state_list_get_params->peers_list,
                                    &(peers_state_list_get_params->
                                      peers_list_cnt));
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(peers_state_list_get_params);
    (*snd_len) = sizeof(*peers_state_list_get_params);

bail:
    return err;
}

/**
 * Gets the current values of the corresponding MLAG protocol counters.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_counters_get(uint8_t *rcv_msg_body,
                               uint32_t rcv_len,
                               uint8_t **snd_body,
                               uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(counters_get_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(counters_get_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get mlag protocol counters.\n");

    err = mlag_counters_get(counters_get_params->ipl_list,
                            counters_get_params->ipls_cnt,
                            counters_get_params->counters_list);
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(counters_get_params);
    (*snd_len) = sizeof(*counters_get_params);

bail:
    return err;
}

/**
 * Clears MLAG protocol counters.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_counters_clear(uint8_t *rcv_msg_body,
                                 uint32_t rcv_len,
                                 uint8_t **snd_body,
                                 uint32_t *snd_len)
{
    int err = 0;
    UNUSED_PARAM(rcv_msg_body);

    err = check_message_size_equal(rcv_len, sizeof(unsigned int));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Clear mlag protocol counters\n");

    err = mlag_counters_clear();
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Specifies the maximum period of time that MLAG ports are disabled after restarting
 * the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the MLAG protocol information before opening the MLAG ports.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_reload_delay_set(uint8_t *rcv_msg_body,
                                   uint32_t rcv_len,
                                   uint8_t **snd_body,
                                   uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(unsigned int, rpc_param);

    /*validate parameter */
    MLAG_BAIL_CHECK((*rpc_param <= MLAG_RELOAD_DELAY_MAX),
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Reload delay set. sec [%u]\n",
             *rpc_param);

    err = mlag_reload_delay_set(*rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets the current value of the maximum period of time that MLAG ports are
 * disabled after restarting the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the MLAG protocol information before opening the MLAG ports.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_reload_delay_get(uint8_t *rcv_msg_body,
                                   uint32_t rcv_len,
                                   uint8_t **snd_body,
                                   uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(unsigned int, rpc_param);

    /* validate parameter */
    MLAG_BAIL_CHECK(rpc_param != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get reload delay value\n");

    err = mlag_reload_delay_get(rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Reload delay value is [%u]\n",
             *rpc_param);

    (*snd_body) = (uint8_t *)(rpc_param);
    (*snd_len) = sizeof(*rpc_param);

bail:
    return err;
}

/**
 * Specifies the interval during which keep-alive messages are issued.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_keepalive_interval_set(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len,
                                         uint8_t **snd_body,
                                         uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(unsigned int, rpc_param);

    /* validate parameter */
    MLAG_BAIL_CHECK((*rpc_param >= MLAG_KEEPALIVE_INTERVAL_MIN) &&
                    (*rpc_param <= MLAG_KEEPALIVE_INTERVAL_MAX), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Keep-alive interval set. sec [%u]\n",
             *rpc_param);

    err = mlag_keepalive_interval_set(*rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Populates sec with the current value of the interval at which keep-alive messages are issued.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_keepalive_interval_get(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len,
                                         uint8_t **snd_body,
                                         uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(unsigned int, rpc_param);

    /* validate parameter */
    MLAG_BAIL_CHECK(rpc_param != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get keep-alive interval value\n");

    err = mlag_keepalive_interval_get(rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Keepalive interval value is [%u]\n",
             *rpc_param);

    (*snd_body) = (uint8_t *)(rpc_param);
    (*snd_len) = sizeof(*rpc_param);

bail:
    return err;
}

/**
 * Adds/deletes MLAG port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_port_set(uint8_t *rcv_msg_body,
                           uint32_t rcv_len,
                           uint8_t **snd_body,
                           uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(port_set_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(port_set_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "%s mlag port. Port id is [%lu]\n",
             ACCESS_COMMAND_STR(port_set_params->access_cmd),
             port_set_params->port_id);

    err = mlag_port_set(port_set_params->access_cmd,
                        port_set_params->port_id);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Creates/deletes an IPL.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ipl_set(uint8_t *rcv_msg_body,
                          uint32_t rcv_len,
                          uint8_t **snd_body,
                          uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(ipl_set_params);

    /* validate parameter */
    MLAG_BAIL_CHECK(ipl_set_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK((ipl_set_params->access_cmd == ACCESS_CMD_CREATE) ||
                    (ipl_set_params->access_cmd == ACCESS_CMD_DELETE),
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "%s an IPL\n",
             ACCESS_COMMAND_STR(ipl_set_params->access_cmd));

    err = mlag_ipl_set(ipl_set_params->access_cmd,
                       &(ipl_set_params->ipl_id));
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets all the IPL IDs that have been created.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ipl_ids_get(uint8_t *rcv_msg_body,
                              uint32_t rcv_len,
                              uint8_t **snd_body,
                              uint32_t *snd_len)
{
    int err = 0;
    struct mlag_api_ipl_ids_get_params *ipl_ids_get_params = NULL;

    err =
        check_message_size_less(rcv_len,
                                sizeof(struct mlag_api_ipl_ids_get_params));
    MLAG_BAIL_ERROR(err);
    ipl_ids_get_params = (struct mlag_api_ipl_ids_get_params *)rcv_msg_body;

    /* validate parameter */
    MLAG_BAIL_CHECK(ipl_ids_get_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK(ipl_ids_get_params->ipl_ids_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get ipl ids\n");

    err = mlag_ipl_ids_get(ipl_ids_get_params->ipl_ids,
                           &(ipl_ids_get_params->ipl_ids_cnt));
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(ipl_ids_get_params);
    (*snd_len) = sizeof(struct mlag_api_ipl_ids_get_params) +
                 (ipl_ids_get_params->ipl_ids_cnt * sizeof(unsigned int));

bail:
    return err;
}

/**
 * Binds/unbinds port to an IPL.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ipl_port_set(uint8_t *rcv_msg_body,
                               uint32_t rcv_len,
                               uint8_t **snd_body,
                               uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(ipl_port_set_params);

    /* validate parameters */
    MLAG_BAIL_CHECK((ipl_port_set_params->access_cmd == ACCESS_CMD_ADD) ||
                    (ipl_port_set_params->access_cmd == ACCESS_CMD_DELETE),
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s port to an IPL. The ipl is [%d], the port is [%lu]\n",
             ACCESS_COMMAND_STR(ipl_port_set_params->access_cmd),
             ipl_port_set_params->ipl_id,
             ipl_port_set_params->port_id);

    err = mlag_ipl_port_set(ipl_port_set_params->access_cmd,
                            ipl_port_set_params->ipl_id,
                            ipl_port_set_params->port_id);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets the port ID that have been binded to the given IPL.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ipl_port_get(uint8_t *rcv_msg_body,
                               uint32_t rcv_len,
                               uint8_t **snd_body,
                               uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(ipl_port_get_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(ipl_port_get_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get ipl port. Ipl is %d\n",
             ipl_port_get_params->ipl_id);

    err = mlag_ipl_port_get(ipl_port_get_params->ipl_id,
                            &(ipl_port_get_params->port_id));
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Port id value retrieved is [%lu]\n",
             ipl_port_get_params->port_id);

    (*snd_body) = (uint8_t *)(ipl_port_get_params);
    (*snd_len) = sizeof(*ipl_port_get_params);

bail:
    return err;
}

/**
 * Sets/unsets the local and peer IP addresses for an IPL.
 * The IP is referring to the IPL interface VLAN.
 * The given IP addresses are ignored in the context of DELETE command.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 * @return -EAFNOSUPPORT- IPv6 currently not supported.
 */
int
mlag_internal_api_ipl_ip_set(uint8_t *rcv_msg_body,
                             uint32_t rcv_len,
                             uint8_t **snd_body,
                             uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(ipl_ip_set_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(ipl_ip_set_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK((ipl_ip_set_params->access_cmd == ACCESS_CMD_ADD) ||
                    (ipl_ip_set_params->access_cmd == ACCESS_CMD_DELETE),
                    -EINVAL);
    MLAG_BAIL_CHECK((ipl_ip_set_params->vlan_id >= MLAG_VLAN_ID_MIN) &&
                    (ipl_ip_set_params->vlan_id <= MLAG_VLAN_ID_MAX), -EINVAL);
    if (ipl_ip_set_params->access_cmd == ACCESS_CMD_ADD) {
        MLAG_BAIL_CHECK(
            ipl_ip_set_params->local_ipl_ip_address.version == OES_IPV4,
            -EAFNOSUPPORT);
        MLAG_BAIL_CHECK(
            ipl_ip_set_params->peer_ipl_ip_address.version == OES_IPV4,
            -EAFNOSUPPORT);
    }

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s ipl IP addresses. Local IP version [%s], Local IP is [%s],"
             "Peer Ip version [%s], Peer IP is [%s], ipl id is [%d]\n",
             ACCESS_COMMAND_STR(
                 ipl_ip_set_params->access_cmd),
             (ipl_ip_set_params->local_ipl_ip_address.version == OES_IPV6) ? "IPV6" : "IPV4",
             inet_ntoa(
                 ipl_ip_set_params->local_ipl_ip_address.addr.ipv4),
             (ipl_ip_set_params->peer_ipl_ip_address.version == OES_IPV6) ? "IPV6" : "IPV4",
             inet_ntoa(ipl_ip_set_params->peer_ipl_ip_address.addr.ipv4),
             ipl_ip_set_params->ipl_id);

    err = mlag_ipl_ip_set(ipl_ip_set_params->access_cmd,
                          ipl_ip_set_params->ipl_id,
                          ipl_ip_set_params->vlan_id,
                          &(ipl_ip_set_params->local_ipl_ip_address),
                          &(ipl_ip_set_params->peer_ipl_ip_address));
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets the IP of the local/peer and VLAN ID based of the given IPL ID.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_ipl_ip_get(uint8_t *rcv_msg_body,
                             uint32_t rcv_len,
                             uint8_t **snd_body,
                             uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(ipl_ip_get_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(ipl_ip_get_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Get the IP of the local/peer and vlan id. Ipl is %d\n",
             ipl_ip_get_params->ipl_id);

    err = mlag_ipl_ip_get(ipl_ip_get_params->ipl_id,
                          &(ipl_ip_get_params->vlan_id),
                          &(ipl_ip_get_params->local_ipl_ip_address),
                          &(ipl_ip_get_params->peer_ipl_ip_address));
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(ipl_ip_get_params);
    (*snd_len) = sizeof(*ipl_ip_get_params);

bail:
    return err;
}

/**
 * Enable the MLAG protocol.
 * Start is called to enable the protocol. From this stage on the protocol
 * is running and thus exchanges messages with peers, triggers configuration toward
 * HAL (Hardware Abstraction Layer) and events toward the system.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_start(uint8_t *rcv_msg_body,
                        uint32_t rcv_len,
                        uint8_t **snd_body,
                        uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(start_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(start_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Enable the mlag protocol.System id is [%llu]\n",
             start_params->system_id);

    err = mlag_start(start_params->system_id,
                     &(start_params->features));
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Disables the MLAG protocol.
 * Stop means stopping the MLAG capability of the switch. No messages or
 * configurations are sent from any MLAG module after handling stop request.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_stop(uint8_t *rcv_msg_body,
                       uint32_t rcv_len,
                       uint8_t **snd_body,
                       uint32_t *snd_len)
{
    int err = 0;
    UNUSED_PARAM(rcv_msg_body);

    err = check_message_size_equal(rcv_len, sizeof(unsigned int));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Disable the mlag protocol\n");

    err = mlag_stop();
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Sets module log verbosity level.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 */
int
mlag_internal_api_log_verbosity_set(uint8_t *rcv_msg_body,
                                    uint32_t rcv_len,
                                    uint8_t **snd_body,
                                    uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(log_verbosity_params);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Set module log verbosity level. Module is [%u], log level is [%u]\n",
             log_verbosity_params->module,
             log_verbosity_params->verbosity);

    switch (log_verbosity_params->module) {
    case MLAG_LOG_MODULE_INTERNAL_API:
        LOG_VAR_NAME(__MODULE__) = log_verbosity_params->verbosity;
        break;
    case MLAG_LOG_ALL:
        LOG_VAR_NAME(__MODULE__) = log_verbosity_params->verbosity;
        err = mlag_log_verbosity_set(log_verbosity_params->module,
                                     log_verbosity_params->verbosity);
        MLAG_BAIL_CHECK_NO_MSG(err);
        break;
    default:
        err = mlag_log_verbosity_set(log_verbosity_params->module,
                                     log_verbosity_params->verbosity);
        MLAG_BAIL_CHECK_NO_MSG(err);
    }

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets module log verbosity level.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 */
int
mlag_internal_api_log_verbosity_get(uint8_t *rcv_msg_body,
                                    uint32_t rcv_len,
                                    uint8_t **snd_body,
                                    uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(log_verbosity_params);

    MLAG_BAIL_CHECK(log_verbosity_params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Get module log verbosity level. Module is [%u]\n",
             log_verbosity_params->module);

    if (log_verbosity_params->module == MLAG_LOG_MODULE_INTERNAL_API) {
        log_verbosity_params->verbosity = LOG_VAR_NAME(__MODULE__);
    }
    else {
        err = mlag_log_verbosity_get(log_verbosity_params->module,
                                     &(log_verbosity_params->verbosity));
        MLAG_BAIL_CHECK_NO_MSG(err);
    }

    (*snd_body) = (uint8_t *)(log_verbosity_params);
    (*snd_len) = sizeof(*log_verbosity_params);

bail:
    return err;
}

/**
 * Adds/deletes UC MAC and UC LAG MAC entries in the FDB. Currently it support only static
 * MAC insertion.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_uc_mac_addr_set(uint8_t *rcv_msg_body,
                                      uint32_t rcv_len,
                                      uint8_t **snd_body,
                                      uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(fdb_uc_mac_addr_set_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(fdb_uc_mac_addr_set_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK(fdb_uc_mac_addr_set_params->mac_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK((fdb_uc_mac_addr_set_params->mac_entry.mac_addr_params.vid
                     >= MLAG_VLAN_ID_MIN) &&
                    (fdb_uc_mac_addr_set_params->mac_entry.mac_addr_params.vid
                     <= MLAG_VLAN_ID_MAX), -EINVAL);
    MLAG_BAIL_CHECK((fdb_uc_mac_addr_set_params->access_cmd == ACCESS_CMD_ADD) ||
                    (fdb_uc_mac_addr_set_params->access_cmd ==
                     ACCESS_CMD_DELETE),
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "%s UC MAC and UC LAG MAC entries in the FDB\n",
             ACCESS_COMMAND_STR(fdb_uc_mac_addr_set_params->access_cmd));

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_api_fdb_uc_mac_addr_set(
        convert_mlag_access_cmd_to_oes(fdb_uc_mac_addr_set_params->access_cmd),
        &(fdb_uc_mac_addr_set_params->mac_entry),
        &(fdb_uc_mac_addr_set_params->mac_cnt),
        (void *)fdb_uc_mac_addr_set_params->originator_cookie, 1);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Flushes the entire FDB table.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_uc_flush_set(uint8_t *rcv_msg_body,
                                   uint32_t rcv_len,
                                   uint8_t **snd_body,
                                   uint32_t *snd_len)
{
    int err = 0;
    INIT_RPC_POINTER_PARAM_AND_CHECK(long, rpc_param);

    /* validate parameter */
    MLAG_BAIL_CHECK(rpc_param != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Flush the entire FDB table\n");

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_api_fdb_uc_flush_set((void *)rpc_param);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Flushes all FDB table entries that have been learned on the given port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_uc_flush_port_set(uint8_t *rcv_msg_body,
                                        uint32_t rcv_len,
                                        uint8_t **snd_body,
                                        uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(fdb_uc_flush_port_set_params);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Flush all FDB table entries that were learned on the given port. Port id is [%lu]\n",
             fdb_uc_flush_port_set_params->port_id);

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_api_fdb_uc_flush_port_set(
        fdb_uc_flush_port_set_params->port_id,
        (void *)fdb_uc_flush_port_set_params->originator_cookie);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Flushes the FDB table entries that have been learned on the given VLAN ID.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_uc_flush_vid_set(uint8_t *rcv_msg_body,
                                       uint32_t rcv_len,
                                       uint8_t **snd_body,
                                       uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(fdb_uc_flush_vid_set_params);

    /* validate parameters */
    MLAG_BAIL_CHECK((fdb_uc_flush_vid_set_params->vlan_id >= MLAG_VLAN_ID_MIN) &&
                    (fdb_uc_flush_vid_set_params->vlan_id <= MLAG_VLAN_ID_MAX),
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Flush the FDB table entries that were learned on the given vlan id. Vlan id is [%hu]\n",
             fdb_uc_flush_vid_set_params->vlan_id);

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_api_fdb_uc_flush_vid_set(
        fdb_uc_flush_vid_set_params->vlan_id,
        (void *)fdb_uc_flush_vid_set_params->originator_cookie);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Flushes all FDB table entries that have been learned on the given VLAN ID and port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_uc_flush_port_vid_set(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(fdb_uc_flush_port_vid_set_params);

    /* validate parameters */
    MLAG_BAIL_CHECK((fdb_uc_flush_port_vid_set_params->vlan_id >=
                     MLAG_VLAN_ID_MIN) &&
                    (fdb_uc_flush_port_vid_set_params->vlan_id <=
                     MLAG_VLAN_ID_MAX), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Flush all FDB table entries that were learned on the given vlan id and port. Vlan id is [%hu], port id [%lu]\n",
             fdb_uc_flush_port_vid_set_params->vlan_id,
             fdb_uc_flush_port_vid_set_params->port_id);

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_api_fdb_uc_flush_port_vid_set(
        fdb_uc_flush_port_vid_set_params->vlan_id,
        fdb_uc_flush_port_vid_set_params->port_id,
        (void *)fdb_uc_flush_port_vid_set_params->originator_cookie);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets MAC entries from the SW FDB table, which is an exact copy of HW DB on any
 * device.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_uc_mac_addr_get(uint8_t *rcv_msg_body,
                                      uint32_t rcv_len,
                                      uint8_t **snd_body,
                                      uint32_t *snd_len)
{
    int err = 0;
    struct mlag_api_fdb_uc_mac_addr_get_params *fdb_uc_mac_addr_get_params =
        NULL;

    err =
        check_message_size_less(rcv_len,
                                sizeof(struct
                                       mlag_api_fdb_uc_mac_addr_get_params));
    MLAG_BAIL_ERROR(err);
    fdb_uc_mac_addr_get_params =
        (struct mlag_api_fdb_uc_mac_addr_get_params *)rcv_msg_body;

    /* validate parameters */
    MLAG_BAIL_CHECK(fdb_uc_mac_addr_get_params != NULL, -EINVAL);
    MLAG_BAIL_CHECK(fdb_uc_mac_addr_get_params->data_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK((fdb_uc_mac_addr_get_params->access_cmd == ACCESS_CMD_GET) ||
                    (fdb_uc_mac_addr_get_params->access_cmd ==
                     ACCESS_CMD_GET_FIRST) ||
                    (fdb_uc_mac_addr_get_params->access_cmd ==
                     ACCESS_CMD_GET_NEXT),
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s MAC entries from the SW FDB table, which is exact copy of HW DB on any device\n",
             ACCESS_COMMAND_STR(fdb_uc_mac_addr_get_params->access_cmd));

    BAIL_MLAG_NOT_INIT();

    err =
        ctrl_learn_api_uc_mac_addr_get(convert_mlag_access_cmd_to_oes(
                                           fdb_uc_mac_addr_get_params->
                                           access_cmd),
                                       &(fdb_uc_mac_addr_get_params->key_filter),
                                       fdb_uc_mac_addr_get_params->mac_list,
                                       &(fdb_uc_mac_addr_get_params->data_cnt),
                                       1);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(fdb_uc_mac_addr_get_params);
    (*snd_len) = sizeof(struct mlag_api_fdb_uc_mac_addr_get_params) +
                 (fdb_uc_mac_addr_get_params->data_cnt *
                  sizeof(struct fdb_uc_mac_addr_params));

bail:
    return err;
}

/**
 * Initializes the control learning library.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_init(uint8_t *rcv_msg_body,
                           uint32_t rcv_len,
                           uint8_t **snd_body,
                           uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(fdb_init_params);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Initialize the control learning library. Bridge id is [%d]\n",
             fdb_init_params->br_id);

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_init(fdb_init_params->br_id, 1,
                          fdb_init_params->logging_cb);
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * De-initializes the control learning library.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_deinit(uint8_t *rcv_msg_body,
                             uint32_t rcv_len,
                             uint8_t **snd_body,
                             uint32_t *snd_len)
{
    int err = 0;
    UNUSED_PARAM(rcv_msg_body);

    err = check_message_size_equal(rcv_len, sizeof(unsigned int));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "De-Initialize the control learning library\n");

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_deinit();
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Starts the control learning library and listens to FDB events.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_start(uint8_t *rcv_msg_body,
                            uint32_t rcv_len,
                            uint8_t **snd_body,
                            uint32_t *snd_len)
{
    int err = 0;
    UNUSED_PARAM(rcv_msg_body);

    err = check_message_size_equal(rcv_len, sizeof(unsigned int));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Start the control learning library\n");

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_start();
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Stops control learning functionality.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_fdb_stop(uint8_t *rcv_msg_body,
                           uint32_t rcv_len,
                           uint8_t **snd_body,
                           uint32_t *snd_len)
{
    int err = 0;
    UNUSED_PARAM(rcv_msg_body);

    err = check_message_size_equal(rcv_len, sizeof(unsigned int));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Stop the control learning library\n");

    BAIL_MLAG_NOT_INIT();

    err = ctrl_learn_stop();
    CONVERT_CTRL_LEARN_ERR_TO_API(err);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Generates MLAG system dump.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_dump(uint8_t *rcv_msg_body,
                       uint32_t rcv_len,
                       uint8_t **snd_body,
                       uint32_t *snd_len)
{
    int err = 0;
    char *rpc_param = NULL;

    rpc_param = (char *)rcv_msg_body;

    err = check_message_size_equal(rcv_len, (strlen(rpc_param) + 1));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Generate mlag system dump. The dump file path is %s\n",
             rpc_param);

    BAIL_MLAG_NOT_INIT();

    err = mlag_dump(rpc_param);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Sets router MAC.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_router_mac_set(uint8_t *rcv_msg_body,
                                 uint32_t rcv_len,
                                 uint8_t **snd_body,
                                 uint32_t *snd_len)
{
    int err = 0;
    struct mlag_api_router_mac_set_params *router_mac_set_params = NULL;
    err = check_message_size_less(rcv_len,
                                  sizeof(struct mlag_api_router_mac_set_params));
    MLAG_BAIL_ERROR(err);

    router_mac_set_params =
        (struct mlag_api_router_mac_set_params *)rcv_msg_body;

    /* validate parameters */
    MLAG_BAIL_CHECK((router_mac_set_params->access_cmd == ACCESS_CMD_ADD) ||
                    (router_mac_set_params->access_cmd == ACCESS_CMD_DELETE),
                    -EINVAL);
    MLAG_BAIL_CHECK(router_mac_set_params->router_macs_list_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK(router_mac_set_params->router_macs_list != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s router mac. First router mac is [%llu], the vlan id is [%hu]\n",
             ACCESS_COMMAND_STR(router_mac_set_params->access_cmd),
             router_mac_set_params->router_macs_list[0].mac,
             router_mac_set_params->router_macs_list[0].vlan_id);

    err = mlag_router_mac_set(router_mac_set_params->access_cmd,
                              router_mac_set_params->router_macs_list,
                              router_mac_set_params->router_macs_list_cnt);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Sets port mode.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_port_mode_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                uint8_t **snd_body, uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(port_mode_params);

    /* validate parameters */
    MLAG_BAIL_CHECK(port_mode_params->port_mode < MLAG_PORT_MODE_LAST,
                    -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Set port [%lu] to mode [%u]\n",
             port_mode_params->port_id, port_mode_params->port_mode);

    err = mlag_port_mode_set(port_mode_params->port_id,
                             port_mode_params->port_mode);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets port mode.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_port_mode_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                uint8_t **snd_body, uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(port_mode_params);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get port [%lu] mode]\n",
             port_mode_params->port_id);

    err = mlag_port_mode_get(port_mode_params->port_id,
                             &port_mode_params->port_mode);
    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(port_mode_params);
    (*snd_len) = sizeof(struct mlag_api_port_mode_params);

bail:
    return err;
}

/**
 * Sets the local system ID for LACP PDUs.
 * This function works asynchronously.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_lacp_local_sys_id_set(uint8_t *rcv_msg_body,
                                        uint32_t rcv_len,
                                        uint8_t **snd_body, uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(lacp_actor_params);

    MLAG_LOG(MLAG_LOG_DEBUG, "Set local sys_id to [%llu] \n",
             lacp_actor_params->system_id);

    err = mlag_lacp_local_sys_id_set(lacp_actor_params->system_id);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}

/**
 * Gets actor attributes. The actor attributes are
 * system ID to be used in the LACP PDU and a chassis ID
 * which is an index of this node within the MLAG
 * cluster, with a value in the range of 0..15
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_lacp_actor_parameters_get(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(lacp_actor_params);

    err = mlag_lacp_actor_parameters_get(&lacp_actor_params->system_id,
                                         &lacp_actor_params->chassis_id);

    MLAG_BAIL_CHECK_NO_MSG(err);

    (*snd_body) = (uint8_t *)(lacp_actor_params);
    (*snd_len) = sizeof(struct mlag_api_lacp_actor_params);

bail:
    return err;
}

/**
 * Triggers a selection query to the MLAG module.
 * Since MLAG is distributed, this request may involve a remote peer, so
 * this function works asynchronously. The response for this request is
 * guaranteed and it is issued as a notification when the response is available.
 * When a delete command is used, only port_id parameter is relevant.
 * Force option is relevant for ADD command and is given in order to allow
 * releasing currently used key and migrating to the given partner.
 *
 * @param[in] rcv_msg_body - Contains the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_internal_api_lacp_selection_request(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len,
                                         uint8_t **snd_body,
                                         uint32_t *snd_len)
{
    int err = 0;
    INIT_STRUCT_AND_CHECK(lacp_selection_request_params);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "lacp selection request cmd [%s] req_id [%u] port [%lu] sys_id [%llu] key [%u] force [%u]\n",
             ACCESS_COMMAND_STR(lacp_selection_request_params->access_cmd),
             lacp_selection_request_params->request_id,
             lacp_selection_request_params->port_id,
             lacp_selection_request_params->partner_sys_id,
             lacp_selection_request_params->partner_key,
             lacp_selection_request_params->force);

    err = mlag_lacp_selection_request(
        lacp_selection_request_params->access_cmd,
        lacp_selection_request_params->request_id,
        lacp_selection_request_params->port_id,
        lacp_selection_request_params->partner_sys_id,
        lacp_selection_request_params->partner_key,
        lacp_selection_request_params->force);
    MLAG_BAIL_CHECK_NO_MSG(err);

    RETURN_EMPTY_REPLY(snd_body, snd_len);

bail:
    return err;
}


