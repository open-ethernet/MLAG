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

#ifndef MLAG_BAIL_H_
#define MLAG_BAIL_H_

#include "mlag_log.h"
#include <errno.h>

/************************************************
 *  Defines
 ***********************************************/

#define MLAG_BAIL_ERROR(_error_)                                            \
    if (_error_) {                                                          \
        MLAG_LOG(MLAG_LOG_ERROR, "Error code %d returned\n", _error_);        \
        goto bail;                                                          \
    }                                                                       \

#define MLAG_BAIL_ERROR_MSG(_error_, format, ...)                           \
    if (_error_) {                                                          \
        MLAG_LOG(MLAG_LOG_ERROR, format, ## __VA_ARGS__);                     \
        goto bail;                                                          \
    }

#define MLAG_BAIL_NOCHECK_MSG(format, ...)                                  \
    MLAG_LOG(MLAG_LOG_ERROR, format, ## __VA_ARGS__);                         \
    goto bail;                                                              \


#define MLAG_BAIL_CHECK_NO_MSG(_error_)                                     \
    if (_error_) {                                                          \
        goto bail;                                                          \
    }                                                                       \

#define MLAG_BAIL_CHECK(cond, error_value) \
    do {if (!(cond)) { \
            MLAG_LOG(MLAG_LOG_ERROR, \
                     "Validation error : %s\n", \
                     #cond);  \
            err = error_value; \
            goto bail; } \
    } while (0) \

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

#endif
