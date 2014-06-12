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

#ifndef MLAG_HEALTH_DISPATCH_H_
#define MLAG_HEALTH_DISPATCH_H_


/************************************************
 *  Defines
 ***********************************************/
#define HEALTH_DISPATCHER_HANDLERS_NUM 4

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
 *  @param[in] verbosity - new log verbosity
 *
 * @return void
 */
void health_dispatcher_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function inits all health sub-modules
 *
 * @return ERROR if operation failed.
 */
int mlag_health_dispatcher_init(void);

/**
 *  This function de-Inits Mlag health
 *
 * @return int as error code.
 */
int mlag_health_dispatcher_deinit(void);

#endif /* MLAG_HEALTH_DISPATCH_H_ */
