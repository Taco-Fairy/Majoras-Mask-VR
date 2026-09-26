// Two repeatable debug-only merchants; native skeleton, talk acquisition, item
// presentation and message lifetime remain in the original game's systems.
void ExchangeReply(EnSellnuts* npc,PlayState* play,int action,bool animated){
 const bool accepted=action==PLAYER_IA_MOONS_TEAR;
 const char* text=action<=PLAYER_IA_MINUS1?"Exchange canceled. Talk again to retry.":
  accepted?"Moon's Tear accepted! Item kept for repeat testing.":"Wrong item. I need Moon's Tear. Item kept; talk again to retry.";
 CustomMessage::SetActiveCustomMessage(text);
 auto* p=GET_PLAYER(play);p->actor.textId=CUSTOM_MESSAGE_ID;
 if(animated)Message_CloseTextbox(play); // Native give animation opens the reply.
 else Message_ContinueTextbox(play,CUSTOM_MESSAGE_ID);
 npc->unk_366=2;
 Log()<<"exchange-station mode="<<(npc->actor.params&1)<<" action="<<action<<" accepted="<<accepted<<" canceled="<<(action<=PLAYER_IA_MINUS1)<<"\n"<<std::flush;
}
void ExchangeNpcAction(EnSellnuts* npc,PlayState* play){
 auto* p=GET_PLAYER(play);
 Math_SmoothStepToS(&npc->actor.shape.rot.y,npc->actor.yawTowardsPlayer,3,0x7D0,0);
 npc->actor.world.rot.y=npc->actor.shape.rot.y;
 if(npc->unk_366==2){
  if(Message_GetState(&play->msgCtx)==TEXT_STATE_EVENT&&Message_ShouldAdvance(play))Message_CloseTextbox(play);
  if(play->msgCtx.msgMode==MSGMODE_NONE&&!(p->stateFlags1&PLAYER_STATE1_TALKING))npc->unk_366=0;
  return;
 }
 if(npc->unk_366==1){
  if(Message_GetState(&play->msgCtx)==TEXT_STATE_PAUSE_MENU){
   const int action=func_80123810(play);
   if(action!=PLAYER_IA_NONE)ExchangeReply(npc,play,action,action>PLAYER_IA_NONE);
  }
  if(play->msgCtx.msgMode==MSGMODE_NONE&&!(p->stateFlags1&PLAYER_STATE1_TALKING))npc->unk_366=0;
  return;
 }
 if(Actor_TalkOfferAccepted(&npc->actor,&play->state)){
  if(Player_GetExchangeItemAction(play)>PLAYER_IA_NONE){
   ExchangeReply(npc,play,Player_GetExchangeItemAction(play),true);
  }else{
   Message_StartTextbox(play,0xFF,&npc->actor);npc->unk_366=1;
  }
 }else{
  npc->actor.textId=0xFF;
  Actor_OfferTalkExchangeEquiCylinder(&npc->actor,play,110.f,
    PLAYER_IA_MOONS_TEAR);
 }
}
void SupplyExchangeItem(PlayState* play){
 // Intentional test-pad action, never a campaign flag or automatic save rewrite.
 gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED]=ITEM_MOONS_TEAR;
 for(int b=EQUIP_SLOT_C_LEFT;b<=EQUIP_SLOT_C_RIGHT;++b)if(C_SLOT_EQUIP(0,b)==SLOT_TRADE_DEED){
  BUTTON_ITEM_EQUIP(0,b)=ITEM_MOONS_TEAR;Interface_LoadItemIcon(play,b);
 }
 Log()<<"exchange-supply item="<<ITEM_MOONS_TEAR<<"\n"<<std::flush;
}
