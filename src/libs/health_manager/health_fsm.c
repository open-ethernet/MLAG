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
#include "mlag_api_defs.h"
#include <oes_types.h>
#include "health_manager.h"
#include "health_fsm.h"
#include <libs/mlag_manager/mlag_manager_db.h>
#include <utils/mlag_log.h>




#undef  __MODULE__
#define __MODULE__ MLAG_HEALTH_FSM
/************************************************
 *  Local Defines
 ***********************************************/

static int
get_ipl_state(health_fsm  * fsm);

static int
get_mgmt_state(health_fsm * fsm);

#define UP OES_PORT_UP
#define DOWN OES_PORT_DOWN

#ifdef FSM_COMPILED
#include \


stateMachine
{
health_fsm ()

events {
	peer_add_ev     (int ipl_id)
	peer_del_ev     ()
	ka_up_ev        ()
	ka_down_ev      ()
	mgmt_up_ev      ()
	mgmt_down_ev    ()
    ipl_change_ev	(int ipl_id)
    role_change_ev  ()
}


state idle {
default
 transitions {
    { peer_add_ev     , NULL    , peer_down  ,  on_peer_add() }

  }
xf = on_exit_idle
}
// ***********************************
state peer_down {
 transitions {
 { peer_del_ev  , NULL  			                                        , idle  	, on_peer_del() }
 { mgmt_up_ev	, $ (fsm->ka_state == UP) && (get_ipl_state(fsm) == UP) $   , peer_up	, NULL  }
 { mgmt_up_ev	, $ else $	                                                , IN_STATE	, NULL  }
 { mgmt_down_ev	, NULL				                                        , peer_down	, NULL  }
 { ka_up_ev		, $ (get_mgmt_state(fsm) == MGMT_UP) && (get_ipl_state(fsm) == UP) $
                                                                             , peer_up	, on_ka_up()    }
 { ka_up_ev		, $ else $	                                                , IN_STATE	, on_ka_up()    }
 { ipl_change_ev, $ (get_ipl_state(fsm) == UP)&&(fsm->ka_state == UP)&&(get_mgmt_state(fsm) == MGMT_UP) $
                                                                             , peer_up   , NULL  }
 { ipl_change_ev, NULL                                                      , IN_STATE  , NULL  }
 { role_change_ev, NULL                                                     , IN_STATE  , notify_peer_state(HEALTH_PEER_DOWN)  }
 }
ef = peer_down_entry
}
// ***********************************

state comm_down {
 transitions {
 { peer_del_ev  , NULL  			                        , idle  	, on_peer_del() }
 { mgmt_up_ev   , $ (fsm->ka_state == UP) &&
                    (get_ipl_state(fsm) == UP) $  	        , peer_up 	, NULL  }
 { mgmt_up_ev	, $ else $			                        , IN_STATE 	, NULL  }
 { ka_up_ev	    , $ (get_mgmt_state(fsm) == UP) &&
                    (get_ipl_state(fsm) == UP) $            , peer_up 	, on_ka_up()    }
 { ka_up_ev	    , $ else $			                        , IN_STATE 	, on_ka_up()    }
 { mgmt_down_ev	, NULL				                        , peer_down , NULL}
 { ipl_change_ev, $ (get_ipl_state(fsm) == UP)&&(fsm->ka_state == UP)&&(get_mgmt_state(fsm) == MGMT_UP) $
                                                            , peer_up   , NULL   }
 { ipl_change_ev, NULL                                      , IN_STATE  , NULL  }
 { role_change_ev, NULL                                     , IN_STATE  , notify_peer_state(HEALTH_PEER_COMM_DOWN)  }

 }
 ef = comm_down_entry
}
// ***********************************
state peer_up {
  transitions {
 { peer_del_ev  , NULL  			                        , idle      , on_peer_del() }
 { mgmt_up_ev	, NULL				                        , IN_STATE  , NULL  }
 { mgmt_down_ev , NULL  			                        , IN_STATE  , NULL}
 { ka_down_ev	, $ get_mgmt_state(fsm) == MGMT_DOWN $ 	    , peer_down , on_ka_down()  }
 { ka_down_ev	, $ else $			                        , down_wait , on_ka_down()  }
 { ipl_change_ev, $ (get_ipl_state(fsm) == DOWN)
                   && (get_mgmt_state(fsm) == MGMT_DOWN) $	, peer_down , NULL }
 { ipl_change_ev, $ (get_ipl_state(fsm) == DOWN) $	        , down_wait , NULL }
 { ipl_change_ev, $ else $                                  , IN_STATE  , NULL }
 { role_change_ev, NULL                                     , IN_STATE  , notify_peer_state(HEALTH_PEER_UP)  }
 }
ef = peer_up_entry
}

// ***********************************

state down_wait {
tm = HEALTH_PEER_DOWN_WAIT_TIMER_MS
 transitions {
 { peer_del_ev  , NULL    			                , idle  	, on_peer_del() }
 { mgmt_down_ev , NULL          		            , peer_down , NULL }
 { ka_down_ev	, NULL				                , IN_STATE 	, on_ka_down() }
 { ka_up_ev	    , $ (get_ipl_state(fsm) == UP) $	, peer_up	, on_down_ka_up() }
 { ipl_change_ev, $ (get_ipl_state(fsm) == DOWN) $	, IN_STATE	, NULL }
 { ipl_change_ev, $ (get_ipl_state(fsm) == UP) &&
             (fsm->ka_state == UP) $	            , peer_up	, on_down_ipl_up() }
 { TIMER_EVENT	, NULL				                , comm_down	, NULL }
 }
ef = down_wait_entry
}

}// end of stateMachine


/* *********************************** */
#endif

/*#$*/
 /* The code  inside placeholders  generated by tool and cannot be modifyed*/


/*----------------------------------------------------------------
            Generated enumerator for  Events of FSM   
---------------------------------------------------------------*/
enum health_fsm_events_t{
    peer_add_ev  = 0, 
    peer_del_ev  = 1, 
    ka_up_ev  = 2, 
    ka_down_ev  = 3, 
    mgmt_up_ev  = 4, 
    mgmt_down_ev  = 5, 
    ipl_change_ev  = 6, 
    role_change_ev  = 7, 
 };

/*----------------------------------------------------------------
            Generated structures for  Events of FSM   
---------------------------------------------------------------*/
struct  health_fsm_peer_add_ev_t
{
    int  opcode;
    const char *name;
    int  ipl_id ; 
};

struct  health_fsm_peer_del_ev_t
{
    int  opcode;
    const char *name;
};

struct  health_fsm_ka_up_ev_t
{
    int  opcode;
    const char *name;
};

struct  health_fsm_ka_down_ev_t
{
    int  opcode;
    const char *name;
};

struct  health_fsm_mgmt_up_ev_t
{
    int  opcode;
    const char *name;
};

struct  health_fsm_mgmt_down_ev_t
{
    int  opcode;
    const char *name;
};

struct  health_fsm_ipl_change_ev_t
{
    int  opcode;
    const char *name;
    int  ipl_id ; 
};

struct  health_fsm_role_change_ev_t
{
    int  opcode;
    const char *name;
};

/*----------------------------------------------------------------
           State entry/exit functions prototypes  
---------------------------------------------------------------*/
static int idle_exit_func(health_fsm *fsm, struct fsm_event_base *ev);
static int peer_down_entry_func(health_fsm *fsm, struct fsm_event_base *ev);
static int comm_down_entry_func(health_fsm *fsm, struct fsm_event_base *ev);
static int peer_up_entry_func(health_fsm *fsm, struct fsm_event_base *ev);
static int down_wait_entry_func(health_fsm *fsm, struct fsm_event_base *ev);
/*----------------------------------------------------------------
            Generated functions for  Events of FSM   
---------------------------------------------------------------*/
int   health_fsm_peer_add_ev(struct health_fsm  * fsm,  int  ipl_id)
{
     struct health_fsm_peer_add_ev_t ev;
     ev.opcode  =  peer_add_ev ;
     ev.name    = "peer_add_ev" ;
     ev.ipl_id = ipl_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_peer_del_ev(struct health_fsm  * fsm)
{
     struct health_fsm_peer_del_ev_t ev;
     ev.opcode  =  peer_del_ev ;
     ev.name    = "peer_del_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_ka_up_ev(struct health_fsm  * fsm)
{
     struct health_fsm_ka_up_ev_t ev;
     ev.opcode  =  ka_up_ev ;
     ev.name    = "ka_up_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_ka_down_ev(struct health_fsm  * fsm)
{
     struct health_fsm_ka_down_ev_t ev;
     ev.opcode  =  ka_down_ev ;
     ev.name    = "ka_down_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_mgmt_up_ev(struct health_fsm  * fsm)
{
     struct health_fsm_mgmt_up_ev_t ev;
     ev.opcode  =  mgmt_up_ev ;
     ev.name    = "mgmt_up_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_mgmt_down_ev(struct health_fsm  * fsm)
{
     struct health_fsm_mgmt_down_ev_t ev;
     ev.opcode  =  mgmt_down_ev ;
     ev.name    = "mgmt_down_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_ipl_change_ev(struct health_fsm  * fsm,  int  ipl_id)
{
     struct health_fsm_ipl_change_ev_t ev;
     ev.opcode  =  ipl_change_ev ;
     ev.name    = "ipl_change_ev" ;
     ev.ipl_id = ipl_id ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
int   health_fsm_role_change_ev(struct health_fsm  * fsm)
{
     struct health_fsm_role_change_ev_t ev;
     ev.opcode  =  role_change_ev ;
     ev.name    = "role_change_ev" ;

     return fsm_handle_event(&fsm->base, (struct fsm_event_base *)&ev);
}
/*----------------------------------------------------------------
                 Reactions of FSM    
---------------------------------------------------------------*/
static  int on_peer_add (health_fsm  * fsm, int parameter, struct fsm_event_base *event);
static  int on_peer_del (health_fsm  * fsm, int parameter, struct fsm_event_base *event);
static  int on_ka_up (health_fsm  * fsm, int parameter, struct fsm_event_base *event);
static  int notify_peer_state (health_fsm  * fsm, int parameter, struct fsm_event_base *event);
static  int on_ka_down (health_fsm  * fsm, int parameter, struct fsm_event_base *event);
static  int on_down_ka_up (health_fsm  * fsm, int parameter, struct fsm_event_base *event);
static  int on_down_ipl_up (health_fsm  * fsm, int parameter, struct fsm_event_base *event);


/*----------------------------------------------------------------
           State Dispatcher function  
---------------------------------------------------------------*/
static int health_fsm_state_dispatcher( health_fsm  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state );
/*----------------------------------------------------------------
           Initialized  State classes of FSM  
---------------------------------------------------------------*/
 struct fsm_state_base            state_health_fsm_idle; 
 struct fsm_state_base            state_health_fsm_peer_down; 
 struct fsm_state_base            state_health_fsm_comm_down; 
 struct fsm_state_base            state_health_fsm_peer_up; 
 struct fsm_state_base            state_health_fsm_down_wait; 


 struct fsm_state_base      state_health_fsm_idle ={ "idle", health_fsm_idle , SIMPLE, NULL, NULL, &health_fsm_state_dispatcher, NULL, idle_exit_func };

 struct fsm_state_base      state_health_fsm_peer_down ={ "peer_down", health_fsm_peer_down , SIMPLE, NULL, NULL, &health_fsm_state_dispatcher, peer_down_entry_func , NULL };

 struct fsm_state_base      state_health_fsm_comm_down ={ "comm_down", health_fsm_comm_down , SIMPLE, NULL, NULL, &health_fsm_state_dispatcher, comm_down_entry_func , NULL };

 struct fsm_state_base      state_health_fsm_peer_up ={ "peer_up", health_fsm_peer_up , SIMPLE, NULL, NULL, &health_fsm_state_dispatcher, peer_up_entry_func , NULL };

 struct fsm_state_base      state_health_fsm_down_wait ={ "down_wait", health_fsm_down_wait , SIMPLE, NULL, NULL, &health_fsm_state_dispatcher, down_wait_entry_func , NULL };

static struct fsm_static_data static_data= {0,0,1,0,0 ,{ &state_health_fsm_idle,  &state_health_fsm_peer_down,  &state_health_fsm_comm_down, 
                                                  &state_health_fsm_peer_up,  &state_health_fsm_down_wait, }
};

 static struct fsm_state_base * default_state = &state_health_fsm_idle ;
/*----------------------------------------------------------------
           StateDispatcher of FSM  
---------------------------------------------------------------*/

static int health_fsm_state_dispatcher( health_fsm  * fsm, uint16 state, struct fsm_event_base *evt, struct fsm_state_base** target_state )
{
  int err=0;
  struct fsm_timer_event *event = (  struct fsm_timer_event * )evt;
  switch(state) 
  {

   case health_fsm_idle : 
    {
      if ( event->opcode == peer_add_ev )
      {
         SET_EVENT(health_fsm , peer_add_ev)
         {/*tr00:*/
            err = on_peer_add( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 59*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

    }break;

   case health_fsm_peer_down : 
    {
      if ( event->opcode == peer_del_ev )
      {
         SET_EVENT(health_fsm , peer_del_ev)
         {/*tr10:*/
            err = on_peer_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 67*/ 
            *target_state =  &state_health_fsm_idle;
            return err;
         }
      }

      else if ( event->opcode == mgmt_up_ev )
      {
         SET_EVENT(health_fsm , mgmt_up_ev)
         if(  (fsm->ka_state == UP) && (get_ipl_state(fsm) == UP)  )
         {/*tr11:*/
              /* Source line = 68*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
         else
         {/*tr12:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 69*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

      else if ( event->opcode == mgmt_down_ev )
      {
         SET_EVENT(health_fsm , mgmt_down_ev)
         {/*tr13:*/
              /* Source line = 70*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

      else if ( event->opcode == ka_up_ev )
      {
         SET_EVENT(health_fsm , ka_up_ev)
         if(  (get_mgmt_state(fsm) == MGMT_UP) && (get_ipl_state(fsm) == UP)  )
         {/*tr14:*/
            err = on_ka_up( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 72*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
         else
         {/*tr15:*/
           fsm->base.reaction_in_state = 1;
           err = on_ka_up( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 73*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

      else if ( event->opcode == ipl_change_ev )
      {
         SET_EVENT(health_fsm , ipl_change_ev)
         if(  (get_ipl_state(fsm) == UP)&&(fsm->ka_state == UP)&&(get_mgmt_state(fsm) == MGMT_UP)  )
         {/*tr16:*/
              /* Source line = 75*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
         {/*tr17:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 76*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

      else if ( event->opcode == role_change_ev )
      {
         SET_EVENT(health_fsm , role_change_ev)
         {/*tr18:*/
           fsm->base.reaction_in_state = 1;
           err = notify_peer_state( fsm, HEALTH_PEER_DOWN, evt);  /* Source line = 77*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

    }break;

   case health_fsm_comm_down : 
    {
      if ( event->opcode == peer_del_ev )
      {
         SET_EVENT(health_fsm , peer_del_ev)
         {/*tr20:*/
            err = on_peer_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 85*/ 
            *target_state =  &state_health_fsm_idle;
            return err;
         }
      }

      else if ( event->opcode == mgmt_up_ev )
      {
         SET_EVENT(health_fsm , mgmt_up_ev)
         if(  (fsm->ka_state == UP) &&
                    (get_ipl_state(fsm) == UP)  )
         {/*tr21:*/
              /* Source line = 87*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
         else
         {/*tr22:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 88*/ 
            *target_state =  &state_health_fsm_comm_down;
            return err;
         }
      }

      else if ( event->opcode == ka_up_ev )
      {
         SET_EVENT(health_fsm , ka_up_ev)
         if(  (get_mgmt_state(fsm) == UP) &&
                    (get_ipl_state(fsm) == UP)  )
         {/*tr23:*/
            err = on_ka_up( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 90*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
         else
         {/*tr24:*/
           fsm->base.reaction_in_state = 1;
           err = on_ka_up( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 91*/ 
            *target_state =  &state_health_fsm_comm_down;
            return err;
         }
      }

      else if ( event->opcode == mgmt_down_ev )
      {
         SET_EVENT(health_fsm , mgmt_down_ev)
         {/*tr25:*/
              /* Source line = 92*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

      else if ( event->opcode == ipl_change_ev )
      {
         SET_EVENT(health_fsm , ipl_change_ev)
         if(  (get_ipl_state(fsm) == UP)&&(fsm->ka_state == UP)&&(get_mgmt_state(fsm) == MGMT_UP)  )
         {/*tr26:*/
              /* Source line = 94*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
         {/*tr27:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 95*/ 
            *target_state =  &state_health_fsm_comm_down;
            return err;
         }
      }

      else if ( event->opcode == role_change_ev )
      {
         SET_EVENT(health_fsm , role_change_ev)
         {/*tr28:*/
           fsm->base.reaction_in_state = 1;
           err = notify_peer_state( fsm, HEALTH_PEER_COMM_DOWN, evt);  /* Source line = 96*/ 
            *target_state =  &state_health_fsm_comm_down;
            return err;
         }
      }

    }break;

   case health_fsm_peer_up : 
    {
      if ( event->opcode == peer_del_ev )
      {
         SET_EVENT(health_fsm , peer_del_ev)
         {/*tr30:*/
            err = on_peer_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 104*/ 
            *target_state =  &state_health_fsm_idle;
            return err;
         }
      }

      else if ( event->opcode == mgmt_up_ev )
      {
         SET_EVENT(health_fsm , mgmt_up_ev)
         {/*tr31:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 105*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
      }

      else if ( event->opcode == mgmt_down_ev )
      {
         SET_EVENT(health_fsm , mgmt_down_ev)
         {/*tr32:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 106*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
      }

      else if ( event->opcode == ka_down_ev )
      {
         SET_EVENT(health_fsm , ka_down_ev)
         if(  get_mgmt_state(fsm) == MGMT_DOWN  )
         {/*tr33:*/
            err = on_ka_down( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 107*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
         else
         {/*tr34:*/
            err = on_ka_down( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 108*/ 
            *target_state =  &state_health_fsm_down_wait;
            return err;
         }
      }

      else if ( event->opcode == ipl_change_ev )
      {
         SET_EVENT(health_fsm , ipl_change_ev)
         if(  (get_ipl_state(fsm) == DOWN)
                   && (get_mgmt_state(fsm) == MGMT_DOWN)  )
         {/*tr35:*/
              /* Source line = 110*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
         else
         if(  (get_ipl_state(fsm) == DOWN)  )
         {/*tr36:*/
              /* Source line = 111*/ 
            *target_state =  &state_health_fsm_down_wait;
            return err;
         }
         else
         {/*tr37:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 112*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
      }

      else if ( event->opcode == role_change_ev )
      {
         SET_EVENT(health_fsm , role_change_ev)
         {/*tr38:*/
           fsm->base.reaction_in_state = 1;
           err = notify_peer_state( fsm, HEALTH_PEER_UP, evt);  /* Source line = 113*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
      }

    }break;

   case health_fsm_down_wait : 
    {
      if ( event->opcode == peer_del_ev )
      {
         SET_EVENT(health_fsm , peer_del_ev)
         {/*tr40:*/
            err = on_peer_del( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 123*/ 
            *target_state =  &state_health_fsm_idle;
            return err;
         }
      }

      else if ( event->opcode == mgmt_down_ev )
      {
         SET_EVENT(health_fsm , mgmt_down_ev)
         {/*tr41:*/
              /* Source line = 124*/ 
            *target_state =  &state_health_fsm_peer_down;
            return err;
         }
      }

      else if ( event->opcode == ka_down_ev )
      {
         SET_EVENT(health_fsm , ka_down_ev)
         {/*tr42:*/
           fsm->base.reaction_in_state = 1;
           err = on_ka_down( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 125*/ 
            *target_state =  &state_health_fsm_down_wait;
            return err;
         }
      }

      else if ( event->opcode == ka_up_ev )
      {
         SET_EVENT(health_fsm , ka_up_ev)
         if(  (get_ipl_state(fsm) == UP)  )
         {/*tr43:*/
            err = on_down_ka_up( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 126*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
      }

      else if ( event->opcode == ipl_change_ev )
      {
         SET_EVENT(health_fsm , ipl_change_ev)
         if(  (get_ipl_state(fsm) == DOWN)  )
         {/*tr44:*/
           fsm->base.reaction_in_state = 1;
             /* Source line = 127*/ 
            *target_state =  &state_health_fsm_down_wait;
            return err;
         }
         else
         if(  (get_ipl_state(fsm) == UP) &&
             (fsm->ka_state == UP)  )
         {/*tr45:*/
            err = on_down_ipl_up( fsm, DEFAULT_PARAMS_D, evt);  /* Source line = 129*/ 
            *target_state =  &state_health_fsm_peer_up;
            return err;
         }
      }

      else if (( event->opcode == TIMER_EVENT )&& (event->id == health_fsm_down_wait) )
      {
         {/*tr46:*/
              /* Source line = 130*/ 
            *target_state =  &state_health_fsm_comm_down;
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
int health_fsm_init(struct health_fsm *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func)
 {
       fsm->state_timer[health_fsm_idle] = 0 ;
       fsm->state_timer[health_fsm_peer_down] = 0 ;
       fsm->state_timer[health_fsm_comm_down] = 0 ;
       fsm->state_timer[health_fsm_peer_up] = 0 ;
       fsm->state_timer[health_fsm_down_wait] = HEALTH_PEER_DOWN_WAIT_TIMER_MS ;

       fsm_init(&fsm->base,  default_state, 5, fsm->state_timer, user_trace, sched_func, unsched_func, &static_data);

      return 0;
 }
 /*  per state functions*/
/*----------------------------------------------------------------
              Getters for each State   
---------------------------------------------------------------*/
tbool health_fsm_idle_in(struct health_fsm *fsm){
    return fsm_is_in_state(&fsm->base, health_fsm_idle);
}
tbool health_fsm_peer_down_in(struct health_fsm *fsm){
    return fsm_is_in_state(&fsm->base, health_fsm_peer_down);
}
tbool health_fsm_comm_down_in(struct health_fsm *fsm){
    return fsm_is_in_state(&fsm->base, health_fsm_comm_down);
}
tbool health_fsm_peer_up_in(struct health_fsm *fsm){
    return fsm_is_in_state(&fsm->base, health_fsm_peer_up);
}
tbool health_fsm_down_wait_in(struct health_fsm *fsm){
    return fsm_is_in_state(&fsm->base, health_fsm_down_wait);
}
/*----------------------------------------------------------------
                 Reactions of FSM  (within comments . user may paste function body outside placeholder region)  
---------------------------------------------------------------*/

/*static  int on_peer_add (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , peer_add_ev) ; 
 }
static  int on_peer_del (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , peer_del_ev) ; 
 }
static  int on_ka_up (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , ka_up_ev) ; 
 }
static  int notify_peer_state (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , role_change_ev) ; 
 }
static  int on_ka_down (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , ka_down_ev) ; 
 }
static  int on_down_ka_up (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , ka_up_ev) ; 
 }
static  int on_down_ipl_up (health_fsm  * fsm, int parameter, struct fsm_event_base *event)
 {
   SET_EVENT(health_fsm , ipl_change_ev) ; 
 }


*/
/*   20664*/

/*#$*/

/* Below code written by user */

/*
 * This function is called upon exit of idle state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
idle_exit_func(health_fsm *fsm, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    fsm->ka_state = DOWN;

    return err;
}

/*
 * This function is called upon entry to peer_down state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
peer_down_entry_func(health_fsm *fsm, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    if (fsm->notify_state_cb) {
            fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_DOWN);
    	}

    return err;
}

/*
 * This function is called upon entry to comm_down state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
comm_down_entry_func(health_fsm *fsm, struct fsm_event_base *event)
{
    UNUSED_PARAM(event);
    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_COMM_DOWN);
    }
    return 0;
}

/*
 * This function is called upon entry to peer_up state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
peer_up_entry_func(health_fsm *fsm, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_UP);
    }
    return err;
}

/*
 * This function is called upon entry to down_wait state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] ev - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
down_wait_entry_func(health_fsm *fsm, struct fsm_event_base *ev)
{
    int err = 0;
    UNUSED_PARAM(ev);
    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_DOWN_WAIT);
    }
    return err;
}
/*
 * This function is called as reaction to peer add event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_peer_add(health_fsm  * fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(parameter);
    SET_EVENT(health_fsm, peer_add_ev);
    fsm->ipl_id = ev->ipl_id;
    return err;
}

/*
 * This function is called as reaction to peer del event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_peer_del(health_fsm  * fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_DOWN);
    }
    return err;
}

/*
 * This function is called as reaction to ka up event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_ka_up(health_fsm  * fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    fsm->ka_state = UP;
    return err;
}


/*
 * This function is called as reaction to ka down event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_ka_down(health_fsm  * fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    fsm->ka_state = DOWN;
    return err;
}

/*
 * This function is called as reaction to role change event
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - the current state
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
notify_peer_state(health_fsm  * fsm, int parameter,
                  struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);

    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, parameter);
    }

    return err;
}

/*
 * This function is called as reaction to ka up event, when in down wait state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_down_ka_up(health_fsm  * fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    fsm->ka_state = UP;
    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_COMM_DOWN);
    }
    return err;
}

/*
 * This function is called as reaction to ipl up event, when in down wait state
 *
 * @param[in] fsm - fsm pointer
 * @param[in] parameter - an optional parameter that can be passed
 * @param[in] event - base event pointer
 *
 * @return 0 if operation completes successfully.
 */
static int
on_down_ipl_up(health_fsm  * fsm, int parameter, struct fsm_event_base *event)
{
    int err = 0;
    UNUSED_PARAM(event);
    UNUSED_PARAM(parameter);
    if (fsm->notify_state_cb) {
        fsm->notify_state_cb(fsm->peer_id, HEALTH_PEER_COMM_DOWN);
    }
    return err;
}

/*
 * This function returns the current state of the FSM's IPL
 *
 * @param[in] fsm - fsm pointer
 *
 * @return Up/DOWN according to IPL state.
 */
static int
get_ipl_state(health_fsm  * fsm)
{
    return health_manager_ipl_state_get(fsm->ipl_id);
}

/*
 * This function returns the current state of the FSM's IPL
 *
 * @param[in] fsm - fsm pointer
 *
 * @return Up/DOWN according to IPL state.
 */
static int
get_mgmt_state(health_fsm  * fsm)
{
    return health_manager_mgmt_state_get(fsm->peer_id);
}
