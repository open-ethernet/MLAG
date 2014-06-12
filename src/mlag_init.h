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

#ifndef MLAG_INIT_H_
#define MLAG_INIT_H_

#include "mlag_api_defs.h"
#include "mlag_log.h"

#ifdef MLAG_INIT_C_
/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG
/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/


#endif

#ifdef MLAG_INIT_C_
#define EXTERN
#else
#define EXTERN extern
#endif
/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/
#define BAIL_MLAG_NOT_INIT() do { if (mlag_init_state != TRUE) { \
                                      err = -EPERM;               \
                                      MLAG_BAIL_ERROR_MSG(err, \
                                                          "Mlag not initialized\n");  \
                                  } } while (0)

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
 * Initialize the mlag protocol.
 * *
 * @param[in] system_id - The system identification of the local machine.
 * @param[in] params - Mlag features. Disable (=FALSE) a specific feature or enable (=TRUE) it.
 *                     Pointer to an already allocated memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_init(mlag_log_cb_t log_cb);

/**
 * De-Initialize the mlag protocol.
 * *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Network problem or the operation failed.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_deinit(void);

/**
 * Enable the mlag protocol.
 * Start is called in order to enable the protocol. From this stage on, the protocol
 * is running and thus exchanges messages with peers, triggers configuration toward
 * HAL (Hardware Abstraction Layer) and events toward the system.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @param[in] system_id - The system identification of the local machine.
 * @param[in] params - Mlag features. Disable (=FALSE) a specific feature or enable (=TRUE) it.
 *                     Pointer to an already allocated memory structure.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_start(const unsigned long long system_id,
           const struct mlag_start_params *params);

/**
 * Disable the mlag protocol.
 * Stop means stopping the mlag capability of the switch. No messages or
 * configurations should be sent from any mlag module after handling stop request.
 * This function works asynchronously. After verifying its arguments are valid,
 * it queues the operation and returns immediately.
 *
 * @return 0 - Operation completed successfully.
 * @return -EIO - Operation dispatch failure.
 * @return -EPERM - Operation not permitted - pre-condition failed - Initialize the
 *                  mlag protocol first.
 */
int
mlag_stop(void);

/**
 * Set module log verbosity level.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] module - The module his log verbosity is going to change.
 * @param[in] verbosity - New log verbosity level.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Operation failure.
 */
int
mlag_log_verbosity_set(enum mlag_log_module module,
                       const mlag_verbosity_t verbosity);

/**
 * Get module log verbosity level.
 * This function works synchronously. It blocks until the operation is completed.
 *
 * @param[in] module - The module his log verbosity is going to change.
 * @param[out] verbosity - The log verbosity level.
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 * @return -EIO - Network problem or operation failure.
 */
int
mlag_log_verbosity_get(const enum mlag_log_module module,
                       mlag_verbosity_t *verbosity);

#endif /* MLAG_INIT_H_ */
