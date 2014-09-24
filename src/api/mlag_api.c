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

#include <stdlib.h>
#include <complib/sx_rpc.h>
#include <complib/cl_mem.h>
#include "mlag_api.h"
#include "mlag_bail.h"
#include "mlag_api_rpc.h"
#include <libs/mlag_manager/mlag_manager.h>
#include <arpa/inet.h>

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_API

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/
#define NA 0
/************************************************
 *  Global variables
 ***********************************************/
EXTERN_API int (*g_connector_api_deinit_func)(void);
/************************************************
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

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
 * Populates handle with a new open channel to RPC operations.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] handle - RPC handle.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Operation failure.
 */
static int
open_handle(mlag_handle_t *handle)
{
    int err = 0;

    MLAG_BAIL_CHECK(handle != NULL, -EINVAL);

    /* first open socket */
    err = sx_rpc_open((sx_rpc_handle_t *)handle, MLAG_COMMCHNL_ADDRESS);
    if (err == SX_RPC_STATUS_PARAM_NULL) {
        err = -EINVAL;
    }
    else if (err == SX_RPC_STATUS_ERROR) {
        err = -EIO;
    }
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 * Gets the handle to RPC operations.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] mlag_handle - RPC handle.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Operation failure.
 */
static int
get_handle(mlag_handle_t *mlag_handle)
{
    int err = 0;
    static mlag_handle_t handle = 0; /* The connection handle */
    static int is_initalize = FALSE;

    if (is_initalize == FALSE) {
        err = open_handle(&handle);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to open a handle\n");

        is_initalize = TRUE;
    }

    *mlag_handle = handle;

bail:
    return err;
}

/*
 * Sends command wrapper.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] opcode - Command opcode.
 * @param[in,out] cmd_body_p - Content.
 *                             In: command content.
 *                             Out: reply content.
 * @param[in] cmd_size - Content size.
 * @param[in] expected - Content reply size expected.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or the operation failed.
 */
static int
mlag_api_send_command_wrapper(uint32_t opcode,
                              uint8_t *cmd_body_p,
                              uint32_t cmd_size,
                              uint32_t expected)
{
    int err = 0;
    sx_rpc_api_command_head_t cmd_head;
    sx_rpc_api_reply_head_t reply_head;
    uint32_t get_msg_body = 0;
    mlag_handle_t handle = 0;

    memset(&reply_head, 0, sizeof(reply_head));
    memset(&cmd_head, 0, sizeof(cmd_head));

    cmd_head.opcode = opcode;
    cmd_head.version = 1;
    cmd_head.msg_size = sizeof(sx_rpc_api_command_head_t) + cmd_size;

    err = get_handle(&handle);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get handle\n")

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Send command wrapper. The command is [%u]\n",
             opcode);

    err = sx_rpc_send_command_decoupled(handle,
                                        &cmd_head,
                                        cmd_body_p,
                                        &reply_head,
                                        cmd_body_p,
                                        cmd_size);
    if ((err != SX_RPC_STATUS_SUCCESS) && (err > 0)) {
        /* mlag api return codes is based on -errno */
        err = -EIO;

        MLAG_BAIL_ERROR_MSG(err,
                            "sx_rpc_send_command_decoupled failed. Error code %d returned\n",
                            err);
    }

    get_msg_body = reply_head.msg_size - sizeof(sx_rpc_api_reply_head_t);
    if (!err && expected && (get_msg_body != expected)) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "I/O error, didn't receive the entire message opcode [%u]. Received [%u], expected [%u]\n",
                            opcode,
                            get_msg_body,
                            expected);
    }

bail:
    return err;
}

/*
 * Closes the open channel to RPC operations.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] handle - RPC handle.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Operation failure.
 */

static int
close_handle()
{
    int err = 0;
    mlag_handle_t handle;

    err = get_handle(&handle);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get handle\n")

    err = sx_rpc_close((sx_rpc_handle_t *)&handle);
    if (err == SX_RPC_STATUS_PARAM_NULL) {
        err = -EINVAL;
    }
    else if (err == SX_RPC_STATUS_ERROR) {
        err = -EIO;
    }
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to close RPC socket\n");

    MLAG_LOG(MLAG_LOG_DEBUG, "Closed RPC socket\n");

bail:
    return err;
}

/************************************************
 *  Function implementations
 ***********************************************/

/**
 * De-initializes MLAG.
 * Deinit is called when the MLAG process exits.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] connector_type - Represents the connector type.
 *                             Call with MLAG_LOCAL_CONNECTOR if the connector run in the same
 *                             process as the MLAG core, else with MLAG_REMOTE_CONNECTOR.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or the operation failed.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first
 */
int
mlag_api_deinit(enum mlag_connector_type connector_type)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "De-Initialize the mlag protocol\n");

    /* close the RPC handle */
    err = close_handle();
    MLAG_BAIL_ERROR_MSG(err, "Failed to close the handle\n");

    if (connector_type == MLAG_LOCAL_CONNECTOR) {
        MLAG_BAIL_CHECK(g_connector_api_deinit_func != NULL, -EINVAL);

        err = (*g_connector_api_deinit_func)();
        MLAG_BAIL_CHECK_NO_MSG(err);
    }
    else {
        /* send quit signal to mlag core */
        system("kill -9 `pidof mlag`");
    }

bail:
    return err;
}

/**
 * Enables MLAG.
 * Start is called to enable the protocol. From this stage on, the protocol
 * runs and thus exchanges messages with peers, triggers configuration toward
 * HAL (Hardware Abstraction Layer) and events toward the system.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] system_id - The system identification of the local machine.
 * @param[in] params - MLAG features. Disable (=FALSE) a specific feature or enable (=TRUE) it.
 *                     Pointer to an already allocated memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_start(const unsigned long long system_id,
               const struct mlag_start_params *params)
{
    int err = 0;

    struct mlag_api_start_params cmd_body = {
        .system_id = system_id,
        .features = *params,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK(params != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Enable the mlag protocol. System id is [%llu]\n",
             system_id);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_START,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Disables the MLAG protocol.
 * Stop means stopping the MLAG capability of the switch. No messages or
 * configurations should be sent from any MLAG module after handling stop request.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_stop(void)
{
    int err = 0;
    unsigned int rpc_unused_param;

    MLAG_LOG(MLAG_LOG_NOTICE, "Disable the mlag protocol\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_STOP,
                                        (uint8_t*) &rpc_unused_param,
                                        sizeof(rpc_unused_param),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Notifies on ports operational state changes.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] ports_arr - Port state information record array. Pointer to an already
 *                        allocated memory structure.
 * @param[in] ports_arr_cnt - Array size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ports_state_change_notify(const struct port_state_info *ports_arr,
                                   const unsigned int ports_arr_cnt)
{
    int err = 0;
    struct mlag_api_ports_state_change_notify_params *cmd_body = NULL;
    unsigned int size = 0;

    /* validate parameters */
    MLAG_BAIL_CHECK(ports_arr != NULL, -EINVAL);
    MLAG_BAIL_CHECK(ports_arr_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Notify on ports operational state changes. Ports number is [%u], first port is [%lu], first port state is [%s]\n",
             ports_arr_cnt,
             ports_arr->port_id,
             (ports_arr->port_state == OES_PORT_DOWN) ? "DOWN" : "UP");

    size = sizeof(struct mlag_api_ports_state_change_notify_params) +
           ports_arr_cnt * sizeof(struct port_state_info);
    cmd_body =
        (struct mlag_api_ports_state_change_notify_params *)cl_malloc(size);
    if (cmd_body == NULL) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Failed to allocate cmd_body memory\n");
        goto bail;
    }

    cmd_body->ports_arr_cnt = ports_arr_cnt;
    memcpy(cmd_body->ports_arr, ports_arr, ports_arr_cnt *
           sizeof(struct port_state_info));

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_PORTS_STATE_CHANGE_NOTIFY,
        (uint8_t*)cmd_body,
        size,
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    if (cmd_body) {
        cl_free(cmd_body);
    }
    return err;
}

/**
 * Notifies on VLANs state changes.
 * Invoke on all VLANs.
 * VLAN is considered up if one member port is up, excluding IPL.
 * VLAN considered down if all the member ports of the VLAN down, excluding IPL.
 * This function is temporary and next release it becomes considered as obsolete.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] vlans_arr - VLAN state information record array. Pointer to an already
 *                        allocated memory structure.
 * @param[in] vlans_arr_cnt - Array size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_vlans_state_change_notify(const struct vlan_state_info *vlans_arr,
                                   const unsigned int vlans_arr_cnt)
{
    int err = 0;
    struct mlag_api_vlans_state_change_notify_params *cmd_body = NULL;
    unsigned int size = 0;

    /* validate parameters */
    MLAG_BAIL_CHECK(vlans_arr != NULL, -EINVAL);
    MLAG_BAIL_CHECK(vlans_arr_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Notify on vlans state changes. Vlans number is [%u], first vlan is [%hu], first vlan state is [%s]\n",
             vlans_arr_cnt,
             vlans_arr->vlan_id,
             (vlans_arr->vlan_state == VLAN_DOWN) ? "DOWN" : "UP");

    size = sizeof(struct mlag_api_vlans_state_change_notify_params) +
           vlans_arr_cnt * sizeof(struct vlan_state_info);
    cmd_body =
        (struct mlag_api_vlans_state_change_notify_params *)cl_malloc(size);
    if (cmd_body == NULL) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Failed to allocate cmd_body memory\n");
        goto bail;
    }

    cmd_body->vlans_arr_cnt = vlans_arr_cnt;
    memcpy(cmd_body->vlans_arr, vlans_arr, vlans_arr_cnt *
           sizeof(struct vlan_state_info));

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_VLANS_STATE_CHANGE_NOTIFY,
        (uint8_t*)cmd_body,
        size,
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    if (cmd_body) {
        cl_free(cmd_body);
    }
    return err;
}

/**
 * Notifies on VLANs state change sync done.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_vlans_state_change_sync_finish_notify(void)
{
    int err = 0;
    unsigned int rpc_unused_param;

    MLAG_LOG(MLAG_LOG_DEBUG, "Vlans state change sync done\n");

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_VLANS_STATE_CHANGE_SYNC_FINISH_NOTIFY,
        (uint8_t*) &rpc_unused_param,
        sizeof(rpc_unused_param),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Notifies on peer management connectivity state changes.
 * The system identification refers to the peer.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] peer_system_id - System identification.
 * @param[in] mgmt_state - Up/Down.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_mgmt_peer_state_notify(const unsigned long long peer_system_id,
                                const enum mgmt_oper_state mgmt_state)
{
    int err = 0;
    struct mlag_api_mgmt_peer_state_notify_params cmd_body = {
        .peer_system_id = peer_system_id,
        .mgmt_state = mgmt_state,
    };

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Management peer operational state changes. Peer system id is [%llu], the mgmt state is [%s]\n",
             peer_system_id,
             (mgmt_state == MGMT_DOWN) ? "DOWN" : "UP");

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_MGMT_PEER_STATE_NOTIFY,
        (uint8_t*) &cmd_body,
        sizeof(cmd_body),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Populates mlag_ports_information with all the current MLAG ports.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] mlag_ports_information - MLAG ports. Pointer to an already allocated memory
 *                                      structure.
 * @param[in,out] mlag_ports_cnt - Number of ports to retrieve. If fewer ports have been
 *                                 successfully retrieved, then mlag_ports_cnt will contain the
 *                                 number of successfully retrieved ports.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ports_state_get(struct mlag_port_info *mlag_ports_information,
                         unsigned int *mlag_ports_cnt)
{
    int err = 0;
    struct mlag_api_ports_state_get_params *cmd_body = NULL;
    unsigned int size = 0;

    /* validate parameters */
    MLAG_BAIL_CHECK(mlag_ports_information != NULL, -EINVAL);
    MLAG_BAIL_CHECK(mlag_ports_cnt != NULL, -EINVAL);
    MLAG_BAIL_CHECK(*mlag_ports_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Get mlag ports state. Mlag ports number is [%u]\n",
             *mlag_ports_cnt);

    size = sizeof(struct mlag_api_ports_state_get_params) +
           *mlag_ports_cnt * sizeof(struct mlag_port_info);
    cmd_body = (struct mlag_api_ports_state_get_params *)cl_malloc(size);
    if (cmd_body == NULL) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Failed to allocate cmd_body memory\n");
        goto bail;
    }

    cmd_body->mlag_ports_cnt = *mlag_ports_cnt;
    memset(cmd_body->mlag_ports_information, 0x0, *mlag_ports_cnt *
           sizeof(struct mlag_port_info));

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_PORTS_STATE_GET,
                                        (uint8_t*)cmd_body,
                                        size,
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

    memcpy(mlag_ports_information, cmd_body->mlag_ports_information,
           cmd_body->mlag_ports_cnt * sizeof(struct mlag_port_info));
    *mlag_ports_cnt = cmd_body->mlag_ports_cnt;

bail:
    if (cmd_body) {
        cl_free(cmd_body);
    }
    return err;
}

/**
 * Populates mlag_port_information with the port information of the given port ID.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] port_id - Interface index of port.
 * @param[out] mlag_port_information - MLAG port. Pointer to an already allocated memory
 *                                     structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -ENOENT - Entry not found.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_port_state_get(const unsigned long port_id,
                        struct mlag_port_info *mlag_port_information)
{
    int err = 0;
    struct mlag_api_port_state_get_params cmd_body = {
        .port_id = port_id,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK(mlag_port_information != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get mlag port state. Port id is [%lu]\n",
             port_id);

    memset(&cmd_body.mlag_port_information, 0, sizeof(struct mlag_port_info));

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_PORT_STATE_GET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        sizeof(cmd_body));
    MLAG_BAIL_CHECK_NO_MSG(err);

    *mlag_port_information = cmd_body.mlag_port_information;

bail:
    return err;
}

/**
 * Populates protocol_oper_state with the current protocol operational state.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] protocol_oper_state - Protocol operational state. Pointer to an already allocated
 *                                   memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_protocol_oper_state_get(enum protocol_oper_state *protocol_oper_state)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK(protocol_oper_state != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get mlag protocol operational state\n");

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_PROTOCOL_OPER_STATE_GET,
        (uint8_t*)protocol_oper_state,
        sizeof(*protocol_oper_state),
        sizeof(*protocol_oper_state));
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Populates system_role_state with the current system role state.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] system_role_state - System role state. Pointer to an already allocated memory
 *                                 structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_system_role_state_get(enum system_role_state *system_role_state)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK(system_role_state != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get system role state\n");

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_SYSTEM_ROLE_STATE_GET,
        (uint8_t*)system_role_state,
        sizeof(*system_role_state),
        sizeof(*system_role_state));
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Populates peers_list with all the current peers state.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] peers_list - Peers list state. Pointer to an already allocated memory
 *                          structure.
 * @param[in,out] peers_list_cnt - Number of peers to retrieve. If fewer peers have been
 *                                 successfully retrieved, then peers_list_cnt will contain
 *                                 the number of successfully retrieved peers.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_peers_state_list_get(struct peer_state *peers_list,
                              unsigned int *peers_list_cnt)
{
    int err = 0;
    struct mlag_api_peers_state_list_get_params cmd_body;

    /* validate parameters */
    MLAG_BAIL_CHECK(peers_list != NULL, -EINVAL);
    MLAG_BAIL_CHECK(peers_list_cnt != NULL, -EINVAL);
    MLAG_BAIL_CHECK(*peers_list_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get peers state list. Peers list count is %u\n",
             *peers_list_cnt);

    cmd_body.peers_list_cnt = *peers_list_cnt;

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_PEERS_STATE_LIST_GET,
        (uint8_t*)&cmd_body,
        sizeof(cmd_body),
        sizeof(cmd_body));
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Peers list count retrieved is [%u]\n",
             cmd_body.peers_list_cnt);

    memcpy(peers_list, cmd_body.peers_list, cmd_body.peers_list_cnt *
           sizeof(struct peer_state));
    *peers_list_cnt = cmd_body.peers_list_cnt;

bail:
    return err;
}

/**
 * Populates mlag_counters with the current values of the corresponding MLAG protocol
 * counters.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] ipl_ids - IPL ids array. Pointer to an already allocated memory
 *                      structure.
 * @param[in] ipl_ids_cnt - Number of IPL MLAG counters structure to retrieve.
 * @param[out] counters - MLAG protocol counters. Pointer to an already allocated
 *                        memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_counters_get(const unsigned int *ipl_ids,
                      const unsigned int ipl_ids_cnt,
                      struct mlag_counters *counters)
{
    int err = 0;
    struct mlag_api_counters_get_params cmd_body = {
        .ipls_cnt = ipl_ids_cnt,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK(ipl_ids != NULL, -EINVAL);
    MLAG_BAIL_CHECK(ipl_ids_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK(ipl_ids_cnt <= MLAG_MAX_IPLS, -EINVAL);
    MLAG_BAIL_CHECK(counters != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get mlag protocol counters\n");

    memset(&cmd_body.ipl_list, 0, ipl_ids_cnt * sizeof(unsigned int));
    memset(&cmd_body.counters_list, 0, ipl_ids_cnt *
           sizeof(struct mlag_counters));

    memcpy(cmd_body.ipl_list, ipl_ids, ipl_ids_cnt *
           sizeof(unsigned int));

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_COUNTERS_GET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

    memcpy(counters, cmd_body.counters_list, ipl_ids_cnt *
           sizeof(struct mlag_counters));

bail:
    return err;
}

/**
 * Clears MLAG counters.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_counters_clear(void)
{
    int err = 0;
    unsigned int rpc_unused_param;

    MLAG_LOG(MLAG_LOG_DEBUG, "Clear mlag protocol counters\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_COUNTERS_CLEAR,
                                        (uint8_t*) &rpc_unused_param,
                                        sizeof(rpc_unused_param),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

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
 * @param[in] sec - The period that MLAG ports are disabled after restarting the feature.
 *		    Value ranges from 0 to 300 (5 min), inclusive.
 *                  Default interval is 30 sec.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_reload_delay_set(const unsigned int sec)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK((sec <= MLAG_RELOAD_DELAY_MAX), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Reload delay set. sec [%u]\n", sec);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_RELOAD_DELAY_SET,
                                        (uint8_t*)&sec,
                                        sizeof(sec),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Populates sec with the current value of the maximum period of time
 * that MLAG ports are disabled after restarting the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the MLAG protocol information before opening the MLAG ports.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] sec - The period that MLAG ports are disabled after restarting the
 *                   feature. Pointer to an already allocated memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_reload_delay_get(unsigned int *sec)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK(sec != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get reload delay value\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_RELOAD_DELAY_GET,
                                        (uint8_t*)sec,
                                        sizeof(*sec),
                                        sizeof(*sec));
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Reload delay value is [%u]\n",
             *sec);

bail:
    return err;
}

/**
 * Specifies the interval during which keep-alive messages are issued.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.

 * @param[in] sec - Interval duration.
 *		    Value ranges from 1 to 30, inclusive.
 *                  Default interval is 1 sec.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_keepalive_interval_set(const unsigned int sec)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK((sec >= MLAG_KEEPALIVE_INTERVAL_MIN) &&
                    (sec <= MLAG_KEEPALIVE_INTERVAL_MAX), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Keep-alive interval set. sec [%u]\n", sec);

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_KEEPALIVE_INTERVAL_SET,
        (uint8_t*)&sec,
        sizeof(sec),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Populates sec with the current duration of the interval during which keep-alive
 * messages are issued.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] sec - Interval duration.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_keepalive_interval_get(unsigned int *sec)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK(sec != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get keep-alive interval value\n");

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_KEEPALIVE_INTERVAL_GET,
        (uint8_t*)sec,
        sizeof(*sec),
        sizeof(*sec));
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Keepalive interval value is [%u]\n",
             *sec);

bail:
    return err;
}

/**
 * Adds/deletes MLAG port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - Add/Delete.
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_port_set(const enum access_cmd access_cmd,
                  const unsigned long port_id)
{
    int err = 0;
    struct mlag_api_port_set_params cmd_body = {
        .access_cmd = access_cmd,
        .port_id = port_id,
    };

    MLAG_LOG(MLAG_LOG_DEBUG, "%s mlag port. Port id is [%lu]\n",
             ACCESS_COMMAND_STR(access_cmd),
             port_id);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_PORT_SET,
                                        (uint8_t*) &cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Creates/deletes an IPL.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] access_cmd - Create/delete.
 * @param[int,out] ipl_id - IPL ID.
 *                          In: Already created IPL ID, associated with delete
 *                              command.
 *                          Out: Newly created IPL ID, associated with create
 *                               command.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ipl_set(const enum access_cmd access_cmd,
                 unsigned int *ipl_id)
{
    int err = 0;
    struct mlag_api_ipl_set_params cmd_body = {
        .access_cmd = access_cmd,
    };

    /* validate parameter */
    MLAG_BAIL_CHECK(ipl_id != NULL, -EINVAL);
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_CREATE) ||
                    (access_cmd == ACCESS_CMD_DELETE), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "%s an IPL\n",
             ACCESS_COMMAND_STR(access_cmd));

    if (access_cmd == ACCESS_CMD_DELETE) {
        cmd_body.ipl_id = *ipl_id;
        err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_SET,
                                            (uint8_t*) &cmd_body,
                                            sizeof(cmd_body),
                                            NA);
        MLAG_BAIL_CHECK_NO_MSG(err);
    }
    else {
        err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_SET,
                                            (uint8_t*) &cmd_body,
                                            sizeof(cmd_body),
                                            NA);
        MLAG_BAIL_CHECK_NO_MSG(err);

        *ipl_id = cmd_body.ipl_id;
    }

bail:
    return err;
}

/**
 * Populates ipl_ids with all the IPL IDs that have been created.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[out] ipl_ids - IPL IDs. Pointer to an already allocated memory
 *                       structure.
 * @param[in,out] ipl_ids_cnt - Number of IPL to retrieve. If fewer IPLs have been
 *                               successfully retrieved, then ipl_ids_cnt will contain the
 *                              number of successfully retrieved IPL.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ipl_ids_get(unsigned int *ipl_ids,
                     unsigned int *ipl_ids_cnt)
{
    int err = 0;
    struct mlag_api_ipl_ids_get_params *cmd_body = NULL;
    unsigned int size = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK(ipl_ids != NULL, -EINVAL);
    MLAG_BAIL_CHECK(ipl_ids_cnt != NULL, -EINVAL);
    MLAG_BAIL_CHECK(*ipl_ids_cnt > 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get ipl ids\n");

    size = sizeof(struct mlag_api_ipl_ids_get_params) + *ipl_ids_cnt *
           sizeof(unsigned int);
    cmd_body = (struct mlag_api_ipl_ids_get_params *)cl_malloc(size);

    if (cmd_body == NULL) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Failed to allocate cmd_body memory\n");
        goto bail;
    }

    cmd_body->ipl_ids_cnt = *ipl_ids_cnt;

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_IDS_GET,
                                        (uint8_t*)cmd_body,
                                        size,
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

    memcpy(ipl_ids, cmd_body->ipl_ids, cmd_body->ipl_ids_cnt *
           sizeof(unsigned int));
    *ipl_ids_cnt = cmd_body->ipl_ids_cnt;

bail:
    if (cmd_body) {
        cl_free(cmd_body);
    }
    return err;
}

/**
 * Binds/unbind port to an IPL.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.

 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] ipl_id - IPL ID.
 * @param[in] port_id - Interface index of port. Must represent a valid port.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ipl_port_set(const enum access_cmd access_cmd,
                      const int ipl_id,
                      const unsigned long port_id)
{
    int err = 0;
    struct mlag_api_ipl_port_set_params cmd_body = {
        .access_cmd = access_cmd,
        .ipl_id = ipl_id,
        .port_id = port_id,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_ADD) ||
                    (access_cmd == ACCESS_CMD_DELETE), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s port to an IPL. The ipl is [%d], the port is [%lu]\n",
             ACCESS_COMMAND_STR(access_cmd),
             ipl_id,
             port_id);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_PORT_SET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Populates port_id with the port IDs that have been bound to the given IPL.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] ipl_id - IPL ID.
 * @param[out] port_id - Interface index of port. Pointer to an already allocated
 *                       memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ipl_port_get(const unsigned int ipl_id,
                      unsigned long *port_id)
{
    int err = 0;
    struct mlag_api_ipl_port_get_params cmd_body = {
        .ipl_id = ipl_id,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK(port_id != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "Get ipl port. Ipl is %d\n",
             ipl_id);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_PORT_GET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        sizeof(cmd_body));
    MLAG_BAIL_CHECK_NO_MSG(err);

    MLAG_LOG(MLAG_LOG_DEBUG, "Port id value retrieved is [%lu]\n",
             cmd_body.port_id);

    *port_id = cmd_body.port_id;

bail:
    return err;
}

/**
 * Sets/unsets the local and peer IP addresses for an IPL.
 * The IP is referring to the IPL interface VLAN.
 * The given IP addresses are ignored in the context of the DELETE command.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] ipl_id - IPL ID.
 * @param[in] vlan_id - The VLAN ID which the IPL will be a member.
 *                      Value ranges from 1 to 4095, inclusive.
 * @param[in] local_ipl_ip_address - The IPL IP address of the local machine to set.
 * @param[in] peer_ipl_ip_address - The IPL IP address of the peer machine to set.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 * @return -EAFNOSUPPORT- IPv6 currently not supported.
 */
int
mlag_api_ipl_ip_set(const enum access_cmd access_cmd,
                    const unsigned int ipl_id,
                    const unsigned short vlan_id,
                    const struct oes_ip_addr *local_ipl_ip_address,
                    const struct oes_ip_addr *peer_ipl_ip_address)
{
    int err = 0;
    struct mlag_api_ipl_ip_set_params cmd_body = {
        .access_cmd = access_cmd,
        .ipl_id = ipl_id,
        .vlan_id = vlan_id,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_ADD) ||
                    (access_cmd == ACCESS_CMD_DELETE), -EINVAL);
    MLAG_BAIL_CHECK((vlan_id >= MLAG_VLAN_ID_MIN) &&
                    (vlan_id <= MLAG_VLAN_ID_MAX), -EINVAL);
    if (access_cmd == ACCESS_CMD_ADD) {
        MLAG_BAIL_CHECK(local_ipl_ip_address != NULL, -EINVAL);
        MLAG_BAIL_CHECK(local_ipl_ip_address->version == OES_IPV4,
                        -EAFNOSUPPORT);
        MLAG_BAIL_CHECK(peer_ipl_ip_address != NULL, -EINVAL);
        MLAG_BAIL_CHECK(peer_ipl_ip_address->version == OES_IPV4,
                        -EAFNOSUPPORT);
        MLAG_BAIL_CHECK(((local_ipl_ip_address->addr.ipv4.s_addr == 0) ||
                         (local_ipl_ip_address->addr.ipv4.s_addr !=
                          peer_ipl_ip_address->addr.ipv4.s_addr)), -EINVAL);
        cmd_body.local_ipl_ip_address = *local_ipl_ip_address;
        cmd_body.peer_ipl_ip_address = *peer_ipl_ip_address;

        MLAG_LOG(MLAG_LOG_DEBUG,
                 "%s ipl IP addresses. Local IP version [%s], Local IP is [%s],"
                 "Peer Ip version [%s], Peer IP is [%s], ipl id is [%d]\n",
                 ACCESS_COMMAND_STR(
                     access_cmd),
                 (local_ipl_ip_address->version == OES_IPV6) ? "IPV6" : "IPV4",
                 inet_ntoa(local_ipl_ip_address->addr.ipv4),
                 (peer_ipl_ip_address->version == OES_IPV6) ? "IPV6" : "IPV4",
                 inet_ntoa(peer_ipl_ip_address->addr.ipv4),
                 ipl_id);
    }
    else {
        MLAG_LOG(MLAG_LOG_DEBUG,
                 "%s ipl IP addresses. Ipl id is [%d]\n",
                 ACCESS_COMMAND_STR(access_cmd),
                 ipl_id);
    }

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_IP_SET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Gets the IP of the local/peer and VLAN ID based of the given IPL ID.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] ipl_id - IPL ID.
 * @param[out] vlan_id - The VLAN ID which the IPL is a member.
 * @param[out] local_ipl_ip_address - The IPL IP address of the local machine.
 * @param[out] peer_ipl_ip_address - The IPL IP address of the peer machine.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_ipl_ip_get(const int ipl_id,
                    unsigned short *vlan_id,
                    struct oes_ip_addr *local_ipl_ip_address,
                    struct oes_ip_addr *peer_ipl_ip_address)
{
    int err = 0;
    struct mlag_api_ipl_ip_get_params cmd_body = {
        .ipl_id = ipl_id,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK(vlan_id != NULL, -EINVAL);
    MLAG_BAIL_CHECK(local_ipl_ip_address != NULL, -EINVAL);
    MLAG_BAIL_CHECK(peer_ipl_ip_address != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Get the IP of the local/peer and vlan id. Ipl is %d\n",
             ipl_id);

    memset(&cmd_body.vlan_id, 0, sizeof(unsigned short));
    memset(&cmd_body.local_ipl_ip_address, 0, sizeof(struct oes_ip_addr));
    memset(&cmd_body.peer_ipl_ip_address, 0, sizeof(struct oes_ip_addr));

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_IPL_IP_GET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        sizeof(cmd_body));
    MLAG_BAIL_CHECK_NO_MSG(err);

    *vlan_id = cmd_body.vlan_id;
    *local_ipl_ip_address = cmd_body.local_ipl_ip_address;
    *peer_ipl_ip_address = cmd_body.peer_ipl_ip_address;

bail:
    return err;
}

/**
 * Sets module log verbosity level.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] module - The module this log verbosity is going to change.
 * @param[in] verbosity - New log verbosity level.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Operation failure.
 */
int
mlag_api_log_verbosity_set(const enum mlag_log_module module,
                           const mlag_verbosity_t verbosity)
{
    int err = 0;
    struct mlag_api_log_verbosity_params cmd_body = {
        .module = module,
        .verbosity = verbosity,
    };

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Set module log verbosity level. Module is [%u], log level is [%u]\n",
             module,
             verbosity);

    switch (module) {
    case MLAG_LOG_MODULE_API:
        LOG_VAR_NAME(__MODULE__) = verbosity;
        break;
    case MLAG_LOG_ALL:
        LOG_VAR_NAME(__MODULE__) = verbosity;
        err = mlag_api_send_command_wrapper(
            MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_SET,
            (uint8_t*) &cmd_body,
            sizeof(cmd_body),
            NA);
        MLAG_BAIL_CHECK_NO_MSG(err);
        break;
    default:
        err = mlag_api_send_command_wrapper(
            MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_SET,
            (uint8_t*) &cmd_body,
            sizeof(cmd_body),
            NA);
        MLAG_BAIL_CHECK_NO_MSG(err);
    }

bail:
    return err;
}

/**
 * Gets module log verbosity level.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] module - The module this log verbosity is going to change.
 * @param[out] verbosity - The log verbosity level.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 */
int
mlag_api_log_verbosity_get(const enum mlag_log_module module,
                           mlag_verbosity_t *verbosity)
{
    int err = 0;
    struct mlag_api_log_verbosity_params cmd_body = {
        .module = module,
    };

    MLAG_BAIL_CHECK(verbosity != NULL, -EINVAL);
    MLAG_BAIL_CHECK(module != MLAG_LOG_ALL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Get module log verbosity level. Module is [%u]\n",
             module);

    if (module == MLAG_LOG_MODULE_API) {
        *verbosity = LOG_VAR_NAME(__MODULE__);
    }
    else {
        err = mlag_api_send_command_wrapper(
            MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_GET,
            (uint8_t*) &cmd_body,
            sizeof(cmd_body),
            sizeof(cmd_body));
        MLAG_BAIL_CHECK_NO_MSG(err);

        *verbosity = cmd_body.verbosity;
    }

bail:
    return err;
}

/**
 * Adds/deletes UC MAC and UC LAG MAC entries in the FDB. Currently it support only static
 * MAC insertion.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - Add/Delete.
 * @param[in] mac_entry - MAC entry. Pointer to an already allocated memory
 *                        structure.
 * @param[in] mac_cnt - Array size.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_uc_mac_addr_set(const enum access_cmd access_cmd,
                             struct fdb_uc_mac_addr_params *mac_entry,
                             unsigned short mac_cnt,
                             const long originator_cookie)
{
    int err = 0;
    struct mlag_api_fdb_uc_mac_addr_set_params cmd_body = {
        .access_cmd = access_cmd,
        .mac_cnt = mac_cnt,
        .originator_cookie = originator_cookie,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK(mac_entry != NULL, -EINVAL);
    MLAG_BAIL_CHECK(mac_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK((mac_entry->mac_addr_params.vid >= MLAG_VLAN_ID_MIN) &&
                    (mac_entry->mac_addr_params.vid <= MLAG_VLAN_ID_MAX),
                    -EINVAL);
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_ADD) ||
                    (access_cmd == ACCESS_CMD_DELETE), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "%s UC MAC and UC LAG MAC entries in the FDB\n",
             ACCESS_COMMAND_STR(access_cmd));

    cmd_body.mac_entry = *mac_entry;

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_SET,
        (uint8_t*) &cmd_body,
        sizeof(cmd_body),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Flush the entire FDB table.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] originator_cookie - Originator cookie passed as parameter to the notification
 *                                callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_uc_flush_set(const long originator_cookie)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "Flush the entire FDB table\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_SET,
                                        (uint8_t*)&originator_cookie,
                                        sizeof(originator_cookie),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Flushes all FDB table entries that have been learned on the given port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] port_id - Interface index of port.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_uc_flush_port_set(const unsigned long port_id,
                               const long originator_cookie)
{
    int err = 0;
    struct mlag_api_fdb_uc_flush_port_set_params cmd_body = {
        .port_id = port_id,
        .originator_cookie = originator_cookie,
    };

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Flush all FDB table entries that were learned on the given port. Port id is [%lu]\n",
             port_id);

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_PORT_SET,
        (uint8_t*) &cmd_body,
        sizeof(cmd_body),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Flushes the FDB table entries that have been learned on the given VLAN ID.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] vlan_id - VLAN ID. Value ranges from 1 to 4095, inclusive.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_uc_flush_vid_set(const unsigned short vlan_id,
                              const long originator_cookie)
{
    int err = 0;
    struct mlag_api_fdb_uc_flush_vid_set_params cmd_body = {
        .vlan_id = vlan_id,
        .originator_cookie = originator_cookie,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK((vlan_id >= MLAG_VLAN_ID_MIN) &&
                    (vlan_id <= MLAG_VLAN_ID_MAX), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Flush the FDB table entries that were learned on the given vlan id. Vlan id is [%hu]\n",
             vlan_id);

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_VID_SET,
        (uint8_t*) &cmd_body,
        sizeof(cmd_body),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Flushes all FDB table entries that have been learned on the given VLAN ID and port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] vlan_id - VLAN ID. Value ranges from 1 to 4095, inclusive.
 * @param[in] port_id - Interface index of port.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_uc_flush_port_vid_set(const unsigned short vlan_id,
                                   const unsigned long port_id,
                                   const long originator_cookie)
{
    int err = 0;
    struct mlag_api_fdb_uc_flush_port_vid_set_params cmd_body = {
        .vlan_id = vlan_id,
        .port_id = port_id,
        .originator_cookie = originator_cookie,
    };

    /* validate parameters */
    MLAG_BAIL_CHECK((vlan_id >= MLAG_VLAN_ID_MIN) &&
                    (vlan_id <= MLAG_VLAN_ID_MAX), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Flush all FDB table entries that were learned on the given vlan id and port. Vlan id is [%hu], port id [%lu]\n",
             vlan_id,
             port_id);


    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_PORT_VID_SET,
        (uint8_t*) &cmd_body,
        sizeof(cmd_body),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Gets MAC entries from the SW FDB table, which is an exact copy of HW DB on any
 * device.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] access_cmd - Get/Get next/Get first.
 * @param[in] key_filter - Filter types used on the mac_list -
 *                         vid / mac / logical port / entry cookie.
 * @param[in,out] mac_list - Mac record array. Pointer to an already allocated memory
 *                        structure.
 * @param[in,out] data_cnt - Number of macs to retrieve. If fewer macs have been
 *                           successfully retrieved, then data_cnt will contain the
 *                           number of successfully retrieved macs.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_uc_mac_addr_get(const enum access_cmd access_cmd,
                             const struct fdb_uc_key_filter *key_filter,
                             struct fdb_uc_mac_addr_params *mac_list,
                             unsigned short *data_cnt)
{
    int err = 0;
    struct mlag_api_fdb_uc_mac_addr_get_params *cmd_body = NULL;
    unsigned int size = 0;

    /* validate parameters */
    MLAG_BAIL_CHECK(mac_list != NULL, -EINVAL);
    MLAG_BAIL_CHECK(key_filter != NULL, -EINVAL);
    MLAG_BAIL_CHECK(data_cnt != NULL, -EINVAL);
    MLAG_BAIL_CHECK(*data_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_GET) ||
                    (access_cmd == ACCESS_CMD_GET_FIRST) ||
                    (access_cmd == ACCESS_CMD_GET_NEXT), -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s MAC entries from the SW FDB table, which is exact copy of HW DB on any device\n",
             ACCESS_COMMAND_STR(access_cmd));

    size = sizeof(struct mlag_api_fdb_uc_mac_addr_get_params) +
           *data_cnt * sizeof(struct fdb_uc_mac_addr_params);
    cmd_body = (struct mlag_api_fdb_uc_mac_addr_get_params *)cl_malloc(size);
    if (cmd_body == NULL) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Failed to allocate cmd_body memory\n");
        goto bail;
    }

    cmd_body->access_cmd = access_cmd;
    cmd_body->key_filter = *key_filter;
    cmd_body->data_cnt = *data_cnt;
    memcpy(cmd_body->mac_list, mac_list, *data_cnt *
           sizeof(struct fdb_uc_mac_addr_params));

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_GET,
        (uint8_t*)cmd_body,
        size,
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

    memcpy(mac_list, cmd_body->mac_list, cmd_body->data_cnt *
           sizeof(struct fdb_uc_mac_addr_params));
    *data_cnt = cmd_body->data_cnt;

bail:
    if (cmd_body) {
        cl_free(cmd_body);
    }
    return err;
}

/**
 * Initializes the control learning library.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] br_id - Bridge ID associated with the control learning.
 * @param[in] logging_cb - Optional log messages callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_init(const int br_id, const ctrl_learn_log_cb logging_cb)
{
    int err = 0;
    struct mlag_api_fdb_init_params cmd_body = {
        .br_id = br_id,
        .logging_cb = logging_cb,
    };

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Initialize the control learning library. Bridge id is [%d]\n",
             br_id);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_FDB_INIT,
                                        (uint8_t*) &cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * De-initializes the control learning library.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_deinit(void)
{
    int err = 0;
    unsigned int rpc_unused_param;

    MLAG_LOG(MLAG_LOG_DEBUG, "De-Initialize the control learning library\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_FDB_DEINIT,
                                        (uint8_t*) &rpc_unused_param,
                                        sizeof(rpc_unused_param),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Starts the control learning library and listens to FDB events.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_start(void)
{
    int err = 0;
    unsigned int rpc_unused_param;

    MLAG_LOG(MLAG_LOG_DEBUG, "Start the control learning library\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_FDB_START,
                                        (uint8_t*) &rpc_unused_param,
                                        sizeof(rpc_unused_param),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Stops control learning functionality.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_stop(void)
{
    int err = 0;
    unsigned int rpc_unused_param;

    MLAG_LOG(MLAG_LOG_DEBUG, "Stop the control learning library\n");

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_FDB_STOP,
                                        (uint8_t*) &rpc_unused_param,
                                        sizeof(rpc_unused_param),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Generates MLAG system dump.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] file_path - The file path where the system information will be written.
 *			  Pointer to an already allocated memory.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_dump(const char *dump_file_path)
{
    int err = 0;

    /* validate parameter */
    MLAG_BAIL_CHECK(dump_file_path != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Generate mlag system dump. The dump file path is %s\n",
             dump_file_path);

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_DUMP,
                                        (uint8_t*)dump_file_path,
                                        (strlen(dump_file_path) + 1),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Sets router MAC.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] router_macs_list - Router MACs list to set. Pointer to an already
 *                               allocated memory structure.
 * @param[in] router_macs_list_cnt - List size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_router_mac_set(const enum access_cmd access_cmd,
                        const struct router_mac *router_macs_list,
                        const unsigned int router_macs_list_cnt)
{
    int err = 0;
    struct mlag_api_router_mac_set_params *cmd_body = NULL;
    unsigned int size = 0;

    /* validate parameters */
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_ADD) ||
                    (access_cmd == ACCESS_CMD_DELETE), -EINVAL);
    MLAG_BAIL_CHECK(router_macs_list_cnt > 0, -EINVAL);
    MLAG_BAIL_CHECK(router_macs_list != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "%s router mac. First router mac is [%llu], the vlan id is [%hu]\n",
             ACCESS_COMMAND_STR(access_cmd),
             router_macs_list[0].mac,
             router_macs_list[0].vlan_id);

    size = sizeof(struct mlag_api_router_mac_set_params) +
           router_macs_list_cnt * sizeof(struct router_mac);

    cmd_body = (struct mlag_api_router_mac_set_params *)cl_malloc(size);
    if (cmd_body == NULL) {
        MLAG_LOG(MLAG_LOG_ERROR,
                 "Failed to allocate cmd_body memory\n");
        goto bail;
    }

    cmd_body->access_cmd = access_cmd;
    cmd_body->router_macs_list_cnt = router_macs_list_cnt;
    memcpy(cmd_body->router_macs_list, router_macs_list, router_macs_list_cnt *
           sizeof(struct router_mac));

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_ROUTER_MAC_SET,
                                        (uint8_t*)cmd_body,
                                        size,
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    if (cmd_body) {
        cl_free(cmd_body);
    }

    return err;
}


/**
 * Sets MLAG port mode. Port mode can be either static or dynamic LAG.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 * @param[in] port_mode - MLAG port mode of operation.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_port_mode_set(const unsigned long port_id,
                       const enum mlag_port_mode port_mode)
{
    int err = 0;
    struct mlag_api_port_mode_params cmd_body;

    /* validate parameters */
    MLAG_BAIL_CHECK(port_mode < MLAG_PORT_MODE_LAST, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "port ID [%lu] set to mode [%u]\n", port_id,
             port_mode);

    cmd_body.port_id = port_id;
    cmd_body.port_mode = port_mode;

    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_PORT_MODE_SET,
                                        (uint8_t*)&cmd_body,
                                        sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Returns the current MLAG port mode.
 *
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 * @param[out] port_mode - MLAG port mode of operation.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_port_mode_get(const unsigned long port_id,
                       enum mlag_port_mode *port_mode)
{
    int err = 0;
    struct mlag_api_port_mode_params cmd_body;
    /* validate parameters */
    MLAG_BAIL_CHECK(port_mode != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "get port mode port ID [%lu]\n", port_id);

    cmd_body.port_id = port_id;
    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_PORT_MODE_GET,
                                        (uint8_t*) &cmd_body, sizeof(cmd_body),
                                        sizeof(cmd_body));
    MLAG_BAIL_CHECK_NO_MSG(err);

    *port_mode = cmd_body.port_mode;

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
 * releasing currently used key and migrating to the given partner. Force
 * is not yet supported, and should be set to 0.
 *
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] request_id - index given by caller that will appear in reply
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 * @param[in] partner_sys_id - partner ID (taken from LACP PDU)
 * @param[in] partner_key - partner operational Key (taken from LACP PDU)
 * @param[in] force - force partner to be selected (will release partner in use)
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_lacp_selection_request(const enum access_cmd access_cmd,
                                const unsigned int request_id,
                                const unsigned long port_id,
                                const unsigned long long partner_sys_id,
                                const unsigned int partner_key,
                                const unsigned char force)
{
    int err = 0;
    struct mlag_api_lacp_selection_request_params cmd_body;
    /* validate parameters */
    MLAG_BAIL_CHECK((access_cmd == ACCESS_CMD_ADD) ||
                    (access_cmd == ACCESS_CMD_DELETE), -EINVAL);

    MLAG_BAIL_CHECK(force == 0, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG,
             "lacp selection request cmd [%s] req_id [%u] port [%lu] sys_id [%llu] key [%u] force [%u]\n",
             ACCESS_COMMAND_STR(access_cmd), request_id, port_id,
             partner_sys_id, partner_key, force);

    cmd_body.access_cmd = access_cmd;
    cmd_body.request_id = request_id;
    cmd_body.port_id = port_id;
    cmd_body.partner_sys_id = partner_sys_id;
    cmd_body.partner_key = partner_key;
    cmd_body.force = force;

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_LACP_SELECT_REQUEST,
        (uint8_t*)&cmd_body,
        sizeof(cmd_body),
        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Sets the local system ID for LACP PDUs. This API should be called before
 * starting mlag protocol when LACP is enabled.
 * This function works asynchronously.
 *
 * @param[in] local_sys_id - local sys ID (for LACP PDU)
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_lacp_local_sys_id_set(const unsigned long long local_sys_id)
{
    int err = 0;
    struct mlag_api_lacp_actor_params cmd_body;

    MLAG_LOG(MLAG_LOG_DEBUG,
             "local system ID set to [%llu] \n", local_sys_id);

    cmd_body.system_id = local_sys_id;
    err = mlag_api_send_command_wrapper(MLAG_INTERNAL_API_CMD_LACP_SYS_ID_SET,
                                        (uint8_t*) &cmd_body, sizeof(cmd_body),
                                        NA);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

/**
 * Gets actor attributes. The actor attributes are
 * system ID to be used in the LACP PDU and a chassis ID
 * which is an index of this node within the MLAG
 * cluster, with a value in the range of 0..15
 *
 * @param[out] actor_sys_id - actor sys ID (for LACP PDU)
 * @param[out] chassis_id - MLAG cluster chassis ID, range 0..15
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_lacp_actor_parameters_get(unsigned long long *actor_sys_id,
                                   unsigned int *chassis_id)
{
    int err = 0;
    struct mlag_api_lacp_actor_params cmd_body;
    /* validate parameters */
    MLAG_BAIL_CHECK(actor_sys_id != NULL, -EINVAL);
    MLAG_BAIL_CHECK(chassis_id != NULL, -EINVAL);

    MLAG_LOG(MLAG_LOG_DEBUG, "lacp actor parameters get\n");

    err = mlag_api_send_command_wrapper(
        MLAG_INTERNAL_API_CMD_LACP_ACTOR_PARAMS_GET,
        (uint8_t*) &cmd_body, sizeof(cmd_body),
        sizeof(cmd_body));
    MLAG_BAIL_CHECK_NO_MSG(err);

    *actor_sys_id = cmd_body.system_id;
    *chassis_id = cmd_body.chassis_id;

bail:
    return err;
}


