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

#ifndef MLAG_MAIN_H_
#define MLAG_MAIN_H_

/************************************************
 *  Defines
 ***********************************************/

enum {
    MLAG_CONNECTOR = 1000,
    MLAG_DEBUG = 1001,
    MLAG_VERSION = 1002,
    MLAG_LOGGER = 1003
};
/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

typedef void (*connector_cb_t)(int (*connector_api_deinit_fun)(void));
typedef int (*connector_deinit_cb_t)(void);

typedef void (*debug_lib_cb_t)(void);
typedef int (*debug_lib_deinit_cb_t)(void);

struct mlag_args_t {
    int verbosity_level;
    mlag_log_cb_t log_cb;
    connector_cb_t connector_cb;
    connector_deinit_cb_t connector_deinit_cb;
    debug_lib_cb_t debug_cb;
    debug_lib_deinit_cb_t debug_deinit_cb;
};
/************************************************
 *  Global variables
 ***********************************************/


/************************************************
 *  Function declarations
 ***********************************************/
void
mlag_main_deinit();

#endif
