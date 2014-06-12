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
#include <libs/mlag_manager/mlag_manager.h>
#include <libs/mlag_topology/mlag_topology.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include "port_db.h"
#include "port_manager.h"
#include <libs/service_layer/service_layer.h>
#include "port_peer_remote.h"

/************************************************
 *  Local Defines
 ***********************************************/
#define UP MLAG_PORT_OPER_UP
#define DOWN MLAG_PORT_OPER_DOWN

static int
is_all_remotes_deleted(int port_id);

static int
is_all_remotes_up(int port_id);

static int
conditioned_all_remotes_up(int port_id, int peer_id);


#undef  __MODULE__
#define __MODULE__ PORT_MANAGER

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;

#ifdef FSM_COMPILED
#include \


stateMachine
{
port_peer_remote ()

events {
	port_add_ev         (int peer_id)
	port_del_ev         (int peer_id)
	port_global_en      ()
	port_global_dis     ()
	port_global_down    ()
	peer_port_down_ev   (int peer_id)
    peer_port_up_ev     (int peer_id)
    peer_down_ev        (int peer_id)
    peer_enable_ev      (int peer_id)
}

state idle {
default
 transitions {
  { port_add_ev         , NULL  , global_down   , on_port_add()  }
  { port_global_down    , NULL  , IN_STATE      , NULL           }
  { port_global_en      , NULL  , IN_STATE      , NULL           }

 }
}
// ***********************************
state global_down {
 transitions {
 { port_del_ev      , $ is_all_remotes_deleted(fsm->port_id) $                  , idle          , on_port_del()             }
 { port_add_ev      , NULL                                                      , IN_STATE      , on_port_add()             }
 { port_global_en   , $ is_all_remotes_up(fsm->port_id) $                       , remotes_up    , isolate_port()            }
 { port_global_en   , $ else $                                                  , remote_fault  , NULL                      }
 { port_global_down , NULL                                                      , IN_STATE      , NULL                      }
 { peer_port_down_ev, NULL                                                      , IN_STATE      , update_peer_state(DOWN)   }
 { peer_port_up_ev  , NULL                                                      , IN_STATE      , update_peer_state(UP)     }
 { peer_down_ev     , NULL                                                      , IN_STATE      , update_peer_state(DOWN)   }
 }
}
// ***********************************

state remote_fault {
 transitions {
 { port_del_ev      , $ is_all_remotes_deleted(fsm->port_id) $                      , idle          , on_port_del()             }
 { port_add_ev      , NULL                                                          , IN_STATE      , on_port_add()             }
 { port_global_dis  , NULL                                                          , global_down   , NULL                      }
 { port_global_down , NULL                                                          , global_down   , NULL                      }
 { peer_port_down_ev, NULL                                                          , IN_STATE      , update_peer_state(DOWN)   }
 { peer_port_up_ev  , $ conditioned_all_remotes_up(fsm->port_id,ev->peer_id) $      , remotes_up    , update_peer_state(UP)     }
 { peer_port_up_ev  , $ else $                                                      , IN_STATE      , update_peer_state(UP)     }
 { peer_down_ev     , $ is_all_remotes_up(fsm->port_id) $                           , remotes_up    , update_peer_state(DOWN)   }
 { peer_down_ev     , $ else $                                                      , IN_STATE      , update_peer_state(DOWN)   }
 { peer_enable_ev   , $ is_all_remotes_up(fsm->port_id) $                           , remotes_up    , NULL                      }
 { peer_enable_ev   , $ else $                                                      , IN_STATE      , NULL                      }
 }
 ef = remote_fault_entry
 xf = remote_fault_exit
}
// ***********************************
state remotes_up {
  transitions {
 { port_del_ev      , $ is_all_remotes_deleted(fsm->port_id) $                      , idle          , on_port_del()             }
 { port_add_ev      , NULL                                                          , IN_STATE      , on_port_add()             }
 { port_global_dis  , NULL                                                          , global_down   , NULL                      }
 { port_global_down , NULL                                                          , global_down   , NULL                      }
 { peer_port_down_ev, NULL                                                          , remote_fault  , update_peer_state(DOWN)   }
 { peer_port_up_ev  , $ conditioned_all_remotes_up(fsm->port_id,ev->peer_id) $      , IN_STATE      , update_peer_state(UP)     }
 { peer_port_up_ev  , $ else $                                                      , remote_fault  , update_peer_state(UP)     }
 { peer_down_ev     , $ is_all_remotes_up(fsm->port_id) $                           , IN_STATE      , update_peer_state(DOWN)   }
 { peer_down_ev     , $ else $                                                      , remote_fault  , update_peer_state(DOWN)   }
 { peer_enable_ev   , $ is_all_remotes_up(fsm->port_id) $                           , IN_STATE      , NULL                      }
 { peer_enable_ev   , $ else $                                                      , remote_fault  , NULL                      }
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
enum port_peer_remote_events_t{
    port_add_ev  = 0,
    port_del_ev  = 1,
    port_global_en  = 2,
    port_global_dis  = 3,
    port_global_down  = 4,
    peer_port_down_ev  = 5,
    peer_port_up_ev  = 6,
    peer_down_ev  = 7,
    peer_enable_ev  = 8,
 };

/*----------------------------------------------------------------
            Generated structures for  Events of FSM
---------------------------------------------------------------*/
struct  port_peer_remote_port_add_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_peer_remote_port_del_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_peer_remote_port_global_en_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_remote_port_global_dis_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_remote_port_global_down_t
{
    int  opcode;
    const char *name;
};

struct  port_peer_remote_peer_port_down_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_peer_remote_peer_port_up_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_peer_remote_peer_down_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_peer_remote_peer_enable_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

/*----------------------------------------------------------------
           State entry/exit functions prototypes
---------------------------------------------------------------*/
static int remote_fault_entry_func(port_peer_remote *fsm, struct fsm_event_base *ev);
static int remote_fault_exit_func(port_peer_remote *fsm, struct fsm_event_base *ev);
/*----------------------------------------------------------------
            Generated functions for  Events of FSM
---------------------------------------------------------------*/
int   port_peer_remote_port_add_ev(struct port_peer_remote  * fsm,  int  peer_id)
{
     struct port_peer_remote_port_add_ev_t ev;
     ev.opcode  =  port_add_ev ;
     ev.name    = "port_add_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_port_del_ev(struct port_peer_remote  * fsm,  int  peer_id)
{
     struct port_peer_remote_port_del_ev_t ev;
     ev.opcode  =  port_del_ev ;
     ev.name    = "port_del_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_port_global_en(struct port_peer_remote  * fsm)
{
     struct port_peer_remote_port_global_en_t ev;
     ev.opcode  =  port_global_en ;
     ev.name    = "port_global_en" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_port_global_dis(struct port_peer_remote  * fsm)
{
     struct port_peer_remote_port_global_dis_t ev;
     ev.opcode  =  port_global_dis ;
     ev.name    = "port_global_dis" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_port_global_down(struct port_peer_remote  * fsm)
{
     struct port_peer_remote_port_global_down_t ev;
     ev.opcode  =  port_global_down ;
     ev.name    = "port_global_down" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_peer_port_down_ev(struct port_peer_remote  * fsm,  int  peer_id)
{
     struct port_peer_remote_peer_port_down_ev_t ev;
     ev.opcode  =  peer_port_down_ev ;
     ev.name    = "peer_port_down_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_peer_port_up_ev(struct port_peer_remote  * fsm,  int  peer_id)
{
     struct port_peer_remote_peer_port_up_ev_t ev;
     ev.opcode  =  peer_port_up_ev ;
     ev.name    = "peer_port_up_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_peer_down_ev(struct port_peer_remote  * fsm,  int  peer_id)
{
     struct port_peer_remote_peer_down_ev_t ev;
     ev.opcode  =  peer_down_ev ;
     ev.name    = "peer_down_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_peer_remote_peer_enable_ev(struct port_peer_remote  * fsm,  int  peer_id)
{
     struct port_peer_remote_peer_enable_ev_t ev;
     ev.opcode  =  peer_enable_ev ;
     ev.name    = "peer_enable_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
/*----------------------------------------------------------------
                 Reactions of FSM
---------------------------------------------------------------*/
static  int on_port_add (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event);
static  int on_port_del (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event);
static  int isolate_port (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event);
static  int update_peer_state (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event);


/*----------------------------------------------------------------
           State Dispatcher function
---------------------------------------------------------------*/
static int port_peer_remote_state_dispatcher( port_peer_remote  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state );
/*----------------------------------------------------------------
           Initialized  State classes of FSM
---------------------------------------------------------------*/
 struct fsm_state_base            state_port_peer_remote_idle;
 struct fsm_state_base            state_port_peer_remote_global_down;
 struct fsm_state_base            state_port_peer_remote_remote_fault;
 struct fsm_state_base            state_port_peer_remote_remotes_up;


 struct fsm_state_base      state_port_peer_remote_idle ={ "idle", port_peer_remote_idle , SIMPLE, NULL, NULL, &port_peer_remote_state_dispatcher, NULL, NULL };

 struct fsm_state_base      state_port_peer_remote_global_down ={ "global_down", port_peer_remote_global_down , SIMPLE, NULL, NULL, &port_peer_remote_state_dispatcher, NULL, NULL };

 struct fsm_state_base      state_port_peer_remote_remote_fault ={ "remote_fault", port_peer_remote_remote_fault , SIMPLE, NULL, NULL, &port_peer_remote_state_dispatcher, remote_fault_entry_func , remote_fault_exit_func };

 struct fsm_state_base      state_port_peer_remote_remotes_up ={ "remotes_up", port_peer_remote_remotes_up , SIMPLE, NULL, NULL, &port_peer_remote_state_dispatcher, NULL, NULL };

static struct fsm_static_data static_data= {0,0,0,0,0 ,{ &state_port_peer_remote_idle,  &state_port_peer_remote_global_down,  &state_port_peer_remote_remote_fault,
                                                  &state_port_peer_remote_remotes_up, }
};

 static struct fsm_state_base * default_state = &state_port_peer_remote_idle ;
/*----------------------------------------------------------------
           StateDispatcher of FSM
---------------------------------------------------------------*/

static int port_peer_remote_state_dispatcher( port_peer_remote  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state )
{
  int err=0;
  struct fsm_timer_event *event = (  struct fsm_timer_event * )evt;
  switch(state)
  {

   case port_peer_remote_idle :
    {
      if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_peer_remote , port_add_ev)
         {/*tr00:*/
            err = on_port_add( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 76*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_down )
      {
         SET_EVENT(port_peer_remote , port_global_down)
         {/*tr01:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 77*/
            *target_state =  &state_port_peer_remote_idle;
            return err;
         }
      }

      else if ( event->opcode == port_global_en )
      {
         SET_EVENT(port_peer_remote , port_global_en)
         {/*tr02:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 78*/
            *target_state =  &state_port_peer_remote_idle;
            return err;
         }
      }

    }break;

   case port_peer_remote_global_down :
    {
      if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_peer_remote , port_del_ev)
         if(  is_all_remotes_deleted(fsm->port_id)  )
         {/*tr10:*/
            err = on_port_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 85*/
            *target_state =  &state_port_peer_remote_idle;
            return err;
         }
      }

      else if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_peer_remote , port_add_ev)
         {/*tr11:*/
           fsm->base.reaction_in_state = 1;
           err = on_port_add( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 86*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_en )
      {
         SET_EVENT(port_peer_remote , port_global_en)
         if(  is_all_remotes_up(fsm->port_id)  )
         {/*tr12:*/
            err = isolate_port( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 87*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr13:*/
              /* Source line = 88*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == port_global_down )
      {
         SET_EVENT(port_peer_remote , port_global_down)
         {/*tr14:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 89*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_port_down_ev )
      {
         SET_EVENT(port_peer_remote , peer_port_down_ev)
         {/*tr15:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, DOWN, evt);  /* Source line = 90*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_port_up_ev )
      {
         SET_EVENT(port_peer_remote , peer_port_up_ev)
         {/*tr16:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, UP, evt);  /* Source line = 91*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_down_ev )
      {
         SET_EVENT(port_peer_remote , peer_down_ev)
         {/*tr17:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, DOWN, evt);  /* Source line = 92*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

    }break;

   case port_peer_remote_remote_fault :
    {
      if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_peer_remote , port_del_ev)
         if(  is_all_remotes_deleted(fsm->port_id)  )
         {/*tr20:*/
            err = on_port_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 99*/
            *target_state =  &state_port_peer_remote_idle;
            return err;
         }
      }

      else if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_peer_remote , port_add_ev)
         {/*tr21:*/
           fsm->base.reaction_in_state = 1;
           err = on_port_add( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 100*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == port_global_dis )
      {
         SET_EVENT(port_peer_remote , port_global_dis)
         {/*tr22:*/
              /* Source line = 101*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_down )
      {
         SET_EVENT(port_peer_remote , port_global_down)
         {/*tr23:*/
              /* Source line = 102*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_port_down_ev )
      {
         SET_EVENT(port_peer_remote , peer_port_down_ev)
         {/*tr24:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, DOWN, evt);  /* Source line = 103*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == peer_port_up_ev )
      {
         SET_EVENT(port_peer_remote , peer_port_up_ev)
         if(  conditioned_all_remotes_up(fsm->port_id,ev->peer_id)  )
         {/*tr25:*/
            err = update_peer_state( fsm, UP, evt);  /* Source line = 104*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr26:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, UP, evt);  /* Source line = 105*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == peer_down_ev )
      {
         SET_EVENT(port_peer_remote , peer_down_ev)
         if(  is_all_remotes_up(fsm->port_id)  )
         {/*tr27:*/
            err = update_peer_state( fsm, DOWN, evt);  /* Source line = 106*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr28:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, DOWN, evt);  /* Source line = 107*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == peer_enable_ev )
      {
         SET_EVENT(port_peer_remote , peer_enable_ev)
         if(  is_all_remotes_up(fsm->port_id)  )
         {/*tr29:*/
              /* Source line = 108*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr210:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 109*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

    }break;

   case port_peer_remote_remotes_up :
    {
      if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_peer_remote , port_del_ev)
         if(  is_all_remotes_deleted(fsm->port_id)  )
         {/*tr30:*/
            err = on_port_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 117*/
            *target_state =  &state_port_peer_remote_idle;
            return err;
         }
      }

      else if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_peer_remote , port_add_ev)
         {/*tr31:*/
           fsm->base.reaction_in_state = 1;
           err = on_port_add( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 118*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
      }

      else if ( event->opcode == port_global_dis )
      {
         SET_EVENT(port_peer_remote , port_global_dis)
         {/*tr32:*/
              /* Source line = 119*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_global_down )
      {
         SET_EVENT(port_peer_remote , port_global_down)
         {/*tr33:*/
              /* Source line = 120*/
            *target_state =  &state_port_peer_remote_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_port_down_ev )
      {
         SET_EVENT(port_peer_remote , peer_port_down_ev)
         {/*tr34:*/
            err = update_peer_state( fsm, DOWN, evt);  /* Source line = 121*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == peer_port_up_ev )
      {
         SET_EVENT(port_peer_remote , peer_port_up_ev)
         if(  conditioned_all_remotes_up(fsm->port_id,ev->peer_id)  )
         {/*tr35:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, UP, evt);  /* Source line = 122*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr36:*/
            err = update_peer_state( fsm, UP, evt);  /* Source line = 123*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == peer_down_ev )
      {
         SET_EVENT(port_peer_remote , peer_down_ev)
         if(  is_all_remotes_up(fsm->port_id)  )
         {/*tr37:*/
           fsm->base.reaction_in_state = 1;
           err = update_peer_state( fsm, DOWN, evt);  /* Source line = 124*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr38:*/
            err = update_peer_state( fsm, DOWN, evt);  /* Source line = 125*/
            *target_state =  &state_port_peer_remote_remote_fault;
            return err;
         }
      }

      else if ( event->opcode == peer_enable_ev )
      {
         SET_EVENT(port_peer_remote , peer_enable_ev)
         if(  is_all_remotes_up(fsm->port_id)  )
         {/*tr39:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 126*/
            *target_state =  &state_port_peer_remote_remotes_up;
            return err;
         }
         else
         {/*tr310:*/
              /* Source line = 127*/
            *target_state =  &state_port_peer_remote_remote_fault;
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
int port_peer_remote_init(struct port_peer_remote *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func)
 {

       fsm_init(&fsm->base,  default_state, 4, NULL, user_trace, sched_func, unsched_func, &static_data);

      return 0;
 }
 /*  per state functions*/
/*----------------------------------------------------------------
              Getters for each State
---------------------------------------------------------------*/
tbool port_peer_remote_idle_in(struct port_peer_remote *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_remote_idle);
}
tbool port_peer_remote_global_down_in(struct port_peer_remote *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_remote_global_down);
}
tbool port_peer_remote_remote_fault_in(struct port_peer_remote *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_remote_remote_fault);
}
tbool port_peer_remote_remotes_up_in(struct port_peer_remote *fsm){
    return fsm_is_in_state(&fsm->base, port_peer_remote_remotes_up);
}
/*----------------------------------------------------------------
                 Reactions of FSM  (within comments . user may paste function body outside placeholder region)
---------------------------------------------------------------*/

/*static  int on_port_add (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_remote , port_add_ev) ;
 }
static  int on_port_del (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_remote , port_del_ev) ;
 }
static  int isolate_port (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_remote , port_global_en) ;
 }
static  int update_peer_state (port_peer_remote  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_peer_remote , peer_port_down_ev) ;
 }


*/
/*   20592*/

/*#$*/

/* Below code written by user */
/*
 * This function is called when moving to remotes up state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
isolate_port(port_peer_remote  * fsm, int parameter,
             struct fsm_event_base *event)
{
    int err = 0;
    unsigned long ipl_port_id;
    unsigned int ipl_id;

    UNUSED_PARAM(parameter);
    UNUSED_PARAM(event);

    if (fsm->isolated == FALSE) {
        err = mlag_topology_redirect_ipl_id_get(&ipl_id, &ipl_port_id);
        if (err == -EINVAL) {
            /* IPL not defined yet, ignore */
            err = 0;
            goto bail;
        }
        else {
            MLAG_BAIL_ERROR_MSG(err, "Failed in getting isolate IPL id\n");
        }

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Restore port [%lu] isolation from ipl_id [%lu] (no rmt fault)\n",
                 fsm->port_id, ipl_port_id);

        err = sl_api_port_isolation_set(OES_ACCESS_CMD_ADD, fsm->port_id,
                                        &ipl_port_id, 1);
        MLAG_BAIL_ERROR_MSG(err, "Failed isolation add port [%lu] err [%d]\n",
                            fsm->port_id, err);
        fsm->isolated = TRUE;
    }

bail:
    return err;
}

/*
 * This function is called when entering remote fault state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
remote_fault_entry_func(port_peer_remote *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    unsigned long ipl_port_id;
    unsigned int ipl_id;

    UNUSED_PARAM(ev);

    if (fsm->isolated == TRUE) {
        err = mlag_topology_redirect_ipl_id_get(&ipl_id, &ipl_port_id);
        if (err == -EINVAL) {
            /* IPL not defined yet, ignore */
            err = 0;
            goto bail;
        }
        else {
            MLAG_BAIL_ERROR_MSG(err, "Failed in getting isolate IPL id\n");
        }

        MLAG_LOG(MLAG_LOG_NOTICE,
                 "Remove Port [%lu] Isolation from ipl_id [%lu] (rmt fault)\n",
                 fsm->port_id, ipl_port_id);

        err = sl_api_port_isolation_set(OES_ACCESS_CMD_DELETE, fsm->port_id,
                                        &ipl_port_id, 1);
        MLAG_BAIL_ERROR_MSG(err, "Failed isolation del port [%lu] err [%d]\n",
                            fsm->port_id, err);
        fsm->isolated = FALSE;
    }

bail:
    return err;
}

/*
 * This function is called when leaving remote fault state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
remote_fault_exit_func(port_peer_remote *fsm, struct fsm_event_base *ev)
{
    int err = 0;

    UNUSED_PARAM(ev);

    err = isolate_port(fsm, 0, ev);
    MLAG_BAIL_ERROR_MSG(err, "Failed to isolate port [%lu]\n", fsm->port_id);

bail:
    return err;
}

/*
 * This function is called as reaction to port add event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_port_add(port_peer_remote  * fsm, int parameter,
            struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(fsm);
    UNUSED_PARAM(parameter);
    UNUSED_PARAM(event);

    return err;
}

/*
 * This function is called as reaction to port del event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_port_del(port_peer_remote  * fsm, int parameter,
            struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(fsm);
    UNUSED_PARAM(parameter);
    UNUSED_PARAM(event);

    return err;
}

/*
 * This function is called as reaction when peer port state is changed
 * Relevant when port or peer changes its state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - new state value
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
update_peer_state(port_peer_remote  * fsm, int parameter,
                  struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_peer_remote, peer_port_down_ev);

    MLAG_LOG(MLAG_LOG_DEBUG, "port [%lu] peer [%d] new oper state [%d]\n",
             fsm->port_id, ev->peer_id, parameter);

    return err;
}

/*
 * This function checks if the port is the last peer port
 *
 * @param[in] port_id - port index
 *
 * @return 1 if last peer port, 0 otherwise
 */
static int
is_all_remotes_deleted(int port_id)
{
    int err = 0, val = 0;
    uint32_t port_conf_states = 0;

    err = port_db_peer_conf_state_vector_get(port_id, &port_conf_states);
    MLAG_BAIL_ERROR_MSG(err, "Port [%d] not found\n", port_id);

    port_conf_states >>= 1;
    if ((port_conf_states & REMOTE_PEERS_MASK) == 0) {
        val = 1;
    }

bail:
    if (err) {
        return 0;
    }
    return val;
}

/*
 * This function checks if the port is the last peer port
 *
 * @param[in] port_id - port index
 *
 * @return 1 if all peer port are oper up , 0 otherwise
 */
static int
is_all_remotes_up(int port_id)
{
    int err = 0, val = 0;
    uint32_t peer_states, port_conf_states = 0;

    err = port_db_peer_conf_state_vector_get(port_id, &port_conf_states);
    MLAG_BAIL_ERROR_MSG(err, "Port [%d] not found\n", port_id);

    err = port_db_peer_oper_state_vector_get(port_id, &peer_states);
    MLAG_BAIL_ERROR_MSG(err, "Port [%d] not found\n", port_id);

    MLAG_LOG(MLAG_LOG_DEBUG, "conf states [0x%x] peer_states [0x%x]\n",
             port_conf_states, peer_states);

    /* clear local state */
    port_conf_states >>= 1;
    peer_states >>= 1;

    if ((port_conf_states ^ peer_states) == 0) {
        val = 1;
    }

bail:
    if (err) {
        return 0;
    }
    return val;
}

/*
 * This function checks if in the condition of the port state change to up
 * all remotes will be up
 *
 * @param[in] port_id - port index
 * @param[in] peer_id - Peer index
 *
 * @return 1 if all remotes considered up after this update, 0 otherwise
 */
static int
conditioned_all_remotes_up(int port_id, int peer_id)
{
    int err = 0, val = 0;
    uint32_t port_conf_states, port_oper_states = 0;

    err = port_db_peer_oper_state_vector_get(port_id, &port_oper_states);
    MLAG_BAIL_ERROR_MSG(err, "Port [%d] not found\n", port_id);

    err = port_db_peer_conf_state_vector_get(port_id, &port_conf_states);
    MLAG_BAIL_ERROR_MSG(err, "Port [%d] not found\n", port_id);

    port_oper_states |= (1 << peer_id);

    /* clear local state */
    port_oper_states >>= 1;
    port_conf_states >>= 1;

    if ((port_oper_states ^ port_conf_states) == 0) {
        val = 1;
    }

bail:
    if (err) {
        return 0;
    }
    return val;
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_peer_remote_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}

