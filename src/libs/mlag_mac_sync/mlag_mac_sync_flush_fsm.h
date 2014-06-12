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

#ifndef _MAC_SYNC_FLUSH_FSM_H
#define _MAC_SYNC_FLUSH_FSM_H

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void mlag_mac_sync_flush_fsm_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return void
 */
mlag_verbosity_t mlag_mac_sync_flush_fsm_log_verbosity_get(void);




/*#$*/
#include "fsm_tool_framework.h" 
/*----------------------------------------------------------------
            Generated enumerator for  States of FSM   
---------------------------------------------------------------*/
 enum mlag_mac_sync_flush_fsm_states_t{
    mlag_mac_sync_flush_fsm_idle  = 0, 
    mlag_mac_sync_flush_fsm_wait_peers  = 1, 
 };
struct mlag_mac_sync_flush_fsm;
/*----------------------------------------------------------------
            Events of FSM   
---------------------------------------------------------------*/
 int  mlag_mac_sync_flush_fsm_start_ev(struct mlag_mac_sync_flush_fsm  * fsm,  void*  msg);
 int  mlag_mac_sync_flush_fsm_stop_ev(struct mlag_mac_sync_flush_fsm  * fsm);
 int  mlag_mac_sync_flush_fsm_peer_ack(struct mlag_mac_sync_flush_fsm  * fsm,  int  peer_id);
/*----------------------------------------------------------------
              Getters for each State   
---------------------------------------------------------------*/
tbool mlag_mac_sync_flush_fsm_idle_in(struct mlag_mac_sync_flush_fsm *fsm);
tbool mlag_mac_sync_flush_fsm_wait_peers_in(struct mlag_mac_sync_flush_fsm *fsm);
/*----------------------------------------------------------------
              Constructor of FSM   
---------------------------------------------------------------*/
int mlag_mac_sync_flush_fsm_init(struct mlag_mac_sync_flush_fsm *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func);
/*###############################################################
            ###### declaration of FSM  #########  
//################################################################*/
typedef struct mlag_mac_sync_flush_fsm  
 {
  struct  fsm_base base;
  struct  fsm_tm_base tm;
  int state_timer[2];
/*----------------------------------------------------------------
           Private attributes :  
---------------------------------------------------------------*/

/*#$*/
    int responded_peers[MLAG_MAX_PEERS];
    uint64_t key;
    /*  int timeouts_cnt;*/
} mlag_mac_sync_flush_fsm;

/**
 *  This function prints mac sync flush fsm data
 *
 *  @param [in] fsm - pointer to state machine data structure
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_mac_sync_flush_fsm_print(struct mlag_mac_sync_flush_fsm *fsm);
/*
 *  This function verifies whether FSM is "busy" with flush
 *
 * @param[in] fsm   - pointer to the flush FSM instance
 * @param[out] busy - pointer to the timer event
 *
 * @return 0 when successful, otherwise ERROR
 */

int mlag_mac_sync_flush_is_busy(struct mlag_mac_sync_flush_fsm *fsm,
                                int *busy);


#endif /* _MAC_SYNC_FLUSH_FSM_H */
