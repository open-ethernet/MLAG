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

#define NOTIFICATION_LAYER_C_
#include <errno.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_defs.h>
#include <utils/mlag_events.h>
#include "notification_layer.h"
/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/
static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_DEBUG;

static const char *mlag_notification_str[MLAG_NOTIFY_LAST] = {
    "Aggregator Response",
    "Aggregator Release",
    "System Id change",
};
/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/**
 * Used for dispatching mlag protocol notification
 *
 * @param[in] mlag_notification - mlag notification data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_notify(struct mlag_notification *notification)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "mlag_notify [%s]\n",
             mlag_notification_str[notification->notification_type]);

    if (notification->notification_type == MLAG_NOTIFY_AGGREGATOR_RESPONSE) {
        MLAG_LOG(MLAG_LOG_DEBUG, "req [%u] port [%lu]  : response [%u]\n",
                 notification->notification_info.agg_response.request_id,
                 notification->notification_info.agg_response.req_port_id,
                 notification->notification_info.agg_response.response);
        MLAG_LOG(MLAG_LOG_DEBUG, "key [%u] partner [%llu] \n",
                 notification->notification_info.agg_response.req_partner_key,
                 notification->notification_info.agg_response.req_partner_id);
    }
    if (notification->notification_type == MLAG_NOTIFY_AGGREGATOR_RELEASE) {
        MLAG_LOG(MLAG_LOG_DEBUG, "Released port id [%lu] \n",
                 notification->notification_info.agg_release.port_id);
    }

    return err;
}
