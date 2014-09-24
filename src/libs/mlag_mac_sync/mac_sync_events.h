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
#ifndef MAC_SYNC_EVENTS_H_
#define MAC_SYNC_EVENTS_H_

#include "mlag_defs.h"
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include "oes_types.h"
#include "lib_ctrl_learn_defs.h"
#include "lib_ctrl_learn.h"

/************************************************
 *  Defines
 ***********************************************/
#define NON_MLAG_PORT 0xffffffff
/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
typedef  struct fdb_uc_key_filter fdb_uc_key_filter_t;

#pragma pack(push,1)


struct mac_sync_mac_params {
    unsigned short vid;
    struct ether_addr mac_addr;
};

/*  mac_sync specific events*/

struct mac_sync_uc_mac_addr_params {
    unsigned short vid;
    struct ether_addr mac_addr;
    unsigned long log_port;
    enum fdb_uc_mac_entry_type entry_type;
};

struct mac_sync_learn_event_data {
    struct mac_sync_uc_mac_addr_params mac_params;
    uint32_t port_cookie;
    uint8_t originator_peer_id;
};

struct mac_sync_age_event_data {
    /*uint16_t opcode; */
    struct oes_fdb_uc_mac_addr_params mac_params;
    uint8_t originator_peer_id;
};


/* constant length buffer used for accumulation only */
struct mac_sync_multiple_age_buffer {
    uint16_t opcode;
    uint16_t num_msg;
    struct mac_sync_age_event_data msg[CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
};
/* floating length  message  */
struct mac_sync_multiple_age_event_data {
    uint16_t opcode;
    uint16_t num_msg;
    /* struct mac_sync_age_event_data msg;*/
};

/* fix length age message  */
struct mac_sync_fixed_age_event_data {
    uint16_t opcode;
    uint16_t num_msg;
    struct mac_sync_age_event_data msg;
};



/* constant length buffer used for accumulation only */
struct mac_sync_multiple_learn_buffer {
    uint16_t opcode;
    uint16_t num_msg;
    struct mac_sync_learn_event_data msg[CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
};
/* floating length  message  */
struct mac_sync_multiple_learn_event_data {
    uint16_t opcode;
    uint16_t num_msg;
    struct mac_sync_learn_event_data msg;
};




struct mac_sync_global_reject_event_data {
    uint16_t opcode;
    struct mac_sync_learn_event_data * learn_msg;
    int error_cause;
};


struct mac_sync_mac_sync_master_fdb_get_event_data {
    uint16_t opcode;
    uint8_t peer_id;
};

struct mac_sync_master_fdb_export_event_data {
    uint16_t opcode;
    uint32_t num_entries;
    struct mac_sync_learn_event_data entry;
};



struct mac_sync_flush_generic_data {
    fdb_uc_key_filter_t filter;
    uint8_t peer_originator;
    uint8_t non_mlag_port_flush;
};

struct mac_sync_flush_master_sends_start_event_data {
    uint16_t opcode;
    struct mac_sync_flush_generic_data gen_data;
    uint32_t number_mac_params;
    struct mac_sync_mac_params mac_params;
};

struct mac_sync_flush_peer_sends_start_event_data {
    uint16_t opcode;
    struct mac_sync_flush_generic_data gen_data;
    uint32_t number_mac_params;
    struct mac_sync_mac_params mac_params;
};

struct mac_sync_flush_peer_ack_event_data {
    uint16_t opcode;
    struct mac_sync_flush_generic_data gen_data;
    uint8_t peer_id;
};

struct mac_sync_flush_completed_event_data {
    uint16_t opcode;
};

#pragma pack(pop)
/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

#endif
