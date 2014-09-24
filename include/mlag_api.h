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

#ifndef MLAG_API_H_
#define MLAG_API_H_

#include "mlag_log.h"
#include "mlag_api_defs.h"
#include <errno.h>

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
#ifdef MLAG_API_C_
#define EXTERN_API extern
#else
#define EXTERN_API
#endif
/************************************************
 *  Global variables
 ***********************************************/
/************************************************
 *  Function declarations
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
mlag_api_deinit(enum mlag_connector_type connector_type);

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
               const struct mlag_start_params *params);

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
mlag_api_stop(void);

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
                                   const unsigned int ports_arr_cnt);

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
                                   const unsigned int vlans_arr_cnt);

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
mlag_api_vlans_state_change_sync_finish_notify(void);

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
                                const enum mgmt_oper_state mgmt_state);

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
                         unsigned int *mlag_ports_cnt);

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
                        struct mlag_port_info *mlag_port_information);

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
mlag_api_protocol_oper_state_get(enum protocol_oper_state *protocol_oper_state);

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
mlag_api_system_role_state_get(enum system_role_state *system_role_state);

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
                              unsigned int *peers_list_cnt);

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
                      struct mlag_counters *counters);

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
mlag_api_counters_clear(void);

/**
 * Specifies the maximum period of time that MLAG ports are disabled after restarting
 * the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the MLAG protocol information before opening the MLAG ports.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] sec - The period that MLAG ports are disabled after restarting the feature.
 *          Value ranges from 0 to 300 (5 min), inclusive.
 *                  Default interval is 30 sec.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_reload_delay_set(const unsigned int sec);

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
mlag_api_reload_delay_get(unsigned int *sec);

/**
 * Specifies the interval during which keep-alive messages are issued.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.

 * @param[in] sec - Interval duration.
 *          Value ranges from 1 to 30, inclusive.
 *                  Default interval is 1 sec.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_keepalive_interval_set(const unsigned int sec);

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
mlag_api_keepalive_interval_get(unsigned int *sec);

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
                  const unsigned long port_id);

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
mlag_api_ipl_set(const enum access_cmd access_cmd, unsigned int *ipl_id);

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
mlag_api_ipl_ids_get(unsigned int *ipl_ids, unsigned int *ipl_ids_cnt);

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
mlag_api_ipl_port_set(const enum access_cmd access_cmd, const int ipl_id,
                      const unsigned long port_id);

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
mlag_api_ipl_port_get(const unsigned int ipl_id, unsigned long *port_id);

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
                    const unsigned int ipl_id, const unsigned short vlan_id,
                    const struct oes_ip_addr *local_ipl_ip_address,
                    const struct oes_ip_addr *peer_ipl_ip_address);

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
mlag_api_ipl_ip_get(const int ipl_id, unsigned short *vlan_id,
                    struct oes_ip_addr *local_ipl_ip_address,
                    struct oes_ip_addr *peer_ipl_ip_address);

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
                           const mlag_verbosity_t verbosity);

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
                           mlag_verbosity_t *verbosity);

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
                             const long originator_cookie);

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
mlag_api_fdb_uc_flush_set(const long originator_cookie);

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
                               const long originator_cookie);

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
                              const long originator_cookie);

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
                                   const long originator_cookie);

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
                             unsigned short *data_cnt);

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
mlag_api_fdb_init(const int br_id, const ctrl_learn_log_cb logging_cb);

/**
 * De-initializes the control learning library.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_deinit(void);

/**
 * Starts the control learning library and listens to FDB events.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_start(void);

/**
 * Stops control learning functionality.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_fdb_stop(void);

/**
 * Generates MLAG system dump.
 * This function works synchronously and blocks until the operation is completed.
 *
 * @param[in] file_path - The file path where the system information will be written.
 *            Pointer to an already allocated memory.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_dump(const char *dump_file_path);

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
                        const unsigned int router_macs_list_cnt);

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
                       const enum mlag_port_mode port_mode);

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
                       enum mlag_port_mode *port_mode);

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
                                const unsigned char force);

/**
 * Sets the local system ID for LACP PDUs. This API should be called before
 * starting mlag protocol when LACP is enabled.
 * This function works asynchronously.
 *
 * @param[in] local_sys_id - local sys ID (for LACP PDU)
 * @param[in] local_sys_prio - local sys priority (for LACP PDU)
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_api_lacp_local_sys_id_set(const unsigned long long local_sys_id);

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
                                   unsigned int *chassis_id);

#endif
