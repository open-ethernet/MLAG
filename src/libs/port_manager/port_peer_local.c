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
#include <netinet/in.h>
#include <mlag_api_defs.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/mlag_topology/mlag_topology.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include "port_manager.h"
#include <libs/service_layer/service_layer.h>
#include "port_peer_local.h"

/************************************************
 *  Local Defines
 ***********************************************/
#define UP MLAG_PORT_OPER_UP
#define DOWN MLAG_PORT_OPER_DOWN

#undef  __MODULE__
#define __MODULE__ PORT_MANAGER

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

#ifdef FSM_COMPILED
#include \


stateMachine
{
port_peer_local ()

events {
	port_add_ev     ()
	port_del_ev     ()
	port_global_en  ()
	port_global_dis ()
	port_global_down()
	port_global_up  ()
	port_down_ev    ()
    port_up_ev      ()

}

state idle {
default
 transitions {
  { port_add_ev     , NULL  , global_down   , on_port_add()  }
  { port_global_dis , NULL  , IN_STATE      , on_port_disable() }
 }
}
// ***********************************
state global_down {
 transitions {
 { port_del_ev      , NULL                          , idle          , on_port_del()             }
 { port_global_en   , $ fsm->oper_state == DOWN $   , local_fault   , on_port_enable()          }
 { port_global_en   , $ else $                      , local_up      , on_port_enable()          }
 { port_global_up   , $ fsm->oper_state == DOWN $   , local_fault   , NULL                      }
 { port_global_up   , $ else $                      , local_up      , NULL                      }
 { port_global_dis  , NULL                          , IN_STATE      , on_port_disable()         }
 { port_global_down , NULL                          , IN_STATE      , NULL                      }
 { port_up_ev       , NULL                          , IN_STATE      , notify_oper_state(UP)     }
 { port_down_ev     , NULL                          , IN_STATE      , notify_oper_state(DOWN)   }
 }
}
// ***********************************

state local_fault {
 transitions {
 { port_del_ev      , NULL                      , idle          , on_port_del()             }
 { port_global_dis  , NULL                      , global_down   , on_port_disable()         }
 { port_global_down , NULL                      , global_down   , NULL                      }
 { port_global_en   , NULL                      , IN_STATE      , NULL                      }
 { port_global_up   , NULL                      , IN_STATE      , NULL                      }
 { port_up_ev       , NULL                      , local_up      , notify_oper_state(UP)     }
 { port_down_ev     , NULL                      , IN_STATE      , notify_oper_state(DOWN)   }
 }
 ef = local_fault_entry
 xf = local_fault_exit
}
// ***********************************
state local_up {
  transitions {
 { port_del_ev      , NULL                      , idle          , on_port_del()             }
 { port_global_dis  , NULL                      , global_down   , on_port_disable()         }
 { port_global_en   , NULL                      , IN_STATE      , NULL                      }
 { port_global_up   , NULL                      , IN_STATE      , NULL                      }
 { port_up_ev       , NULL                      , IN_STATE      , notify_oper_state(UP)     }
 { port_down_ev     , NULL                      , local_fault   , notify_oper_state(DOWN)   }

 }

}

}// end of stateMachine


/* *********************************** */
#endif

/*#$*/
 /* The code  inside placeholders  generated by tool and cannot be modifyed*/


/*----------------------------------------------------------------
            Generated enumerator for  Events of FSM
---------------------------------------------------------------*/
enum port_peer_local_events_t{
    port_add_ev  = 0,
    port_del_ev  = 1,
    port_global_en  = 2,
    port_global_dis  = 3,
    port_global_down  = 4,
    port_global_up  = 5,
    port_down_ev  = 6,
    port_up_ev  = 7,
 };

/*----------------------------------------------------------------
            Generated structures for  Events of FSM
---------------------------------------------------------------*/
struct  port_peer_local_port_add_ev_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_del_ev_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_global_en_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_global_dis_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_global_down_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_global_up_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_down_ev_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_local_port_up_ev_t
{
    int  opcode;
    const char *name;
};

/*----------------------------------------------------------------
           State entry/exit functions prototypes
---------------------------------------------------------------*/
static int local_fault_entry_func(port_peer_local *fsm, struct fsm_event_base *ev);
static int local_fault_exit_func(port_peer_local *fsm, struct fsm_event_base *ev);
/*----------------------------------------------------------------
            Generated functions for  Events of FSM
---------------------------------------------------------------*/
int   port_peer_local_port_add_ev(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_add_ev_t ev;
     ev.opcode  =  port_add_ev ;
     ev.name    = "port_add_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_del_ev(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_del_ev_t ev;
     ev.opcode  =  port_del_ev ;
     ev.name    = "port_del_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_global_en(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_global_en_t ev;
     ev.opcode  =  port_global_en ;
     ev.name    = "port_global_en" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_global_dis(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_global_dis_t ev;
     ev.opcode  =  port_global_dis ;
     ev.name    = "port_global_dis" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_global_down(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_global_down_t ev;
     ev.opcode  =  port_global_down ;
     ev.name    = "port_global_down" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_global_up(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_global_up_t ev;
     ev.opcode  =  port_global_up ;
     ev.name    = "port_global_up" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_down_ev(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_down_ev_t ev;
     ev.opcode  =  port_down_ev ;
     ev.name    = "port_down_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_local_port_up_ev(struct port_peer_local  * fsm)
{
     struct port_peer_local_port_up_ev_t ev;
     ev.opcode  =  port_up_ev ;
     ev.name    = "port_up_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
/*----------------------------------------------------------------
                 Reactions of FSM
---------------------------------------------------------------*/
static  int on_port_add (port_peer_local  * fsm, int parameter, struct fsm_event_base *event);
static  int on_port_disable (port_peer_local  * fsm, int parameter, struct fsm_event_base *event);
static  int on_port_del (port_peer_local  * fsm, int parameter, struct fsm_event_base *event);
static  int on_port_enable (port_peer_local  * fsm, int parameter, struct fsm_event_base *event);
static  int notify_oper_state (port_peer_local  * fsm, int parameter, struct fsm_event_base *event);


/*----------------------------------------------------------------
           State Dispatcher function
---------------------------------------------------------------*/
static int port_peer_local_state_dispatcher( port_peer_local  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state );
/*----------------------------------------------------------------
           Initialized  State classes of FSM
---------------------------------------------------------------*/
 struct fsm_state_base            state_port_peer_local_idle;
 struct fsm_state_base            state_port_peer_local_global_down;
 struct fsm_state_base            state_port_peer_local_local_fault;
 struct fsm_state_base            state_port_peer_local_local_up;


 struct fsm_state_base      state_port_peer_local_idle ={ "idle", port_peer_local_idle , SIMPLE, NULL, NULL, &port_peer_local_state_dispatcher, NULL, NULL };

 struct fsm_state_base      state_port_peer_local_global_down ={ "global_down", port_peer_local_global_down , SIMPLE, NULL, NULL, &port_peer_local_state_dispatcher, NULL, NULL };

 struct fsm_state_base      state_port_peer_local_local_fault ={ "local_fault", port_peer_local_local_fault , SIMPLE, NULL, NULL, &port_peer_local_state_dispatcher, local_fault_entry_func , local_fault_exit_func };

 struct fsm_state_base      state_port_peer_local_local_up ={ "local_up", port_peer_local_local_up , SIMPLE, NULL, NULL, &port_peer_local_state_dispatcher, NULL, NULL };

static struct fsm_static_data static_data= {0,0,0,0,0 ,{ &state_port_peer_local_idle,  &state_port_peer_local_global_down,  &state_port_peer_local_local_fault,
                                                  &state_port_peer_local_local_up, }
};

 static struct fsm_state_base * default_state = &state_port_peer_local_idle ;
/*----------------------------------------------------------------
           StateDispatcher of FSM
---------------------------------------------------------------*/

static int port_peer_local_state_dispatcher( port_peer_local  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state )
{
  int err=0;
  struct fsm_timer_event *event = (  struct fsm_timer_event * )evt;
  switch(state)
  {

   case port_peer_local_idle :
    {
      if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_peer_local , port_add_ev)
         {/*tr00:*/
            err = on_port_add( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 64*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_dis )
      {
         SET_EVENT(port_peer_local , port_global_dis)
         {/*tr01:*/
           fsm->base.reaction_in_state = 1;
           err = on_port_disable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 65*/
            *target_state =  &state_port_peer_local_idle;
            return err;
         }
      }

    }break;

   case port_peer_local_global_down :
    {
      if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_peer_local , port_del_ev)
         {/*tr10:*/
            err = on_port_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 71*/
            *target_state =  &state_port_peer_local_idle;
            return err;
         }
      }

      else if ( event->opcode == port_global_en )
      {
         SET_EVENT(port_peer_local , port_global_en)
         if(  fsm->oper_state == DOWN  )
         {/*tr11:*/
            err = on_port_enable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 72*/
            *target_state =  &state_port_peer_local_local_fault;
            return err;
         }
         else
         {/*tr12:*/
            err = on_port_enable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 73*/
            *target_state =  &state_port_peer_local_local_up;
            return err;
         }
      }

      else if ( event->opcode == port_global_up )
      {
         SET_EVENT(port_peer_local , port_global_up)
         if(  fsm->oper_state == DOWN  )
         {/*tr13:*/
              /* Source line = 74*/
            *target_state =  &state_port_peer_local_local_fault;
            return err;
         }
         else
         {/*tr14:*/
              /* Source line = 75*/
            *target_state =  &state_port_peer_local_local_up;
            return err;
         }
      }

      else if ( event->opcode == port_global_dis )
      {
         SET_EVENT(port_peer_local , port_global_dis)
         {/*tr15:*/
           fsm->base.reaction_in_state = 1;
           err = on_port_disable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 76*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_down )
      {
         SET_EVENT(port_peer_local , port_global_down)
         {/*tr16:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 77*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_up_ev )
      {
         SET_EVENT(port_peer_local , port_up_ev)
         {/*tr17:*/
           fsm->base.reaction_in_state = 1;
           err = notify_oper_state( fsm, UP, evt);  /* Source line = 78*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_down_ev )
      {
         SET_EVENT(port_peer_local , port_down_ev)
         {/*tr18:*/
           fsm->base.reaction_in_state = 1;
           err = notify_oper_state( fsm, DOWN, evt);  /* Source line = 79*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

    }break;

   case port_peer_local_local_fault :
    {
      if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_peer_local , port_del_ev)
         {/*tr20:*/
            err = on_port_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 86*/
            *target_state =  &state_port_peer_local_idle;
            return err;
         }
      }

      else if ( event->opcode == port_global_dis )
      {
         SET_EVENT(port_peer_local , port_global_dis)
         {/*tr21:*/
            err = on_port_disable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 87*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_down )
      {
         SET_EVENT(port_peer_local , port_global_down)
         {/*tr22:*/
              /* Source line = 88*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_en )
      {
         SET_EVENT(port_peer_local , port_global_en)
         {/*tr23:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 89*/
            *target_state =  &state_port_peer_local_local_fault;
            return err;
         }
      }

      else if ( event->opcode == port_global_up )
      {
         SET_EVENT(port_peer_local , port_global_up)
         {/*tr24:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 90*/
            *target_state =  &state_port_peer_local_local_fault;
            return err;
         }
      }

      else if ( event->opcode == port_up_ev )
      {
         SET_EVENT(port_peer_local , port_up_ev)
         {/*tr25:*/
            err = notify_oper_state( fsm, UP, evt);  /* Source line = 91*/
            *target_state =  &state_port_peer_local_local_up;
            return err;
         }
      }

      else if ( event->opcode == port_down_ev )
      {
         SET_EVENT(port_peer_local , port_down_ev)
         {/*tr26:*/
           fsm->base.reaction_in_state = 1;
           err = notify_oper_state( fsm, DOWN, evt);  /* Source line = 92*/
            *target_state =  &state_port_peer_local_local_fault;
            return err;
         }
      }

    }break;

   case port_peer_local_local_up :
    {
      if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_peer_local , port_del_ev)
         {/*tr30:*/
            err = on_port_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 100*/
            *target_state =  &state_port_peer_local_idle;
            return err;
         }
      }

      else if ( event->opcode == port_global_dis )
      {
         SET_EVENT(port_peer_local , port_global_dis)
         {/*tr31:*/
            err = on_port_disable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 101*/
            *target_state =  &state_port_peer_local_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_en )
      {
         SET_EVENT(port_peer_local , port_global_en)
         {/*tr32:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 102*/
            *target_state =  &state_port_peer_local_local_up;
            return err;
         }
      }

      else if ( event->opcode == port_global_up )
      {
         SET_EVENT(port_peer_local , port_global_up)
         {/*tr33:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 103*/
            *target_state =  &state_port_peer_local_local_up;
            return err;
         }
      }

      else if ( event->opcode == port_up_ev )
      {
         SET_EVENT(port_peer_local , port_up_ev)
         {/*tr34:*/
           fsm->base.reaction_in_state = 1;
           err = notify_oper_state( fsm, UP, evt);  /* Source line = 104*/
            *target_state =  &state_port_peer_local_local_up;
            return err;
         }
      }

      else if ( event->opcode == port_down_ev )
      {
         SET_EVENT(port_peer_local , port_down_ev)
         {/*tr35:*/
            err = notify_oper_state( fsm, DOWN, evt);  /* Source line = 105*/
            *target_state =  &state_port_peer_local_local_fault;
            return err;
         }
      }

    }break;

   default:
   break;

  }
  return FSM_NOT_CONSUMED;

}
/*----------------------------------------------------------------
              Constructor of FSM
---------------------------------------------------------------*/
int port_peer_local_init(struct port_peer_local *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func)
 {

       fsm_init(&fsm->base,  default_state, 4, NULL, user_trace, sched_func, unsched_func, &static_data);

      return 0;
 }
 /*  per state functions*/
/*----------------------------------------------------------------
              Getters for each State
---------------------------------------------------------------*/
tbool port_peer_local_idle_in(struct port_peer_local *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_local_idle);
}
tbool port_peer_local_global_down_in(struct port_peer_local *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_local_global_down);
}
tbool port_peer_local_local_fault_in(struct port_peer_local *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_local_local_fault);
}
tbool port_peer_local_local_up_in(struct port_peer_local *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_local_local_up);
}
/*----------------------------------------------------------------
                 Reactions of FSM  (within comments . user may paste function body outside placeholder region)
---------------------------------------------------------------*/

/*static  int on_port_add (port_peer_local  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_local , port_add_ev) ;
 }
static  int on_port_disable (port_peer_local  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_local , port_global_dis) ;
 }
static  int on_port_del (port_peer_local  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_local , port_del_ev) ;
 }
static  int on_port_enable (port_peer_local  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_local , port_global_en) ;
 }
static  int notify_oper_state (port_peer_local  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_local , port_up_ev) ;
 }


*/
/*   16982*/

/*#$*/

/* Below code written by user */

/*
 * This function is called when entering local fault state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
local_fault_entry_func(port_peer_local *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    unsigned long ipl_port_id;
    unsigned int ipl_id;

    UNUSED_PARAM(ev);

    err = mlag_topology_redirect_ipl_id_get(&ipl_id, &ipl_port_id);
    if (err == -EINVAL) {
        /* IPL not defined yet, ignore */
        err = 0;
        goto bail;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err, "Failed in getting redirect IPL id\n");
    }

    MLAG_LOG(MLAG_LOG_NOTICE,
             "Activate redirect port [%lu] to ipl [%u] ipl id [%lu]\n",
             fsm->port_id, ipl_id, ipl_port_id);

    err = sl_api_port_redirect_set(OES_ACCESS_CMD_CREATE, fsm->port_id,
                                   ipl_port_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to redirect port [%lu] to ipl [%u]\n",
                        fsm->port_id, ipl_id);

bail:
    return err;
}

/*
 * This function is called when exiting local fault state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
local_fault_exit_func(port_peer_local *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    unsigned long ipl_port_id;
    unsigned int ipl_id;

    UNUSED_PARAM(ev);

    MLAG_LOG(MLAG_LOG_NOTICE, "Remove redirect port [%lu]\n", fsm->port_id);

    err = mlag_topology_redirect_ipl_id_get(&ipl_id, &ipl_port_id);
    if (err == -EINVAL) {
        /* IPL not defined yet, ignore */
        goto bail;
    }
    else {
        MLAG_BAIL_ERROR_MSG(err, "Failed in getting redirect IPL id\n");
    }

    MLAG_LOG(MLAG_LOG_DEBUG,
             "Remove redirect port [%lu] to ipl [%u] ipl id [%lu]\n",
             fsm->port_id, ipl_id, ipl_port_id);

    err = sl_api_port_redirect_set(OES_ACCESS_CMD_DESTROY, fsm->port_id,
                                   ipl_port_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to remove redirect port [%lu] ipl [%u]\n",
                        fsm->port_id, ipl_id);

bail:
    return err;
}

/*
 * This function is called as reaction to port add
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_port_add(port_peer_local *fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_peer_local, port_add_ev);
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    UNUSED_PARAM(fsm);
    fsm->admin_state = PORT_DISABLED;
    return err;
}

/*
 * This function is called as reaction to port delete
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_port_del(port_peer_local  * fsm, int parameter,
            struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_peer_local, port_del_ev);
    UNUSED_PARAM(fsm);
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    return err;
}

/*
 * This function is called as reaction to port disable
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_port_disable(port_peer_local  * fsm, int parameter,
                struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_peer_local, port_global_dis);
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);

    MLAG_LOG(MLAG_LOG_NOTICE, "port [%lu] Disable\n", fsm->port_id);
    if (fsm->admin_state == PORT_ENABLED) {
        err = sl_api_port_state_set(fsm->port_id, OES_PORT_ADMIN_DISABLE);
        MLAG_BAIL_ERROR_MSG(err, "Failed admin disable port [%lu] err [%d]\n",
                            fsm->port_id, err);
    }
    fsm->admin_state = PORT_DISABLED;

bail:
    return err;
}

/*
 * This function is called as reaction to port disable
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_port_enable(port_peer_local  * fsm, int parameter,
               struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_peer_local, port_global_dis);
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);

    MLAG_LOG(MLAG_LOG_NOTICE, "port [%lu] Enable\n", fsm->port_id);
    if (fsm->admin_state == PORT_DISABLED) {
        err = sl_api_port_state_set(fsm->port_id, OES_PORT_ADMIN_ENABLE);
        MLAG_BAIL_ERROR_MSG(err, "Failed admin enable port [%lu] err [%d]\n",
                            fsm->port_id, err);
    }
    fsm->admin_state = PORT_ENABLED;

bail:
    return err;
}

/*
 * This function is called as reaction to port state change
 * it notifies master logic on new local state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter - port state
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
notify_oper_state(port_peer_local  * fsm, int parameter,
                  struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_peer_local, port_up_ev);
    UNUSED_PARAM(event);

    fsm->oper_state = parameter;
    MLAG_LOG(MLAG_LOG_DEBUG, "port [%lu] local state [%d]\n", fsm->port_id,
             parameter);

    return err;
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_peer_local_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}
