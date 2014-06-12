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
typedef void (*notify_state_cb_t) (int peer_id, int state_id);
/*#$*/
#include "fsm_tool_framework.h" 
/*----------------------------------------------------------------
            Generated enumerator for  States of FSM   
---------------------------------------------------------------*/
 enum health_fsm_states_t{
    health_fsm_idle  = 0, 
    health_fsm_peer_down  = 1, 
    health_fsm_comm_down  = 2, 
    health_fsm_peer_up  = 3, 
    health_fsm_down_wait  = 4, 
 };
struct health_fsm;
/*----------------------------------------------------------------
            Events of FSM   
---------------------------------------------------------------*/
 int  health_fsm_peer_add_ev(struct health_fsm  * fsm,  int  ipl_id);
 int  health_fsm_peer_del_ev(struct health_fsm  * fsm);
 int  health_fsm_ka_up_ev(struct health_fsm  * fsm);
 int  health_fsm_ka_down_ev(struct health_fsm  * fsm);
 int  health_fsm_mgmt_up_ev(struct health_fsm  * fsm);
 int  health_fsm_mgmt_down_ev(struct health_fsm  * fsm);
 int  health_fsm_ipl_change_ev(struct health_fsm  * fsm,  int  ipl_id);
 int  health_fsm_role_change_ev(struct health_fsm  * fsm);
/*----------------------------------------------------------------
              Getters for each State   
---------------------------------------------------------------*/
tbool health_fsm_idle_in(struct health_fsm *fsm);
tbool health_fsm_peer_down_in(struct health_fsm *fsm);
tbool health_fsm_comm_down_in(struct health_fsm *fsm);
tbool health_fsm_peer_up_in(struct health_fsm *fsm);
tbool health_fsm_down_wait_in(struct health_fsm *fsm);
/*----------------------------------------------------------------
              Constructor of FSM   
---------------------------------------------------------------*/
int health_fsm_init(struct health_fsm *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func);
/*###############################################################
            ###### declaration of FSM  #########  
//################################################################*/
typedef struct health_fsm  
 {
  struct  fsm_base base;
  struct  fsm_tm_base tm;
  int state_timer[5];
/*----------------------------------------------------------------
           Private attributes :  
---------------------------------------------------------------*/

/*#$*/
    int ipl_id;
    int ka_state;
    int peer_id;
    notify_state_cb_t notify_state_cb;
}health_fsm;
