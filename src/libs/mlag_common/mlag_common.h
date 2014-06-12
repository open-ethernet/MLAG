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

#ifndef MLAG_COMMON_H_
#define MLAG_COMMON_H_

#include <mlnx_lib/lib_event_disp.h>
#include <complib/cl_map.h>
#include <complib/cl_passivelock.h>

/************************************************
 *  Defines
 ***********************************************/
#define DISPATCHER_NAME_MAX_CHARS   20
#define MAX_DISPATCHER_FD 10

#define HIGH_PRIORITY 0
#define MEDIUM_PRIORITY 1
#define LOW_PRIORITY 2

/************************************************
 *  Macros
 ***********************************************/
#define DISPATCHER_CONF_SET(db_name, index, u_fd, u_prio, u_handler, u_data, buf, size) \
    do { db_name.handler[index].fd = u_fd;                      \
         db_name.handler[index].priority = u_prio;              \
         db_name.handler[index].fd_handler = u_handler;         \
         db_name.handler[index].handler_data = u_data;          \
         db_name.handler[index].msg_buf = buf;                  \
         db_name.handler[index].buf_size = size;                \
    } while (0)

#define DISPATCHER_CONF_RESET(db_name, index)        \
    do { db_name.handler[index].fd = 0;              \
         db_name.handler[index].priority = 0;        \
         db_name.handler[index].fd_handler = NULL;   \
         db_name.handler[index].handler_data = NULL; \
         db_name.handler[index].msg_buf = NULL;      \
         db_name.handler[index].buf_size = 0;        \
    } while (0)

/************************************************
 *  Type definitions
 ***********************************************/
typedef int (*fd_handler_func) (int fd, void *data, char *buf, int buf_size);

struct dispatcher_handler {
    int fd;
    int priority;
    fd_handler_func fd_handler;
    void *handler_data;
    char *msg_buf;
    int buf_size;
};

struct dispatcher_conf {
    char name[DISPATCHER_NAME_MAX_CHARS];
    int handlers_num;
    struct dispatcher_handler handler[MAX_DISPATCHER_FD];
};

/**
 * Command DB
 */
typedef struct cmd_db_handle {
    cl_qmap_t cmd_map;
    cl_plock_t cmd_db_mutex;
} cmd_db_handle_t;

typedef int (*command_fp_t) (uint8_t *);
typedef int (*net_order_fp_t) (uint8_t *, int);

typedef struct handler_command {
    int cmd_id;                             /* enum representing command				*/
    char name[100];                         /* command name for logging purpose			*/
    command_fp_t func;                      /* func pointer								*/
    net_order_fp_t net_order_func;          /* pointer to network order handler			*/
} handler_command_t;

typedef int (*insert_to_command_db_func) (handler_command_t commands[]);

struct mapped_command {
    cl_map_item_t map_item;
    handler_command_t cmd;
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function open event dispatcher API and registers
 *  the given events to the priority queues
 *
 * @param[in] event_fds - pointer to event FDs struct
 * @param[in] high_prio_events - Array of high priority events
 * @param[in] high_prio_num - Number of events in the high
 *       priority events array
 * @param[in] medium_prio_events - Array of medium priority
 *       events
 * @param[in] medium_prio_num - Number of events in the medium
 *       priority events array
 * @param[in] low_prio_events - Array of low priority events
 * @param[in] low_prio_num - Number of events in the low
 *       priority events array
 *
 * @return 0 if operation completes successfully.
 */
int register_events(event_disp_fds_t *event_fds, int high_prio_events[],
                    int high_prio_num, int medium_prio_events[],
                    int medium_prio_num, int low_prio_events[],
                    int low_prio_num);


/**
 *  This function closes event dispatcher API
 *
 * @param[in] event_fds - pointer to event FDs struct
 *
 * @return 0 if operation completes successfully.
 */
int close_events(event_disp_fds_t *event_fds);

/**
 *  This function send a system event
 *
 * @param[in] event_id - event ID
 * @param[in] data - pointer to data start
 * @param[in] data_size - data size in bytes
 *
 * @return 0 if operation completes successfully.
 * @return -EIO for communication error
 */
int send_system_event(int event_id, void *data, int data_size);

/**
 *  this is a generic handler function for system events
 *  it gets as input a command DB (cmd_db_handle_t) pointer,
 *  which it uses for calling the function that matches the cmd
 *  id
 *
 * @param[in] fd - file descriptor that received the event
 * @param[in] data - pointer to configuration
 *
 * @return 0 if successful
 * @return -EIO if communication error on getting events
 * @return handler function error otherwise
 */
int dispatcher_sys_event_handler(int fd, void *data, char *mlag_event, int buf_size);

/**
 *  This is a implementation of dispatcher core activities
 *  the dispatcher is an infinite loop of reading events
 *  and applying actions when such arrive, all according to
 *  configuration.
 *  Note : this thread expects configuration of type struct
 *  dispatcher_conf
 *
 * @param[in] data - pointer to configuration
 *
 * @return void
 */
void dispatcher_thread_routine(void *data);

/**
 *  This function Inits a command DB and fills it with the given
 *  commands array
 *
 * @param[in,out] db_handle - pointer that will be assigned with
 *       DB handle
 *
 * @return 0 if operation completes successfully.
 */
int init_command_db(cmd_db_handle_t **db_handle);

/**
 *  This function fills command DB with the given
 *  commands array
 *
 * @param[in,out] db_handle - pointer that will be assigned with
 *       DB handle
 * @param[in] commands - array of commands, must have size of 1
 *       at least
 *
 * @return 0 if operation completes successfully.
 */
int
insert_to_command_db(cmd_db_handle_t *db_handle, handler_command_t commands[]);

/**
 *  This function terminates and clears the command db instance
 *
 * @param[in] db_handle - command DB handle
 *
 * @return 0 if operation completes successfully.
 */
int deinit_command_db(cmd_db_handle_t *db_handle);

/**
 *  This function Inits a command DB and fills it with the given
 *  commands array
 *
 * @param[in] db_handle - command DB handle
 * @param[in] cmd_data - command data struct
 *
 * @return 0 if operation completes successfully.
 */
int set_command(cmd_db_handle_t *db_handle, handler_command_t *cmd_data);

/*
 *  This function gets a command from a command DB according to cmd_id
 *
 * @param[in] db_handle - command DB handle
 * @param[in] cmd_id - this is the key for searching the command DB
 * @param[in,out] cmd_data - command data struct
 *
 * @return 0 if operation completes successfully.
 */
int get_command(cmd_db_handle_t *db_handle, int cmd_id,
                handler_command_t *cmd_data);

/*
 * Handle deinit event.
 *
 * @param[in] buffer - event data
 *
 * @return -ECANCELED - Operation completed successfully.
 */
int
dispatch_deinit_event(uint8_t *buffer);

#endif /* MLAG_COMMON_H_ */
