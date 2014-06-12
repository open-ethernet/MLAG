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

#ifndef MLAG_DEFS_H_
#define MLAG_DEFS_H_

#include <errno.h>
/************************************************
 *  Defines
 ***********************************************/
#define MLAG_MAX_PORTS          64
#define MLAG_MAX_PEERS          2
#define MLAG_MAX_IPLS           1
#define HEALTH_PEER_DOWN_WAIT_TIMER_MS 30000
#define RELOAD_DELAY_DEFAULT_MSEC   30000
#define MAX_ROUTER_MACS 64

/**
 * UDP port for heartbeat
 */
#define HEARTBEAT_PORT          51234
#define DEFAULT_HEARTBEAT_MSEC  1000

/**
 * TCP ports definition
 */
#define MLAG_DISPATCHER_TCP_PORT  51235
#define MAC_SYNC_TCP_PORT         51236
#define TUNNELING_PORT            51237

/**
 * Socket definitions
 */
#define MAX_DGRAM_QLEN         50000
#define RMEM_MAX_SOCK_BUF_SIZE 100000
#define WMEM_MAX_SOCK_BUF_SIZE 100000
#define MLAG_DISP_RMEM_SOCK_BUF_SIZE RMEM_MAX_SOCK_BUF_SIZE/10
#define MLAG_DISP_WMEM_SOCK_BUF_SIZE WMEM_MAX_SOCK_BUF_SIZE/10
#define MAC_SYNC_DISP_RMEM_SOCK_BUF_SIZE RMEM_MAX_SOCK_BUF_SIZE
#define MAC_SYNC_DISP_WMEM_SOCK_BUF_SIZE WMEM_MAX_SOCK_BUF_SIZE

/************************************************
 *  Macros
 ***********************************************/
#ifndef UNUSED_PARAM
#define UNUSED_PARAM(PARAM) ((void)(PARAM))
#endif

#define NUM_ELEMS(arr) (sizeof(arr) / sizeof(arr[0]))

#define SAFE_MEMCPY(dest, src)       memcpy(dest, src, sizeof(*(dest)))
#define SAFE_MEMSET(dest, val)       memset(dest, (val), sizeof(*(dest)))

#define NUM_ELEMS(arr)               (sizeof(arr) / sizeof(arr[0]))

#ifdef DEBUG
#define ASSERT(cond) do {if (!(cond)) { \
                             MLAG_LOG(MLAG_LOG_ERROR, "ASSERT %s. %s:%d\n", \
                                      #cond, __FILE__, __LINE__); \
                             *(volatile int *)0 = 1; \
                             goto bail;  } \
} while (0)
#else
#define ASSERT(cond) do {if (!(cond)) { \
                             MLAG_LOG(MLAG_LOG_ERROR, \
                                      "Unexpected error : %s\n", \
                                      #cond);  \
                             err = -EINVAL; \
                             goto bail; } \
} while (0)
#endif

#define DUMP_OR_LOG(str, arg ...)     \
    do {                              \
        if (dump_cb != NULL) {        \
            dump_cb(str, ## arg);     \
        } else {                      \
            MLAG_LOG(MLAG_LOG_NOTICE, \
                     str, ## arg);    \
        }                             \
    } while (0)

/************************************************
 *  Type definitions
 ***********************************************/
typedef enum sx_template_err {
	SX_TEMPLATE_SUCCESS = 0,
	SX_TEMPLATE_NULL_POINTER = 1,
	SX_TEMPLATE_UNEXPECTED_CASE = 2,
} sx_template_err_t;

typedef enum sx_template_enum {
	SX_TEMPLATE_MIN = 0, SX_TEMPLATE_MAX = 1,
} sx_template_enum_t;

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

#endif /* MLAG_DEFS_H_ */
