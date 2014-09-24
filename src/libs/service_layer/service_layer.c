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

#include <netinet/in.h>
#include <net/ethernet.h>
#include <service_layer.h>
#include "mlag_log.h"
#include "mlag_bail.h"

/************************************************
 *  Local Defines
 ***********************************************/


/************************************************
 *  Local Macros
 ***********************************************/


/************************************************
 *  Local Type definitions
 ***********************************************/


/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/


/************************************************
 *  Local function declarations
 ***********************************************/


/************************************************
 *  Function implementations
 ***********************************************/

/**
 * This function initialize the service layer data base, and load libraries dynamically.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_init(void *log_cb)
{
    int err = 0;

    UNUSED_PARAM(log_cb);

    goto bail;

bail:
    return err;
}


/**
 * This function reset the service layer data base, and unload libraries.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_deinit(void)
{
    int err = 0;

    goto bail;

bail:
    return err;
}


/**
 * This function open handles to the proper outer components and save the handles
 * toward them in database.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_start(void)
{
    int err = 0;

    goto bail;

bail:
    return err;
}

/**
 * This function close handles to the proper outer components and delete the handles
 * from database.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_stop(void)
{
    int err = 0;

    goto bail;

bail:
    return err;
}

/**
 * This function CREATEs/DESTROYs a new/existing MLAG ports
 *
 * @param[in] port_id       - the MLAG port id.
 * @param[in] access_cmd    - access_cmd - CREATE/DESTROY.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_set(const enum oes_access_cmd access_cmd,
                const unsigned long port_id)
{
    int err = 0;

    UNUSED_PARAM(port_id);
    UNUSED_PARAM(access_cmd);

    goto bail;

bail:
    return err;
}


/**
 * This function sets the port administrative State
 *
 * @param[in] port_id       - the MLAG port id.
 * @param[in] admin_state   - port administrative state.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_state_set(const unsigned long port_id,
                      const enum oes_port_admin_state admin_state)
{
    int err = 0;

    UNUSED_PARAM(port_id);
    UNUSED_PARAM(admin_state);

    goto bail;

bail:
    return err;
}

/**
 * This function gets the port administrative State
 *
 * @param[in] port_id       - the MLAG port id.
 * @param[out] admin_state   - port administrative state.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_state_get(const unsigned long port_id,
                      enum oes_port_admin_state *admin_state)
{
    int err = 0;

    UNUSED_PARAM(port_id);
    UNUSED_PARAM(admin_state);

    goto bail;

bail:
    return err;
}


/**
 * This function CREATEs/DESTROYs a redirection between a MLAG port and a destination IPL.
 * Redirection affects only TX traffic.
 *
 * @param[in] access_cmd        - CREATE/DESTROY.
 * @param[in] port_id           - the MLAG port id.
 * @param[in] redirect_port_id  - the IPL port id.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_redirect_set(const enum oes_access_cmd access_cmd,
                         const unsigned long port_id,
                         const unsigned long redirect_port_id)
{
    int err = 0;

    UNUSED_PARAM(access_cmd);
    UNUSED_PARAM(port_id);
    UNUSED_PARAM(redirect_port_id);

    goto bail;

bail:
    return err;
}



/**
 *  This function returns information whether a given MLAG port is redirected.
 *  If so, the redirected IPL port ID is return.
 *
 * @param[in] port_id           - the MLAG port id.
 * @param[in] is_redirected     - indicates if MLAG port is redirected. (1 == redirected, else 0).
 * @param[in] redirect_port_id  - the IPL port id.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_redirect_get(const unsigned long port_id,
                         int *is_redirected,
                         unsigned long *redirected_port_id)
{
    int err = 0;

    UNUSED_PARAM(port_id);
    UNUSED_PARAM(is_redirected);
    UNUSED_PARAM(redirected_port_id);

    goto bail;

bail:
    return err;
}


/**
* This function sets the isolation group of the port (a list of ports from which
* traffic should not be transmitted to port id).
* Create - add ports to isolation group (overwrites previous configuration)
* Add - add ports to isolation group (additionally to previous configuration)
* Delete - remove ports from isolation group
* Delete All - empty isolation group
*
* @param[in] access_cmd         - CREATE/ADD/DELETE/DELETE_ALL
* @param[in] port_id            - IPL port id.
* @param[in] isolated_port_list - list of ports which won't transmit data which came from port_id
* @param[in] port_list_len      - number of ports in the list
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_port_isolation_set(const enum oes_access_cmd access_cmd,
                          const unsigned long port_id,
                          const unsigned long const *isolated_port_list,
                          const unsigned short port_list_len)
{
    int err = 0;

    UNUSED_PARAM(access_cmd);
    UNUSED_PARAM(port_id);
    UNUSED_PARAM(isolated_port_list);
    UNUSED_PARAM(port_list_len);

    goto bail;

bail:
    return err;
}


/**
* This function retrieves the isolation group of the port (a list of ports from which
* traffic should not be transmitted to log_port).
*
* @param[in]     port_id             - IPL port ID.
* @param[out]    isolated_port_list  - list of isolated ports
* @param[in,out] port_list_len       - In: array size
*                                         Out: Number of Ports in the isolation group.
*
* @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_isolate_get(const unsigned long port_id,
                        unsigned long *isolated_port_list,
                        unsigned short *port_list_len)
{
    int err = 0;

    UNUSED_PARAM(port_id);
    UNUSED_PARAM(isolated_port_list);
    UNUSED_PARAM(port_list_len);

    goto bail;

bail:
    return err;
}


/**
*  This function sets the VLANs list to a port
*
* @param[in] access_cmd     -   ADD - Add list of VLANs to port
*                               DELETE - Remove a list VLANs from port
* @param[in] port_id        -   port id
* @param[in] vlan_list      -   VLANs list
* @param[in] vlan_list_len  -   vlan_list length
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_ipl_vlan_membership_action(const enum oes_access_cmd access_cmd,
                                  const unsigned long ipl_port_id,
                                  const unsigned short *vlan_list,
                                  const unsigned short vlan_list_len)
{
    int err = 0;

    UNUSED_PARAM(access_cmd);
    UNUSED_PARAM(ipl_port_id);
    UNUSED_PARAM(vlan_list);
    UNUSED_PARAM(vlan_list_len);

    goto bail;

bail:
    return err;
}


/**
* This function trigger a system event in order to get notified on
* VLAN operational status.
* the notification will be pushed by mlag API: mlag_api_vlans_state_change_notify
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_vlan_oper_status_trigger_get(void)
{
    int err = 0;

    goto bail;

bail:
    return err;
}

/**
 * This function trigger a system event in order to get notified on
 * port operational status.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
sl_api_port_oper_status_trigger(const unsigned long port_id,
                                const enum oes_port_oper_state operstate) {
    int err = 0;
    
    UNUSED_PARAM(port_id);
    UNUSED_PARAM(operstate);

    goto bail;

bail:
    return err;
}


/**
* This function retrieves the file descriptor of the current open channel
* used for receiving a packet
*
* @param[out] fd - file descriptor
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_trap_fd_open(int *fd)
{
    int err = 0;

    UNUSED_PARAM(fd);

    goto bail;

bail:
    return err;
}


/**
* This function closes and reset the file descriptor of the current open channel
* used for receiving a packet
*
* @param[in,out] fd - file descriptor
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_trap_fd_close(int *fd)
{
    int err = 0;

    UNUSED_PARAM(fd);

    goto bail;

bail:
    return err;
}


/**
* Register / DeRegister Traps (STP , IGMP)  or Events (Port up /
* down , Temperature event) in the driver. Configure the driver
* to pass packets matching this trap ID / Event ID, to the client.
*
* @param[in] cmd        - ENABLE   - register to trap trap_id
*                         DISABLE  - de-register to trap trap_id
* @param[in] fd         - The file descriptor for the packets to be trapped.
* @param[in] l2_trap_id - Trap ID.
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_trap_register(const enum oes_access_cmd access_cmd,
                     const int fd,
                     const enum oes_l2_packet l2_trap_id)
{
    int err = 0;

    UNUSED_PARAM(access_cmd);
    UNUSED_PARAM(fd);
    UNUSED_PARAM(l2_trap_id);

    goto bail;

bail:
    return err;
}


/**
* This API enables the client to receive Traps or Events.
* Trap RX : relevant returned information is packet_p and packet_size_p.
* Event RX : relevant returned information is event info.
*       Receive info struct is relevant for both traps & events and
*       it contains the port_id in a case the event is port related event ,
*       and the ingress port_id in case of a trap.
*       Read operation is blocking.
*
* @param[in]     fd              - File descriptor to listen on.
* @param[out]    receive_info    - information regarding the source of the packet (RX port id)
* @param[out]    pkt             - copy received packet to this buffer.
* @param[in,out] pkt_size        - in : the size of packet.
*                                 out: size of received packet. (in bytes)
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_pkt_receive(const int fd,
                   struct sl_trap_receive_info *receive_info,
                   void *pkt,
                   unsigned long *pkt_size)
{
    int err = 0;

    UNUSED_PARAM(fd);
    UNUSED_PARAM(receive_info);
    UNUSED_PARAM(pkt);
    UNUSED_PARAM(pkt_size);

    goto bail;

bail:
    return err;
}


/**
*  Send a packet to a loopback interface. the packet will be treated as if it was
*  received from the HW from ingress_port_id,
*  and dispatched to the relevant application except for the
*  application who sent it.
*
* @param[in] fd                 - File descriptor to send from.
* @param[in] pkt                - buffer containing the packet to send.
* @param[in] pkt_size           - size of packet.
* @param[in] l2_trap_id         - trap ID of the packet.
* @param[in] ingress_port_id    - source mlag port id.
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_pkt_send_loopback_ctrl(const int fd,
                              const void const *pkt,
                              const unsigned long pkt_size,
                              const enum oes_l2_packet l2_trap_id,
                              const unsigned long ingress_port_id)
{
    int err = 0;

    UNUSED_PARAM(fd);
    UNUSED_PARAM(pkt);
    UNUSED_PARAM(pkt_size);
    UNUSED_PARAM(l2_trap_id);
    UNUSED_PARAM(ingress_port_id);

    goto bail;

bail:
    return err;
}

/**
* @param[in] fd             - File descriptor to send from.
* @param[in] pkt            - buffer containing the packet to send.
* @param[in] pkt_size       - size of packet.
* @param[in] egress_port_id - destination mlag port id.
*
* @return 0 when successful, otherwise ERROR
*/
int
sl_api_ctrl_pkt_send(const int fd,
                     const struct sl_api_ctrl_pkt_data *pkt_data,
                     const unsigned long pkt_size,
                     const unsigned long egress_port_id)
{
    int err = 0;

    UNUSED_PARAM(fd);
    UNUSED_PARAM(pkt_data);
    UNUSED_PARAM(pkt_size);
    UNUSED_PARAM(egress_port_id);

    goto bail;

bail:
    return err;
}

