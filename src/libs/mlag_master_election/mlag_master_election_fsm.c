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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "mlag_log.h"
#include "mlag_bail.h"
#include "mlag_events.h"
#include "mlag_common.h"
#include "mlag_master_election.h"
#include "mlag_master_election_fsm.h"
#include "health_manager.h"
#include "mlag_manager_db.h"

#undef  __MODULE__
#define __MODULE__ MLAG_MASTER_ELECTION_FSM

/************************************************
 *  Local Defines
 ***********************************************/

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

static int is_master(struct mlag_master_election_fsm *fsm);
static int is_slave(struct mlag_master_election_fsm *fsm);
static int send_switch_status_change_event(mlag_master_election_fsm *fsm);
static int send_notification_message(struct mlag_master_election_fsm *fsm,
                                     int parameter,
                                     struct fsm_event_base *event);

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_master_election_fsm_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

/**
 *  This function gets module log verbosity level
 *
 * @return void
 */
mlag_verbosity_t
mlag_master_election_fsm_log_verbosity_get(void)
{
    return LOG_VAR_NAME(__MODULE__);
}

/************************************************
 *  Master Election FSM declarations
 ***********************************************/

#ifdef FSM_COMPILED

/*  declaration of FSM using pseudo language */

/* *********************************** */
stateMachine
{
    mlag_master_election_fsm()

/* *********************************** */

    events {
        start_ev()
        stop_ev()
        config_change_ev()
        peer_status_change_ev(int peer_id, int state)
    }

    state idle {
        default
        transitions {
            { start_ev, NULL, is_master, NULL }
        }
    }

    state master {
        transitions {
            { peer_status_change_ev, NULL, master, $ fsm->peer_status =
                  ev->state;
              $ }
        }
    }

    state slave {
        transitions {
            { peer_status_change_ev, $ ev->state == HEALTH_PEER_DOWN $,
              standalone, on_master_down() }
        }
        ef = send_notification_msg
    }

    state standalone {
        transitions {
            { peer_status_change_ev, NULL, is_master, $ fsm->peer_status =
                  ev->state;
              $ }
        }
        ef = send_notification_msg
    }

    state is_master {
        transitions {
            { NULL_EVENT, $ is_master(fsm) $, master,
              send_notification_message() }
            { NULL_EVENT, $ is_slave(fsm)  $, slave,
              NULL                        }
            { NULL_EVENT, $ else $, standalone, NULL                        }
                  }
                  ef = calc_switch_status
                       }

                       state mlag_enable {
                      transitions {
                          { stop_ev, NULL, idle, NULL }
                          { config_change_ev, NULL, is_master, NULL }
                      }
                      substates( master, slave, standalone, is_master )
                  }
                  } /* end of stateMachine */

#endif

/*#$*/
/* The code  inside placeholders  generated by tool and cannot be modifyed*/


/*----------------------------------------------------------------
            Generated enumerator for  Events of FSM
   ---------------------------------------------------------------*/
enum mlag_master_election_fsm_events_t {
    start_ev = 0,
    stop_ev = 1,
    config_change_ev = 2,
    peer_status_change_ev = 3,
};

/*----------------------------------------------------------------
            Generated structures for  Events of FSM
   ---------------------------------------------------------------*/
struct  mlag_master_election_fsm_start_ev_t {
    int opcode;
    const char *name;
};

struct  mlag_master_election_fsm_stop_ev_t {
    int opcode;
    const char *name;
};

struct  mlag_master_election_fsm_config_change_ev_t {
    int opcode;
    const char *name;
};

struct  mlag_master_election_fsm_peer_status_change_ev_t {
    int opcode;
    const char *name;
    int peer_id;
    int state;
};

/*----------------------------------------------------------------
           State entry/exit functions prototypes
   ---------------------------------------------------------------*/
static int slave_entry_func(mlag_master_election_fsm *fsm,
                            struct fsm_event_base *ev);
static int standalone_entry_func(mlag_master_election_fsm *fsm,
                                 struct fsm_event_base *ev);
static int is_master_entry_func(mlag_master_election_fsm *fsm,
                                struct fsm_event_base *ev);
/*----------------------------------------------------------------
            Generated functions for  Events of FSM
   ---------------------------------------------------------------*/
int
mlag_master_election_fsm_start_ev(struct mlag_master_election_fsm  * fsm)
{
    struct mlag_master_election_fsm_start_ev_t ev;
    ev.opcode = start_ev;
    ev.name = "start_ev";

    return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int
mlag_master_election_fsm_stop_ev(struct mlag_master_election_fsm  * fsm)
{
    struct mlag_master_election_fsm_stop_ev_t ev;
    ev.opcode = stop_ev;
    ev.name = "stop_ev";

    return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int
mlag_master_election_fsm_config_change_ev(
    struct mlag_master_election_fsm  * fsm)
{
    struct mlag_master_election_fsm_config_change_ev_t ev;
    ev.opcode = config_change_ev;
    ev.name = "config_change_ev";

    return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int
mlag_master_election_fsm_peer_status_change_ev(
    struct mlag_master_election_fsm  * fsm,  int peer_id,  int state)
{
    struct mlag_master_election_fsm_peer_status_change_ev_t ev;
    ev.opcode = peer_status_change_ev;
    ev.name = "peer_status_change_ev";
    ev.peer_id = peer_id;
    ev.state = state;

    return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
/*----------------------------------------------------------------
                 Reactions of FSM
   ---------------------------------------------------------------*/
static int on_master_down(mlag_master_election_fsm  * fsm, int parameter,
                          struct fsm_event_base *event);
static int send_notification_message(mlag_master_election_fsm  * fsm,
                                     int parameter,
                                     struct fsm_event_base *event);


/*----------------------------------------------------------------
           State Dispatcher function
   ---------------------------------------------------------------*/
static int mlag_master_election_fsm_state_dispatcher(
    mlag_master_election_fsm  * fsm, uint16 state, struct fsm_event_base *evt,
    struct fsm_state_base** target_state );
/*----------------------------------------------------------------
           Initialized  State classes of FSM
   ---------------------------------------------------------------*/
struct fsm_state_base state_mlag_master_election_fsm_idle;
struct fsm_state_base state_mlag_master_election_fsm_master;
struct fsm_state_base state_mlag_master_election_fsm_slave;
struct fsm_state_base state_mlag_master_election_fsm_standalone;
struct fsm_state_base state_mlag_master_election_fsm_is_master;
struct fsm_state_base state_mlag_master_election_fsm_mlag_enable;


struct fsm_state_base state_mlag_master_election_fsm_idle =
{ "idle", mlag_master_election_fsm_idle, SIMPLE, NULL, NULL,
  &mlag_master_election_fsm_state_dispatcher, NULL, NULL };

struct fsm_state_base state_mlag_master_election_fsm_master =
{ "master", mlag_master_election_fsm_master, SIMPLE,
  &state_mlag_master_election_fsm_mlag_enable, NULL,
  &mlag_master_election_fsm_state_dispatcher, NULL, NULL };

struct fsm_state_base state_mlag_master_election_fsm_slave =
{ "slave", mlag_master_election_fsm_slave, SIMPLE,
  &state_mlag_master_election_fsm_mlag_enable, NULL,
  &mlag_master_election_fsm_state_dispatcher, slave_entry_func, NULL };

struct fsm_state_base state_mlag_master_election_fsm_standalone =
{ "standalone", mlag_master_election_fsm_standalone, SIMPLE,
  &state_mlag_master_election_fsm_mlag_enable, NULL,
  &mlag_master_election_fsm_state_dispatcher, standalone_entry_func, NULL };

struct fsm_state_base state_mlag_master_election_fsm_is_master =
{ "is_master", mlag_master_election_fsm_is_master, CONDITION,
  &state_mlag_master_election_fsm_mlag_enable, NULL,
  &mlag_master_election_fsm_state_dispatcher, is_master_entry_func, NULL };

struct fsm_state_base state_mlag_master_election_fsm_mlag_enable =
{ "mlag_enable", mlag_master_election_fsm_mlag_enable,  COMPOSED, NULL,
  &state_mlag_master_election_fsm_master,
  &mlag_master_election_fsm_state_dispatcher, NULL, NULL };

static struct fsm_static_data static_data =
{0, 0, 0, 0, 0,
 { &state_mlag_master_election_fsm_idle,
   &state_mlag_master_election_fsm_master,
   &state_mlag_master_election_fsm_slave,
   &
   state_mlag_master_election_fsm_standalone,
   &state_mlag_master_election_fsm_is_master,
   &state_mlag_master_election_fsm_mlag_enable, }};

static struct fsm_state_base * default_state =
    &state_mlag_master_election_fsm_idle;
/*----------------------------------------------------------------
           StateDispatcher of FSM
   ---------------------------------------------------------------*/

static int
mlag_master_election_fsm_state_dispatcher( mlag_master_election_fsm  * fsm,
                                           uint16 state,
                                           struct fsm_event_base *evt,
                                           struct fsm_state_base** target_state )
{
    int err = 0;
    struct fsm_timer_event *event = (  struct fsm_timer_event * )evt;
    switch (state) {
    case mlag_master_election_fsm_idle:
        if (event->opcode == start_ev) {
            SET_EVENT(mlag_master_election_fsm, start_ev)
            { /*tr00:*/
              /* Source line = 115*/
                *target_state = &state_mlag_master_election_fsm_is_master;
                return err;
            }
        }

        break;

    case mlag_master_election_fsm_master:
        if (event->opcode == peer_status_change_ev) {
            SET_EVENT(mlag_master_election_fsm, peer_status_change_ev)
            { /*tr10:*/
                fsm->peer_status =
                    ev->state; /* Source line = 121*/
                *target_state = &state_mlag_master_election_fsm_master;
                return err;
            }
        }

        break;

    case mlag_master_election_fsm_slave:
        if (event->opcode == peer_status_change_ev) {
            SET_EVENT(mlag_master_election_fsm, peer_status_change_ev)
            if (ev->state == HEALTH_PEER_DOWN) { /*tr20:*/
                err = on_master_down( fsm, DEFAULT_PARAMS_D, evt); /* Source line = 129*/
                *target_state = &state_mlag_master_election_fsm_standalone;
                return err;
            }
        }

        break;

    case mlag_master_election_fsm_standalone:
        if (event->opcode == peer_status_change_ev) {
            SET_EVENT(mlag_master_election_fsm, peer_status_change_ev)
            { /*tr30:*/
                fsm->peer_status =
                    ev->state; /* Source line = 136*/
                *target_state = &state_mlag_master_election_fsm_is_master;
                return err;
            }
        }

        break;

    case mlag_master_election_fsm_is_master:
        if (is_master(fsm)) {     /*tr40:*/
            err = send_notification_message( fsm, DEFAULT_PARAMS_D, evt);     /* Source line = 144*/
            *target_state = &state_mlag_master_election_fsm_master;
            return err;
        }
        else if (is_slave(fsm)) {     /*tr41:*/
            /* Source line = 145*/
            *target_state = &state_mlag_master_election_fsm_slave;
            return err;
        }
        else {     /*tr42:*/
                   /* Source line = 146*/
            *target_state = &state_mlag_master_election_fsm_standalone;
            return err;
        }

        break;

    case mlag_master_election_fsm_mlag_enable:
        if (event->opcode == stop_ev) {
            SET_EVENT(mlag_master_election_fsm, stop_ev)
            { /*tr50:*/
              /* Source line = 153*/
                *target_state = &state_mlag_master_election_fsm_idle;
                return err;
            }
        }
        else if (event->opcode == config_change_ev) {
            SET_EVENT(mlag_master_election_fsm, config_change_ev)
            { /*tr51:*/
              /* Source line = 154*/
                *target_state = &state_mlag_master_election_fsm_is_master;
                return err;
            }
        }

        break;

    default:
        break;
    }
    return FSM_NOT_CONSUMED;
}
/*----------------------------------------------------------------
              Constructor of FSM
   ---------------------------------------------------------------*/
int
mlag_master_election_fsm_init(struct mlag_master_election_fsm *fsm,
                              fsm_user_trace user_trace, void * sched_func,
                              void * unsched_func)
{
    fsm_init(&fsm->base,  default_state, 6, NULL, user_trace, sched_func,
             unsched_func, &static_data);

    return 0;
}
/*  per state functions*/
/*----------------------------------------------------------------
              Getters for each State
   ---------------------------------------------------------------*/
tbool
mlag_master_election_fsm_idle_in(struct mlag_master_election_fsm *fsm)
{
    return fsm_is_in_state(&fsm->base, mlag_master_election_fsm_idle);
}
tbool
mlag_master_election_fsm_master_in(struct mlag_master_election_fsm *fsm)
{
    return fsm_is_in_state(&fsm->base, mlag_master_election_fsm_master);
}
tbool
mlag_master_election_fsm_slave_in(struct mlag_master_election_fsm *fsm)
{
    return fsm_is_in_state(&fsm->base, mlag_master_election_fsm_slave);
}
tbool
mlag_master_election_fsm_standalone_in(struct mlag_master_election_fsm *fsm)
{
    return fsm_is_in_state(&fsm->base, mlag_master_election_fsm_standalone);
}
tbool
mlag_master_election_fsm_is_master_in(struct mlag_master_election_fsm *fsm)
{
    return fsm_is_in_state(&fsm->base, mlag_master_election_fsm_is_master);
}
tbool
mlag_master_election_fsm_mlag_enable_in(struct mlag_master_election_fsm *fsm)
{
    return fsm_is_in_state(&fsm->base, mlag_master_election_fsm_mlag_enable);
}
/*----------------------------------------------------------------
                 Reactions of FSM  (within comments . user may paste function body outside placeholder region)
   ---------------------------------------------------------------*/

/*static  int on_master_down (mlag_master_election_fsm  * fsm, int parameter, struct fsm_event_base *event)
   {
   SET_EVENT(mlag_master_election_fsm , peer_status_change_ev) ;
   }
   static  int send_notification_message (mlag_master_election_fsm  * fsm, int parameter, struct fsm_event_base *event)
   {
   SET_EVENT(mlag_master_election_fsm , NULL_EVENT) ;
   }


 */
/*   11831*/

/*#$*/

/* Below code written by user */

/**
 *  This function sends switch status change event
 *
 *  @param [in] fsm - pointer to state machine data structure
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
send_switch_status_change_event(mlag_master_election_fsm *fsm)
{
    int err = 0;
    struct switch_status_change_event_data ev;

    fsm_trace((struct fsm_base *)fsm,
              "Enter to send_switch_status_change_event: curr=%d, prev=%d\n",
              fsm->current_status, fsm->previous_status);

    /* Set my peer id into Mlag manager db */
    if (fsm->my_ip_addr || (fsm->current_status == STANDALONE)) {
        err = mlag_manager_db_mlag_peer_id_set(fsm->my_ip_addr,
                                               (fsm->current_status == SLAVE)
                                               ? SLAVE_PEER_ID : MASTER_PEER_ID);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to set mlag peer id of local peer to mlag manager database\n");
    }

    /* Set peer id into Mlag manager db */
    if (fsm->peer_ip_addr) {
        err = mlag_manager_db_mlag_peer_id_set(fsm->peer_ip_addr,
                                               (fsm->current_status == MASTER)
                                               ? SLAVE_PEER_ID : MASTER_PEER_ID);
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed to set mlag peer id of remote peer to mlag manager database\n");

        fsm_trace((struct fsm_base *)fsm,
                  "Called mlag_manager_db_mlag_peer_id_set: peer_ip_addr=0x%08x, peer_id=%s\n",
                  fsm->peer_ip_addr,
                  (fsm->current_status ==
                   MASTER) ? "SLAVE_PEER_ID" : "MASTER_PEER_ID");
    }

    /* If switch status changed send notification message to registered modules */
    if (fsm->current_status != fsm->previous_status) {
        fsm_trace((struct fsm_base *)fsm,
                  "Sending switch status change event\n");
        ev.current_status = fsm->current_status;
        ev.previous_status = fsm->previous_status;
        ev.my_ip = fsm->my_ip_addr;
        ev.peer_ip = fsm->peer_ip_addr;
        ev.my_peer_id = fsm->my_peer_id;
        ev.master_peer_id = fsm->master_peer_id;

        err = send_system_event(
            MLAG_MASTER_ELECTION_SWITCH_STATUS_CHANGE_EVENT,
            &ev, sizeof(ev));
        MLAG_BAIL_ERROR_MSG(err,
                            "Failed in sending switch status change event\n");
    }
    mlag_master_election_inc_cnt(SWITCH_STATUS_CHANGE_EVENTS_SENT);
    fsm_trace((struct fsm_base *)fsm,
              "Exit from send_switch_status_change_event: curr=%d, prev=%d\n",
              fsm->current_status, fsm->previous_status);

bail:
    return err;
}

/**
 *  This function is an action on entry in IsMaster state
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] ev  - pointer to event triggered the state machine
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
is_master_entry_func(mlag_master_election_fsm *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    UNUSED_PARAM(ev);

    fsm_trace((struct fsm_base *)fsm,
              "Enter to is_master_entry_func, curr=%d, prev=%d\n",
              fsm->current_status, fsm->previous_status);

    if ((fsm->my_ip_addr != 0) &&
        (fsm->peer_ip_addr != 0)) {
        if (fsm->peer_status == HEALTH_PEER_DOWN) {
            /* peer down */
            if (fsm->current_status == MASTER) {
                /* already MASTER */
                fsm->previous_status = MASTER;
                fsm->master_peer_id = MASTER_PEER_ID;
                fsm->my_peer_id = MASTER_PEER_ID;
            }
            else if (fsm->current_status == SLAVE) {
                fsm->current_status = STANDALONE;
                fsm->previous_status = SLAVE;
                fsm->master_peer_id = MASTER_PEER_ID;
                fsm->my_peer_id = MASTER_PEER_ID;
            }
            else if (fsm->current_status == STANDALONE) {
                /* already STANDALONE */
                fsm->previous_status = STANDALONE;
                fsm->master_peer_id = MASTER_PEER_ID;
                fsm->my_peer_id = MASTER_PEER_ID;
            }
        }
        else {
            /* Can choose master */
            if (fsm->my_ip_addr < fsm->peer_ip_addr) {
                fsm->previous_status = fsm->current_status;
                fsm->current_status = SLAVE;
                fsm->master_peer_id = MASTER_PEER_ID;
                fsm->my_peer_id = SLAVE_PEER_ID;
            }
            else if (fsm->my_ip_addr > fsm->peer_ip_addr) {
                fsm->previous_status = fsm->current_status;
                fsm->current_status = MASTER;
                fsm->master_peer_id = MASTER_PEER_ID;
                fsm->my_peer_id = MASTER_PEER_ID;
            }
            else {
                /* the same ip addr */
                MLAG_LOG(MLAG_LOG_ERROR,
                         "Local ip address is the same with peer ip address: 0x%08x\n",
                         fsm->my_ip_addr);
            }
        }
    }
    else {
        fsm->previous_status = fsm->current_status;
        fsm->current_status = STANDALONE;
        fsm->master_peer_id = MASTER_PEER_ID;
        fsm->my_peer_id = MASTER_PEER_ID;
    }
    fsm_trace((struct fsm_base *)fsm,
              "Exit from is_master_entry_func, curr=%d, prev=%d\n",
              fsm->current_status, fsm->previous_status);
    return err;
}

/**
 *  This function is an action on entry in Slave state
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] ev  - pointer to event triggered the state machine
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
slave_entry_func(mlag_master_election_fsm *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    UNUSED_PARAM(ev);
    err = send_switch_status_change_event(fsm);
    MLAG_BAIL_ERROR(err);
bail:
    return err;
}

/**
 *  This function is an action on entry in Standalone state
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] ev  - pointer to event triggered the state machine
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
standalone_entry_func(mlag_master_election_fsm *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    UNUSED_PARAM(ev);
    err = send_switch_status_change_event(fsm);
    MLAG_BAIL_ERROR(err);
bail:
    return err;
}

/**
 *  This function checks if current state is MASTER
 *  in IsMaster condition state
 *
 *  @param [in] fsm - pointer to state machine data structure
 *
 * @return 1 Master, otherwise 0
 */
static int
is_master(struct mlag_master_election_fsm *fsm)
{
    return (fsm->current_status == MASTER);
}

/**
 *  This function checks if current state is SLAVE
 *  in IsMaster condition state
 *
 *  @param [in] fsm - pointer to state machine data structure
 *
 * @return 1 for Slave, otherwise 0
 */
static int
is_slave(struct mlag_master_election_fsm *fsm)
{
    return (fsm->current_status == SLAVE);
}

/**
 *  This function is reaction on master down event
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] parameter - can be used to distinguish between different
 *                          reactions when used single function, not used here
 *  @param [in] ev  - pointer to event triggered the state machine
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
on_master_down(struct mlag_master_election_fsm *fsm, int parameter,
               struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(parameter);
    SET_EVENT(mlag_master_election_fsm, peer_status_change_ev);
    UNUSED_PARAM(ev);
    fsm->current_status = STANDALONE;
    fsm->previous_status = SLAVE;
    fsm->peer_status = HEALTH_PEER_DOWN;
    fsm->master_peer_id = MASTER_PEER_ID;
    fsm->my_peer_id = MASTER_PEER_ID;
    return err;
}

/**
 *  This function is reaction on transition from is_master state to master state
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] parameter - can be used to distinguish between different
 *                          reactions when used single function, not used here
 *  @param [in] ev  - pointer to event triggered the state machine
 *
 * @return 0 when successful, otherwise ERROR
 */
static int
send_notification_message(struct mlag_master_election_fsm *fsm, int parameter,
                          struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(parameter);
    UNUSED_PARAM(event);
    err = send_switch_status_change_event(fsm);
    MLAG_BAIL_ERROR(err);
bail:
    return err;
}

/**
 *  This function prints master election fsm data
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int
mlag_master_election_fsm_print(struct mlag_master_election_fsm *fsm,
                               void (*dump_cb)(const char *, ...))
{
    int err = 0;
    char *current_status_str;
    char *previous_switch_str;

    err = mlag_master_election_get_switch_status_str(fsm->current_status,
                                                     &current_status_str);
    MLAG_BAIL_ERROR(err);
    err = mlag_master_election_get_switch_status_str(fsm->previous_status,
                                                     &previous_switch_str);
    MLAG_BAIL_ERROR(err);

    if (dump_cb == NULL) {
        MLAG_LOG(MLAG_LOG_INFO, "FSM: "
                 "Master Election FSM internal attributes:\n");
        MLAG_LOG(MLAG_LOG_INFO, "FSM: "
                 "current_status = %s, previous_status = %s\n",
                 current_status_str, previous_switch_str);
        MLAG_LOG(MLAG_LOG_INFO, "FSM: "
                 "local ip address = 0x%08x, peer ip address = ox%08x\n",
                 fsm->my_ip_addr, fsm->peer_ip_addr);
        MLAG_LOG(MLAG_LOG_INFO, "FSM: "
                 "peer_status = %s\n",
                 (fsm->peer_status == HEALTH_PEER_DOWN) ? "DOWN" : "UP");
        MLAG_LOG(MLAG_LOG_INFO, "FSM: "
                 "local_peer_id = %d, master_peer_id = %d\n",
                 fsm->my_peer_id, fsm->master_peer_id);
        fsm_print((struct fsm_base *)fsm);
        MLAG_LOG(MLAG_LOG_INFO, "FSM: "
                 "End of Master Election FSM internal attributes\n");
    }
    else {
        dump_cb("FSM: "
                "Master Election FSM internal attributes:\n");
        dump_cb("FSM: "
                "current_status = %s, previous_status = %s\n",
                current_status_str, previous_switch_str);
        dump_cb("FSM: "
                "local ip address = 0x%08x, peer ip address = ox%08x\n",
                fsm->my_ip_addr, fsm->peer_ip_addr);
        dump_cb("FSM: "
                "peer_status = %s\n",
                (fsm->peer_status == HEALTH_PEER_DOWN) ? "DOWN" : "UP");
        dump_cb("FSM: "
                "local_peer_id = %d, master_peer_id = %d\n",
                fsm->my_peer_id, fsm->master_peer_id);
        dump_cb("FSM: "
                "End of Master Election FSM internal attributes\n");
    }

bail:
    return 0;
}
