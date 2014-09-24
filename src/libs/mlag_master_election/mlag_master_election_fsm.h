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

#ifndef _MASTER_ELECTIONS_FSM_H
#define _MASTER_ELECTIONS_FSM_H

/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void mlag_master_election_fsm_log_verbosity_set(mlag_verbosity_t verbosity);

/**
 *  This function gets module log verbosity level
 *
 * @return void
 */
mlag_verbosity_t mlag_master_election_fsm_log_verbosity_get(void);


/*#$*/
#include "fsm_tool_framework.h"
/*----------------------------------------------------------------
            Generated enumerator for  States of FSM
   ---------------------------------------------------------------*/
enum mlag_master_election_fsm_states_t {
    mlag_master_election_fsm_idle = 0,
    mlag_master_election_fsm_master = 1,
    mlag_master_election_fsm_slave = 2,
    mlag_master_election_fsm_standalone = 3,
    mlag_master_election_fsm_is_master = 4,
    mlag_master_election_fsm_mlag_enable = 5,
};
struct mlag_master_election_fsm;
/*----------------------------------------------------------------
            Events of FSM
   ---------------------------------------------------------------*/
int mlag_master_election_fsm_start_ev(struct mlag_master_election_fsm  * fsm);
int mlag_master_election_fsm_stop_ev(struct mlag_master_election_fsm  * fsm);
int mlag_master_election_fsm_config_change_ev(
    struct mlag_master_election_fsm  * fsm);
int mlag_master_election_fsm_peer_status_change_ev(
    struct mlag_master_election_fsm  * fsm,  int peer_id,  int state);
/*----------------------------------------------------------------
              Getters for each State
   ---------------------------------------------------------------*/
tbool mlag_master_election_fsm_idle_in(struct mlag_master_election_fsm *fsm);
tbool mlag_master_election_fsm_master_in(struct mlag_master_election_fsm *fsm);
tbool mlag_master_election_fsm_slave_in(struct mlag_master_election_fsm *fsm);
tbool mlag_master_election_fsm_standalone_in(
    struct mlag_master_election_fsm *fsm);
tbool mlag_master_election_fsm_is_master_in(
    struct mlag_master_election_fsm *fsm);
tbool mlag_master_election_fsm_mlag_enable_in(
    struct mlag_master_election_fsm *fsm);
/*----------------------------------------------------------------
              Constructor of FSM
   ---------------------------------------------------------------*/
int mlag_master_election_fsm_init(struct mlag_master_election_fsm *fsm,
                                  fsm_user_trace user_trace, void * sched_func,
                                  void * unsched_func);
/*###############################################################
 ###### declaration of FSM  #########
   //################################################################*/
typedef struct mlag_master_election_fsm {
    struct  fsm_base base;
/*----------------------------------------------------------------
           Private attributes :
   ---------------------------------------------------------------*/

/*#$*/
    int current_status;
    int previous_status;
    uint32 my_ip_addr;
    uint32 peer_ip_addr;
    uint8 peer_status;
    uint8 my_peer_id;
    uint8 master_peer_id;
} mlag_master_election_fsm;

/**
 *  This function prints master election fsm data
 *
 *  @param [in] fsm - pointer to state machine data structure
 *  @param [in] dump_cb - callback for dumping, if NULL, log will be used
 *
 * @return 0 when successful, otherwise ERROR
 */
int mlag_master_election_fsm_print(struct mlag_master_election_fsm *fsm, void (*dump_cb)(
                                       const char *,
                                       ...));

#endif /* _MASTER_ELECTIONS_FSM_H */
