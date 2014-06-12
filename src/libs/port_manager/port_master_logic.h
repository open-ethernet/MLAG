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
#ifndef PORT_MASTER_LOGIC_HH
#define PORT_MASTER_LOGIC_HH

#include <libs/mlag_master_election/mlag_master_election.h>
#include "lib_commu.h"
#include <utils/mlag_events.h>
#include "mlag_comm_layer_wrapper.h"
/**
 *  This function sets module log verbosity level
 *
 *  @param verbosity - new log verbosity
 *
 * @return void
 */
void
port_master_logic_log_verbosity_set(mlag_verbosity_t verbosity);

typedef int (*send_message_cb) (enum mlag_events opcode,
                                void* payload, uint32_t payload_len,
                                uint8_t dest_peer_id,
                                enum message_originator);

/*#$*/
#include "fsm_tool_framework.h"
/*----------------------------------------------------------------
            Generated enumerator for  States of FSM
---------------------------------------------------------------*/
 enum port_master_logic_states_t{
    port_master_logic_idle  = 0,
    port_master_logic_disabled  = 1,
    port_master_logic_global_down  = 2,
    port_master_logic_global_up  = 3,
 };
struct port_master_logic;
/*----------------------------------------------------------------
            Events of FSM
---------------------------------------------------------------*/
 int  port_master_logic_port_add_ev(struct port_master_logic  * fsm,  int  peer_id);
 int  port_master_logic_port_del_ev(struct port_master_logic  * fsm,  int  peer_id);
 int  port_master_logic_peer_down_ev(struct port_master_logic  * fsm,  int  peer_id);
 int  port_master_logic_peer_active_ev(struct port_master_logic  * fsm,  int  peer_id);
 int  port_master_logic_port_up_ev(struct port_master_logic  * fsm,  int  peer_id);
 int  port_master_logic_port_down_ev(struct port_master_logic  * fsm,  int  peer_id);
/*----------------------------------------------------------------
              Getters for each State
---------------------------------------------------------------*/
tbool port_master_logic_idle_in(struct port_master_logic *fsm);
tbool port_master_logic_disabled_in(struct port_master_logic *fsm);
tbool port_master_logic_global_down_in(struct port_master_logic *fsm);
tbool port_master_logic_global_up_in(struct port_master_logic *fsm);
/*----------------------------------------------------------------
              Constructor of FSM
---------------------------------------------------------------*/
int port_master_logic_init(struct port_master_logic *fsm, fsm_user_trace user_trace, void * sched_func, void * unsched_func);
/*###############################################################
            ###### declaration of FSM  #########
//################################################################*/
typedef struct port_master_logic
 {
  struct  fsm_base base;
/*----------------------------------------------------------------
           Private attributes :
---------------------------------------------------------------*/

/*#$*/
    unsigned long port_id;
    send_message_cb message_send_func;
}port_master_logic;

#endif /* PORT_MASTER_LOGIC_HH */
