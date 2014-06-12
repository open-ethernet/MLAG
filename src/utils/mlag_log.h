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

#ifndef MLAG_LOG_H_
#define MLAG_LOG_H_

#include <complib/sx_log.h>

/************************************************
 *  Defines
 ***********************************************/
/* Temporary using sx log as our logging utility
   In the future need to replace this with different
   logging mechanism */
#define MLAG_LOG_INIT(cb) sx_log_init(TRUE, NULL, cb)
#define MLAG_LOG_CLOSE() sx_log_close()
#define SX_LG(level, fmt, arg...)								\
	do {											\
		uint32_t __verbosity_level = 0;							\
		SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(level, __verbosity_level);			\
		if (LOG_VAR_NAME(__MODULE__) >= __verbosity_level) { 			 	\
			sx_log(level, QUOTEME(__MODULE__), "[%d]- %s: " fmt,			\
				 __LINE__, __FUNCTION__, ##arg);			\
		}										\
	} while (0)
#define MLAG_LOG(level, fmt, arg ...) SX_LOG(level, fmt, ## arg)

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
typedef sx_log_cb_t mlag_log_cb_t;

typedef enum mlag_verbosity_level {
    MLAG_VERBOSITY_LEVEL_NONE = SX_VERBOSITY_LEVEL_NONE,
    MLAG_VERBOSITY_LEVEL_ERROR = SX_VERBOSITY_LEVEL_ERROR,
    MLAG_VERBOSITY_LEVEL_WARNING = SX_VERBOSITY_LEVEL_WARNING,
    MLAG_VERBOSITY_LEVEL_NOTICE = SX_VERBOSITY_LEVEL_NOTICE,
    MLAG_VERBOSITY_LEVEL_INFO = SX_VERBOSITY_LEVEL_INFO,
    MLAG_VERBOSITY_LEVEL_DEBUG = SX_VERBOSITY_LEVEL_DEBUG,
    MLAG_VERBOSITY_LEVEL_FUNCS = SX_VERBOSITY_LEVEL_FUNCS,
    MLAG_VERBOSITY_LEVEL_ALL,

    MLAG_VERBOSITY_LEVEL_MIN = MLAG_VERBOSITY_LEVEL_NONE,
    MLAG_VERBOSITY_LEVEL_MAX = MLAG_VERBOSITY_LEVEL_ALL,
} mlag_verbosity_t;

enum mlag_log_levels {
    MLAG_LOG_NONE = SX_LOG_NONE,
    MLAG_LOG_ERROR = SX_LOG_ERROR,
    MLAG_LOG_WARNING = SX_LOG_WARNING,
    MLAG_LOG_NOTICE = SX_LOG_NOTICE,
    MLAG_LOG_INFO = SX_LOG_INFO,
    MLAG_LOG_DEBUG = SX_LOG_DEBUG,
    MLAG_LOG_FUNCS = SX_LOG_FUNCS
};
/************************************************
 *  Global variables
 ***********************************************/


/************************************************
 *  Function declarations
 ***********************************************/


#endif /* MLAG_LOG_H_ */
