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
#ifndef PORT_PEER_REMOTE_HH
#define PORT_PEER_REMOTE_HH

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_peer_remote_log_verbosity_set(mlag_verbosity_t verbosity);

/*#$*/
#include "fsm_tool_framework.h"
/*----------------------------------------------------------------
            Generated enumerator for  States of FSM
---------------------------------------------------------------*/
 enum port_peer_remote_states_t{
    port_peer_remote_idle  = 0,
    port_peer_remote_global_down  = 1,
    port_peer_remote_remote_fault  = 2,
    port_peer_remote_remotes_up  = 3,
 };
struct port_peer_remote;
/*----------------------------------------------------------------
            Events of FSM
---------------------------------------------------------------*/
 int  port_peer_remote_port_add_ev(struct port_peer_remote  * fsm,  int  peer_id);
 int  port_peer_remote_port_del_ev(struct port_peer_remote  * fsm,  int  peer_id);
 int  port_peer_remote_port_global_en(struct port_peer_remote  * fsm);
 int  port_peer_remote_port_global_dis(struct port_peer_remote  * fsm);
 int  port_peer_remote_port_global_down(struct port_peer_remote  * fsm);
 int  port_peer_remote_peer_port_down_ev(struct port_peer_remote  * fsm,  int  peer_id);
 int  port_peer_remote_peer_port_up_ev(struct port_peer_remote  * fsm,  int  peer_id);
 int  port_peer_remote_peer_down_ev(struct port_peer_remote  * fsm,  int  peer_id);
 int  port_peer_remote_peer_enable_ev(struct port_peer_remote  * fsm,  int  peer_id);
/*----------------------------------------------------------------
              Getters for each State
---------------------------------------------------------------*/
tbool port_peer_remote_idle_in(struct port_peer_remote *fsm);
tbool port_peer_remote_global_down_in(struct port_peer_remote *fsm);
tbool port_peer_remote_remote_fault_in(struct port_peer_remote *fsm);
tbool port_peer_remote_remotes_up_in(struct port_peer_remote *fsm);
/*----------------------------------------------------------------
              Constructor of FSM
---------------------------------------------------------------*/
int port_peer_remote_init(struct port_peer_remote *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func);
/*###############################################################
            ###### declaration of FSM  #########
//################################################################*/
typedef struct port_peer_remote
 {
  struct  fsm_base base;
/*----------------------------------------------------------------
           Private attributes :
---------------------------------------------------------------*/

/*#$*/
    unsigned long port_id;
    int isolated;
}port_peer_remote;

#endif /* PORT_PEER_REMOTE_HH */
