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

#ifndef MLAG_TOPOLOGY_H_
#define MLAG_TOPOLOGY_H_

#include <oes_types.h>

#ifdef MLAG_TOPOLOGY_C

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_TOPOLOGY

/************************************************
 *  Local Macros
 ***********************************************/
#define INVALID 0
#define VALID 1

/************************************************
 *  Local Type definitions
 ***********************************************/

struct ipl_info {
    unsigned long port_id;
    struct oes_ip_addr peer_ip;
    struct oes_ip_addr local_ip;
    unsigned short vlan_id;
    int current_state;
    int valid;
    int port_valid;
};

/************************************************
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static struct ipl_info ipl_db[MLAG_MAX_IPLS];
/************************************************
 *  Local function declarations
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
 *  This function creates an IPL port element
 *
 * @param[out] ipl_id - allocated IPL index
 * *
 * @return 0 if operation completes successfully.
 * @return -ENOSPC if no more IPLs are available
 *
 */
int
mlag_topology_ipl_create(unsigned int *ipl_id);

/**
 *  This function deletes IPL port
 *
 * @param[in] ipl_id - IPL index
 * @param[in] port_id - the port ID for the IPL
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_delete(unsigned int ipl_id);

/**
 *  This function sets IPL port index
 *
 * @param[in] ipl_id - IPL index
 * @param[in] port_id - the port ID for the IPL
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_set(unsigned int ipl_id, unsigned long port_id);

/**
 *  This function gets IPL port index by IPL ID
 *
 * @param[in] ipl_id - IPL index
 * @param[out] port_id - the port ID for the IPL
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_get(unsigned int ipl_id, unsigned long *port_id);

/**
 *  This function deletes IPL port index
 *
 * @param[in] ipl_id - IPL index
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_delete(unsigned int ipl_id);

/**
 *  This function gets IPL port index from a given port ID
 *
 * @param[in] port_id - port ID
 * @param[out] ipl_id -  the given IPL ID
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_id_get(unsigned long port_id, unsigned int * ipl_id);

/**
 * This function sets IPL vlan id.
 *
 * @param[in] ipl_id - IPL index.
 * @param[in] vlan_id - Vlan id of the IPL.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL if IPL is not defined.
 */
int
mlag_topology_ipl_vlan_id_set(unsigned int ipl_id,
                              const unsigned short vlan_id);

/**
 * This function gets IPL vlan id.
 *
 * @param[in] ipl_id - IPL index.
 * @param[out] vlan_id - Vlan id of the IPL.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL if IPL is not defined.
 */
int
mlag_topology_ipl_vlan_id_get(unsigned int ipl_id, unsigned short *vlan_id);

/**
 *  This function sets IPL peer IP
 *
 * @param[in] ipl_id - IPL index
 * @param[in] peer_ip - the peer IP of the IPL
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_peer_ip_set(unsigned int ipl_id,
                              const struct oes_ip_addr *peer_ip);

/**
 *  This function gets IPL peer IP
 *
 * @param[in] ipl_id - IPL index
 * @param[out] peer_ip - the peer IP of the IPL
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_peer_ip_get(unsigned int ipl_id,
                              struct oes_ip_addr *peer_ip);

/**
 * This function sets IPL local IP.
 *
 * @param[in] ipl_id - IPL index.
 * @param[in] local_ip - the local IP of the IPL.
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined.
 */
int
mlag_topology_ipl_local_ip_set(unsigned int ipl_id,
                               const struct oes_ip_addr *local_ip);

/**
 * This function gets IPL local IP.
 *
 * @param[in] ipl_id - IPL index
 * @param[out] local_ip - the local IP of the IPL
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_local_ip_get(unsigned int ipl_id,
                               struct oes_ip_addr *local_ip);

/**
 *  This function sets IPL port state
 *
 * @param[in] ipl_id - IPL index
 * @param[in] port_state - the IPL port state
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_state_set(unsigned int ipl_id,
                                 enum oes_port_oper_state port_state);

/**
 *  This function gets IPL port state
 *
 * @param[in] ipl_id - IPL index
 * @param[out] port_state - the IPL port state
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_state_get(unsigned int ipl_id,
                                 enum oes_port_oper_state *port_state);

/**
 *  This function gets all IPL IDs
 *
 * @param[out] ipl_ids - array for holding IPL, allocated by caller
 * @param[in,out] ipl_ids_cnt - count of IPL IDs
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_ids_get(unsigned int *ipl_ids, unsigned int *ipl_ids_cnt);

/**
 *  This function gets the IPL ID to use for redirect
 *
 * @param[out] ipl_id - the IPL ID to be used for redirect
 * @param[out] port_id - port ID of the IPL for redirect
 *
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_redirect_ipl_id_get(unsigned int *ipl_id,
                                  unsigned long *port_id);

#endif /* MLAG_TOPOLOGY_H_ */
