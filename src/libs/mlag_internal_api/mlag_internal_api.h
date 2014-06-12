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
 * Notify on ports operational state changes.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ports_state_change_notify(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len);

/**
 * Notify on vlans state changes.
 * Invoke on all vlans.
 * Vlan is considered as up if one member port is up, excluding IPL.
 * Vlan considered as down if all the member ports of the vlan down, excluding IPL.
 * This function is temporary and on next release it will be considered as obsolete.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_vlans_state_change_notify(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len);

/**
 * Notify on vlans state change sync done.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_vlans_state_change_sync_finish_notify(uint8_t *rcv_msg_body,
                                                        uint32_t rcv_len,
                                                        uint8_t **snd_body,
                                                        uint32_t *snd_len);

/**
 * Notify on peer management connectivity state changes.
 * The system identification is referring to the peer.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_mgmt_peer_state_notify(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

/**
 * Get all the current mlag ports state information.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ports_state_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                  uint8_t **snd_body, uint32_t *snd_len);

/**
 * Populates mlag_port_information with the port information of the given port id.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_port_state_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                 uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get the current protocol operational state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_protocol_oper_state_get(uint8_t *rcv_msg_body,
                                          uint32_t rcv_len, uint8_t **snd_body,
                                          uint32_t *snd_len);

/**
 * Get the current system role state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_system_role_state_get(uint8_t *rcv_msg_body,
                                        uint32_t rcv_len, uint8_t **snd_body,
                                        uint32_t *snd_len);

/**
 * Get the current peers state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_peers_state_list_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                       uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get the current values of the corresponding mlag protocol counters.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_counters_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                               uint8_t **snd_body, uint32_t *snd_len);

/**
 * Clear the mlag protocol counters.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_counters_clear(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                 uint8_t **snd_body, uint32_t *snd_len);

/**
 * Specifies the maximum period of time that mlag ports are disabled after restarting
 * the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the mlag protocol information before opening the mlag ports.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_reload_delay_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                   uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get the current value of the maximum period of time
 * that mlag ports are disabled after restarting the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the mlag protocol information before opening the mlag ports.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_reload_delay_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                   uint8_t **snd_body, uint32_t *snd_len);

/**
 * Specifies the interval at which keep-alive messages are issued.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_keepalive_interval_set(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

/**
 * Get the current value of the interval at which keep-alive messages are issued.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_keepalive_interval_get(uint8_t *rcv_msg_body,
                                         uint32_t rcv_len, uint8_t **snd_body,
                                         uint32_t *snd_len);

/**
 * Add/Delete mlag port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_port_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                           uint8_t **snd_body, uint32_t *snd_len);

/**
 * Create/Delete an IPL.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ipl_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                          uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get all the ipl ids that were created.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ipl_ids_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                              uint8_t **snd_body, uint32_t *snd_len);

/**
 * Bind/Unbind port to an IPL.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ipl_port_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                               uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get the port id that were binded to the given ipl.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ipl_port_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                               uint8_t **snd_body, uint32_t *snd_len);

/**
 * Set/Unset the local and peer IP addresses for an IPL.
 * The IP is referring to the IPL interface vlan.
 * The given IP addresses are ignored in the context of DELETE command.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 * @return -EAFNOSUPPORT- IPv6 currently not supported.
 */
int
mlag_internal_api_ipl_ip_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                             uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get the IP of the local/peer and vlan id based of the given ipl id.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_ipl_ip_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                             uint8_t **snd_body, uint32_t *snd_len);

/**
 * Enable the mlag protocol.
 * Start is called in order to enable the protocol. From this stage on, the protocol
 * is running and thus exchanges messages with peers, triggers configuration toward
 * HAL (Hardware Abstraction Layer) and events toward the system.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_start(uint8_t *rcv_msg_body, uint32_t rcv_len,
                        uint8_t **snd_body, uint32_t *snd_len);

/**
 * Disable the mlag protocol.
 * Stop means stopping the mlag capability of the switch. No messages or
 * configurations should be sent from any mlag module after handling stop request.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_stop(uint8_t *rcv_msg_body, uint32_t rcv_len,
                       uint8_t **snd_body, uint32_t *snd_len);

/**
 * Set module log verbosity level.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 */
int
mlag_internal_api_log_verbosity_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                    uint8_t **snd_body, uint32_t *snd_len);

/**
 * Get module log verbosity level.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 */
int
mlag_internal_api_log_verbosity_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                    uint8_t **snd_body, uint32_t *snd_len);

/**
 * Add/Delete UC MAC and UC LAG MAC entries in the FDB. Currently it support only static
 * mac insertion.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - Add/Delete.
 * @param[in] mac_entry - Mac entry. Pointer to an already allocated memory
 *                        structure.
 * @param[in] mac_cnt - Array size.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_uc_mac_addr_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                      uint8_t **snd_body, uint32_t *snd_len);

/**
 * Flush the entire FDB table.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_uc_flush_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                   uint8_t **snd_body, uint32_t *snd_len);

/**
 * Flush all FDB table entries that were learned on the given port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] port_id - Interface index of port.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_uc_flush_port_set(uint8_t *rcv_msg_body,
                                        uint32_t rcv_len, uint8_t **snd_body,
                                        uint32_t *snd_len);

/**
 * Flush the FDB table entries that were learned on the given vlan id.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] vlan_id - Vlan id. Value ranges from 1 to 4095, inclusive.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_uc_flush_vid_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                       uint8_t **snd_body, uint32_t *snd_len);

/**
 * Flush all FDB table entries that were learned on the given vlan id and port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] vlan_id - Vlan id. Value ranges from 1 to 4095, inclusive.
 * @param[in] port_id - Interface index of port.
 * @param[in] originator_cookie - Originator cookie passed as parameter to the
 *                                notification callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_uc_flush_port_vid_set(uint8_t *rcv_msg_body,
                                            uint32_t rcv_len,
                                            uint8_t **snd_body,
                                            uint32_t *snd_len);

/**
 * Get MAC entries from the SW FDB table, which is exact copy of HW DB on any
 * device.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] access_cmd - Get/Get next/Get first.
 * @param[in] key_filter - Filter types used on the mac_list -
 *                         vid / mac / logical port / entry cookie.
 * @param[out] mac_list - Mac record array. Pointer to an already allocated memory
 *                        structure.
 * @param[in,out] data_cnt - Number of macs to retrieve. If fewer macs have been
 *                           successfully retrieved, then data_cnt will contain the
 *                           number of successfully retrieved macs.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_uc_mac_addr_get(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                      uint8_t **snd_body, uint32_t *snd_len);

/**
 * Initialize the control learning library.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] br_id - Bridge id associated with the control learning.
 * @param[in] logging_cb - Optional log messages callback.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_init(uint8_t *rcv_msg_body, uint32_t rcv_len,
                           uint8_t **snd_body, uint32_t *snd_len);

/**
 * De-Initialize the control learning library.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_deinit(uint8_t *rcv_msg_body, uint32_t rcv_len,
                             uint8_t **snd_body, uint32_t *snd_len);

/**
 * Start the control learning library. It start listening to FDB events.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_start(uint8_t *rcv_msg_body, uint32_t rcv_len,
                            uint8_t **snd_body, uint32_t *snd_len);

/**
 * Stop control learning functionality.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_fdb_stop(uint8_t *rcv_msg_body, uint32_t rcv_len,
                           uint8_t **snd_body, uint32_t *snd_len);

/**
 * Generate mlag system dump.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_dump(uint8_t *rcv_msg_body, uint32_t rcv_len,
                       uint8_t **snd_body, uint32_t *snd_len);

/**
 * Sets router mac.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] rcv_msg_body - Contain the necessary parameters.
 *                           Pointer to an already allocated memory structure.
 * @param[in] rcv_len - Receive bytes.
 * @param[out] snd_body - Response content.
 * @param[out] snd_len - Response size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_internal_api_router_mac_set(uint8_t *rcv_msg_body, uint32_t rcv_len,
                                 uint8_t **snd_body, uint32_t *snd_len);

/**
 * Initialize the RPC layer.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] log_cb - Logging callback.
 *
 * @return 0 - Operation completed successfully.
 * @return SX_RPC_STATUS_ERROR - Operation failure.
 */
int
mlag_rpc_init_procedure(sx_log_cb_t log_cb);

/**
 * De-Initialize the RPC layer.
 * Deinit is called when mlag process exits.
 * This function works synchronously. It get blocking until the operation finish.
 *
 * @return 0 - Operation completed successfully.
 * @return SX_RPC_STATUS_ERROR - Operation failure.
 */
int
mlag_rpc_deinit();

/**
 * Infinate loop. Wait on rpc_select_event.
 * This function works synchronously. It get blocking until the operation finish.
 *
 * @return 0 - Operation completed successfully.
 */
int
mlag_rpc_start_loop(void);

#endif
