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

#ifndef PORT_MANAGER_H_
#define PORT_MANAGER_H_
#include <mlag_api_defs.h>
#include <utils/mlag_events.h>
#include <libs/mlag_common/mlag_common.h>

#ifdef PORT_MANAGER_C_

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ PORT_MANAGER

static int
rcv_msg_handler(uint8_t *payload_data);

static int
net_order_msg_handler(uint8_t *data, int oper);
/************************************************
 *  Local Macros
 ***********************************************/
#define INC_COUNTER(counter) (counter) += 1


/************************************************
 *  Local Type definitions
 ***********************************************/


struct ports_get_data {
    int port_max;
    unsigned long *ports_arr;
    enum mlag_port_oper_state *states_arr;
    int port_num;
};

struct peer_down_ports_data {
    int peer_id;
    int port_num;
    unsigned long ports_to_delete[MLAG_MAX_PORTS];
};

#endif

/************************************************
 *  Defines
 ***********************************************/
#define PORT_CONFIGURED    1
#define PORT_DELETED       0
#define PORT_DISABLED      0
#define PORT_ENABLED       1

/************************************************
 *  Macros
 ***********************************************/


/************************************************
 *  Type definitions
 ***********************************************/

enum pm_counters {
    PM_CNT_PROTOCOL_TX = 0,
    PM_CNT_PROTOCOL_RX,

    PORT_MANAGER_LAST_COUNTER
};

enum mlag_port_state {
    MLAG_PORT_OPER_UP,
    MLAG_PORT_OPER_DOWN,
    MLAG_PORT_HOST_DOWN,
    MLAG_PORT_ADMIN_DOWN,
};


enum mlag_global_port_state {
    MLAG_PORT_GLOBAL_DISABLED,
    MLAG_PORT_GLOBAL_ENABLED,
    MLAG_PORT_GLOBAL_DOWN,
    MLAG_PORT_GLOBAL_UP,
};

struct port_manager_counters {
    uint64_t rx_protocol_msg;
    uint64_t tx_protocol_msg;
};

struct __attribute__((__packed__)) peer_port_sync_message {
    uint16_t opcode;
    uint32_t port_num;
    uint32_t del_ports;     /* add or delete */
    int mlag_id;
    unsigned long port_id[MLAG_MAX_PORTS];
};

struct __attribute__((__packed__)) peer_port_oper_sync_message {
    uint16_t opcode;
    uint32_t port_num;
    int mlag_id;
    int oper_state[MLAG_MAX_PORTS];
    unsigned long port_id[MLAG_MAX_PORTS];
};
/************************************************
 *  Global variables
 ***********************************************/


/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function returns module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t port_manager_log_verbosity_get(void);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void port_manager_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits port_manager
 *
 *  @param[in] insert_msgs_cb - callback for registering PDU handlers
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_init(insert_to_command_db_func insert_msgs_cb);

/**
 *  This function deinits port_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_deinit(void);

/**
 *  This function starts port_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_start(void);

/**
 *  This function stops port_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_stop(void);

/**
 *  This function adds configured local ports
 *
 * @param[in] mlag_ports - array of ports to delete
 * @param[in] port_num - number of ports to delete
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_mlag_ports_add(unsigned long *mlag_ports, int port_num);

/**
 *  This function deletes configured local port
 *
 * @param[in] mlag_ports - port to delete
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_mlag_port_delete(unsigned long mlag_port);

/**
 *  This function handles remote peers ports configuration
 *
 * @param[in] peer_id   - peer index
 * @param[in] mlag_ports - array of ports to add
 * @param[in] port_num - number of ports to add
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_peer_mlag_ports_add(int peer_id, unsigned long *mlag_ports,
                                     int port_num);

/**
 *  This function handles remote peers ports deletion
 *
 * @param[in] peer_id   - peer index
 * @param[in] mlag_ports - array of ports to add
 * @param[in] port_num - number of ports to add
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_peer_mlag_ports_delete(int peer_id, unsigned long *mlag_ports,
                                        int port_num);

/**
 *  This function handles global oper state notification
 *  Global oper state is set by the MLAG master port logic
 *  Down means that there are no active links for th
 *
 * @param[in] mlag_ports - array of ports
 * @param[in] new_states - port states, expected the same length
 *       as ports aray
 * @param[in] port_num - number of ports in the arrays above
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_global_oper_state_set(unsigned long *mlag_ports,
                                       int *oper_states, int port_num);

/**
 *  This function handles global admin state notification
 *  Global admin state is set by the MLAG master port logic Port
 *  is Disabled when it is not configured in all active peers
 *
 * @param[in] mlag_ports - array of ports
 * @param[in] admin_states - port states, expected the same
 *       length as ports aray
 * @param[in] port_num - number of ports in the arrays above
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_port_admin_state_set(unsigned long *mlag_ports,
                                      int *admin_states, int port_num);

/**
 *  This function handles local port up notification
 *
 * @param[in] mlag_port - port id
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_local_port_up(unsigned long mlag_port);

/**
 *  This function handles local port down notification
 *
 * @param[in] mlag_port - port id
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_local_port_down(unsigned long mlag_port);

/**
 *  This function handles peer port oper status change notification
 *  this message comes as a notification from peer (local and
 *  remote)
 *
 * @param[in] oper_chg - event carrying oper state change data
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_peer_oper_state_change(
    struct port_oper_state_change_data *oper_chg);

/**
 *  This function handles peer state change notification.
 *
 * @param[in] state_change - state  change data
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_peer_state_change(struct peer_state_change_data *state_change);

/**
 *  This function handles peer enable notification.Peer enable
 *  may affect all ports states. When peer changes its state to
 *  enabled it may affect remote port state
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_peer_enable(int mlag_peer_id);

/**
 *  This function handles peer start notification.Peer start
 *  triggers port sync process start. the local peer logic sends update
 *  of its configured ports to the Master logic
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_peer_start(int mlag_peer_id);

/**
 *  This function handles local peer role change.
 *  port manager enables global FSMs according to role
 *
 * @param[in] current_role - current role of local peer
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_role_change(int current_role);

/**
 *  This function gets a mlag port state for a given list.
 *  Caller should allocate memory for the port ids and port
 *  states array and supply their size, then the actual number
 *  of ports (<= size) is returned
 *
 * @param[out] mlag_ports - array of port IDs, allocated by
 *       caller
 * @param[out] states - array of port states, allocated by the
 *       user, if NULL it is ignored
 * @param[in,out] port_num - input the size of the arrays above,
 *       output the actual number of ports given
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_mlag_ports_get(unsigned long *mlag_ports,
                                enum mlag_port_oper_state *states,
                                int *port_num);

/**
 * This function gets mlag port state for a given id.
 * Caller should allocate memory for port info.
 *
 * @param[in] port_id - Port id.
 * @param[out] port_info - Port information, allocated by the
 *       user, if NULL it is ignored.
 *
 * @return 0 - Operation completed successfully.
 * @return -ENOENT - Entry not found.
 */
int port_manager_mlag_port_get(unsigned long port_id,
                               struct mlag_port_info *port_info);

/**
 *  This function gets port_manager module counters.
 *  counters are copied to the given struct
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_counters_get(struct mlag_counters *counters);

/**
 *  This function clears port_manager module counters.
 *
 *
 * @return void
 */
void port_manager_counters_clear(void);

/**
 *  This function disables all mlag ports. After this API is called
 *  port manager module cease to control the ports.
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_mlag_ports_disable(void);

/**
 *  This function allows port manager to manage all mlag port.
 *  After this API call port manager module may control port states
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_mlag_ports_enable(void);

/**
 *  This function handles port configuration update message receive
 *
 * @param[in] port_sync - contains port sync data
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_port_update(struct peer_port_sync_message *port_sync);

/**
 *  This function handles oper states sync done event
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_oper_sync_done(void);

/**
 *  This function handles port sync message receive
 *
 * @param[in] port_sync - contains port sync data
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_port_sync(struct peer_port_sync_message *port_sync);

/**
 *  This function handles port sync done message receive
 *
 * @param[in] port_sync - contains peer sync data
 *
 * @return 0 when successful, otherwise ERROR
 */
int port_manager_sync_finish(struct sync_event_data *port_sync);

/**
 *  This function cause port dump, either using print_cb
 *  Or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_dump(void (*dump_cb)(const char *, ...));

/**
 *  This function sets port mode, either static LAG or LACP LAG
 *
 * @param[in] port_id - mlag port id
 * @param[in] port_mode - static or LACP LAG
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_port_mode_set(unsigned long port_id,
                               enum mlag_port_mode port_mode);

/**
 *  This function gets port mode, either static LAG or LACP LAG
 *
 * @param[in] port_id - mlag port id
 * @param[out] port_mode - static or LACP LAG
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_port_mode_get(unsigned long port_id,
                               enum mlag_port_mode *port_mode);

/**
 *  This function handles LACP system id change in Slave
 *
 * @return 0 if operation completes successfully.
 */
int port_manager_lacp_sys_id_update_handle(uint8_t *data);

#endif /* PORT_MANAGER_H_ */
