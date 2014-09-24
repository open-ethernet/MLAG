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

#ifndef MLAG_PEERING_FSM_H_
#define MLAG_PEERING_FSM_H_
#include "mlag_manager.h"
typedef int (*notify_peer_done_cb_t) (int peer_id);
typedef int (*notify_peer_start_cb_t) (int peer_id);

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
mlag_peering_fsm_log_verbosity_set(mlag_verbosity_t verbosity);

/*#$*/
#include "fsm_tool_framework.h" 
/*----------------------------------------------------------------
            Generated enumerator for  States of FSM   
---------------------------------------------------------------*/
 enum mlag_peering_fsm_states_t{
    mlag_peering_fsm_configured  = 0, 
    mlag_peering_fsm_peering  = 1, 
    mlag_peering_fsm_peer_up  = 2, 
 };
struct mlag_peering_fsm;
/*----------------------------------------------------------------
            Events of FSM   
---------------------------------------------------------------*/
 int  mlag_peering_fsm_peer_up_ev(struct mlag_peering_fsm  * fsm);
 int  mlag_peering_fsm_peer_conn_ev(struct mlag_peering_fsm  * fsm);
 int  mlag_peering_fsm_peer_down_ev(struct mlag_peering_fsm  * fsm);
 int  mlag_peering_fsm_sync_arrived_ev(struct mlag_peering_fsm  * fsm,  int  sync_type);
/*----------------------------------------------------------------
              Getters for each State   
---------------------------------------------------------------*/
tbool mlag_peering_fsm_configured_in(struct mlag_peering_fsm *fsm);
tbool mlag_peering_fsm_peering_in(struct mlag_peering_fsm *fsm);
tbool mlag_peering_fsm_peer_up_in(struct mlag_peering_fsm *fsm);
/*----------------------------------------------------------------
              Constructor of FSM   
---------------------------------------------------------------*/
int mlag_peering_fsm_init(struct mlag_peering_fsm *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func);
/*###############################################################
            ###### declaration of FSM  #########  
//################################################################*/
typedef struct mlag_peering_fsm  
 {
  struct  fsm_base base;
/*----------------------------------------------------------------
           Private attributes :  
---------------------------------------------------------------*/

/*#$*/
  int       peer_id;
  int       conn_state;
  int       peer_state;
  uint32_t  sync_states;
  notify_peer_done_cb_t peer_sync_done_cb;
  notify_peer_start_cb_t peer_sync_start_cb;
  unsigned char igmp_enabled;
  unsigned char lacp_enabled;
 }mlag_peering_fsm;

#endif /* MLAG_PEERING_FSM_H_ */
