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

#ifndef MLAG_INTERNAL_API_H_
#define MLAG_INTERNAL_API_H_

#include "mlag_log.h"
#include "mlag_api_rpc.h"
#include "mlag_api_defs.h"
#include "mlag_init.h"
#include "mlag_conf.h"

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

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
                                            uint32_t *snd_len);

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
                                            uint32_t *snd_len);

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
                                                        uint32_t *snd_len);

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
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

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
mlag_internal_api_ports_state_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                  uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_port_state_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                 uint8_t **snd_body, uint32_t *snd_len);

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
                                          uint32_t rcv_len, uint8_t **snd_body,
                                          uint32_t *snd_len);

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
                                        uint32_t rcv_len, uint8_t **snd_body,
                                        uint32_t *snd_len);

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
mlag_internal_api_peers_state_list_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                       uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_counters_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                               uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_counters_clear(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                 uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_reload_delay_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                   uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_reload_delay_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                   uint8_t **snd_body, uint32_t *snd_len);

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
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

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
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

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
mlag_internal_api_port_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                           uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_ipl_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                          uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_ipl_ids_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                              uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_ipl_port_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                               uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_ipl_port_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                               uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_ipl_ip_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                             uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_ipl_ip_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                             uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_start(uint8_t *rcv_msg_body, uint32_t rcv_len,
                        uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_stop(uint8_t *rcv_msg_body, uint32_t rcv_len,
                       uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_log_verbosity_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                    uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_log_verbosity_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                    uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_fdb_uc_mac_addr_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                      uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_fdb_uc_flush_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                   uint8_t **snd_body, uint32_t *snd_len);

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
                                        uint32_t rcv_len, uint8_t **snd_body,
                                        uint32_t *snd_len);

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
mlag_internal_api_fdb_uc_flush_vid_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                       uint8_t **snd_body, uint32_t *snd_len);

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
                                            uint32_t *snd_len);

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
mlag_internal_api_fdb_uc_mac_addr_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                      uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_fdb_init(uint8_t *rcv_msg_body, uint32_t rcv_len,
                           uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_fdb_deinit(uint8_t *rcv_msg_body, uint32_t rcv_len,
                             uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_fdb_start(uint8_t *rcv_msg_body, uint32_t rcv_len,
                            uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_fdb_stop(uint8_t *rcv_msg_body, uint32_t rcv_len,
                           uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_dump(uint8_t *rcv_msg_body, uint32_t rcv_len,
                       uint8_t **snd_body, uint32_t *snd_len);

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
mlag_internal_api_router_mac_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                 uint8_t **snd_body, uint32_t *snd_len);

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
                                uint8_t **snd_body, uint32_t *snd_len);

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
                                uint8_t **snd_body, uint32_t *snd_len);

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
                                        uint32_t rcv_len, uint8_t **snd_body,
                                        uint32_t *snd_len);

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
                                            uint32_t *snd_len);

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
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

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
mlag_rpc_init_procedure(sx_log_cb_t log_cb);

/**
 * De-initializes the RPC layer.
 * Deinit is called when MLAG process exits.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation failure.
 */
int
mlag_rpc_deinit();

/**
 * Infinate loop. Wait on rpc_select_event.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_rpc_start_loop(void);

#endif
