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


#ifndef NOTIFICATION_LAYER_H_
#define NOTIFICATION_LAYER_H_


#ifdef NOTIFICATION_LAYER_C_
/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ NOTIFICATION_LAYER
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

/**
 * mlag_notification_type enum is used to define
 * possible notifications coming from MLAG protocol
 */
enum mlag_notification_type {
    MLAG_NOTIFY_AGGREGATOR_RESPONSE,
    MLAG_NOTIFY_AGGREGATOR_RELEASE,
    MLAG_NOTIFY_LACP_SYSTEM_ID_CHANGE,
    MLAG_NOTIFY_LAST
};

/**
 * mlag_lacp_aggregator_response struct contains
 * response information for LACP aggregator requests
 * issued
 */
struct mlag_lacp_aggregator_response {
    unsigned int request_id;
    unsigned int response;
    unsigned long req_port_id;
    unsigned long long req_partner_id;
    unsigned int req_partner_key;
};

/**
 * mlag_lacp_aggregator_release struct is
 * used for notifying aggregator information
 * for a certain port is no longer valid
 */
struct mlag_lacp_aggregator_release {
    unsigned long port_id;
};

/**
 * mlag_lacp_aggregator_release struct is
 * used for notifying aggregator information
 * for a certain port is no longer valid
 */
struct mlag_lacp_sys_id_change {
    unsigned char active;
    unsigned long long system_id;
};

/**
 * mlag_notification_info union contains
 * different types of notification data fields
 */
union mlag_notification_info {
    struct mlag_lacp_aggregator_response agg_response;
    struct mlag_lacp_aggregator_release agg_release;
    struct mlag_lacp_sys_id_change sys_id_change;
};

/**
 * mlag_notification struct contains
 * MLAG notifications attributes
 */
struct mlag_notification {
    enum mlag_notification_type notification_type;
    union mlag_notification_info notification_info;
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 * Used for dispatching mlag protocol notification
 *
 * @param[in] mlag_notification - mlag notification data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_notify(struct mlag_notification *mlag_notification);



#endif /* NOTIFICATION_LAYER_H_ */
