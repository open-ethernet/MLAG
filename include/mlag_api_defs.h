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


#ifndef MLAG_API_DEFS_H_
#define MLAG_API_DEFS_H_

#include <netinet/in.h>
#include <net/ethernet.h>
#include <oes_types.h>
#include <mlnx_lib/lib_ctrl_learn.h>

/************************************************
 *  Defines
 ***********************************************/
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/* if change value, then update the validations occur in mlag_api.c and mlag_internal_api.c */
#define MLAG_RELOAD_DELAY_MIN 0
#define MLAG_RELOAD_DELAY_MAX 300
/* if change value, then update the validations occur in mlag_api.c and mlag_internal_api.c */
#define MLAG_KEEPALIVE_INTERVAL_MIN 1
#define MLAG_KEEPALIVE_INTERVAL_MAX 30
/* if change value, then update the validations occur in mlag_api.c and mlag_internal_api.c */
#define MLAG_VLAN_ID_MIN 1
#define MLAG_VLAN_ID_MAX 4095

#define ACCESS_COMMAND_STR(index) ((ACCESS_CMD_LAST > \
                                    index) ? access_command_str[index] : \
                                   "UNKNOWN")
/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

struct port_state_info {
    enum oes_port_oper_state port_state;
    unsigned long port_id;
};

enum vlan_state {
    VLAN_UP = 0,
    VLAN_DOWN,
};

#pragma pack(push,1)
struct vlan_state_info {
    uint8_t vlan_state; /* use enum vlan_state for value */
    unsigned short vlan_id;
};
#pragma pack(pop)

enum mgmt_oper_state {
    MGMT_UP = 0,
    MGMT_DOWN,
};

enum access_cmd {
    ACCESS_CMD_ADD = 0,
    ACCESS_CMD_DELETE,
    ACCESS_CMD_CREATE,
    ACCESS_CMD_GET,
    ACCESS_CMD_GET_FIRST,
    ACCESS_CMD_GET_NEXT,

    ACCESS_CMD_LAST
};

/**
 * MLAG protocol counters structure.
 */
struct mlag_counters {
    unsigned long long rx_heartbeat;
    unsigned long long tx_heartbeat;
    unsigned long long rx_igmp_tunnel;
    unsigned long long tx_igmp_tunnel;
    unsigned long long rx_xstp_tunnel;
    unsigned long long tx_xstp_tunnel;
    unsigned long long tx_mlag_notification;
    unsigned long long rx_mlag_notification;
    unsigned long long rx_port_notification;
    unsigned long long tx_port_notification;
    unsigned long long rx_fdb_sync;
    unsigned long long tx_fdb_sync;
    unsigned long long rx_lacp_manager;
    unsigned long long tx_lacp_manager;
};

/**
 * MLAG port operational state.
 */
enum mlag_port_oper_state {
    INACTIVE = 0,
    ACTIVE_PARTIAL,
    ACTIVE_FULL,
    DISABLED
};

/**
 * MLAG protocol operational state.
 */
enum protocol_oper_state {
    MLAG_INIT = 0,
    MLAG_RELOAD_DELAY,
    MLAG_DOWN,
    MLAG_UP,
};
#pragma pack(push,1)
/**
 * MLAG port operational state information.
 */
struct mlag_port_info {
    enum mlag_port_oper_state port_oper_state;
    unsigned long port_id;
};

/**
 * MLAG features.
 */
struct mlag_start_params {
    unsigned char stp_enable;
    unsigned char lacp_enable;
    unsigned char igmp_enable;
};
#pragma pack(pop)
/**
 * Enumerated type that represents the module this log verbosity is going to change.
 */
enum mlag_log_module {
    MLAG_LOG_MODULE_API = 0,                            /* change only API verbosity level */
    MLAG_LOG_MODULE_INTERNAL_API,                       /* change only Internal API verbosity level */
    MLAG_LOG_MODULE_PORT_MANAGER,                       /* change only Port Manager verbosity level */
    MLAG_LOG_MODULE_L3_INTERFACE_MANAGER,               /* change only L3 Interface Manager verbosity level */
    MLAG_LOG_MODULE_MASTER_ELECTION,                    /* change only Master Election verbosity level */
    MLAG_LOG_MODULE_MAC_SYNC,                           /* change only MAC Sync verbosity level */
    MLAG_LOG_MODULE_TUNNELING,                          /* change only Tunneling verbosity level */
    MLAG_LOG_MODULE_MLAG_MANAGER,                       /* change only Mlag Manager verbosity level */
    MLAG_LOG_MODULE_MLAG_DISPATCHER,                    /* change only Mlag Dispatcher verbosity level */
    MLAG_LOG_MODULE_MAC_SYNC_DISPATCHER,                /* change only MAC Sync Dispatcher verbosity level */
    MLAG_LOG_MODULE_LACP_MANAGER,                       /* change only LACP manager verbosity level */
    MLAG_LOG_ALL,                                       /* change both Modules & API verbosity level */

    MLAG_LOG_MODULE_MIN = MLAG_LOG_MODULE_API,       /* Minimum value of verbosity target */
    MLAG_LOG_MODULE_MAX = MLAG_LOG_ALL               /* Maximum value of verbosity target */
};

/**
 * Enumerated type that represents the type of the connector.
 */
enum mlag_connector_type {
    MLAG_LOCAL_CONNECTOR = 0,
    MLAG_REMOTE_CONNECTOR,
};

/**
 * Enumerated type that represents the system role.
 */
enum system_role_state {
    MLAG_MASTER = 0,
    MLAG_SLAVE,
    MLAG_STANDALONE,
    MLAG_NONE,
    MLAG_LAST_STATUS
};

/**
 * Enumerated type that represents the peer current state.
 */
enum mlag_peer_state {
    MLAG_PEER_DOWN = 0,
    MLAG_PEER_PEERING,
    MLAG_PEER_UP,
};

/**
 * MLAG peer state information.
 */
struct peer_state {
    enum mlag_peer_state peer_state;
    unsigned long long system_peer_id;
};

/**
 * MLAG router MAC information.
 */
struct router_mac {
    unsigned long long mac;
    unsigned short vlan_id;
};

/**
 * Enumerated type that represents port aggregation mode
 */
enum mlag_port_mode {
    MLAG_PORT_MODE_STATIC = 0,
    MLAG_PORT_MODE_LACP,
    MLAG_PORT_MODE_LAST
};


/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

#endif /* MLAG_API_DEFS_H_ */
