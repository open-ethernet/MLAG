 graph 
 [
  node
   [
     id 0
     label "idle (d) "
 graphics
      [
       x   68     y   35   w   160  h   80  
       type	"roundrectangle"   hasFill	0 outline	"#000080"   outlineWidth	2 
      ] 
  
   ]
  node
   [
     id 1
     label "wait_peers 
 tm 8000   "
 graphics
      [
       x   68     y   35   w   160  h   80  
       type	"roundrectangle"   hasFill	0 outline	"#008000"   outlineWidth	2 
      ] 
  
   ]
  edge
   [
     source 0
     target 1
     label "tr00,ln117 : on_start (NULL) 

    int i;
    int err = 0;
    int res, msg_size =0;

    UNUSED_PARAM(parameter);
    SET_EVENT(mlag_mac_sync_flush_fsm, start_ev);
    struct mac_sync_flush_master_sends_start_event_data * msg = NULL;
    /*fsm->timeouts_cnt =0;*/

    msg = (struct mac_sync_flush_master_sends_start_event_data *)ev->msg;
    /* modify only opcode and send message back to peer*/
    msg->opcode = MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT;
    msg_size =
    (msg->number_mac_params-1) *sizeof(struct mac_sync_mac_params) +
    sizeof(struct mac_sync_flush_master_sends_start_event_data);

    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        fsm->responded_peers[i] = 0;
        mlag_mac_sync_master_logic_is_peer_enabled(i, &amp;res);
        if (res) {
            /*Send message to peer about local flush with flush type*/
            //memcpy(&amp;msg.filter, &amp;ev->type, sizeof(msg.filter));

            err = mlag_mac_sync_dispatcher_message_send(
                MLAG_MAC_SYNC_GLOBAL_FLUSH_MASTER_SENDS_START_EVENT,
                (void *)msg, msg_size, i, MASTER_LOGIC);
            MLAG_BAIL_ERROR(err);
        }
    }
bail:
    return err;
 " 
  graphics  [ width	1  fill 	 "#FF0000"   targetArrow "standard" ] 
  
     LabelGraphics [ fontSize 0   fontName  "Dialog" visible 1  model "free"]
   LabelGraphics 
[
text "start_ev  " fontSize 11   fontName  "Miriam"  model "free" ] 
   ]
  edge
   [
     source 1
     target 0
     label "tr10,ln123 : on_pass_to_idle (NULL) 

   int err = 0;
   UNUSED_PARAM(parameter);

   SET_EVENT(mlag_mac_sync_flush_fsm , stop_ev) ;

   err = mlag_mac_sync_master_logic_notify_fsm_idle(fsm->key);
   MLAG_BAIL_ERROR(err);

 bail:
       return err;
  " 
  graphics  [ width	1  fill 	 "#FF0000"   targetArrow "standard" ] 
  
     LabelGraphics [ fontSize 0   fontName  "Dialog" visible 1  model "free"]
   LabelGraphics 
[
text "stop_ev  " fontSize 11   fontName  "Miriam"  model "free" ] 
   ]
  edge
   [
     source 1
     target 0
     label "tr11,ln124 : on_pass_to_idle (NULL) 

   int err = 0;
   UNUSED_PARAM(parameter);

   SET_EVENT(mlag_mac_sync_flush_fsm , stop_ev) ;

   err = mlag_mac_sync_master_logic_notify_fsm_idle(fsm->key);
   MLAG_BAIL_ERROR(err);

 bail:
       return err;
  " 
  graphics  [ width	1  fill 	 "#FF0000"   targetArrow "standard" ] 
  
     LabelGraphics [ fontSize 0   fontName  "Dialog" visible 1  model "free"]
   LabelGraphics 
[
text "peer_ack 
[ all_peers_ack(fsm, evt) ]" fontSize 11   fontName  "Miriam"  model "free" ] 
   ]
  edge
   [
     source 1
     target 1
     label "tr12,ln125 : NULL " 
  graphics  [ type	"arc"  width 1  fill  "#0000FF"  targetArrow	"standard" arcType	"fixedRatio" arcHeight	27.0 arcRatio	2.0 ] 
 edgeAnchor [ xSource	-0.4 ySource	-1  xTarget	0.5  yTarget	-1 ] 
     LabelGraphics [ fontSize 0   fontName  "Dialog" visible 1  model "free"]
   LabelGraphics 
[
text "peer_ack 
[ else ]" fontSize 11   fontName  "Miriam"  model "free" ] 
   ]
  edge
   [
     source 1
     target 0
     label "tr13,ln127 : on_timer (NULL) 

    int i;
    int err = 0, res;

    UNUSED_PARAM(parameter);

    SET_EVENT(mlag_mac_sync_flush_fsm, TIMER_EVENT);
    for (i = 0; i < MLAG_MAX_PEERS; i++) {
        mlag_mac_sync_master_logic_is_peer_enabled(i, &amp;res);
        if (res &amp;&amp; (fsm->responded_peers[i] == 0)) {
            MLAG_BAIL_ERROR_MSG(MLAG_API_INVALID_PARAM,
                                &quot;Timeout in Global Flush process for peer %d - Fatal!!&quot;,
                                i);
        }
    }

    err = mlag_mac_sync_master_logic_notify_fsm_idle(fsm->key);
    MLAG_BAIL_ERROR(err);
bail:
    return err;
 " 
  graphics  [ width	1  fill 	 "#FF0000"   targetArrow "standard" ] 
  
     LabelGraphics [ fontSize 0   fontName  "Dialog" visible 1  model "free"]
   LabelGraphics 
[
text "TIMER_EVENT  " fontSize 11   fontName  "Miriam"  model "free" ] 
   ]
 ]
