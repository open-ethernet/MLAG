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


#ifndef MLAG_API_RPC_H_
#define MLAG_API_RPC_H_

#include "mlag_defs.h"
#include "mlag_api_defs.h"
#include <complib/sx_rpc.h>

/************************************************
 *  Defines
 ***********************************************/
#define MLAG_COMMCHNL_ADDRESS "/tmp/mlag"

#define MLAG_RPC_API_MESSAGE_SIZE_LIMIT (16000)

enum mlag_internal_api_cmd {
    MLAG_INTERNAL_API_CMD_DEINIT = SX_RPC_API_LAST_INTERNAL_COMMAND,
    MLAG_INTERNAL_API_CMD_START,
    MLAG_INTERNAL_API_CMD_STOP,
    MLAG_INTERNAL_API_CMD_FDB_INIT,
    MLAG_INTERNAL_API_CMD_FDB_DEINIT,
    MLAG_INTERNAL_API_CMD_FDB_START,
    MLAG_INTERNAL_API_CMD_FDB_STOP,
    MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_GET,
    MLAG_INTERNAL_API_CMD_FDB_UC_MAC_ADDR_SET,
    MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_SET,
    MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_PORT_SET,
    MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_VID_SET,
    MLAG_INTERNAL_API_CMD_FDB_UC_FLUSH_PORT_VID_SET,
    MLAG_INTERNAL_API_CMD_RELOAD_DELAY_SET,
    MLAG_INTERNAL_API_CMD_RELOAD_DELAY_GET,
    MLAG_INTERNAL_API_CMD_KEEPALIVE_INTERVAL_SET,
    MLAG_INTERNAL_API_CMD_KEEPALIVE_INTERVAL_GET,
    MLAG_INTERNAL_API_CMD_PORTS_STATE_CHANGE_NOTIFY,
    MLAG_INTERNAL_API_CMD_PROTOCOL_OPER_STATE_GET,
    MLAG_INTERNAL_API_CMD_SYSTEM_ROLE_STATE_GET,
    MLAG_INTERNAL_API_CMD_PEERS_STATE_LIST_GET,
    MLAG_INTERNAL_API_CMD_VLANS_STATE_CHANGE_NOTIFY,
    MLAG_INTERNAL_API_CMD_VLANS_STATE_CHANGE_SYNC_FINISH_NOTIFY,
    MLAG_INTERNAL_API_CMD_MGMT_PEER_STATE_NOTIFY,
    MLAG_INTERNAL_API_CMD_PORTS_STATE_GET,
    MLAG_INTERNAL_API_CMD_PORT_STATE_GET,
    MLAG_INTERNAL_API_CMD_COUNTERS_GET,
    MLAG_INTERNAL_API_CMD_COUNTERS_CLEAR,
    MLAG_INTERNAL_API_CMD_PORT_SET,
    MLAG_INTERNAL_API_CMD_IPL_SET,
    MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_SET,
    MLAG_INTERNAL_API_CMD_LOG_VERBOSITY_GET,
    MLAG_INTERNAL_API_CMD_IPL_IDS_GET,
    MLAG_INTERNAL_API_CMD_IPL_PORT_SET,
    MLAG_INTERNAL_API_CMD_IPL_PORT_GET,
    MLAG_INTERNAL_API_CMD_IPL_IP_SET,
    MLAG_INTERNAL_API_CMD_IPL_IP_GET,
    MLAG_INTERNAL_API_CMD_DUMP,
    MLAG_INTERNAL_API_CMD_ROUTER_MAC_SET,
    MLAG_INTERNAL_API_CMD_PORT_MODE_SET,
    MLAG_INTERNAL_API_CMD_PORT_MODE_GET,
    MLAG_INTERNAL_API_CMD_LACP_SYS_ID_SET,
    MLAG_INTERNAL_API_CMD_LACP_ACTOR_PARAMS_GET,
    MLAG_INTERNAL_API_CMD_LACP_SELECT_REQUEST,
};

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
/**
 * Describes the action on the activity bit.
 */
typedef uint64_t mlag_handle_t;

/**
 * mlag_api_init_params structure is used to store
 * mlag_api_init function parameters.
 */
struct mlag_api_start_params {
    const unsigned long long system_id;
    const struct mlag_start_params features;
};

/**
 * mlag_api_fdb_init_params structure is used to store
 * mlag_api_fdb_init function parameters.
 */
struct mlag_api_fdb_init_params {
    const int br_id;
    const ctrl_learn_log_cb logging_cb;
};

/**
 * mlag_api_fdb_uc_mac_addr_get_params structure is used to store
 * mlag_api_fdb_uc_mac_addr_get function parameters.
 */
struct mlag_api_fdb_uc_mac_addr_get_params {
    enum access_cmd access_cmd;
    struct fdb_uc_key_filter key_filter;
    unsigned short data_cnt;
    struct fdb_uc_mac_addr_params mac_list[0];
};

/**
 * mlag_api_fdb_uc_flush_port_vid_set_params structure is used to store
 * mlag_api_fdb_uc_flush_port_vid_set function parameters.
 */
struct mlag_api_fdb_uc_flush_port_vid_set_params {
    const unsigned short vlan_id;
    const unsigned long port_id;
    const long originator_cookie;
};

/**
 * mlag_api_fdb_uc_flush_vid_set_params structure is used to store
 * mlag_api_fdb_uc_flush_vid_set function parameters.
 */
struct mlag_api_fdb_uc_flush_vid_set_params {
    const unsigned short vlan_id;
    const long originator_cookie;
};

/**
 * mlag_api_fdb_uc_flush_port_set_params structure is used to store
 * mlag_api_fdb_uc_flush_port_set function parameters.
 */
struct mlag_api_fdb_uc_flush_port_set_params {
    const unsigned long port_id;
    const long originator_cookie;
};

/**
 * mlag_api_fdb_uc_mac_addr_set_params structure is used to store
 * mlag_api_fdb_uc_mac_addr_set function parameters.
 */
struct mlag_api_fdb_uc_mac_addr_set_params {
    const enum access_cmd access_cmd;
    struct fdb_uc_mac_addr_params mac_entry;
    unsigned short mac_cnt;
    const long originator_cookie;
};

/**
 * mlag_api_ports_state_change_notify_params structure is used to store
 * mlag_api_ports_state_change_notify function parameters.
 */
struct mlag_api_ports_state_change_notify_params {
    unsigned int ports_arr_cnt;
    struct port_state_info ports_arr[0];
};

/**
 * mlag_api_vlans_state_change_notify_params structure is used to store
 * mlag_api_vlans_state_change_notify function parameters.
 */
struct mlag_api_vlans_state_change_notify_params {
    unsigned int vlans_arr_cnt;
    struct vlan_state_info vlans_arr[0];
};

/**
 * mlag_api_mgmt_peer_state_notify_params structure is used to store
 * mlag_api_mgmt_peer_state_notify function parameters.
 */
struct mlag_api_mgmt_peer_state_notify_params {
    const unsigned long long peer_system_id;
    const enum mgmt_oper_state mgmt_state;
};

/**
 * mlag_api_ports_state_get_params structure is used to store
 * mlag_api_ports_state_get function parameters.
 */
struct mlag_api_ports_state_get_params {
    unsigned int mlag_ports_cnt;
    struct mlag_port_info mlag_ports_information[0];
};

/**
 * mlag_api_counters_get_params structure is used to store
 * mlag_api_counters_get function parameters.
 */
struct mlag_api_counters_get_params {
    const unsigned int ipls_cnt;
    unsigned int ipl_list[MLAG_MAX_IPLS];
    struct mlag_counters counters_list[MLAG_MAX_IPLS];
};

/**
 * mlag_api_port_set_params structure is used to store
 * mlag_api_port_set function parameters.
 */
struct mlag_api_port_set_params {
    const enum access_cmd access_cmd;
    const unsigned long port_id;
};

/**
 * mlag_api_ipl_set_params structure is used to store
 * mlag_api_ipl_set function parameters.
 */
struct mlag_api_ipl_set_params {
    const enum access_cmd access_cmd;
    unsigned int ipl_id;
};

/**
 * mlag_api_log_verbosity_set_params structure is used to store
 * mlag_api_log_verbosity_set/get function parameters.
 */
struct mlag_api_log_verbosity_params {
    const enum mlag_log_module module;
    mlag_verbosity_t verbosity;
};

/**
 * mlag_api_ipl_ids_get_params structure is used to store
 * mlag_api_ipl_ids_get function parameters.
 */
struct mlag_api_ipl_ids_get_params {
    unsigned int ipl_ids_cnt;
    unsigned int ipl_ids[0];
};

/**
 * mlag_api_ipl_port_set_params structure is used to store
 * mlag_api_ipl_port_set function parameters.
 */
struct mlag_api_ipl_port_set_params {
    const enum access_cmd access_cmd;
    const int ipl_id;
    const unsigned long port_id;
};

/**
 * mlag_api_ipl_port_get_params structure is used to store
 * mlag_api_ipl_port_get function parameters.
 */
struct mlag_api_ipl_port_get_params {
    const int ipl_id;
    unsigned long port_id;
};

/**
 * mlag_api_port_state_get_params structure is used to store
 * mlag_api_port_state_get function parameters.
 */
struct mlag_api_port_state_get_params {
    const unsigned long port_id;
    struct mlag_port_info mlag_port_information;
};

/**
 * mlag_api_ipl_ip_set_params structure is used to store
 * mlag_api_ipl_ip_set function parameters.
 */
struct mlag_api_ipl_ip_set_params {
    const enum access_cmd access_cmd;
    const int ipl_id;
    const unsigned short vlan_id;
    struct oes_ip_addr local_ipl_ip_address;
    struct oes_ip_addr peer_ipl_ip_address;
};

/**
 * mlag_api_ipl_ip_get_params structure is used to store
 * mlag_api_ipl_ip_get function parameters.
 */
struct mlag_api_ipl_ip_get_params {
    const int ipl_id;
    unsigned short vlan_id;
    struct oes_ip_addr local_ipl_ip_address;
    struct oes_ip_addr peer_ipl_ip_address;
};

/**
 * mlag_api_peers_state_list_get_params structure is used to store
 * mlag_api_peers_state_list_get function parameters.
 */
struct mlag_api_peers_state_list_get_params {
    struct peer_state peers_list[MLAG_MAX_PEERS];
    unsigned int peers_list_cnt;
};

/**
 * mlag_api_router_mac_set_params structure is used to store
 * mlag_api_router_mac_set function parameters.
 */
struct mlag_api_router_mac_set_params {
    enum access_cmd access_cmd;
    unsigned int router_macs_list_cnt;
    struct router_mac router_macs_list[0];
};

/**
 * mlag_api_port_mode_set_params structure is used to store
 * mlag_api_port_mode_set function parameters.
 */
struct mlag_api_port_mode_params {
    unsigned long port_id;
    enum mlag_port_mode port_mode;
};

/**
 * mlag_api_lacp_actor_params structure is used to store
 * mlag_api_lacp_actor_parameters_get function parameters.
 */
struct mlag_api_lacp_actor_params {
    unsigned long long system_id;
    unsigned int chassis_id;
};

/**
 * mlag_api_lacp_selection_request_params structure is used to store
 * mlag_api_lacp_selection_request function parameters.
 */
struct mlag_api_lacp_selection_request_params {
    enum access_cmd access_cmd;
    unsigned int request_id;
    unsigned long port_id;
    unsigned long long partner_sys_id;
    unsigned int partner_key;
    unsigned char force;
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

#endif /* MLAG_API_RPC_H_ */
