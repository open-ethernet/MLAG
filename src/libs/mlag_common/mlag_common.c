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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <dlfcn.h>
#include <errno.h>
#include <complib/cl_init.h>
#include <complib/cl_timer.h>
#include <complib/cl_mem.h>
#include <complib/cl_thread.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include "lib_commu.h"
#include <utils/mlag_events.h>
#include <utils/mlag_defs.h>
#include <mlnx_lib/lib_event_disp.h>
#include "mlag_common.h"

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG_COMMON

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/


/************************************************
 *  Local variables
 ***********************************************/

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
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
int
register_events(event_disp_fds_t *event_fds,
                int high_prio_events[],
                int high_prio_num,
                int medium_prio_events[],
                int medium_prio_num,
                int low_prio_events[],
                int low_prio_num)
{
    event_disp_status_t event_status;
    int err = 0;
    int high_prio_event_deinit = MLAG_DEINIT_EVENT;

    event_status = event_disp_api_open(event_fds);
    MLAG_BAIL_ERROR_MSG(event_status,
                        "Failed to open event dispatcher library [%u]\n",
                        event_status);

    /* insert deinit_event automatically */
    event_status = event_disp_api_register_events(event_fds, HIGH_PRIO,
                                                  &high_prio_event_deinit, 1);

    if (event_status != EVENT_DISP_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to register high_prio_event_deinit event in events dispatcher [%u]\n",
                            event_status);
    }

    if (high_prio_num) {
        event_status = event_disp_api_register_events(event_fds, HIGH_PRIO,
                                                      high_prio_events,
                                                      high_prio_num);

        if (event_status != EVENT_DISP_STATUS_SUCCESS) {
            err = -EIO;
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to register high_prio events in events dispatcher [%u]\n",
                                event_status);
        }
    }

    if (medium_prio_num) {
        event_status = event_disp_api_register_events(event_fds, MED_PRIO,
                                                      medium_prio_events,
                                                      medium_prio_num);

        if (event_status != EVENT_DISP_STATUS_SUCCESS) {
            err = -EIO;
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to register med_prio events in events dispatcher [%u]\n",
                                event_status);
        }
    }

    if (low_prio_num) {
        event_status = event_disp_api_register_events(event_fds, LOW_PRIO,
                                                      low_prio_events,
                                                      low_prio_num);

        if (event_status != EVENT_DISP_STATUS_SUCCESS) {
            err = -EIO;
            MLAG_BAIL_ERROR_MSG(err,
                                "Failed to register low_prio events in events dispatcher [%u]\n",
                                event_status);
        }
    }

bail:
    return err;
}

/**
 *  This function closes event dispatcher registaration
 *
 * @param[in] event_fds - pointer to event FDs struct
 *
 * @return 0 if operation completes successfully.
 */
int
close_events(event_disp_fds_t *event_fds)
{
    event_disp_status_t event_status;
    int err = 0;

    event_status = event_disp_api_close(event_fds);
    if (event_status != EVENT_DISP_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to close event dispatcher API [%u]\n",
                            event_status);
    }

bail:
    return err;
}

/**
 *  This function send a system event
 *
 * @param[in] event_id - event ID
 * @param[in] data - pointer to data start
 * @param[in] data_size - data size in bytes
 *
 * @return 0 if operation completes successfully.
 * @return EIO for communication error
 */
int
send_system_event(int event_id, void *data, int data_size)
{
    event_disp_status_t event_status;
    int err = 0;

    event_status = event_disp_api_generate_event_no_copy(event_id, data,
                                                         data_size);
    if (event_status != EVENT_DISP_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_NOCHECK_MSG("Failed to generate event [%u] err [%u]\n",
                              event_id, event_status);
    }

bail:
    return err;
}

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
 * @return EIO if communication error on getting events
 * @return handler function error otherwise
 */
int
dispatcher_sys_event_handler(int fd, void *data, char *mlag_event,
                             int buf_size)
{
    int err = 0;
    event_disp_status_t event_status;
    handler_command_t cmd_data;
    cmd_db_handle_t *cmd_db_handle = NULL;

    ASSERT(data != NULL);
    cmd_db_handle = (cmd_db_handle_t *)data;

    event_status = event_disp_api_get_event_to_user_buf(fd, mlag_event,
                                                        buf_size);
    if (event_status != EVENT_DISP_STATUS_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR_MSG(err, "Failed to receive event err [%u]\n",
                            event_status);
    }

    MLAG_LOG(MLAG_LOG_DEBUG, "Received event [%d]\n",
             *(unsigned short*)mlag_event);

    err = get_command(cmd_db_handle, *(unsigned short*)mlag_event, &cmd_data);
    MLAG_BAIL_ERROR_MSG(err, "Command [%d] not found in cmd db\n",
                        *(unsigned short*)mlag_event);

    MLAG_LOG(MLAG_LOG_DEBUG, "Executing command : [%u] [%s]\n",
             cmd_data.cmd_id, cmd_data.name);
    if (cmd_data.func == NULL) {
        MLAG_LOG(MLAG_LOG_NOTICE, "Empty function\n");
        goto bail;
    }
    err = cmd_data.func((uint8_t *)mlag_event);
    if (err != -ECANCELED) {
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

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
void
dispatcher_thread_routine(void *data)
{
    fd_set input;
    int max_fd, i, err = 0;

    struct dispatcher_conf *fd_conf = (struct dispatcher_conf *)data;
    MLAG_LOG(MLAG_LOG_NOTICE, "Thread %s is running\n", fd_conf->name);

    while (1) {
        FD_ZERO(&input);
        max_fd = -1;
        for (i = 0; i < fd_conf->handlers_num; i++) {
            if (fd_conf->handler[i].fd) {
                FD_SET(fd_conf->handler[i].fd, &input);
            }
            if (fd_conf->handler[i].fd > max_fd) {
                max_fd = fd_conf->handler[i].fd;
            }
        }
        max_fd++;
        err = select(max_fd, &input, NULL, NULL, NULL);
        if (err < 0) {
            MLAG_LOG(MLAG_LOG_NOTICE,
                     "%s dispatcher: unexpected select time out [%d]\n",
                     fd_conf->name, err);
            continue;
        }
        for (i = 0; i < fd_conf->handlers_num; i++) {
            if (fd_conf->handler[i].fd &&
                FD_ISSET(fd_conf->handler[i].fd, &input)) {
                err = fd_conf->handler[i].fd_handler(
                    fd_conf->handler[i].fd,
                    fd_conf->handler[i].handler_data,
                    fd_conf->handler[i].msg_buf,
                    fd_conf->handler[i].buf_size);
                if (err == -ECANCELED) {
                    MLAG_LOG(MLAG_LOG_NOTICE, "%s dispatcher stopping\n",
                             fd_conf->name);
                    goto bail;
                }
                /*  strict priority with RR on same priorities */
                if (((i + 1) < fd_conf->handlers_num) &&
                    (fd_conf->handler[i].priority ==
                     fd_conf->handler[i + 1].priority)) {
                    continue;
                }
                break;
            }
        }
    }
bail:
    return;
}

/*
 *  This function gets a command from a command DB according to cmd_id
 *
 * @param[in] db_handle - command DB handle
 * @param[in] cmd_id - this is the key for searching the command DB
 * @param[in,out] cmd_data - command data struct
 *
 * @return 0 if operation completes successfully.
 */
int
get_command(cmd_db_handle_t *db_handle, int cmd_id,
            handler_command_t *cmd_data)
{
    int err = 0;
    cl_map_item_t *map_item_p = NULL;
    struct mapped_command *cmd;

    ASSERT(db_handle != NULL);

    cl_plock_acquire(&(db_handle->cmd_db_mutex));

    map_item_p = cl_qmap_get(&(db_handle->cmd_map), (uint64_t)cmd_id);

    if (map_item_p == cl_qmap_end(&(db_handle->cmd_map))) {
        err = -ENODATA;
        MLAG_BAIL_ERROR_MSG(err, "command %u not found in cmd_db\n",
                            cmd_id);
    }

    cmd = (struct mapped_command *)map_item_p;
    SAFE_MEMCPY(cmd_data, &(cmd->cmd));

bail:
    cl_plock_release(&(db_handle->cmd_db_mutex));
    return err;
}

/**
 *  This function Inits a command DB
 *
 * @param[in,out] db_handle - pointer that will be assigned with
 *       DB handle
 *
 * @return 0 if operation completes successfully.
 */
int
init_command_db(cmd_db_handle_t **db_handle)
{
    int err = 0;
    cl_status_t cl_err;
    handler_command_t deinit_command[] = {
        {MLAG_DEINIT_EVENT, "Deinit event", dispatch_deinit_event, NULL },
        {0, "", NULL, NULL}
    };

    ASSERT(db_handle != NULL);

    *db_handle = (cmd_db_handle_t *)cl_malloc(sizeof(cmd_db_handle_t));
    if (*db_handle == NULL) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR(err);
    }

    cl_qmap_init(&((*db_handle)->cmd_map));

    cl_err = cl_plock_init(&((*db_handle)->cmd_db_mutex));
    if (cl_err != CL_SUCCESS) {
        err = -EIO;
        MLAG_BAIL_ERROR(err);
    }

    /* insert deinit event automatically */
    err = insert_to_command_db(*db_handle, deinit_command);
    MLAG_BAIL_CHECK_NO_MSG(err);

bail:
    return err;
}

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
insert_to_command_db(cmd_db_handle_t *db_handle, handler_command_t commands[])
{
    int i = 0, err = 0;

    ASSERT(db_handle != NULL);

    /* Add commands to DB */
    while (commands[i].func != NULL) {
        set_command(db_handle, &commands[i]);
        i++;
    }

bail:
    return err;
}

/*
 *  This function will free all map items
 *  needed before cl_map is destroyed
 *
 * @param[in] map - map object that may contain items
 *
 * @return 0 if operation completes successfully.
 */
static int
free_cmd_map_items(cl_qmap_t *map)
{
    int err = 0;
    cl_status_t cl_err;
    cl_map_item_t   *map_item_p = NULL;
    cl_map_item_t   *next_map_item_p = NULL;

    next_map_item_p = cl_qmap_head(map);
    map_item_p = next_map_item_p;

    while (map_item_p != &map->nil) {
        next_map_item_p = cl_qmap_next(map_item_p);
        cl_err = cl_free(map_item_p);
        if (cl_err != CL_SUCCESS) {
            err = -ENOMEM;
            MLAG_BAIL_ERROR_MSG(err, "Error freeing cmd map items\n");
        }
        map_item_p = next_map_item_p;
    }

bail:
    return err;
}

/**
 *  This function terminates and clears the command db instance
 *
 * @param[in] db_handle - command DB handle
 *
 * @return 0 if operation completes successfully.
 */
int
deinit_command_db(cmd_db_handle_t *db_handle)
{
    cl_status_t cl_err;
    int err = 0;

    cl_plock_destroy(&(db_handle->cmd_db_mutex));
    err = free_cmd_map_items(&(db_handle->cmd_map));
    MLAG_BAIL_ERROR(err);

    cl_err = cl_free(db_handle);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        MLAG_BAIL_ERROR(err);
    }

bail:
    return err;
}

/**
 *  This function Inits a command DB and fills it with the given
 *  commands array
 *
 * @param[in] db_handle - command DB handle
 * @param[in] cmd_data - command data struct
 *
 * @return 0 if operation completes successfully.
 */
int
set_command(cmd_db_handle_t *db_handle, handler_command_t *cmd_data)
{
    struct mapped_command *cmd_2b_inserted;
    int err = 0;
    cl_map_item_t *map_item_p = NULL;

    cl_plock_excl_acquire(&(db_handle->cmd_db_mutex));

    map_item_p = cl_qmap_get(&(db_handle->cmd_map),
                             (uint64_t)(cmd_data->cmd_id));
    if (map_item_p == cl_qmap_end(&(db_handle->cmd_map))) {
        cmd_2b_inserted =
            (struct mapped_command *) cl_malloc(sizeof(struct mapped_command));
        if (cmd_2b_inserted == NULL) {
            err = -ENOMEM;
            MLAG_BAIL_ERROR(err);
        }
        SAFE_MEMCPY(&(cmd_2b_inserted->cmd), cmd_data);
        cl_qmap_insert(&(db_handle->cmd_map), (uint64_t) cmd_data->cmd_id,
                       (cl_map_item_t *)cmd_2b_inserted);
    }
    else {
        cmd_2b_inserted = (struct mapped_command *)map_item_p;
        SAFE_MEMCPY((&(cmd_2b_inserted->cmd)), (cmd_data));
    }
bail:
    cl_plock_release(&(db_handle->cmd_db_mutex));
    return err;
}

/*
 * Handle deinit event.
 *
 * @param[in] buffer - event data
 *
 * @return -ECANCELED - Operation completed successfully.
 */
int
dispatch_deinit_event(uint8_t *buffer)
{
    UNUSED_PARAM(buffer);
    int err = -ECANCELED;

    goto bail;

bail:
    return err;
}

