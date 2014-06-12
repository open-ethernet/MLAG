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


#ifndef MLAG_TUNNELING_H_
#define MLAG_TUNNELING_H_

#include <utils/mlag_defs.h>

/************************************************
 *  Defines
 ***********************************************/
#define MLAG_TUNNELING_HANDLERS_NUM 4

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
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_tunneling_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return verbosity level
 */
mlag_verbosity_t
mlag_tunneling_log_verbosity_get(void);

/**
 * Populates tunneling relevant counters.
 *
 * @param[out] counters - Mlag protocol counters. Pointer to an already
 *                        allocated memory structure. Only the IGMP and xSTP
 *                        coutners are relevant to this function
 *
 * @return 0 - Operation completed successfully.
 * @return -EINVAL - If any input parameter is invalid.
 */
int
mlag_tunneling_counters_get(struct mlag_counters *counters);

/**
 * Clear the tunneling counters
 */
void
mlag_tunneling_clear_counters(void);

/**
 *  This function inits all tunneling sub-module
 *
 * @return ERROR if operation failed.
 */
int
mlag_tunneling_init(void);

/**
 *  This function de-Inits Mlag tunneling
 *
 * @return int as error code.
 */
int
mlag_tunneling_deinit(void);

/**
 *  This function cause dump, either using print_cb
 *  Or if NULL prints to LOG facility
 *
 * @param[in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 if operation completes successfully.
 */
int
mlag_tunneling_dump(void (*dump_cb)(const char *, ...));

#endif /* MLAG_TUNNELING_H_ */
