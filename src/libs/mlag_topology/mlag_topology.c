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

#define MLAG_TOPOLOGY_C
#include <netinet/in.h>
#include <mlag_api_defs.h>
#include <errno.h>
#include "mlag_defs.h"
#include "mlag_bail.h"
#include "mlag_topology.h"

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function creates an IPL port element
 *
 * @param[out] ipl_id - allocated IPL index
 *
 * @return 0 if operation completes successfully.
 * @return -ENOSPC if no more IPLs are available
 *
 */
int
mlag_topology_ipl_create(unsigned int *ipl_id)
{
    int err = -ENOSPC;
    int idx;

    ASSERT(ipl_id != NULL);

    for (idx = 0; idx < MLAG_MAX_IPLS; idx++) {
        if (ipl_db[idx].valid == INVALID) {
            *ipl_id = idx;
            ipl_db[idx].valid = VALID;
            ipl_db[idx].port_valid = INVALID;
            ipl_db[idx].current_state = OES_PORT_DOWN;
            ipl_db[idx].peer_ip.addr.ipv4.s_addr = 0;
            ipl_db[idx].local_ip.addr.ipv4.s_addr = 0;
            ipl_db[idx].vlan_id = 0;
            err = 0;
            goto bail;
        }
    }

bail:
    return err;
}

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
mlag_topology_ipl_delete(unsigned int ipl_id)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    ipl_db[ipl_id].valid = INVALID;
    ipl_db[ipl_id].port_valid = INVALID;
    ipl_db[ipl_id].current_state = OES_PORT_DOWN;
    ipl_db[ipl_id].port_id = 0;
    ipl_db[ipl_id].peer_ip.addr.ipv4.s_addr = 0;
    ipl_db[ipl_id].local_ip.addr.ipv4.s_addr = 0;
    ipl_db[ipl_id].vlan_id = 0;

bail:
    return err;
}

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
mlag_topology_ipl_port_set(unsigned int ipl_id, unsigned long port_id)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    ipl_db[ipl_id].port_valid = VALID;
    ipl_db[ipl_id].port_id = port_id;

bail:
    return err;
}

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
mlag_topology_ipl_port_get(unsigned int ipl_id, unsigned long *port_id)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);
    ASSERT(port_id != NULL);

    *port_id = ipl_db[ipl_id].port_id;

bail:
    return err;
}


/**
 *  This function deletes IPL port index
 *
 * @param[in] ipl_id - IPL index
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_delete(unsigned int ipl_id)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    ipl_db[ipl_id].port_valid = INVALID;
    ipl_db[ipl_id].port_id = 0;

bail:
    return err;
}

/**
 *  This function gets IPL port index
 *
 * @param[in] port_id - port ID
 * @param[out] ipl_id -  the given IPL ID
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL if IPL is not defined
 */
int
mlag_topology_ipl_port_id_get(unsigned long port_id, unsigned int * ipl_id)
{
    int err = -EINVAL;
    int idx;

    ASSERT(ipl_id != NULL);

    for (idx = 0; idx < MLAG_MAX_IPLS; idx++) {
        if ((ipl_db[idx].port_valid == VALID) &&
            (ipl_db[idx].port_id == port_id)) {
            *ipl_id = idx;
            err = 0;
            goto bail;
        }
    }

bail:
    return err;
}

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
                              const unsigned short vlan_id)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    if (ipl_db[ipl_id].valid == VALID) {
        ipl_db[ipl_id].vlan_id = vlan_id;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

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
mlag_topology_ipl_vlan_id_get(unsigned int ipl_id,
                              unsigned short *vlan_id)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);
    ASSERT(vlan_id != NULL);

    if (ipl_db[ipl_id].valid == VALID) {
        *vlan_id = ipl_db[ipl_id].vlan_id;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

/**
 * This function sets IPL peer IP
 *
 * @param[in] ipl_id - IPL index
 * @param[in] peer_ip - the peer IP of the IPL
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL if IPL is not defined.
 */
int
mlag_topology_ipl_peer_ip_set(unsigned int ipl_id,
                              const struct oes_ip_addr *peer_ip)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    if (ipl_db[ipl_id].valid == VALID) {
        ipl_db[ipl_id].peer_ip = *peer_ip;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

/**
 * This function sets IPL local IP.
 *
 * @param[in] ipl_id - IPL index.
 * @param[in] local_ip - the local IP of the IPL.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL if IPL is not defined.
 */
int
mlag_topology_ipl_local_ip_set(unsigned int ipl_id,
                               const struct oes_ip_addr *local_ip)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    if (ipl_db[ipl_id].valid == VALID) {
        ipl_db[ipl_id].local_ip = *local_ip;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

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
mlag_topology_ipl_peer_ip_get(unsigned int ipl_id, struct oes_ip_addr *peer_ip)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);
    ASSERT(peer_ip != NULL);

    if (ipl_db[ipl_id].valid == VALID) {
        *peer_ip = ipl_db[ipl_id].peer_ip;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

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
                               struct oes_ip_addr *local_ip)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);
    ASSERT(local_ip != NULL);

    if (ipl_db[ipl_id].valid == VALID) {
        *local_ip = ipl_db[ipl_id].local_ip;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

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
                                 enum oes_port_oper_state port_state)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);

    if (ipl_db[ipl_id].valid == VALID) {
        ipl_db[ipl_id].current_state = port_state;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

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
                                 enum oes_port_oper_state *port_state)
{
    int err = 0;

    ASSERT(ipl_id < MLAG_MAX_IPLS);
    ASSERT(port_state != NULL);

    if (ipl_db[ipl_id].valid == VALID) {
        *port_state = ipl_db[ipl_id].current_state;
    }
    else {
        err = -EINVAL;
        MLAG_BAIL_ERROR_MSG(err, "IPL index [%u] not found\n", ipl_id);
    }

bail:
    return err;
}

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
mlag_topology_ipl_ids_get(unsigned int *ipl_ids, unsigned int *ipl_ids_cnt)
{
    int err = 0;
    unsigned int idx, ipl_max_cnt;


    ASSERT(ipl_ids != NULL);
    ASSERT(ipl_ids_cnt != NULL);

    ipl_max_cnt = *ipl_ids_cnt;
    *ipl_ids_cnt = 0;

    for (idx = 0; idx < ipl_max_cnt && idx < MLAG_MAX_IPLS; idx++) {
        if (ipl_db[idx].valid == VALID) {
            ipl_ids[*ipl_ids_cnt] = idx;
            *ipl_ids_cnt += 1;
        }
    }

bail:
    return err;
}

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
mlag_topology_redirect_ipl_id_get(unsigned int *ipl_id, unsigned long *port_id)
{
    int err = 0;

    ASSERT(ipl_id != NULL);
    ASSERT(port_id != NULL);

    *ipl_id = 0;
    if (ipl_db[0].port_valid == VALID) {
        *port_id = ipl_db[0].port_id;
    }
    else {
        err = -EINVAL;
        goto bail;
    }

bail:
    return err;
}

