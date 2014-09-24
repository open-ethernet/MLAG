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

#define MLAG_CONF_C_

#include <complib/sx_rpc.h>
#include "mlag_init.h"
#include <errno.h>
#include <mlag_api_defs.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/mlag_topology/mlag_topology.h>
#include "mlag_conf.h"
#include <libs/port_manager/port_manager.h>
#include <libs/lacp_manager/lacp_manager.h>
#include <libs/mlag_tunneling/mlag_tunneling.h>
#include <libs/health_manager/health_manager.h>
#include <libs/mlag_manager/mlag_manager.h>
#include <libs/mlag_mac_sync/mlag_mac_sync_manager.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/mlag_l3_interface_manager/mlag_l3_interface_manager.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include <libs/mlag_mac_sync/mlag_mac_sync_manager.h>
#include <libs/service_layer/service_layer.h>

/************************************************
 *  Global variables
 ***********************************************/
EXTERN int mlag_init_state;
/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/* FILE object for dump functionality */
static FILE *dump_file = NULL;

/************************************************
 *  Local function declarations
 ***********************************************/
static void
dump_to_file(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(dump_file, fmt, args);
    va_end(args);
}
/************************************************
 *  Function implementations
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
int
mlag_ports_state_change_notify(const struct port_state_info *ports_arr,
                               const unsigned int ports_arr_cnt)
{
    int err = 0;
    int ipl_err_check;
    unsigned long i;
    unsigned int ipl_id;
    struct port_oper_state_change_data state_change;

    BAIL_MLAG_NOT_INIT();

    SAFE_MEMSET(&state_change, 0);

    for (i = 0; i < ports_arr_cnt; i++) {
        state_change.port_id = ports_arr[i].port_id;
        state_change.is_ipl = FALSE;
        ipl_err_check =
            mlag_topology_ipl_port_id_get(ports_arr[i].port_id, &ipl_id);
        if (ipl_err_check == 0) {
            state_change.is_ipl = TRUE;
            err = mlag_topology_ipl_port_state_set(ipl_id,
                                                   ports_arr[i].port_state);
            state_change.port_id = ipl_id;
            MLAG_BAIL_ERROR(err);
        }

        state_change.state = ports_arr[i].port_state;
        state_change.mlag_id = INVALID_MLAG_PEER_ID;

        err = send_system_event(MLAG_PORT_OPER_STATE_CHANGE_EVENT,
                                &state_change,
                                sizeof(state_change));
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

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
int
mlag_vlans_state_change_notify(const struct vlan_state_info *vlans_arr,
                               const unsigned int vlans_arr_cnt)
{
    int err = 0;
    unsigned long i;
    int ev_len = 0;
    static struct vlan_state_change_event_data state_change;

    BAIL_MLAG_NOT_INIT();

    if ((!vlans_arr_cnt) ||
        (vlans_arr_cnt >= MLAG_VLAN_ID_MAX)) {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(EINVAL,
                            "mlag_vlans_state_change_notify: invalid vlans_arr_cnt %d",
                            vlans_arr_cnt);
    }

    state_change.opcode = MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_EVENT;
    state_change.peer_id = 0;
    state_change.vlans_arr_cnt = vlans_arr_cnt;
    for (i = 0; i < vlans_arr_cnt; i++) {
        state_change.vlan_data[i].vlan_id = vlans_arr[i].vlan_id;
        state_change.vlan_data[i].vlan_state = vlans_arr[i].vlan_state;
    }
    ev_len = sizeof(struct vlan_state_change_base_event_data) +
             sizeof(state_change.vlan_data[0]) * vlans_arr_cnt;

    err = send_system_event(MLAG_L3_INTERFACE_VLAN_LOCAL_STATE_CHANGE_EVENT,
                            &state_change, ev_len);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
mlag_vlans_state_change_sync_finish_notify(void)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = send_system_event(MLAG_L3_INTERFACE_LOCAL_SYNC_DONE_EVENT,
                            NULL, 0);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
int
mlag_mgmt_peer_state_notify(const unsigned long long peer_system_id,
                            const enum mgmt_oper_state mgmt_state)
{
    int err = 0;
    struct mgmt_oper_state_change_data state_change;

    BAIL_MLAG_NOT_INIT();

    state_change.system_id = peer_system_id;
    state_change.state = mgmt_state;

    err = send_system_event(MLAG_MGMT_OPER_STATE_CHANGE_EVENT, &state_change,
                            sizeof(state_change));
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
mlag_counters_get(const unsigned int *ipl_ids,
                  const unsigned int ipl_ids_cnt,
                  struct mlag_counters *counters)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    UNUSED_PARAM(ipl_ids);
    UNUSED_PARAM(ipl_ids_cnt);

    /* get mlag tunneling counters */
    err = mlag_tunneling_counters_get(counters);
    MLAG_BAIL_ERROR(err);

    /* get mlag mac sync counters */
    err = mlag_mac_sync_counters_get(counters);
    MLAG_BAIL_ERROR(err);

    /* get port manager counters */
    err = port_manager_counters_get(counters);
    MLAG_BAIL_ERROR(err);

    /* get mlag manager counters */
    err = mlag_manager_counters_get(counters);
    MLAG_BAIL_ERROR(err);

    /* get health manager counters */
    err = health_manager_counters_get(counters);
    MLAG_BAIL_ERROR(err);

    /* get lacp manager counters */
    err = lacp_manager_counters_get(counters);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
mlag_counters_clear(void)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    mlag_health_manager_counters_clear();
    mlag_tunneling_clear_counters();
    mlag_manager_counters_clear();
    port_manager_counters_clear();
    mlag_mac_sync_counters_clear();
    lacp_manager_counters_clear();

bail:
    return err;
}

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
int
mlag_ports_state_get(struct mlag_port_info *mlag_ports_information,
                     unsigned int *mlag_ports_cnt)
{
    int err = 0;
    int i, port_num;
    static unsigned long ports[MLAG_MAX_PORTS];
    static enum mlag_port_oper_state states[MLAG_MAX_PORTS];

    BAIL_MLAG_NOT_INIT();

    port_num = *mlag_ports_cnt;

    err = port_manager_mlag_ports_get(ports, states, &port_num);
    MLAG_BAIL_ERROR(err);

    for (i = 0; i < port_num; i++) {
        mlag_ports_information[i].port_oper_state = states[i];
        mlag_ports_information[i].port_id = ports[i];
    }
    *mlag_ports_cnt = port_num;

bail:
    return err;
}

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
                    struct mlag_port_info *mlag_port_information)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = port_manager_mlag_port_get(port_id,
                                     mlag_port_information);
    MLAG_BAIL_ERROR_MSG(err, "Failed get port state. Port id is %lu\n",
                        port_id);

bail:
    return err;
}

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
mlag_protocol_oper_state_get(enum protocol_oper_state *protocol_oper_state)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_manager_protocol_oper_state_get(protocol_oper_state);
    MLAG_BAIL_ERROR_MSG(err, "Failed get protocol operation state\n");

bail:
    return err;
}

/**
 * Populates system_role with the current system role state.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[out] system_role - System role state. Pointer to an already allocated memory
 *                           structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_system_role_state_get(enum system_role_state *system_role_state)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_manager_system_role_state_get(system_role_state);
    MLAG_BAIL_ERROR_MSG(err, "Failed get system role\n");

bail:
    return err;
}

/**
 * Populates peers_list with all the current peers state.
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
                          unsigned int *peers_list_cnt)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_manager_peers_state_list_get(peers_list,
                                            peers_list_cnt);
    MLAG_BAIL_ERROR_MSG(err, "Failed get peers state list\n");

bail:
    return err;
}

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
int
mlag_port_set(const enum access_cmd access_cmd,
              const unsigned long port_id)
{
    int err = 0;
    struct port_conf_event_data port_set;

    BAIL_MLAG_NOT_INIT();

    port_set.port_num = 1;
    port_set.ports[0] = port_id;

    switch (access_cmd) {
    case ACCESS_CMD_ADD:
        port_set.del_ports = FALSE;
        err = send_system_event(MLAG_PORT_CFG_EVENT, &port_set,
                                sizeof(port_set));
        MLAG_BAIL_ERROR(err);
        break;
    case ACCESS_CMD_DELETE:
        port_set.del_ports = TRUE;
        err = send_system_event(MLAG_PORT_CFG_EVENT, &port_set,
                                sizeof(port_set));
        MLAG_BAIL_ERROR(err);

        mlag_manager_wait_for_port_delete_done();

        err = sl_api_port_set(OES_ACCESS_CMD_DESTROY, port_id);
        MLAG_BAIL_ERROR(err);
        break;
    default:
        MLAG_BAIL_ERROR_MSG(-EPERM, "Unsupported Command [%d]\n", access_cmd);
        break;
    }

bail:
    return err;
}

/*
 * Handle peer delete sequence. required also when setting peer IP on
 * the IPL
 *
 * @param[in] ipl_id - Ipl id.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If IPL does not exist
 */
static int
peer_delete(const int ipl_id)
{
    int err = 0;
    int mlag_id;
    struct oes_ip_addr del_peer_ip;
    struct peer_conf_event_data peer_conf_data;

    err = mlag_topology_ipl_peer_ip_get(ipl_id, &del_peer_ip);
    MLAG_BAIL_ERROR(err);

    if (del_peer_ip.addr.ipv4.s_addr != 0) {
        mlag_manager_db_lock();

        peer_conf_data.peer_ip = del_peer_ip.addr.ipv4.s_addr;
        err = mlag_manager_db_mlag_peer_id_get(peer_conf_data.peer_ip,
                                               &mlag_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to find peer ip [0x%x] on ipl [%d]\n",
                            peer_conf_data.peer_ip, ipl_id);
        peer_conf_data.mlag_id = mlag_id;
        peer_conf_data.ipl_id = ipl_id;
        peer_conf_data.vlan_id = 0;
        err = send_system_event(MLAG_PEER_DEL_EVENT, &peer_conf_data,
                                sizeof(peer_conf_data));
        MLAG_BAIL_ERROR(err);
        del_peer_ip.addr.ipv4.s_addr = 0;
        err = mlag_topology_ipl_peer_ip_set(ipl_id, &del_peer_ip);
        MLAG_BAIL_ERROR_MSG(err, "Failed to set peer IP in topology module\n");
    }

bail:
    return err;
}

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
int
mlag_ipl_set(const enum access_cmd access_cmd, unsigned int *ipl_id)
{
    int err = 0;
    struct conf_ipl_ip_event_data ipl_lcl_ip;

    BAIL_MLAG_NOT_INIT();

    switch (access_cmd) {
    case ACCESS_CMD_CREATE:
        err = mlag_topology_ipl_create(ipl_id);
        MLAG_BAIL_ERROR(err);
        break;
    case ACCESS_CMD_DELETE:
        /* Clear local IP */
        ipl_lcl_ip.local_ip_addr = 0;
        ipl_lcl_ip.ipl_id = *ipl_id;
        err = send_system_event(MLAG_IPL_IP_ADDR_CONFIG_EVENT, &ipl_lcl_ip,
                                sizeof(ipl_lcl_ip));
        MLAG_BAIL_ERROR(err);
        /* Delete Peer */
        err = peer_delete(*ipl_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to delete peer on IPL [%d]\n",
                            *ipl_id);
        err = mlag_topology_ipl_delete(*ipl_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to delete IPL [%d]\n", *ipl_id);
        break;
    default:
        MLAG_BAIL_ERROR_MSG(-EPERM, "Unsupported Command [%d]\n", access_cmd);
        break;
    }

bail:
    return err;
}

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
mlag_ipl_ids_get(unsigned int *ipl_ids,
                 unsigned int *ipl_ids_cnt)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_topology_ipl_ids_get(ipl_ids, ipl_ids_cnt);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
int
mlag_ipl_port_set(const enum access_cmd access_cmd, const int ipl_id,
                  const unsigned long port_id)
{
    int err = 0;
    struct ipl_port_set_event_data ipl_port_data;

    BAIL_MLAG_NOT_INIT();

    switch (access_cmd) {
    case ACCESS_CMD_ADD:
        err = mlag_topology_ipl_port_set(ipl_id, port_id);
        MLAG_BAIL_ERROR(err);

        ipl_port_data.ipl_id = ipl_id;
        ipl_port_data.ifindex = (uint32_t) port_id;
        err = send_system_event(MLAG_IPL_PORT_SET_EVENT, &ipl_port_data,
                                sizeof(ipl_port_data));
        MLAG_BAIL_ERROR(err);
        break;
    case ACCESS_CMD_DELETE:
        err = mlag_topology_ipl_port_delete(ipl_id);
        MLAG_BAIL_ERROR(err);

        ipl_port_data.ipl_id = ipl_id;
        ipl_port_data.ifindex = 0;
        err = send_system_event(MLAG_IPL_PORT_SET_EVENT, &ipl_port_data,
                                sizeof(ipl_port_data));
        MLAG_BAIL_ERROR(err);
        break;
    default:
        MLAG_BAIL_ERROR_MSG(-EPERM, "Unsupported Command [%d]\n", access_cmd);
        break;
    }

bail:
    return err;
}

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
mlag_ipl_port_get(const int ipl_id,
                  unsigned long *port_id)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_topology_ipl_port_get(ipl_id, port_id);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
 * @return -EIO - Network problem or operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 * @return -EAFNOSUPPORT- IPv6 currently not supported.
 */
int
mlag_ipl_ip_set(const enum access_cmd access_cmd,
                const int ipl_id,
                const unsigned short vlan_id,
                const struct oes_ip_addr *local_ipl_ip_address,
                const struct oes_ip_addr *peer_ipl_ip_address)
{
    int err = 0;
    int peer_id;
    struct conf_ipl_ip_event_data ipl_lcl_ip;
    struct peer_conf_event_data peer_conf_data;
    in_addr_t ipl_local_ip;

    BAIL_MLAG_NOT_INIT();

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    switch (access_cmd) {
    case ACCESS_CMD_ADD:
        ipl_local_ip = local_ipl_ip_address->addr.ipv4.s_addr;

        /* First remove peer if exists */
        err = peer_delete(ipl_id);
        MLAG_BAIL_ERROR(err);
        /* Configure local IP */
        ipl_lcl_ip.local_ip_addr = ipl_local_ip;
        ipl_lcl_ip.ipl_id = ipl_id;
        err = send_system_event(MLAG_IPL_IP_ADDR_CONFIG_EVENT, &ipl_lcl_ip,
                                sizeof(ipl_lcl_ip));
        MLAG_BAIL_ERROR(err);

        /* Try to take lock */
        if (peer_ipl_ip_address->addr.ipv4.s_addr != 0) {
            mlag_manager_db_lock();
            mlag_manager_db_unlock();
            err = mlag_manager_db_peer_add(
                peer_ipl_ip_address->addr.ipv4.s_addr,
                &peer_id);
            MLAG_BAIL_ERROR(err);
            peer_conf_data.peer_ip = peer_ipl_ip_address->addr.ipv4.s_addr;
            peer_conf_data.ipl_id = ipl_id;
            peer_conf_data.vlan_id = vlan_id;
            err = send_system_event(MLAG_PEER_ADD_EVENT, &peer_conf_data,
                                    sizeof(peer_conf_data));
            MLAG_BAIL_ERROR(err);
        }
        err = mlag_topology_ipl_peer_ip_set(ipl_id, peer_ipl_ip_address);
        MLAG_BAIL_ERROR_MSG(err, "Failed to set peer IP in topology module\n");
        err = mlag_topology_ipl_local_ip_set(ipl_id, local_ipl_ip_address);
        MLAG_BAIL_ERROR_MSG(err, "Failed to set peer IP in topology module\n");
        err = mlag_topology_ipl_vlan_id_set(ipl_id, vlan_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to set vlan_id in topology module\n");
        break;
    case ACCESS_CMD_DELETE:
        /* Configure local IP */
        ipl_lcl_ip.local_ip_addr = 0;
        ipl_lcl_ip.ipl_id = ipl_id;
        err = send_system_event(MLAG_IPL_IP_ADDR_CONFIG_EVENT, &ipl_lcl_ip,
                                sizeof(ipl_lcl_ip));
        MLAG_BAIL_ERROR(err);
        /* Peer Delete */
        err = peer_delete(ipl_id);
        MLAG_BAIL_ERROR(err);

        break;
    default:
        err = -EPERM;
        MLAG_BAIL_ERROR(err);
        break;
    }

bail:
    return err;
}

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
mlag_ipl_ip_get(const int ipl_id,
                unsigned short *vlan_id,
                struct oes_ip_addr *local_ipl_ip_address,
                struct oes_ip_addr *peer_ipl_ip_address)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_topology_ipl_peer_ip_get(ipl_id, peer_ipl_ip_address);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get peer IP in topology module\n");
    err = mlag_topology_ipl_local_ip_get(ipl_id, local_ipl_ip_address);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get local IP in topology module\n");
    err = mlag_topology_ipl_vlan_id_get(ipl_id, vlan_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get vlan id in topology module\n");

bail:
    return err;
}

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
int
mlag_reload_delay_set(const unsigned int sec)
{
    int err = 0;
    struct reload_delay_change_data reload_delay_set;

    BAIL_MLAG_NOT_INIT();

    reload_delay_set.msec = sec * 1000;
    err = send_system_event(MLAG_RELOAD_DELAY_INTERVAL_CHANGE,
                            &reload_delay_set,
                            sizeof(reload_delay_set));
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
mlag_reload_delay_get(unsigned int *sec)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = mlag_manager_reload_delay_interval_get(sec);
    MLAG_BAIL_ERROR(err);

    *sec = *sec / 1000;

bail:
    return err;
}

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
int
mlag_keepalive_interval_set(const unsigned int sec)
{
    int err = 0;
    struct ka_interval_change_data ka_set;

    BAIL_MLAG_NOT_INIT();

    ka_set.msec = sec * 1000;
    err =
        send_system_event(MLAG_HEALTH_KA_INTERVAL_CHANGE, &ka_set,
                          sizeof(ka_set));
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
mlag_keepalive_interval_get(unsigned int *sec)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = health_manager_heartbeat_interval_get(sec);
    MLAG_BAIL_ERROR(err);

    *sec = *sec / 1000;

bail:
    return err;
}

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
mlag_dump(char *dump_file_path)
{
    int err = 0;
    dump_file = NULL;

    BAIL_MLAG_NOT_INIT();

    /* open the given file for dump */
    dump_file = fopen(dump_file_path, "a");
    MLAG_BAIL_CHECK(dump_file != NULL, -EINVAL);

    /* Dump modules state */
    err = health_manager_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = port_manager_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = mlag_manager_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = mlag_tunneling_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = mlag_master_election_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = mlag_l3_interface_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = mlag_mac_sync_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

    err = lacp_manager_dump(dump_to_file);
    MLAG_BAIL_ERROR(err);

bail:
    if (dump_file) {
        fclose(dump_file);
        dump_file = NULL;
    }
    return err;
}

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
                    const unsigned int router_macs_list_cnt)
{
    int err = 0;
    struct router_mac_cfg_data state_change;

    BAIL_MLAG_NOT_INIT();

    memset(&state_change, 0, sizeof(state_change));

    state_change.router_macs_list_cnt = router_macs_list_cnt;
    memcpy(state_change.mac_router_list, router_macs_list,
           router_macs_list_cnt * sizeof(router_macs_list[0]));

    state_change.del_command =
        (access_cmd == ACCESS_CMD_DELETE) ? TRUE : FALSE;

    err = send_system_event(MLAG_ROUTER_MAC_CFG_EVENT,
                            &state_change, sizeof(state_change));
    MLAG_BAIL_ERROR(err);

bail:
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
mlag_port_mode_set(const unsigned long port_id,
                   const enum mlag_port_mode port_mode)
{
    int err = 0;
    struct port_mode_set_event_data mode_set;

    BAIL_MLAG_NOT_INIT();

    mode_set.port_id = port_id;
    mode_set.port_mode = port_mode;

    err = send_system_event(MLAG_PORT_MODE_SET_EVENT, &mode_set,
                            sizeof(mode_set));
    MLAG_BAIL_ERROR(err);

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
mlag_port_mode_get(const unsigned long port_id,
                   enum mlag_port_mode *port_mode)
{
    int err = 0;

    BAIL_MLAG_NOT_INIT();

    err = port_manager_port_mode_get(port_id, port_mode);
    MLAG_BAIL_ERROR(err);

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
mlag_lacp_local_sys_id_set(const unsigned long long local_sys_id)
{
    int err = 0;
    struct lacp_sys_id_set_data sys_id_set;

    BAIL_MLAG_NOT_INIT();

    sys_id_set.sys_id = local_sys_id;

    err = send_system_event(MLAG_LACP_SYS_ID_SET, &sys_id_set,
                            sizeof(sys_id_set));
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

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
                               unsigned int *chassis_id)
{
    int err = 0;

    err = lacp_manager_actor_parameters_get(actor_sys_id, chassis_id);
    MLAG_BAIL_ERROR(err);

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
                            unsigned int request_id,
                            unsigned long port_id,
                            unsigned long long partner_sys_id,
                            unsigned int partner_key,
                            unsigned char force)
{
    int err = 0;
    struct lacp_selection_request_event_data request;

    BAIL_MLAG_NOT_INIT();

    request.unselect = FALSE;
    if (access_cmd == ACCESS_CMD_DELETE) {
        request.unselect = TRUE;
    }
    request.request_id = request_id;
    request.port_id = port_id;
    request.partner_sys_id = partner_sys_id;
    request.partner_key = partner_key;
    request.force = force;

    err = send_system_event(MLAG_LACP_SELECTION_REQUEST, &request,
                            sizeof(request));
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}
