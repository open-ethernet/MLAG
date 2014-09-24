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
#ifndef MLAG_EVENTS_H_
#define MLAG_EVENTS_H_

#include "mlag_defs.h"
#include "mlag_api_defs.h"
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if.h>

/************************************************
 *  Defines
 ***********************************************/

enum mlag_events {
    MLAG_START_EVENT = 0,
    MLAG_STOP_EVENT,
    MLAG_PEER_ADD_EVENT,
    MLAG_PEER_ID_SET_EVENT,
    MLAG_PEER_DEL_EVENT,
    MLAG_IPL_IP_ADDR_CONFIG_EVENT,
    MLAG_PORT_CFG_EVENT,
    MLAG_PORT_OPER_STATE_CHANGE_EVENT,
    MLAG_MGMT_OPER_STATE_CHANGE_EVENT,
    MLAG_PEER_STATE_CHANGE_EVENT,
    MLAG_PORT_GLOBAL_STATE_EVENT,
    MLAG_PEER_START_EVENT,
    MLAG_PEER_ENABLE_EVENT,
    MLAG_IPL_PORT_SET_EVENT,
    MLAG_CONN_NOTIFY_EVENT,
    MLAG_PEER_SYNC_DONE,
    MLAG_DEINIT_EVENT,

    /* This section is for private events,
       each module may define TBD private events */
    MLAG_PEER_PORT_OPER_STATE_CHANGE,
    MLAG_PEER_RELOAD_DELAY_EXPIRED,
    MLAG_PORTS_SYNC_DATA,
    MLAG_PORTS_UPDATE_EVENT,
    MLAG_PORTS_OPER_UPDATE,
    MLAG_PORTS_OPER_SYNC_DONE,
    MLAG_PORTS_SYNC_FINISH_EVENT,
    MLAG_RELOAD_DELAY_INTERVAL_CHANGE,
    MLAG_PEER_DEL_SYNC_EVENT,

    /* MAC SYNC section*/
    MLAG_MAC_SYNC_ALL_SYNC_DONE_EVENT,
    MLAG_MAC_SYNC_MASTER_SYNC_DONE_EVENT,
    MLAG_MAC_SYNC_ALL_FDB_GET_EVENT,
    MLAG_MAC_SYNC_ALL_FDB_EXPORT_EVENT,
    MLAG_MAC_SYNC_SYNC_FINISH_EVENT,
    MLAG_MAC_SYNC_LOCAL_LEARNED_EVENT,
    MLAG_MAC_SYNC_LOCAL_AGED_EVENT,
    MLAG_MAC_SYNC_GLOBAL_LEARNED_EVENT,
    MLAG_MAC_SYNC_GLOBAL_AGED_EVENT,
    MLAG_MAC_SYNC_AGE_INTERNAL_EVENT,
    MLAG_MAC_SYNC_GLOBAL_REJECTED_EVENT,
    MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT,
    MLAG_MAC_SYNC_GLOBAL_FLUSH_PEER_SENDS_START_EVENT,
    MLAG_FLUSH_FSM_TIMER,
    MLAG_MAC_SYNC_GLOBAL_FLUSH_ACK_EVENT,
    /* MLAG_MAC_SYNC_GLOBAL_FLUSH_COMPLETED_EVENT,*/

    MLAG_HEALTH_KA_INTERVAL_CHANGE,
    MLAG_HEALTH_HEARTBEAT_TICK,
    MLAG_HEALTH_FSM_TIMER,

    MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,

    MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_EVENT,
    MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_FROM_PEER_EVENT,
    MLAG_L3_INTERFACE_VLAN_GLOBAL_STATE_CHANGE_EVENT,
    MLAG_L3_INTERFACE_LOCAL_SYNC_DONE_EVENT,
    MLAG_L3_INTERFACE_MASTER_SYNC_DONE_EVENT,
    MLAG_L3_SYNC_START_EVENT,
    MLAG_L3_SYNC_FINISH_EVENT,

    MLAG_TUNNELING_IGMP_MESSAGE,

    MLAG_STOP_DONE_EVENT,
    MLAG_ROUTER_MAC_CFG_EVENT,
    MLAG_RECONNECT_EVENT,
    MLAG_PORT_DELETED_EVENT,
    MLAG_PORT_MODE_SET_EVENT,

    MLAG_LACP_SYNC_MSG,
    MLAG_LACP_SYS_ID_SET,
    MLAG_LACP_SELECTION_REQUEST,
    MLAG_LACP_SELECTION_EVENT,
    MLAG_LACP_RELEASE_EVENT,
    MLAG_LACP_SYS_ID_UPDATE_EVENT,

    MLAG_EVENTS_NUM
};

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

#pragma pack(push,1)

struct port_global_state_event_data {
    uint16_t opcode;
    int port_num;
    unsigned long port_id[MLAG_MAX_PORTS];
    int state[MLAG_MAX_PORTS];
};

struct tcp_conn_notification_event_data {
    uint16_t opcode;
    int new_handle;
    uint16_t port;
    uint32_t ipv4_addr;
    void *data;
    int rc;
};

struct reconnect_event_data {
    uint16_t opcode;
    void *data;
};

struct port_oper_state_change_data {
    uint16_t opcode;
    int mlag_id;
    unsigned long port_id;
    int is_ipl;
    int state;
};

struct peer_conf_event_data {
    uint16_t opcode;
    int ipl_id;
    int mlag_id;
    in_addr_t peer_ip; /* 0 if deleted */
    unsigned short vlan_id;
};

struct conf_ipl_ip_event_data {
    uint16_t opcode;
    int ipl_id;
    in_addr_t local_ip_addr; /* 0 if deleted */
};

struct peer_state_change_data {
    uint16_t opcode;
    int mlag_id;        /* Peer global ID */
    int state;
};

struct mgmt_oper_state_change_data {
    uint16_t opcode;
    unsigned long long system_id;
    int state;
};

struct port_conf_event_data {
    uint16_t opcode;
    int del_ports;
    int port_num;
    unsigned long ports[MLAG_MAX_PORTS];
};


struct router_mac_cfg_data {
    uint16_t opcode;
    int del_command;
    int router_macs_list_cnt;
    struct router_mac mac_router_list[MAX_ROUTER_MACS];
};

struct mpo_port_deleted_event_data {
    uint16_t opcode;
    unsigned long port;
    int status;        /* 1  - OK  0  - fault */
};

struct reload_delay_change_data {
    uint16_t opcode;
    unsigned int msec;
};

struct ka_interval_change_data {
    uint16_t opcode;
    unsigned int msec;
};

struct port_mode_set_event_data {
    uint16_t opcode;
    unsigned long port_id;
    unsigned long port_mode;
};

struct switch_status_change_event_data {
    uint16_t opcode;
    int current_status;
    int previous_status;
    in_addr_t my_ip;   /* 0 if not defined */
    in_addr_t peer_ip; /* 0 if not defined */
    int my_peer_id;
    int master_peer_id;
};

struct ipl_port_set_event_data {
    uint16_t opcode;
    int ipl_id;
    uint32_t ifindex;
};

struct vlan_state_change_base_event_data {
    uint16_t opcode;
    int peer_id;
    int vlans_arr_cnt;
};

struct vlan_state_change_event_data {
    uint16_t opcode;
    int peer_id;
    int vlans_arr_cnt;
    struct vlan_state_info vlan_data[MLAG_VLAN_ID_MAX + 1];
};

struct sync_event_data {
    uint16_t opcode;
    int peer_id;
    int sync_type;
    int state; /* 0 -start, 1 - finish */
};

struct timer_event_data {
    uint16_t opcode;
    void *data;
};

struct start_event_data {
    uint16_t opcode;
    struct mlag_start_params start_params;
};

struct lacp_sys_id_set_data {
    uint16_t opcode;
    unsigned long long sys_id;
};

struct lacp_selection_request_event_data {
    uint16_t opcode;
    int unselect;
    unsigned long request_id;
    unsigned long port_id;
    unsigned long long partner_sys_id;
    unsigned int partner_key;
    unsigned char force;
};

#pragma pack(pop)

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

#endif
