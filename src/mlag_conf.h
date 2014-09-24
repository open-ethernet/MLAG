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


#ifndef MLAG_CONF_H_
#define MLAG_CONF_H_

#include "mlag_api_defs.h"
#ifdef MLAG_CONF_C_
/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG
/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/


#endif
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
 * @param[in] ports_arr - Port state information record array. Pointer to an already
 *                        allocated memory structure.
 * @param[in] ports_arr_cnt - Array size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_ports_state_change_notify(const struct port_state_info *ports_arr,
                                   const unsigned int ports_arr_cnt);

/**
 * Notify on vlans state changes.
 * Invoke on all vlans.
 * Vlan is considered as up if one member port is up, excluding IPL.
 * Vlan considered as down if all the member ports of the vlan down, excluding IPL.
 * This function is temporary and on next release it will be considered as obsolete.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] vlans_arr - Vlan state information record array. Pointer to an already
 *                        allocated memory structure.
 * @param[in] vlans_arr_cnt - Array size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_vlans_state_change_notify(const struct vlan_state_info *vlans_arr,
                                   const unsigned int vlans_arr_cnt);

/**
 * Notify on vlans state change sync done.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_vlans_state_change_sync_finish_notify(void);

/**
 * Notify on peer management connectivity state changes.
 * The system identification is referring to the peer.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] peer_system_id - System identification.
 * @param[in] mgmt_state - Up/Down.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_mgmt_peer_state_notify(const unsigned long long peer_system_id,
                                const enum mgmt_oper_state mgmt_state);

/**
 * Populates mlag_counters with the current values of the corresponding mlag protocol
 * counters.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] ipl_ids - Ipl ids array. Pointer to an already allocated memory
 *                      structure.
 * @param[in] ipl_ids_cnt - Number of ipl mlag counters structure to retrieve.
 * @param[out] counters - Mlag protocol counters. Pointer to an already allocated
 *                        memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_counters_get(const unsigned int *ipl_ids, const unsigned int ipl_ids_cnt,
                  struct mlag_counters *counters);

/**
 * Clear the mlag protocol counters.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_counters_clear(void);

/**
 * Populates mlag_ports_information with all the current mlag ports.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] mlag_ports_information - Mlag ports. Pointer to an already allocated
 *                                      memory structure.
 * @param[in,out] mlag_ports_cnt - Number of ports to retrieve. If fewer ports have
 *                                 been successfully retrieved, then mlag_ports_cnt
 *                                 will contain the number of successfully retrieved
 *                                 ports.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_ports_state_get(struct mlag_port_info *mlag_ports_information,
                         unsigned int *mlag_ports_cnt);

/**
 * Populates mlag_port_information with the port information of the given port id.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] port_id - Interface index of port.
 * @param[out] mlag_port_information - Mlag port. Pointer to an already allocated memory
 *                                     structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -ENOENT - Entry not found.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_port_state_get(const unsigned long port_id,
                    struct mlag_port_info *mlag_port_information);

/**
 * Populates protocol_oper_state with the current protocol operational state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] protocol_oper_state - Protocol operational state. Pointer to an already allocated memory
 *                                   structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_protocol_oper_state_get(enum protocol_oper_state *protocol_oper_state);

/**
 * Populates system_role with the current system role state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] system_role_state - System role state. Pointer to an already allocated memory
 *                                 structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_system_role_state_get(enum system_role_state *system_role_state);

/**
 * Populates system_role with the current system role state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] peers_list - Peers list state. Pointer to an already allocated memory
 *                          structure.
 * @param[in,out] peers_list_cnt - Number of peers to retrieve. If fewer peers have been
 *                                 successfully retrieved, then peers_list_cnt will contain the
 *                                 number of successfully retrieved peers.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_peers_state_list_get(struct peer_state *peers_list,
                          unsigned int *peers_list_cnt);

/**
 * Add/Delete mlag port.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - Add/Delete.
 * @param[in] port_id - Interface index of port. Must represent mlag port.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_port_set(const enum access_cmd access_cmd,
                  const unsigned long port_id);

/**
 * Create/Delete an IPL.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] access_cmd - Create/Delete.
 * @param[int,out] ipl_id - Ipl id.
 *                          In: already created IPL id, associated with delete
 *                              command.
 *                          Out: newly created IPL id, associated with create
 *                               command.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_ipl_set(const enum access_cmd access_cmd, unsigned int *ipl_id);

/**
 * Populates ipl_ids with all the ipl ids that was created.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] ipl_ids - Ipl ids. Pointer to an already allocated memory
 *                       structure.
 * @param[in,out] ipl_ids_cnt - Number of ipl to retrieve. If fewer ipl have been
 *                               successfully retrieved, then ipl_ids_cnt will contain the
 *                               number of successfully retrieved ipl.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_ipl_ids_get(unsigned int *ipl_ids, unsigned int *ipl_ids_cnt);

/**
 * Bind/Unbind port to an IPL.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] ipl_id - Ipl id.
 * @param[in] port_id - Interface index of port. Must represent a valid port.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_ipl_port_set(const enum access_cmd access_cmd, const int ipl_id,
                      const unsigned long port_id);

/**
 * Populates port_id with the port id that were binded to the given ipl.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] ipl_id - Ipl id.
 * @param[out] port_id - Interface index of port. Pointer to an already allocated
 *                       memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_ipl_port_get(const int ipl_id, unsigned long *port_id);

/**
 * Set/Unset the local and peer IP addresses for an IPL.
 * The IP is referring to the IPL interface vlan.
 * The given IP addresses are ignored in the context of DELETE command.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] ipl_id - IPL id.
 * @param[in] vlan_id - The vlan id which the IPL will be a member.
 *                      Value ranges from 1 to 4095, inclusive.
 * @param[in] local_ipl_ip_address - The IPL IP address of the local machine to set.
 * @param[in] peer_ipl_ip_address - The IPL IP address of the peer machine to set.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 * @return -EAFNOSUPPORT- IPv6 currently not supported.
 */
int
mlag_ipl_ip_set(const enum access_cmd access_cmd, const int ipl_id,
                const unsigned short vlan_id,
                const struct oes_ip_addr *local_ipl_ip_address,
                const struct oes_ip_addr *peer_ipl_ip_address);

/**
 * Get the IP of the local/peer and vlan id based of the given ipl id.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] ipl_id - IPL id.
 * @param[out] vlan_id - The vlan id which the IPL is a member.
 * @param[out] local_ipl_ip_address - The IPL IP address of the local machine.
 * @param[out] peer_ipl_ip_address - The IPL IP address of the peer machine.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_ipl_ip_get(const int ipl_id, unsigned short *vlan_id,
                struct oes_ip_addr *local_ipl_ip_address,
                struct oes_ip_addr *peer_ipl_ip_address);

/**
 * Specifies the maximum period of time that mlag ports are disabled after restarting
 * the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the mlag protocol information before opening the mlag ports.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] sec - The period that mlag ports are disabled after restarting the
 *                  feature. Value ranges from 0 to 300 (5 min), inclusive.
 *                  Default interval is 30 sec.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_reload_delay_set(const unsigned int sec);

/**
 * Populates sec with the current value of the maximum period of time
 * that mlag ports are disabled after restarting the feature.
 * This interval allows the switch to learn the IPL topology to identify the
 * master and sync the mlag protocol information before opening the mlag ports.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] sec - The period that mlag ports are disabled after restarting the
 *                   feature. Pointer to an already allocated memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_reload_delay_get(unsigned int *sec);

/**
 * Specifies the interval at which keep-alive messages are issued.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.

 * @param[in] sec - The period interval duration.
 *                   Value ranges from 1 to 30, inclusive.
 *                  Default interval is 1 sec.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int mlag_keepalive_interval_set(const unsigned int sec);

/**
 * Populates sec with the current value of the interval at which keep-alive messages are issued.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] sec - The period interval duration.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_keepalive_interval_get(unsigned int *sec);

/**
 * Generate mlag system dump.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] file_path - The file path were the system information will be written.
 *			  Pointer to an already allocated memory.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_dump(char *dump_file_path);

/**
 * Sets router mac.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] router_macs_list - Router macs list to set. Pointer to an already
 *                               allocated memory structure.
 * @param[in] router_macs_list_cnt - List size.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_router_mac_set(const enum access_cmd access_cmd,
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
mlag_port_mode_set(const unsigned long port_id,
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
mlag_port_mode_get(const unsigned long port_id,
                   enum mlag_port_mode *port_mode);

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
mlag_lacp_local_sys_id_set(const unsigned long long local_sys_id);

/**
 * Gets actor attributes. The actor attributes are
 * system ID, system priority to be used in the LACP PDU
 * and a chassis ID which is an index of this node within the MLAG
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
mlag_lacp_actor_parameters_get(unsigned long long *actor_sys_id,
                               unsigned int *chassis_id);

/**
 * Triggers a selection query to the MLAG module.
 * Since MLAG is distributed, this request may involve a remote peer, so
 * this function works asynchronously. The response for this request is
 * guaranteed and it is issued as a notification when the response is available.
 * When a delete command is used, only port_id parameter is relevant.
 * Force option is relevant for ADD command and is given in order to allow
 * releasing currently used key and migrating to the given partner.
 *
 * @param[in] access_cmd - ADD/DELETE.
 * @param[in] request_id - index given by caller that will appear in reply
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 * @param[in] partner_sys_id - partner ID (taken from LACP PDU)
 * @param[in] partner_key - partner operational Key (taken from LACP PDU)
 * @param[in] force - force positive notification (will release key in use)
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If an input parameter is invalid.
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - initialize MLAG first.
 */
int
mlag_lacp_selection_request(enum access_cmd access_cmd,
                            unsigned int request_id, unsigned long port_id,
                            unsigned long long partner_sys_id,
                            unsigned int partner_key, unsigned char force);

#endif /* MLAG_CONF_H_ */
