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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <mlag_api_defs.h>
#include <oes_types.h>
#include "lib_commu.h"
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <utils/mlag_events.h>
#include <libs/mlag_master_election/mlag_master_election.h>
#include <libs/mlag_common/mlag_common.h>
#include <libs/mlag_common/mlag_comm_layer_wrapper.h>
#include <libs/mlag_manager/mlag_manager_db.h>
#include <libs/mlag_manager/mlag_dispatcher.h>
#include <libs/service_layer/service_layer.h>

#include "port_manager.h"
#include "port_db.h"
#include "port_master_logic.h"

/************************************************
 *  Local Defines
 ***********************************************/
#define UP 1
#define DOWN 0


#undef  __MODULE__
#define __MODULE__ PORT_MANAGER


static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_DEBUG;

static int
is_all_peers_active();
static int
is_all_peers_down();
static int
is_all_peers_oper_down();

#ifdef FSM_COMPILED
#include \


stateMachine
{
port_master_logic ()

events {
	port_add_ev     (int peer_id)
	port_del_ev     (int peer_id)
	peer_down_ev    (int peer_id)
	peer_active_ev  (int peer_id)
	port_up_ev      (int peer_id)
	port_down_ev    (int peer_id)
}

state idle {
default
 transitions {
  { port_add_ev , $ is_all_peers_active(fsm->port_id) $     , global_down  , port_add_enable()  }
  { port_add_ev , $ else $                                  , disabled     , port_conf_chg(PORT_CONFIGURED)  }
 }
}
// ***********************************
state disabled {
 transitions {
 { peer_active_ev   , $ is_all_peers_active(fsm->port_id) $     , global_down , port_enable()   }
 { peer_active_ev   , $ else $                                  , IN_STATE    , NULL            }
 { port_del_ev      , $ is_all_peers_down(fsm->port_id) $       , idle        , port_conf_chg(PORT_DELETED)     }
 { port_del_ev      , $ else $                                  , IN_STATE    , port_conf_chg(PORT_DELETED)     }
 { port_add_ev      , $ is_all_peers_active(fsm->port_id) $     , global_down , port_add_enable()               }
 { port_add_ev      , $ else $                                  , IN_STATE    , port_conf_chg(PORT_CONFIGURED)  }
 { peer_down_ev     , $ is_all_peers_active(fsm->port_id) $     , global_down , port_enable()   }

 }
ef = disabled_entry_func
}
// ***********************************

state global_down {
 transitions {
 { port_add_ev      , NULL                                      , IN_STATE    , port_conf_chg(PORT_CONFIGURED)  }
 { port_del_ev      , $ is_all_peers_active(fsm->port_id) == 0 $, disabled    , port_conf_chg(PORT_DELETED)     }
 { port_del_ev      , $ is_all_peers_down(fsm->port_id) $       , idle        , port_conf_chg(PORT_DELETED)     }
 { port_del_ev      , $ else $                                  , IN_STATE    , port_conf_chg(PORT_DELETED)     }
 { peer_down_ev     , $ is_all_peers_down(fsm->port_id) $       , idle        , NULL                            }
 { peer_down_ev     , $ else $                                  , IN_STATE    , NULL                            }
 { peer_active_ev   , $ is_all_peers_active(fsm->port_id) == 0 $, disabled    , NULL                            }
 { peer_active_ev   , $ else $                                  , IN_STATE    , port_global_en(OES_PORT_DOWN)   }
 { port_up_ev       , NULL                                      , global_up   , port_state_chg(OES_PORT_UP)     }
 { port_down_ev     , NULL                                      , IN_STATE    , port_state_chg(OES_PORT_DOWN)   }

 }
 ef = global_down_entry
}
// ***********************************
state global_up {
  transitions {
 { port_add_ev      , NULL                                      , IN_STATE    , port_conf_chg(PORT_CONFIGURED)  }
 { port_del_ev      , $ is_all_peers_active(fsm->port_id) == 0 $, disabled    , port_conf_chg(PORT_DELETED)     }
 { port_del_ev      , $ is_all_peers_down(fsm->port_id) $       , idle        , port_conf_chg(PORT_DELETED)     }
 { port_del_ev      , $ else $                                  , IN_STATE    , port_conf_chg(PORT_DELETED)     }
 { peer_down_ev     , $ is_all_peers_oper_down(fsm->port_id) $  , global_down , NULL                            }
 { peer_down_ev     , $ else $                                  , IN_STATE    , NULL                            }
 { peer_active_ev   , $ is_all_peers_active(fsm->port_id) == 0 $, disabled    , NULL                            }
 { peer_active_ev   , $ else $                                  , IN_STATE    , port_global_en(OES_PORT_UP)     }
 { port_up_ev       , NULL                                      , IN_STATE    , port_state_chg(OES_PORT_UP)     }
 { port_down_ev     , $ is_all_peers_oper_down(fsm->port_id) $  , global_down , port_state_chg(OES_PORT_DOWN)   }
 { port_down_ev     , $ else $                                  , IN_STATE    , port_state_chg(OES_PORT_DOWN)   }
 }
ef = global_up_entry
}

}// end of stateMachine


/* *********************************** */
#endif

/*#$*/
 /* The code  inside placeholders  generated by tool and cannot be modifyed*/


/*----------------------------------------------------------------
            Generated enumerator for  Events of FSM
---------------------------------------------------------------*/
enum port_master_logic_events_t{
    port_add_ev  = 0,
    port_del_ev  = 1,
    peer_down_ev  = 2,
    peer_active_ev  = 3,
    port_up_ev  = 4,
    port_down_ev  = 5,
 };

/*----------------------------------------------------------------
            Generated structures for  Events of FSM
---------------------------------------------------------------*/
struct  port_master_logic_port_add_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_master_logic_port_del_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_master_logic_peer_down_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_master_logic_peer_active_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_master_logic_port_up_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

struct  port_master_logic_port_down_ev_t
{
    int  opcode;
    const char *name;
    int  peer_id ;
};

/*----------------------------------------------------------------
           State entry/exit functions prototypes
---------------------------------------------------------------*/
static int disabled_entry_func(port_master_logic *fsm, struct fsm_event_base *ev);
static int global_down_entry_func(port_master_logic *fsm, struct fsm_event_base *ev);
static int global_up_entry_func(port_master_logic *fsm, struct fsm_event_base *ev);
/*----------------------------------------------------------------
            Generated functions for  Events of FSM
---------------------------------------------------------------*/
int   port_master_logic_port_add_ev(struct port_master_logic  * fsm,  int  peer_id)
{
     struct port_master_logic_port_add_ev_t ev;
     ev.opcode  =  port_add_ev ;
     ev.name    = "port_add_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_master_logic_port_del_ev(struct port_master_logic  * fsm,  int  peer_id)
{
     struct port_master_logic_port_del_ev_t ev;
     ev.opcode  =  port_del_ev ;
     ev.name    = "port_del_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_master_logic_peer_down_ev(struct port_master_logic  * fsm,  int  peer_id)
{
     struct port_master_logic_peer_down_ev_t ev;
     ev.opcode  =  peer_down_ev ;
     ev.name    = "peer_down_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_master_logic_peer_active_ev(struct port_master_logic  * fsm,  int  peer_id)
{
     struct port_master_logic_peer_active_ev_t ev;
     ev.opcode  =  peer_active_ev ;
     ev.name    = "peer_active_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_master_logic_port_up_ev(struct port_master_logic  * fsm,  int  peer_id)
{
     struct port_master_logic_port_up_ev_t ev;
     ev.opcode  =  port_up_ev ;
     ev.name    = "port_up_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   port_master_logic_port_down_ev(struct port_master_logic  * fsm,  int  peer_id)
{
     struct port_master_logic_port_down_ev_t ev;
     ev.opcode  =  port_down_ev ;
     ev.name    = "port_down_ev" ;
     ev.peer_id = peer_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
/*----------------------------------------------------------------
                 Reactions of FSM
---------------------------------------------------------------*/
static  int port_add_enable (port_master_logic  * fsm, int parameter, struct fsm_event_base *event);
static  int port_conf_chg (port_master_logic  * fsm, int parameter, struct fsm_event_base *event);
static  int port_enable (port_master_logic  * fsm, int parameter, struct fsm_event_base *event);
static  int port_global_en (port_master_logic  * fsm, int parameter, struct fsm_event_base *event);
static  int port_state_chg (port_master_logic  * fsm, int parameter, struct fsm_event_base *event);


/*----------------------------------------------------------------
           State Dispatcher function
---------------------------------------------------------------*/
static int port_master_logic_state_dispatcher( port_master_logic  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state );
/*----------------------------------------------------------------
           Initialized  State classes of FSM
---------------------------------------------------------------*/
 struct fsm_state_base            state_port_master_logic_idle;
 struct fsm_state_base            state_port_master_logic_disabled;
 struct fsm_state_base            state_port_master_logic_global_down;
 struct fsm_state_base            state_port_master_logic_global_up;


 struct fsm_state_base      state_port_master_logic_idle ={ "idle", port_master_logic_idle , SIMPLE, NULL, NULL, &port_master_logic_state_dispatcher, NULL, NULL };

 struct fsm_state_base      state_port_master_logic_disabled ={ "disabled", port_master_logic_disabled , SIMPLE, NULL, NULL, &port_master_logic_state_dispatcher, disabled_entry_func , NULL };

 struct fsm_state_base      state_port_master_logic_global_down ={ "global_down", port_master_logic_global_down , SIMPLE, NULL, NULL, &port_master_logic_state_dispatcher, global_down_entry_func , NULL };

 struct fsm_state_base      state_port_master_logic_global_up ={ "global_up", port_master_logic_global_up , SIMPLE, NULL, NULL, &port_master_logic_state_dispatcher, global_up_entry_func , NULL };

static struct fsm_static_data static_data= {0,0,0,0,0 ,{ &state_port_master_logic_idle,  &state_port_master_logic_disabled,  &state_port_master_logic_global_down,
                                                  &state_port_master_logic_global_up, }
};

 static struct fsm_state_base * default_state = &state_port_master_logic_idle ;
/*----------------------------------------------------------------
           StateDispatcher of FSM
---------------------------------------------------------------*/

static int port_master_logic_state_dispatcher( port_master_logic  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state )
{
  int err=0;
  struct fsm_timer_event *event = (  struct fsm_timer_event * )evt;
  switch(state)
  {

   case port_master_logic_idle :
    {
      if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_master_logic , port_add_ev)
         if(  is_all_peers_active(fsm->port_id)  )
         {/*tr00:*/
            err = port_add_enable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 77*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
         else
         {/*tr01:*/
            err = port_conf_chg( fsm, PORT_CONFIGURED, evt);  /* Source line = 78*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
      }

    }break;

   case port_master_logic_disabled :
    {
      if ( event->opcode == peer_active_ev )
      {
         SET_EVENT(port_master_logic , peer_active_ev)
         if(  is_all_peers_active(fsm->port_id)  )
         {/*tr10:*/
            err = port_enable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 84*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
         else
         {/*tr11:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 85*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
      }

      else if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_master_logic , port_del_ev)
         if(  is_all_peers_down(fsm->port_id)  )
         {/*tr12:*/
            err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 86*/
            *target_state =  &state_port_master_logic_idle;
            return err;
         }
         else
         {/*tr13:*/
           fsm->base.reaction_in_state = 1;
           err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 87*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
      }

      else if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_master_logic , port_add_ev)
         if(  is_all_peers_active(fsm->port_id)  )
         {/*tr14:*/
            err = port_add_enable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 88*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
         else
         {/*tr15:*/
           fsm->base.reaction_in_state = 1;
           err = port_conf_chg( fsm, PORT_CONFIGURED, evt);  /* Source line = 89*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
      }

      else if ( event->opcode == peer_down_ev )
      {
         SET_EVENT(port_master_logic , peer_down_ev)
         if(  is_all_peers_active(fsm->port_id)  )
         {/*tr16:*/
            err = port_enable( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 90*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
      }

    }break;

   case port_master_logic_global_down :
    {
      if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_master_logic , port_add_ev)
         {/*tr20:*/
           fsm->base.reaction_in_state = 1;
           err = port_conf_chg( fsm, PORT_CONFIGURED, evt);  /* Source line = 99*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_master_logic , port_del_ev)
         if(  is_all_peers_active(fsm->port_id) == 0  )
         {/*tr21:*/
            err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 100*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
         else
         if(  is_all_peers_down(fsm->port_id)  )
         {/*tr22:*/
            err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 101*/
            *target_state =  &state_port_master_logic_idle;
            return err;
         }
         else
         {/*tr23:*/
           fsm->base.reaction_in_state = 1;
           err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 102*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_down_ev )
      {
         SET_EVENT(port_master_logic , peer_down_ev)
         if(  is_all_peers_down(fsm->port_id)  )
         {/*tr24:*/
              /* Source line = 103*/
            *target_state =  &state_port_master_logic_idle;
            return err;
         }
         else
         {/*tr25:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 104*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
      }

      else if ( event->opcode == peer_active_ev )
      {
         SET_EVENT(port_master_logic , peer_active_ev)
         if(  is_all_peers_active(fsm->port_id) == 0  )
         {/*tr26:*/
              /* Source line = 105*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
         else
         {/*tr27:*/
           fsm->base.reaction_in_state = 1;
           err = port_global_en( fsm, OES_PORT_DOWN, evt);  /* Source line = 106*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
      }

      else if ( event->opcode == port_up_ev )
      {
         SET_EVENT(port_master_logic , port_up_ev)
         {/*tr28:*/
            err = port_state_chg( fsm, OES_PORT_UP, evt);  /* Source line = 107*/
            *target_state =  &state_port_master_logic_global_up;
            return err;
         }
      }

      else if ( event->opcode == port_down_ev )
      {
         SET_EVENT(port_master_logic , port_down_ev)
         {/*tr29:*/
           fsm->base.reaction_in_state = 1;
           err = port_state_chg( fsm, OES_PORT_DOWN, evt);  /* Source line = 108*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
      }

    }break;

   case port_master_logic_global_up :
    {
      if ( event->opcode == port_add_ev )
      {
         SET_EVENT(port_master_logic , port_add_ev)
         {/*tr30:*/
           fsm->base.reaction_in_state = 1;
           err = port_conf_chg( fsm, PORT_CONFIGURED, evt);  /* Source line = 116*/
            *target_state =  &state_port_master_logic_global_up;
            return err;
         }
      }

      else if ( event->opcode == port_del_ev )
      {
         SET_EVENT(port_master_logic , port_del_ev)
         if(  is_all_peers_active(fsm->port_id) == 0  )
         {/*tr31:*/
            err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 117*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
         else
         if(  is_all_peers_down(fsm->port_id)  )
         {/*tr32:*/
            err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 118*/
            *target_state =  &state_port_master_logic_idle;
            return err;
         }
         else
         {/*tr33:*/
           fsm->base.reaction_in_state = 1;
           err = port_conf_chg( fsm, PORT_DELETED, evt);  /* Source line = 119*/
            *target_state =  &state_port_master_logic_global_up;
            return err;
         }
      }

      else if ( event->opcode == peer_down_ev )
      {
         SET_EVENT(port_master_logic , peer_down_ev)
         if(  is_all_peers_oper_down(fsm->port_id)  )
         {/*tr34:*/
              /* Source line = 120*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
         else
         {/*tr35:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 121*/
            *target_state =  &state_port_master_logic_global_up;
            return err;
         }
      }

      else if ( event->opcode == peer_active_ev )
      {
         SET_EVENT(port_master_logic , peer_active_ev)
         if(  is_all_peers_active(fsm->port_id) == 0  )
         {/*tr36:*/
              /* Source line = 122*/
            *target_state =  &state_port_master_logic_disabled;
            return err;
         }
         else
         {/*tr37:*/
           fsm->base.reaction_in_state = 1;
           err = port_global_en( fsm, OES_PORT_UP, evt);  /* Source line = 123*/
            *target_state =  &state_port_master_logic_global_up;
            return err;
         }
      }

      else if ( event->opcode == port_up_ev )
      {
         SET_EVENT(port_master_logic , port_up_ev)
         {/*tr38:*/
           fsm->base.reaction_in_state = 1;
           err = port_state_chg( fsm, OES_PORT_UP, evt);  /* Source line = 124*/
            *target_state =  &state_port_master_logic_global_up;
            return err;
         }
      }

      else if ( event->opcode == port_down_ev )
      {
         SET_EVENT(port_master_logic , port_down_ev)
         if(  is_all_peers_oper_down(fsm->port_id)  )
         {/*tr39:*/
            err = port_state_chg( fsm, OES_PORT_DOWN, evt);  /* Source line = 125*/
            *target_state =  &state_port_master_logic_global_down;
            return err;
         }
         else
         {/*tr310:*/
           fsm->base.reaction_in_state = 1;
           err = port_state_chg( fsm, OES_PORT_DOWN, evt);  /* Source line = 126*/
            *target_state =  &state_port_master_logic_global_up;
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
int port_master_logic_init(struct port_master_logic *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func)
 {

       fsm_init(&fsm->base,  default_state, 4, NULL, user_trace, sched_func, unsched_func, &static_data);

      return 0;
 }
 /*  per state functions*/
/*----------------------------------------------------------------
              Getters for each State
---------------------------------------------------------------*/
tbool port_master_logic_idle_in(struct port_master_logic *fsm){
    return fsm_is_in_state(&fsm->base, port_master_logic_idle);
}
tbool port_master_logic_disabled_in(struct port_master_logic *fsm){
    return fsm_is_in_state(&fsm->base, port_master_logic_disabled);
}
tbool port_master_logic_global_down_in(struct port_master_logic *fsm){
    return fsm_is_in_state(&fsm->base, port_master_logic_global_down);
}
tbool port_master_logic_global_up_in(struct port_master_logic *fsm){
    return fsm_is_in_state(&fsm->base, port_master_logic_global_up);
}
/*----------------------------------------------------------------
                 Reactions of FSM  (within comments . user may paste function body outside placeholder region)
---------------------------------------------------------------*/

/*static  int port_add_enable (port_master_logic  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_master_logic , port_add_ev) ;
 }
static  int port_conf_chg (port_master_logic  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_master_logic , port_add_ev) ;
 }
static  int port_enable (port_master_logic  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_master_logic , peer_active_ev) ;
 }
static  int port_global_en (port_master_logic  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_master_logic , peer_active_ev) ;
 }
static  int port_state_chg (port_master_logic  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(port_master_logic , port_up_ev) ;
 }


*/
/*   18490*/

/*#$*/
/* Below code written by user */

/*
 * This function sends new global state to all remote peers
 *
 * @param[in] msg - port global state change message
 *
 * @return 0 if operation completes successfully.
 */
static int
send_global_state_to_remote_peers(port_master_logic *fsm,
                                  struct port_global_state_event_data *msg)
{
    int err = 0;
    int current_peer = 0;
    int mlag_id;
    enum port_manager_peer_state peer_state;

    for (current_peer = 0; current_peer < MLAG_MAX_PEERS; current_peer++) {
        err = port_db_peer_state_get(current_peer, &peer_state);
        MLAG_BAIL_ERROR_MSG(err, "Failed to get peer [%d] state\n", current_peer);
        if ((peer_state == PM_PEER_ENABLED) &&
            (current_peer != mlag_manager_db_local_peer_id_get())) {
            err = mlag_manager_db_mlag_id_from_local_index_get(current_peer,
                                                               &mlag_id);
            MLAG_BAIL_ERROR_MSG(err, "Failed to get mlag id from index [%d]",
                                current_peer);
            MLAG_LOG(MLAG_LOG_DEBUG, "Send global [%d] to mlag_id [%d]\n",
                     msg->state[0], mlag_id);
            if (fsm->message_send_func != NULL) {
                err = fsm->message_send_func(MLAG_PORT_GLOBAL_STATE_EVENT, msg,
                                             sizeof(*msg), mlag_id,
                                             MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err, "Failed sending MLAG_PORT_GLOBAL_STATE_EVENT\n");
            }
        }
    }

bail:
    return err;
}
/*
 * This function is called as reaction to peer up event
 * it updates the remote peer that the port is globally enabled
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
port_global_en(port_master_logic  * fsm, int parameter,
               struct fsm_event_base *event)
{
    int err = 0;
    int mlag_id;
    struct port_global_state_event_data msg;
    SET_EVENT(port_master_logic, peer_active_ev);


    msg.port_id[0] = fsm->port_id;
    msg.port_num = 1;
    msg.state[0] = MLAG_PORT_GLOBAL_ENABLED;

    err = mlag_manager_db_mlag_id_from_local_index_get(ev->peer_id, &mlag_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get mlag id from local index [%d]\n",
                        ev->peer_id);

    if (fsm->message_send_func != NULL) {
        err = fsm->message_send_func(MLAG_PORT_GLOBAL_STATE_EVENT, &msg,
                                     sizeof(msg), mlag_id, MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err, "Failed sending MLAG_PORT_GLOBAL_ENABLED msg\n");
    }

    /* Send state message */
    msg.state[0] = MLAG_PORT_GLOBAL_DOWN;
    if (parameter == OES_PORT_UP) {
        msg.state[0] = MLAG_PORT_GLOBAL_UP;
    }

    if (fsm->message_send_func != NULL) {
        err = fsm->message_send_func(MLAG_PORT_GLOBAL_STATE_EVENT, &msg,
                                     sizeof(msg), mlag_id, MASTER_LOGIC);
        MLAG_BAIL_ERROR_MSG(err, "Failed sending oper MLAG_PORT_GLOBAL_STATE_EVENT msg\n");
    }

bail:
    return err;
}

/*
 * This function forwards peer oper state change notification to other peers
 *
 * @param[in] msg - peer port oper state change message
 *
 * @return 0 if operation completes successfully.
 */
static int
forward_msg_to_peers(port_master_logic  * fsm, enum mlag_events opcode,
                     void *msg, uint32_t payload_size,
                     int self_id)
{
    int err = 0;
    int current_peer = 0;
    int mlag_id;
    enum port_manager_peer_state peer_state;

    /* remove local peer, ignore origin peer */
    for (current_peer = 0; current_peer < MLAG_MAX_PEERS; current_peer++) {
        err = port_db_peer_state_get(current_peer, &peer_state);
        MLAG_BAIL_ERROR_MSG(err, "Failed getting peer [%d] state", current_peer);
        if ((peer_state != PM_PEER_DOWN) &&
            (current_peer != mlag_manager_db_local_peer_id_get())) {
            /* get mlag_id of peer */
            err = mlag_manager_db_mlag_id_from_local_index_get(current_peer,
                                                               &mlag_id);
            MLAG_BAIL_ERROR_MSG(err, "Failed to get mlag id for local index [%d",
                                current_peer);
            if ((mlag_id != self_id) && (fsm->message_send_func != NULL)) {
                err = fsm->message_send_func(opcode, msg, payload_size,
                                             mlag_id, MASTER_LOGIC);
                MLAG_BAIL_ERROR_MSG(err, "Failed to forward msg opcode [%d] to peers\n",
                                opcode);
            }
        }
    }

bail:
    return err;
}

/*
 * This function is called on entry to disabled state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
disabled_entry_func(port_master_logic *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    UNUSED_PARAM(ev);
    struct port_global_state_event_data port_state;
    /* Send Global Port Disable */
    MLAG_LOG(MLAG_LOG_DEBUG, "Port [%lu] in disable\n", fsm->port_id);
    port_state.port_num = 1;
    port_state.port_id[0] = fsm->port_id;
    port_state.state[0] = MLAG_PORT_GLOBAL_DISABLED;
    err = send_system_event(MLAG_PORT_GLOBAL_STATE_EVENT, &port_state,
                            sizeof(port_state));
    MLAG_BAIL_ERROR_MSG(err, "Failed sending system event MLAG_PORT_GLOBAL_DISABLED\n");

    /* Send to remote peers */
    err = send_global_state_to_remote_peers(fsm, &port_state);
    MLAG_BAIL_ERROR_MSG(err, "Failed sending global disable to peers\n");

bail:
    return err;
}

/*
 * This function is called on entry to down state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
global_down_entry_func(port_master_logic *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    int dispatch_err;
    UNUSED_PARAM(ev);
    struct port_global_state_event_data port_state;
    /* Send Global Port down event*/
    MLAG_LOG(MLAG_LOG_DEBUG, "Port [%lu] in global down\n", fsm->port_id);
    port_state.port_num = 1;
    port_state.port_id[0] = fsm->port_id;
    port_state.state[0] = MLAG_PORT_GLOBAL_DOWN;
    err = send_system_event(MLAG_PORT_GLOBAL_STATE_EVENT, &port_state,
                            sizeof(port_state));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending port global state down event\n");

    dispatch_err = sl_api_port_oper_status_trigger(fsm->port_id, OES_PORT_DOWN);
    if (dispatch_err) {
        MLAG_LOG(MLAG_LOG_INFO, "MLAG operstate-changed event dispatcher failed for port %ld\n", fsm->port_id);
    }

    err = send_global_state_to_remote_peers(fsm, &port_state);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending global down to peers\n");
    
bail:
    return err;
}

/*
 * This function is called on entry to global up state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
global_up_entry_func(port_master_logic *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    int dispatch_err;
    UNUSED_PARAM(ev);
    struct port_global_state_event_data port_state;
    /* Send Global Port down event*/
    MLAG_LOG(MLAG_LOG_DEBUG, "Port [%lu] in global up\n", fsm->port_id);
    port_state.port_num = 1;
    port_state.port_id[0] = fsm->port_id;
    port_state.state[0] = MLAG_PORT_GLOBAL_UP;
    err = send_system_event(MLAG_PORT_GLOBAL_STATE_EVENT, &port_state,
                            sizeof(port_state));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending port global up state event\n");

    dispatch_err = sl_api_port_oper_status_trigger(fsm->port_id, OES_PORT_UP);
    if (dispatch_err) {
        MLAG_LOG(MLAG_LOG_INFO, "MLAG operstate-changed event dispatcher failed for port %ld\n", fsm->port_id);
    }

    err = send_global_state_to_remote_peers(fsm, &port_state);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending port global up to peers\n");
    
bail:
    return err;
}

/*
 * This function is called as reaction to port up event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - optional parameter
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
port_enable(port_master_logic  * fsm, int parameter,
            struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    struct port_global_state_event_data port_state;
    /* Send Global Port Enable*/
    MLAG_LOG(MLAG_LOG_DEBUG, "Port [%lu] moves to Global enable\n",
             fsm->port_id);
    port_state.port_num = 1;
    port_state.port_id[0] = fsm->port_id;
    port_state.state[0] = MLAG_PORT_GLOBAL_ENABLED;
    err = send_system_event(MLAG_PORT_GLOBAL_STATE_EVENT, &port_state,
                            sizeof(port_state));
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending port global enabled state event\n");
    err = send_global_state_to_remote_peers(fsm, &port_state);
    MLAG_BAIL_ERROR_MSG(err, "Failed in sending port global enabled to peers\n");

bail:
    return err;
}

/*
 * This function is called as reaction to port conf (add/del) change event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - state parameter
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
port_conf_chg(port_master_logic  * fsm, int parameter,
              struct fsm_event_base *event)
{
    int err = 0;
    SET_EVENT(port_master_logic, port_up_ev);
    struct peer_port_sync_message port_chg;
    int mlag_id;

    err = mlag_manager_db_mlag_id_from_local_index_get(ev->peer_id, &mlag_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting mlag id from local index [%d]\n",
                        ev->peer_id);

    port_chg.port_num = 1;
    port_chg.del_ports = FALSE;
    if (parameter == PORT_DELETED) {
        port_chg.del_ports = TRUE;
    }
    port_chg.port_id[0] = fsm->port_id;
    port_chg.mlag_id = mlag_id;

    err =
        forward_msg_to_peers(fsm, MLAG_PORTS_UPDATE_EVENT, &port_chg,
                             sizeof(port_chg),
                             mlag_id);
    MLAG_BAIL_ERROR_MSG(err,
                        "Failed in forwarding port configuration message\n");

bail:
    return err;
}

/*
 * This function is called as reaction to port conf (add/del) change event,
 * that also triggers port enable
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - state parameter
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
port_add_enable(port_master_logic  * fsm, int parameter,
                struct fsm_event_base *event)
{
    int err = 0;

    err = port_enable(fsm, parameter, event);
    MLAG_BAIL_ERROR_MSG(err, "Failed to enable added port\n");

    /* Also notify peers on port add */
    port_conf_chg(fsm, PORT_CONFIGURED, event);
    MLAG_BAIL_ERROR_MSG(err, "Failed to notify port add to peers\n");

bail:
    return err;
}

/*
 * This function is called as reaction to port state change event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - state parameter
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
port_state_chg(port_master_logic  * fsm, int parameter,
               struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(parameter);
    SET_EVENT(port_master_logic, port_up_ev);
    struct port_oper_state_change_data oper_state_chg;
    int mlag_id;

    err = mlag_manager_db_mlag_id_from_local_index_get(ev->peer_id, &mlag_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get mlag id for local index [%d]\n",
                        ev->peer_id);

    oper_state_chg.port_id = fsm->port_id;
    oper_state_chg.is_ipl = FALSE;
    oper_state_chg.mlag_id = mlag_id;
    oper_state_chg.state = parameter;

    err = forward_msg_to_peers(fsm, MLAG_PEER_PORT_OPER_STATE_CHANGE,
                               &oper_state_chg, sizeof(oper_state_chg),
                               mlag_id);
    MLAG_BAIL_ERROR_MSG(err, "Failed in forwarding port oper state message\n");

bail:
    return err;
}

/*
 * This function is called as checker is all peers are active
 *
 * @param[in] port_id - port ID
 *
 * @return 0 if all peers are active
 */
static int
is_all_peers_active(int port_id)
{
    int err = 0;
    uint32_t conf_states, peer_states;

    err = port_db_peer_conf_state_vector_get(port_id, &conf_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting peer conf state vector\n");

    err = port_db_peer_state_vector_get(&peer_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting peer states vector\n");

    MLAG_LOG(MLAG_LOG_DEBUG, "peer_states [%x] conf_states [%x]\n",
             peer_states, conf_states);

    if (((peer_states & conf_states) == peer_states) && (peer_states != 0)) {
        return 1;
    }

bail:
    return 0;
}

/*
 * This function is called as checker is all peers are down
 *
 * @param[in] port_id - port ID
 *
 * @return 1 if there are no active peers for that port
 */
static int
is_all_peers_down(int port_id)
{
    int err = 0;
    uint32_t conf_states, peer_states;

    err = port_db_peer_conf_state_vector_get(port_id, &conf_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting peer conf state vector\n");

    err = port_db_peer_state_vector_get(&peer_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed getting peer states vector\n");

    peer_states |= 0x1;

    MLAG_LOG(MLAG_LOG_DEBUG, "peer_states [%x] conf_states [%x]\n",
             peer_states, conf_states);

    if ((peer_states & conf_states) == 0) {
        return 1;
    }

bail:
    return 0;
}

/*
 * This function is called as checker is all peers are oper down
 *
 * @param[in] port_id - port ID
 *
 * @return 1 if all peers are oper down for this port
 */
static int
is_all_peers_oper_down(int port_id)
{
    int err = 0;
    uint32_t oper_states, peer_states;

    err = port_db_peer_oper_state_vector_get(port_id, &oper_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get peer oper state vector\n");

    err = port_db_peer_state_vector_get(&peer_states);
    MLAG_BAIL_ERROR_MSG(err, "Failed to get peers state vector\n");

    MLAG_LOG(MLAG_LOG_DEBUG, "peer_states [%x] oper_states [%x]\n",
             peer_states, oper_states);

    if ((peer_states & oper_states) == 0) {
        return 1;
    }
bail:
    return 0;
}

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_master_logic_log_verbosity_set(mlag_verbosity_t verbosity)
{
    LOG_VAR_NAME(__MODULE__) = verbosity;
}
