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
#define LACP_MANAGER_C_

#include <errno.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_defs.h>
#include <utils/mlag_events.h>
#include <utils/mlag_log.h>
#include <libs/mlag_manager/mlag_manager.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include <libs/health_manager/health_manager.h>
#include "lib_commu.h"
#include "mlag_comm_layer_wrapper.h"
#include <libs/mlag_manager/mlag_dispatcher.h>
#include <libs/port_manager/port_db.h>
#include <libs/notification_layer/notification_layer.h>
#include "lacp_db.h"
#include "lacp_manager.h"



static int rcv_msg_handler(uint8_t *data);
static int net_order_msg_handler(uint8_t *payload, int oper);
/************************************************
 *  Global variables
 ***********************************************/


/************************************************
 *  Local variables
 ***********************************************/
static int started;
static int lacp_enabled;
static enum master_election_switch_status current_role;
static int use_local_lacp_logic;
static handler_command_t lacp_manager_ibc_msgs[] = {
    {MLAG_LACP_SYNC_MSG, "LACP sync message", rcv_msg_handler,
     net_order_msg_handler },
    {MLAG_LACP_SELECTION_EVENT, "Mlag LACP selection event",
     rcv_msg_handler, net_order_msg_handler },
    {MLAG_LACP_RELEASE_EVENT, "Mlag LACP release event",
     rcv_msg_handler, net_order_msg_handler },


    {0, "", NULL, NULL}
};

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_DEBUG;

/************************************************
 *  Local function declarations
 ***********************************************/


/************************************************
 *  Function implementations
 ***********************************************/
/*
 * Move to use local or remote LACP logic
 *
 * @param[in] use_local - whether to use local LACP logic or not
 *
 */
void
set_lacp_logic_origin(int use_local)
{
    int err = 0;

    use_local_lacp_logic = use_local;

    err = lacp_db_port_clear();
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to clear LACP selection db on role change\n");
bail:
    return;
}
/*
 *  Used for sending messages
 *  with implementation of counting mechanism
 *
 * @param[in] opcode - message opcode
 * @param[in] payload - pointer to message data
 * @param[in] payload_len - message data length
 * @param[in] dest_peer_id - mlag id of dest peer
 * @param[in] orig - message originator
 *
 * @return 0 if operation completes successfully.
 */
static int
lacp_manager_message_send(enum mlag_events opcode, void* payload,
                          uint32_t payload_len, uint8_t dest_peer_id,
                          enum message_originator orig)
{
    int err = 0;
    struct mlag_master_election_status me_status;

    err = mlag_dispatcher_message_send(opcode, payload, payload_len,
                                       dest_peer_id, orig);
    MLAG_BAIL_CHECK_NO_MSG(err);

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    /* Counters */
    if ((dest_peer_id != me_status.my_peer_id) ||
        ((orig == PEER_MANAGER) && (current_role == SLAVE))) {
        /* increment IPL counter */
        lacp_db_counters_inc(LM_CNT_PROTOCOL_TX);
    }

bail:
    return err;
}

/*
 * This function is sending sync done message to the master, it should not be
 * called on the slave
 *
 * @param[in] mlag_id - This is the mlag ID
 *
 * @return 0 if operation completes successfully.
 */
static int
lacp_send_sync_done(int mlag_id)
{
    int err = 0;
    struct sync_event_data data;

    data.peer_id = mlag_id;
    data.opcode = 0;
    data.sync_type = LACP_PEER_SYNC;
    data.state = 1; /* finish */

    err = send_system_event(MLAG_PEER_SYNC_DONE, &data, sizeof(data));
    MLAG_BAIL_ERROR(err);

    MLAG_LOG(MLAG_LOG_NOTICE, "Successfully sent SYNC_DONE for LACP\n");

bail:
    return err;
}

/*
 *  Convert struct between network and host order
 *
 * @param[in,out] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_lacp_sync_data(
    struct peer_lacp_sync_message *data, int oper)
{
    if (oper == MESSAGE_SENDING) {
        data->mlag_id = htonl(data->mlag_id);
        data->phase = htonl(data->phase);
    }
    else {
        data->mlag_id = ntohl(data->mlag_id);
        data->phase = ntohl(data->phase);
    }
}

/*
 *  Convert struct between network and host order
 *
 * @param[in,out] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_aggregator_release_data(struct lacp_aggregator_release_message *data,
                                  int oper)
{
    if (oper == MESSAGE_SENDING) {
        data->port_id = htonl(data->port_id);
    }
    else {
        data->port_id = ntohl(data->port_id);
    }
}

/*
 *  Convert struct between network and host order
 *
 * @param[in,out] data - struct body
 * @param[in] oper - convertion direction
 *
 * @return 0 when successful, otherwise ERROR
 */
static void
net_order_aggregator_selection_data(struct lacp_aggregation_message_data *data,
                                    int oper)
{
    if (oper == MESSAGE_SENDING) {
        data->force = htons(data->force);
        data->is_response = htons(data->is_response);
        data->mlag_id = htonl(data->mlag_id);
        data->partner_key = htonl(data->partner_key);
        data->port_id = htonl(data->port_id);
        data->request_id = htonl(data->request_id);
        data->response = htons(data->response);
        data->select = htons(data->select);
    }
    else {
        data->force = ntohs(data->force);
        data->is_response = ntohs(data->is_response);
        data->mlag_id = ntohl(data->mlag_id);
        data->partner_key = ntohl(data->partner_key);
        data->port_id = ntohl(data->port_id);
        data->request_id = ntohl(data->request_id);
        data->response = ntohs(data->response);
        data->select = ntohs(data->select);
    }
}

/*
 *  This function is called to handle network order for IBC message
 *
 * @param[in] payload_data - message body
 * @param[in] oper - on send or receive
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
net_order_msg_handler(uint8_t *data, int oper)
{
    int err = 0;
    uint16_t opcode;
    struct peer_lacp_sync_message *lacp_sync;
    struct lacp_aggregator_release_message *rel_msg;
    struct lacp_aggregation_message_data *agg_msg;

    if (oper == MESSAGE_SENDING) {
        opcode = *(uint16_t*)data;
        *(uint16_t*)data = htons(opcode);
    }
    else {
        opcode = ntohs(*(uint16_t*)data);
        *(uint16_t*)data = opcode;
    }

    MLAG_LOG(MLAG_LOG_DEBUG,
             "lacp_manager net_order_msg_handler: opcode=%d\n",
             opcode);

    switch (opcode) {
    case MLAG_LACP_SYNC_MSG:
        lacp_sync = (struct peer_lacp_sync_message *) data;
        net_order_lacp_sync_data(lacp_sync, oper);
        break;
    case MLAG_LACP_SELECTION_EVENT:
        agg_msg = (struct lacp_aggregation_message_data *) data;
        net_order_aggregator_selection_data(agg_msg, oper);
        break;
    case MLAG_LACP_RELEASE_EVENT:
        rel_msg = (struct lacp_aggregator_release_message *) data;
        net_order_aggregator_release_data(rel_msg, oper);
        break;

    default:
        /* Unknown opcode */
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Unknown opcode [%u] in lacp manager\n",
                            opcode);
        break;
    }
bail:
    return err;
}

/*
 *  Handle incoming LACP sync data
 *
 * @param[in] lacp_sync - message body
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
update_lacp_master_attributes(struct peer_lacp_sync_message *lacp_sync)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "got lacp sync id is [%llu]\n", lacp_sync->sys_id);

    if (current_role == SLAVE) {

    	err = lacp_db_master_system_id_set(lacp_sync->sys_id);
    	MLAG_BAIL_ERROR(err);

    	if (lacp_sync->phase == LACP_SYS_ID_UPDATE) {
    		/* Send system id update event to port manager */
    		err = send_system_event(MLAG_LACP_SYS_ID_UPDATE_EVENT, NULL, 0);
    		MLAG_BAIL_ERROR(err);
    	}
    }

bail:
    return err;
}

/*
 *  Handle incoming LACP sync message
 *
 * @param[in] lacp_sync - message body
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
handle_lacp_sync_msg(struct peer_lacp_sync_message *lacp_sync)
{
    int err = 0;
    struct mlag_master_election_status me_status;
    struct peer_lacp_sync_message sync_message;

    ASSERT(lacp_sync != NULL);

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    switch (lacp_sync->phase) {
    case LACP_SYNC_START:
        /* send sync data if master */
        sync_message.mlag_id = me_status.my_peer_id;
        sync_message.phase = LACP_SYNC_DATA;
        err = lacp_db_local_system_id_get(&sync_message.sys_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed getting local actor attributes\n");

        /* Send message to master */
        MLAG_LOG(MLAG_LOG_DEBUG,
                 "send LACP data to peer [%u] \n", lacp_sync->mlag_id);

        /* Send sync message with data */
        err = lacp_manager_message_send(MLAG_LACP_SYNC_MSG,
                                        &sync_message, sizeof(sync_message),
                                        lacp_sync->mlag_id, MASTER_LOGIC);
        break;
    case LACP_SYNC_DATA:
        /* slave handles sync data */
        err = update_lacp_master_attributes(lacp_sync);
        MLAG_BAIL_ERROR_MSG(err, "Failed updating master LACP attributes\n");
        /* send sync done message */
        sync_message.mlag_id = me_status.my_peer_id;
        sync_message.phase = LACP_SYNC_DONE;
        set_lacp_logic_origin(FALSE);

        /* Send message to master */
        MLAG_LOG(MLAG_LOG_DEBUG,
                 "send LACP data to peer [%u] \n", lacp_sync->mlag_id);

        /* Send to master logic sync message */
        err = lacp_manager_message_send(MLAG_LACP_SYNC_MSG,
                                        &sync_message, sizeof(sync_message),
                                        lacp_sync->mlag_id, PEER_MANAGER);

        break;
    case LACP_SYNC_DONE:
        /* sync done received in master */
        err = lacp_send_sync_done(lacp_sync->mlag_id);
        MLAG_BAIL_ERROR(err);
        break;
    case LACP_SYS_ID_UPDATE:
        /* slave handles lacp system id update from master */
        err = update_lacp_master_attributes(lacp_sync);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed updating master LACP attributes upon LACP_SYS_ID_UPDATE\n");
        break;
    default:
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Invalid LACP sync message\n");
        break;
    }

bail:
    return err;
}

/*
 *  This function is called to handle IBC message
 *
 * @param[in] data - message body
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
rcv_msg_handler(uint8_t *data)
{
    int err = 0;
    struct recv_payload_data *payload_data = (struct recv_payload_data*) data;
    uint16_t opcode;

    struct peer_lacp_sync_message *lacp_sync;
    struct lacp_aggregation_message_data *select_req;
    struct lacp_aggregator_release_message *rel_msg;

    opcode = *((uint16_t*)(payload_data->payload[0]));

    if ((started == FALSE) || (lacp_enabled == FALSE)) {
        goto bail;
    }

    MLAG_LOG(MLAG_LOG_INFO,
             "lacp manager rcv_msg_handler: opcode=%d\n", opcode);

    lacp_db_counters_inc(LM_CNT_PROTOCOL_RX);

    switch (opcode) {
    case MLAG_LACP_SYNC_MSG:
        lacp_sync =
            (struct peer_lacp_sync_message*) payload_data->payload[0];
        err = handle_lacp_sync_msg(lacp_sync);
        MLAG_BAIL_ERROR_MSG(err, "Failed in handling LACP sync message\n");
        break;
    case MLAG_LACP_SELECTION_EVENT:
        select_req =
            (struct lacp_aggregation_message_data *) payload_data->payload[0];

        err = lacp_manager_aggregator_selection_handle(select_req);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to handle aggregation selection event\n");
        break;
    case MLAG_LACP_RELEASE_EVENT:
        rel_msg =
            (struct lacp_aggregator_release_message *) payload_data->payload[0];

        err = lacp_manager_aggregator_free_handle(rel_msg);
        MLAG_BAIL_ERROR_MSG(err, "Failed to handle aggregation free event\n");
        break;
    default:
        /* Unknown opcode */
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Unknown opcode [%u] in lacp manager\n",
                            opcode);
        break;
    }

bail:
    return err;
}

/*
 *  This function implements LACP selection
 *
 * @param[in] mlag_id - mlag id of requestor
 * @param[in] port_id - port ID
 * @param[in] partner_id - request partner ID
 * @param[out] response - selected or not
 * @param[out] current_partner_id - currently selected partner id
 * @param[out] current_partner_key - currently selected partner key
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
lacp_aggregator_select_logic(int mlag_id,
                             unsigned long port_id,
                             unsigned long long partner_id,
                             unsigned int partner_key,
                             enum aggregate_select_response *response,
                             unsigned long long *current_partner_id,
                             unsigned int *current_partner_key)
{
    int err = 0;
    int i;
    struct mlag_lacp_data *lacp_data = NULL;
    *response = LACP_AGGREGATE_DECLINE;

    MLAG_LOG(MLAG_LOG_INFO,
             "Aggregator select logic for port [%lu] partner [%llu] key [%u]\n",
             port_id, partner_id, partner_key);

    err = lacp_db_entry_get(port_id, &lacp_data);
    if (err == -ENOENT) {
        /* create entry for first entry */
        err = lacp_db_entry_allocate(port_id, &lacp_data);
        MLAG_BAIL_ERROR_MSG(err, "Failed to create lacp entry port [%lu]\n",
                            port_id);
        lacp_data->partner_id = partner_id;
        lacp_data->partner_key = partner_key;
        for (i = 0; i < MLAG_MAX_PEERS; i++) {
            lacp_data->peer_state[i] = FALSE;
        }
    }
    if ((lacp_data->partner_id == partner_id) &&
        (lacp_data->partner_key == partner_key)) {
        /* Update users and ACCEPT request */
        lacp_data->peer_state[mlag_id] = TRUE;
        *response = LACP_AGGREGATE_ACCEPT;
    }
    *current_partner_key = lacp_data->partner_key;
    *current_partner_id = lacp_data->partner_id;

bail:
    return err;
}

/*
 *  This function implements LACP selection
 *
 * @param[in] mlag_id - mlag id of requester
 * @param[in] port_id - port ID
 * @param[out] is_free - indicate if aggregator became free
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
lacp_aggregator_free_logic(int mlag_id,
                           unsigned long port_id,
                           int *is_free)
{
    int err = 0;
    int i;
    struct mlag_lacp_data *lacp_data = NULL;
    *is_free = TRUE;

    err = lacp_db_entry_get(port_id, &lacp_data);
    if (err == -ENOENT) {
        /* there is no entry for that port */
        err = 0;
        *is_free = TRUE;
        goto bail;
    }
    /* Update users and ACCEPT request */
    lacp_data->peer_state[mlag_id] = FALSE;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (lacp_data->peer_state[i] == TRUE) {
            *is_free = FALSE;
        }
    }

    if (*is_free == TRUE) {
        /* delete entry from DB */
        err = lacp_db_entry_delete(port_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to delete free lacp port [%lu]\n",
                            port_id);
    }

bail:
    return err;
}

/*
 * Insert to pending requests queue (according to role)
 *
 * @param[in] request - selection request data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
update_pending_selection_queue(struct lacp_aggregation_message_data *request)
{
    int err = 0;

    err = lacp_db_pending_request_add(request->port_id, request);
    MLAG_BAIL_ERROR_MSG(err,
                        "Request ID [%u] port [%lu] insertion to pending DB Failed\n",
                        request->request_id, request->port_id);

bail:
    return err;
}

/*
 * Sends selection requests
 *
 * @param[in] request - selection request data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
send_selection_message(struct lacp_aggregation_message_data *request)
{
    int err = 0;
    struct mlag_master_election_status me_status;

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    /* send message to logic */
    request->mlag_id = me_status.my_peer_id;
    request->select = TRUE;

    if ((current_role == SLAVE) && (use_local_lacp_logic == TRUE)) {
        err = send_system_event(MLAG_LACP_SELECTION_EVENT,
                                request, sizeof(*request));
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending MLAG_LACP_SELECTION_EVENT event\n");
    }
    else {
        err = lacp_manager_message_send(MLAG_LACP_SELECTION_EVENT,
                                        request, sizeof(*request),
                                        me_status.master_peer_id,
                                        PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed sending selection request id [%u] port [%lu]\n",
                            request->request_id, request->port_id);
    }

bail:
    return err;
}

/*
 * Sends release requests
 *
 * @param[in] request - selection request data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
send_release_message(struct lacp_aggregation_message_data *request)
{
    int err = 0;
    struct mlag_master_election_status me_status;

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    /* send message to logic */
    request->mlag_id = me_status.my_peer_id;
    request->select = FALSE;

    if ((current_role == SLAVE) && (use_local_lacp_logic == TRUE)) {
        err = send_system_event(MLAG_LACP_SELECTION_EVENT,
                                request, sizeof(*request));
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending MLAG_LACP_SELECTION_EVENT event\n");
    }
    else {
        err = lacp_manager_message_send(MLAG_LACP_SELECTION_EVENT,
                                        request, sizeof(*request),
                                        me_status.master_peer_id,
                                        PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed sending release request id [%u] port [%lu]\n",
                            request->request_id, request->port_id);
    }

bail:
    return err;
}

/*
 * Sends notification to reject request
 *
 * @param[in] msg - selection request data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
notify_aggregator_selection_response(struct lacp_aggregation_message_data *msg)
{
    int err = 0;
    struct lacp_aggregation_message_data *db_req = NULL;
    struct mlag_notification notify;

    /* check if there is a pending request in DB */
    err = lacp_db_pending_request_get(msg->port_id, &db_req);
    if ((err == -ENOENT) || (db_req == NULL) ||
        (db_req->request_id != msg->request_id)) {
        /* no pending request - ignore */
        err = 0;
        MLAG_LOG(MLAG_LOG_DEBUG, "Ignore response port ID [%lu] req ID [%u]\n",
                 msg->port_id, msg->request_id);
        goto bail;
    }
    MLAG_BAIL_ERROR(err);

    /* fill notification msg */
    notify.notification_type = MLAG_NOTIFY_AGGREGATOR_RESPONSE;
    notify.notification_info.agg_response.request_id = msg->request_id;
    notify.notification_info.agg_response.req_partner_id = msg->partner_id;
    notify.notification_info.agg_response.req_partner_key = msg->partner_key;
    notify.notification_info.agg_response.req_port_id = msg->port_id;
    notify.notification_info.agg_response.response = msg->response;

    err = mlag_notify(&notify);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to notify response for port [%lu] req [%u]",
                        msg->port_id, msg->request_id);

    err = lacp_db_pending_request_delete(msg->port_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to clear pending request port [%lu] request [%u]",
                        msg->port_id, msg->request_id);

bail:
    return err;
}

/*
 * Sends notification to reject request
 *
 * @param[in] msg - selection request data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
notify_aggregator_release(struct lacp_aggregator_release_message *msg)
{
    int err = 0;
    struct mlag_notification notify;

    notify.notification_type = MLAG_NOTIFY_AGGREGATOR_RELEASE;
    notify.notification_info.agg_release.port_id = msg->port_id;

    /* notify on response */
    err = mlag_notify(&notify);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to notify aggregator release for port [%lu]\n",
                        msg->port_id);

bail:
    return err;
}

/*
 * Sends notification to reject request
 *
 * @param[in] request - selection request data
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
reject_pending_request(struct lacp_aggregation_message_data *request)
{
    int err = 0;

    MLAG_LOG(MLAG_LOG_DEBUG, "Reject pending request id [%u] port [%lu]\n",
             request->request_id, request->port_id);

    request->is_response = TRUE;
    request->response = LACP_AGGREGATE_DECLINE;

    err = notify_aggregator_selection_response(request);
    MLAG_BAIL_ERROR(err);

    err = lacp_db_pending_request_delete(request->port_id);
    if (err && (err != -ENOENT)) {
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to clear pending request port [%lu] request [%u]",
                            request->port_id, request->request_id);
    }
    else {
        err = 0;
    }

bail:
    return err;
}

/*
 * Clear pending requests from pending queue
 *
 * @param[in] request - struct containing pending request
 * @param[in] data - an optional parameter, not in use
 *
 * @return 0 if operation completes successfully.
 */
static int
clear_pending_request(struct lacp_aggregation_message_data *request,
                      void *data)
{
    int err = 0;
    struct mlag_notification notify;
    UNUSED_PARAM(data);

    ASSERT(request != NULL);

    /* fill notification msg */
    notify.notification_type = MLAG_NOTIFY_AGGREGATOR_RESPONSE;
    notify.notification_info.agg_response.response = LACP_AGGREGATE_DECLINE;
    notify.notification_info.agg_response.request_id = request->request_id;
    notify.notification_info.agg_response.req_partner_id = request->partner_id;
    notify.notification_info.agg_response.req_partner_key =
        request->partner_key;
    notify.notification_info.agg_response.req_port_id = request->port_id;


    err = mlag_notify(&notify);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to reject pending port [%lu] req [%u]\n",
                        request->port_id, request->request_id);

bail:
    return err;
}

/*
 * Clear pending requests from pending queue
 *
 * @param[in] request - struct containing pending request
 * @param[in] data - an optional parameter, not in use
 *
 * @return 0 if operation completes successfully.
 */
static int
clear_peer(struct mlag_lacp_data *lacp_info, void *data)
{
    int err = 0;
    int is_free = TRUE;
    int i;
    struct lacp_peer_down_ports_data *peer_down =
        (struct lacp_peer_down_ports_data *)data;
    struct lacp_aggregator_release_message release;

    /* check when peer down if ID is free */
    lacp_info->peer_state[peer_down->mlag_id] = FALSE;
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        if (lacp_info->peer_state[i] == TRUE) {
            is_free = FALSE;
        }
    }

    if (is_free) {
        /* notify free ID */
        release.port_id = lacp_info->port_id;
        err = notify_aggregator_release(&release);
        /* Ignore error deliberately */

        /* mark for deletion */
        peer_down->ports_to_delete[peer_down->port_num] = lacp_info->port_id;
        peer_down->port_num++;
    }

    return err;
}
/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
lacp_manager_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
    lacp_db_log_verbosity_set(verbosity);
}

/**
 *  This function returns module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
lacp_manager_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/**
 *  This function inits lacp_manager
 *
 *  @param[in] insert_msgs_cb - callback for registering PDU handlers
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_init(insert_to_command_db_func insert_msgs_cb)
{
    int err = 0;

    err = insert_msgs_cb(lacp_manager_ibc_msgs);
    MLAG_BAIL_ERROR(err);

    err = lacp_db_init();
    MLAG_BAIL_ERROR(err);

    started = FALSE;
    lacp_enabled = FALSE;
    current_role = NONE;
    use_local_lacp_logic = TRUE;

bail:
    return err;
}

/**
 *  This function deinits lacp_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_deinit(void)
{
    int err = 0;

    err = lacp_db_deinit();
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 *  This function starts lacp_manager
 *
 *  param[in] data - start event data
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_start(uint8_t *data)
{
    int err = 0;

    struct start_event_data *params = (struct start_event_data *)data;

    lacp_enabled = params->start_params.lacp_enable;
    started = TRUE;
    use_local_lacp_logic = TRUE;

    return err;
}

/**
 *  This function stops lacp_manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_stop(void)
{
    int err = 0;

    started = FALSE;
    lacp_enabled = FALSE;

    return err;
}

/**
 *  Handle local peer role change.
 *
 * @param[in] new_role - current role of local peer
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_role_change(int new_role)
{
    int err = 0;

    if (started == FALSE) {
        goto bail;
    }

    if (new_role == MASTER) {
        set_lacp_logic_origin(TRUE);
    }

    current_role = new_role;

bail:
    return err;
}

/**
 *  This function handles peer state change notification.
 *
 * @param[in] state_change - state  change data
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_peer_state_change(struct peer_state_change_data *state_change)
{
    int err = 0;
    int i;
    struct lacp_peer_down_ports_data peer_down_data;

    MLAG_LOG(MLAG_LOG_NOTICE, "LACP manager peer state change [%d]\n",
             state_change->state);

    if ((started == FALSE) || (lacp_enabled == FALSE)) {
        goto bail;
    }

    if ((current_role == SLAVE) && (state_change->state != HEALTH_PEER_UP)) {
        /* reject all pending requests */
        err = lacp_db_pending_foreach(clear_pending_request, NULL);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to reject pending requests on peer down\n");
        /* switch to local LACP logic */
        set_lacp_logic_origin(TRUE);
    }
    if ((current_role == MASTER) && (state_change->state != HEALTH_PEER_UP)) {
        peer_down_data.mlag_id = state_change->mlag_id;
        peer_down_data.port_num = 0;
        /* peer down, clear all active system IDs taken by this peer */
        err = lacp_db_port_foreach(clear_peer, &peer_down_data);
        MLAG_BAIL_ERROR_MSG(err, "Failed to clear peer usage on peer down\n");

        /* clear DB from unused entries */
        for (i = 0; i < peer_down_data.port_num; i++) {
            err = lacp_db_entry_delete(peer_down_data.ports_to_delete[i]);
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to remove port [%lu] entry on peer down\n",
                                peer_down_data.ports_to_delete[i]);
        }
    }

bail:
    return err;
}

/**
 *  Handles peer start notification.Peer start
 *  triggers lacp sync process start. The Master
 *  sends its actor attributes to the remotes.
 *
 * @param[in] mlag_peer_id - peer global index
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_peer_start(int mlag_peer_id)
{
    int err = 0;
    struct peer_lacp_sync_message sync_message;
    struct mlag_master_election_status me_status;

    ASSERT(mlag_peer_id < MLAG_MAX_PEERS);

    if ((started == FALSE) || (lacp_enabled == FALSE)) {
        goto bail;
    }

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    /* Master will send sync done for itself */
    if (me_status.current_status == SLAVE) {
        /* Slave will send sync start message */
        sync_message.mlag_id = me_status.my_peer_id;
        sync_message.phase = LACP_SYNC_START;

        /* Send message to master */
        MLAG_LOG(MLAG_LOG_DEBUG,
                 "LACP peer [%d] sync send to master \n",
                 sync_message.mlag_id);

        /* Send to master logic sync message */
        err = lacp_manager_message_send(MLAG_LACP_SYNC_MSG,
                                        &sync_message, sizeof(sync_message),
                                        me_status.master_peer_id,
                                        PEER_MANAGER);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending ports sync message \n");
    }
    else if (mlag_peer_id == me_status.my_peer_id) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Send LACP peer sync\n");
        err = lacp_send_sync_done(mlag_peer_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed in sending lacp sync done\n");
    }

bail:
    return err;
}

/**
 *  Sets the local system Id
 *  The LACP module gives the suggested actor parameters according
 *  to mlag protocol state (possible answers are local parameters set in this
 *  function or the remote side parameters)
 *
 *  @param[in] local_sys_id - local system ID
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_system_id_set(unsigned long long local_sys_id)
{
    int err = 0;
    struct peer_lacp_sync_message sync_message;

    MLAG_LOG(MLAG_LOG_NOTICE,
             "LACP system ID set to [%llu] \n", local_sys_id);

    err = lacp_db_local_system_id_set(local_sys_id);
    MLAG_BAIL_ERROR(err);

    if (current_role == MASTER) {
        sync_message.mlag_id = MASTER_PEER_ID;
        sync_message.phase = LACP_SYS_ID_UPDATE;
        sync_message.sys_id = local_sys_id;

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "send LACP sys id update to peer [%u] \n", SLAVE_PEER_ID);

        /* Send sync message with new system id */
        err = lacp_manager_message_send(MLAG_LACP_SYNC_MSG,
                                        &sync_message, sizeof(sync_message),
                                        SLAVE_PEER_ID, MASTER_LOGIC);

        /* Send system id update event to port manager */
        err = send_system_event(MLAG_LACP_SYS_ID_UPDATE_EVENT, NULL, 0);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 * Returns actor attributes. The actor attributes are
 * system ID, system priority to be used in the LACP PDU
 * and a chassis ID which is an index of this node within the MLAG
 * cluster, with a value in the range of 0..15
 *
 * @param[out] actor_sys_id - actor sys ID (for LACP PDU)
 * @param[out] chassis_id - MLAG cluster chassis ID, range 0..15
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_actor_parameters_get(unsigned long long *actor_sys_id,
                                  unsigned int *chassis_id)
{
    int err = 0;
    struct mlag_master_election_status me_status;

    ASSERT(actor_sys_id != NULL);
    ASSERT(chassis_id != NULL);

    if ((started == FALSE) || (lacp_enabled == FALSE)) {
        *chassis_id = 0;
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Actor parameters requested although LACP module inactive\n");
        err = lacp_db_local_system_id_get(actor_sys_id);
        MLAG_BAIL_ERROR_MSG(err, "Failed to retrieve local LACP attributes\n");
        goto bail;
    }

    err = mlag_master_election_get_status(&me_status);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting ME status\n");

    /* If slave we return master's parameters, otherwise we
     * return the local parameters
     */
    if ((me_status.current_status == SLAVE) &&
        (use_local_lacp_logic == FALSE)) {
        err = lacp_db_master_system_id_get(actor_sys_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to retrieve master LACP attributes\n");
        /* Chassis ID is set by master election module */
        *chassis_id = me_status.my_peer_id;
    }
    else {
        *chassis_id = 0;
        err = lacp_db_local_system_id_get(actor_sys_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to retrieve master LACP attributes\n");
    }

bail:
    return err;
}

/**
 * Handles a selection query .
 * This query may involve a remote peer,
 * When a delete command is used, only port_id parameter is relevant.
 * Force option is given in order to allow
 * releasing currently used key and migrating to the given partner.
 *
 * @param[in] request_id - index given by caller that will appear in reply
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 * @param[in] partner_id - partner system ID (taken from LACP PDU)
 * @param[in] partner_key - partner operational Key (taken from LACP PDU)
 * @param[in] force - force positive notification (will release key in use)
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_selection_request(unsigned int request_id,
                                          unsigned long port_id,
                                          unsigned long long partner_id,
                                          unsigned int partner_key,
                                          unsigned char force)
{
    int err = 0;
    int fail_err = 0;
    struct lacp_aggregation_message_data *previous_req = NULL;
    struct lacp_aggregation_message_data selection_req;
    int reject_req_on_err = FALSE;

    if (lacp_enabled == FALSE) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Request for selection release while LACP disabled\n");
        goto bail;
    }

    /* prepare selection request */
    selection_req.is_response = FALSE;
    selection_req.request_id = request_id;
    selection_req.port_id = port_id;
    selection_req.partner_id = partner_id;
    selection_req.partner_key = partner_key;
    selection_req.force = force;
    reject_req_on_err = TRUE;

    /* If there is an outstanding request -> reject it */
    err = lacp_db_pending_request_get(port_id, &previous_req);
    if (err && (err != -ENOENT)) {
        MLAG_BAIL_ERROR_MSG(err, "Failed in pending request lookup\n");
    }
    else if (err == 0) {
        previous_req->partner_id = partner_id;
        previous_req->partner_key = partner_key;
        err = reject_pending_request(previous_req);
        MLAG_BAIL_ERROR(err);
    }

    /* insert request to pending queue */
    err = update_pending_selection_queue(&selection_req);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to update pending selection req. queue\n");

    /* check that the port is mlag port, if not avoid DB update*/
    if (port_db_entry_exist(port_id) == FALSE) {
        err = -ENOENT;
        MLAG_BAIL_ERROR_MSG(err, "Can not find Port [%lu] in port DB\n",
                            port_id);
    }

    /* send selection request to master logic */
    err = send_selection_message(&selection_req);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed to send selection req [%u] port [%lu]\n",
                        selection_req.request_id, selection_req.port_id);

bail:
    if (err && (reject_req_on_err == TRUE)) {
        fail_err = reject_pending_request(&selection_req);
        if (fail_err) {
            MLAG_LOG(MLAG_LOG_ERROR,
                     "Failed in pending req [%u] port [%lu] reject\n",
                     selection_req.request_id, selection_req.port_id);
        }
    }
    return err;
}

/**
 * Handles a aggregator release
 * This may involve a remote peer,
 *
 * @param[in] request_id - index given by caller that will appear in reply
 * @param[in] port_id - Interface index of port. Must represent MLAG port.
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_selection_release(unsigned int request_id,
                                          unsigned long port_id)
{
    int err = 0;
    struct lacp_aggregation_message_data *previous_req = NULL;
    struct lacp_aggregation_message_data selection_req;

    if (lacp_enabled == FALSE) {
        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Request for selection release while LACP disabled\n");
        goto bail;
    }

    /* Expect there is an outstanding request -> reject it */
    err = lacp_db_pending_request_get(port_id, &previous_req);
    if (err && (err != -ENOENT)) {
        MLAG_BAIL_ERROR_MSG(err, "Failed in pending request lookup\n");
    }
    else if (err == 0) {
        /* Found a pending request - drop it */
        err = lacp_db_pending_request_delete(port_id);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to delete pending request for port [%lu]",
                            port_id);
    }

    /* prepare selection request */
    selection_req.is_response = FALSE;
    selection_req.request_id = request_id;
    selection_req.port_id = port_id;

    /* send selection request to master logic */
    err = send_release_message(&selection_req);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/**
 * Handles an incoming msg of aggregator selection
 *
 * @param[in] msg - aggregator selection message
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_selection_handle(
    struct lacp_aggregation_message_data *msg)
{
    int err = 0;
    enum aggregate_select_response response;
    int dest_peer, is_free;
    unsigned long long selected_partner_id;
    unsigned int selected_partner_key;
    struct lacp_aggregator_release_message rel_msg;

    if (msg->select == FALSE) {
        /* free request */
        err = lacp_aggregator_free_logic(msg->mlag_id, msg->port_id, &is_free);
        MLAG_BAIL_ERROR_MSG(err, "Failed in lacp aggregator free logic\n");

        if (is_free) {
            /* notify free ID */
            rel_msg.port_id = msg->port_id;
            for (dest_peer = 0; dest_peer < MLAG_MAX_PEERS; dest_peer++) {
                if ((current_role == SLAVE) &&
                    (use_local_lacp_logic == TRUE)) {
                    err = send_system_event(MLAG_LACP_RELEASE_EVENT,
                                            &rel_msg, sizeof(rel_msg));
                    MLAG_BAIL_ERROR_MSG(err,
                                        "Failed in sending lacp system event\n");
                }
                else {
                    err = lacp_manager_message_send(MLAG_LACP_RELEASE_EVENT,
                                                    &rel_msg, sizeof(rel_msg),
                                                    dest_peer, MASTER_LOGIC);
                    if (err != -ENOENT) {
                        MLAG_BAIL_ERROR_MSG(err,
                                            "Failed to send lacp release port [%lu] peer [%d]\n",
                                            rel_msg.port_id, dest_peer);
                    }
                    else {
                        err = 0;
                    }
                }
            }
        }
    }
    else if (msg->is_response == FALSE) {
        err = lacp_aggregator_select_logic(msg->mlag_id, msg->port_id,
                                           msg->partner_id, msg->partner_key,
                                           &response, &selected_partner_id,
                                           &selected_partner_key);
        MLAG_BAIL_ERROR_MSG(err, "Failed in lacp aggregator select logic\n");

        /* Send response to the selection request */
        msg->is_response = TRUE;
        msg->response = response;
        msg->partner_id = selected_partner_id;
        msg->partner_key = selected_partner_key;

        if ((current_role == SLAVE) && (use_local_lacp_logic == TRUE)) {
            err = send_system_event(MLAG_LACP_SELECTION_EVENT,
                                    msg, sizeof(*msg));
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed in sending MLAG_LACP_SELECTION_EVENT event\n");
        }
        else {
            err = lacp_manager_message_send(MLAG_LACP_SELECTION_EVENT, msg,
                                            sizeof(*msg), msg->mlag_id,
                                            MASTER_LOGIC);
            MLAG_BAIL_ERROR_MSG(err, "Failed in selection response sending\n");
        }
    }
    else {
        /* notify on response */
        err = notify_aggregator_selection_response(msg);
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 * Handles an incoming msg of aggregator selection
 *
 * @param[in] msg - aggregator release message
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_aggregator_free_handle(struct lacp_aggregator_release_message *msg)
{
    int err = 0;

    if (lacp_enabled == FALSE) {
        goto bail;
    }

    /* notify on response */
    err = notify_aggregator_release(msg);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

/*
 * Dumps specific port info
 *
 * @param[in] lacp_info - struct containing port data
 * @param[in] data - an optional dumping function
 *
 * @return 0 if operation completes successfully.
 */
static int
dump_ports_info(struct mlag_lacp_data *lacp_info, void *data)
{
    int err = 0;
    int i;
    void (*dump_cb)(const char *, ...) = data;

    ASSERT(lacp_info != NULL);

    DUMP_OR_LOG("Port ID [%lu] [0x%lx] \n", lacp_info->port_id,
                lacp_info->port_id);
    DUMP_OR_LOG("===========================\n");
    DUMP_OR_LOG("partner ID [%llu] key [%u]\n", lacp_info->partner_id,
                lacp_info->partner_key);
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        DUMP_OR_LOG("Peer [%d] - %s\n", i,
                    lacp_info->peer_state[i] == TRUE ? "Active" : "Inactive");
    }
    DUMP_OR_LOG("---------------------------\n");

bail:
    return err;
}

/**
 *  This function gets lacp_manager module counters.
 *  counters are copied to the given struct
 *
 * @param[out] counters - struct containing counters
 *
 * @return 0 if operation completes successfully.
 */
int
lacp_manager_counters_get(struct mlag_counters *counters)
{
    return lacp_db_counters_get(counters);
}

/**
 *  This function clears lacp_manager module counters.
 *
 *
 * @return void
 */
void
lacp_manager_counters_clear(void)
{
    lacp_db_counters_clear();
}

/*
 * Dumps pending LACP requests info
 *
 * @param[in] lacp_info - struct containing port data
 * @param[in] data - an optional dumping function
 *
 * @return 0 if operation completes successfully.
 */
static int
dump_pending_info(struct lacp_aggregation_message_data *request, void *data)
{
    int err = 0;
    void (*dump_cb)(const char *, ...) = data;

    ASSERT(request != NULL);

    DUMP_OR_LOG("Pending request ID [%u] port ID [%lu] [0x%lx] \n",
                request->request_id, request->port_id, request->port_id);
    DUMP_OR_LOG("===========================\n");
    DUMP_OR_LOG("partner ID [%llu] key [%u]\n", request->partner_id,
                request->partner_key);
    DUMP_OR_LOG("---------------------------\n");

bail:
    return err;
}

/**
 * Dump internal DB and data
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int
lacp_manager_dump(void (*dump_cb)(const char *, ...))
{
    int err = 0;
    unsigned long long sys_id;
    struct mlag_counters counters;

    DUMP_OR_LOG("=================\nLACP manager Dump\n=================\n");
    DUMP_OR_LOG("started [%d] lacp_enabled [%d] current_role [%d]\n", started,
                lacp_enabled, current_role);
    DUMP_OR_LOG("use local lacp logic [%d]\n", use_local_lacp_logic);
    DUMP_OR_LOG("---------------------------\n");
    DUMP_OR_LOG("Counters :\n");
    err = lacp_manager_counters_get(&counters);
    if (err) {
        DUMP_OR_LOG("Error retrieving counters \n");
    }
    else {
        DUMP_OR_LOG("Tx PDU : %llu \n", counters.tx_lacp_manager);
        DUMP_OR_LOG("Rx PDU : %llu \n", counters.rx_lacp_manager);
    }
    DUMP_OR_LOG("---------------------------\n");
    err = lacp_db_local_system_id_get(&sys_id);
    DUMP_OR_LOG("Local System ID [%llu]\n", sys_id);
    err = lacp_db_master_system_id_get(&sys_id);
    DUMP_OR_LOG("Master System ID [%llu]\n", sys_id);

    DUMP_OR_LOG("---------------------------\n");
    DUMP_OR_LOG("LACP ports DB print \n");
    DUMP_OR_LOG("---------------------------\n");
    err = lacp_db_port_foreach(dump_ports_info, dump_cb);
    MLAG_BAIL_ERROR(err);

    DUMP_OR_LOG("---------------------------\n");
    DUMP_OR_LOG("LACP pending DB print \n");
    DUMP_OR_LOG("---------------------------\n");
    err = lacp_db_pending_foreach(dump_pending_info, dump_cb);
    MLAG_BAIL_ERROR(err);

bail:
    return err;
}

