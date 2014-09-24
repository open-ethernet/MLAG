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

#ifndef MAC_SYNC_DISPATCHER_H_
#define MAC_SYNC_DISPATCHER_H_


/************************************************
 *  Defines
 ***********************************************/
#define MAC_SYNC_FD_INDEX 2

#define MLAG_MAC_SYNC_DISPATCHER_HANDLERS_NUM 4

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
 *  This function sets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t mlag_mac_sync_dispatcher_log_verbosity_get(void);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void mlag_mac_sync_dispatcher_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits mlag dispatcher
 *
 * @return ERROR if operation failed.
 */
int mlag_mac_sync_dispatcher_init(void);

/**
 *  This function deinits mlag dispatcher
 *
 * @return ERROR if operation failed.
 *
 */
int mlag_mac_sync_dispatcher_deinit(void);

/**
 *  This function sends message to destination
 *
 * @param[in] opcode - message id
 * @param[in] payload - message data
 * @param[in] payload_len - message data length
 * @param[in] dest_peer_id - peer id to send message to
 *            used for messages originated by Master Logic only
 * @param[in] orig - message originator - master logic or peer manager
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_dispatcher_message_send(enum mlag_events opcode,
                                          uint8_t* payload,
                                          uint32_t payload_len,
                                          uint8_t dest_peer_id,
                                          enum message_originator orig);

/**
 *  This function prints module's fd db
 *
 * @return current status value
 */
int mlag_mac_sync_dispatcher_print(void);

#endif /* MAC_SYNC_DISPATCHER_H_ */
