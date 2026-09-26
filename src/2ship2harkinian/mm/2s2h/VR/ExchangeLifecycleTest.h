#pragma once
extern "C" int MMVR_OfferingItem(Player*);
#include "ClimbLifecycleTest.h"
#include "overlays/actors/ovl_En_Sellnuts/z_en_sellnuts.h"
static mmvr::Pad NativeExchangeLifecycle(PlayState* play,int tick){
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
 static std::ofstream log("native-exchange-lifecycle.log");
 static int test=0,start=0,pressed=0,promptStart=0;static bool sawReply=false,sawPrompt=false;
 static bool cameraOffsetReady=false;static float cameraOffset=0;static mmvr::TrackingFrame frame;
 if(tick==1){const char* token=std::getenv("MMVR_SESSION_TOKEN");log<<"session="<<(token?token:"")<<"\n";}
 if(tick<30)return pad;
 mmvr::SetNativeTestTracking(true);
 if(!MMVR_DebugRoomActive(play)){log<<"FAIL wrong-room\n";log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
 const int scenario=test%6;const bool pre=scenario>=3,cancel=scenario==2,wrong=scenario==1||scenario==4;const bool station=test<6?pre:!pre;const int hand=1-(test%2);
 EnSellnuts* npc=nullptr;
 for(auto* a=play->actorCtx.actorLists[ACTORCAT_NPC].first;a;a=a->next)if(a->id==ACTOR_EN_SELLNUTS&&a->update&&(a->params&1)==station)npc=reinterpret_cast<EnSellnuts*>(a);
 if(!npc){log<<"FAIL missing-npc\n";log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
 if(!start){
  start=tick;pressed=0;promptStart=0;sawReply=sawPrompt=false;cameraOffsetReady=false;cameraOffset=0;mmvr::SetNativeTestTracking(true);
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,test%2);CVarSetFloat("gVR.SwordLeftHanded",test%2);
  // Establish a clean camera baseline away from the NPC. Move into interaction
  // range later so the fixture can distinguish setup/grounding from dialogue.
  NativeClimbPlace(play,npc->actor.world.pos.x,0,npc->actor.world.pos.z+350);
  p->exchangeItemAction=PLAYER_IA_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
  p->heldItemId=ITEM_NONE;p->talkActor=nullptr;
  const int item=wrong?ITEM_DEED_LAND:ITEM_MOONS_TEAR;
  gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED]=item;
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_TRADE_DEED;gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
  frame=NativeClimbFrame(99000+test);
 }
 const int age=tick-start;
 if(age==14){
  NativeClimbPlace(play,npc->actor.world.pos.x,0,npc->actor.world.pos.z+70);
  p->exchangeItemAction=PLAYER_IA_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
  p->heldItemId=ITEM_NONE;p->talkActor=nullptr;
  const int item=wrong?ITEM_DEED_LAND:ITEM_MOONS_TEAR;
  gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED]=item;
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_TRADE_DEED;gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
  frame=NativeClimbFrame(99000+test);
  cameraOffsetReady=false;cameraOffset=0;
 }
 if(age==20){
  const int item=wrong?ITEM_DEED_LAND:ITEM_MOONS_TEAR;
  mmvrgame::SelectItem(play,SLOT_TRADE_DEED,item);
 }
 frame.timeSeconds=10000+tick*.05;frame.triggers[0]=frame.triggers[1]=0;
 bool prompt=Message_GetState(&play->msgCtx)==TEXT_STATE_PAUSE_MENU;
 if(!pre&&age==20)pad.buttons=BTN_A;
 if(prompt){sawPrompt=true;if(!promptStart)promptStart=tick;}
 if(!pressed&&((pre&&age>=22)||(!pre&&prompt&&tick>=promptStart+3&&age>=20&&(!cancel||gSaveContext.save.unk_06==0)))){pressed=tick;if(cancel)pad.buttons=BTN_B;else frame.triggers[hand]=1;}
 auto view=mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+45,p->actor.world.pos.z);
 // Use the full camera callback, not a bypass around dialogue presentation.
 auto camera=mmvrgame::TestCameraFrame(frame);
 if(age>3&&camera.active){
  auto eye=mmvr::InversePose(camera.view);
  const float actorEye=p->actor.world.pos.y+mmvrgame::FormEyeHeight(p);
  // Let the test camera finish its initial pose interpolation before capturing
  // the stable room-scale offset. Then reject a new vertical jump during offers.
 if(age>=15&&age<=19){cameraOffset=eye.m[3][1]-actorEye;cameraOffsetReady=true;}
 if(age>=20&&cameraOffsetReady){
   const float expected=actorEye+cameraOffset;
   if(std::abs(eye.m[3][1]-expected)>.5f){log<<"FAIL camera-drop eye="<<eye.m[3][1]<<" expected="<<expected<<" baselineOffset="<<cameraOffset<<"\n";log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
  }
 }
 if(MMVR_OfferingItem(p) && p->getItemDrawIdPlusOne>0) {
  mmvr::Matrix offeredHand;float offered[3]{};
  if(mmvrgame::TrackedMaskHand(play,offeredHand,hand)) {
   bool aligned=MMVR_ItemPresentationPosition(offered);
   for(int k=0;k<3;++k)aligned &= std::abs(offered[k]-offeredHand.m[3][k]-(k==1?9.f:0.f))<.001f;
   if(!aligned){log<<"FAIL offer-hand-anchor case="<<test<<"\n";log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
  }
 }
 if(npc->unk_366==2){sawReply=true;
  if((Message_GetState(&play->msgCtx)==TEXT_STATE_DONE||Message_GetState(&play->msgCtx)==TEXT_STATE_EVENT)&&(age%6)==0)pad.buttons=BTN_A;
 }
 if(age%15==0)log<<"tick="<<tick<<" test="<<test<<" mode="<<int(play->msgCtx.msgMode)<<" state="<<int(Message_GetState(&play->msgCtx))<<" text="<<play->msgCtx.currentTextId<<" npc="<<npc->unk_366<<" action="<<int(p->exchangeItemAction)<<" eligible="<<mmvrgame::ExchangePromptActive(play)<<" selected="<<mmvrgame::SelectedItem(play)<<" physical="<<mmvr::PhysicalActionsAllowed()<<" first="<<mmvr::FirstPersonRequested()<<" pressed="<<pressed<<" held="<<int(p->heldItemId)<<" talk="<<(p->talkActor==&npc->actor)<<" flags="<<p->stateFlags1<<"\n";
 if(pre&&age==20)log<<"pre-offer-context active="<<mmvrgame::ExchangeItemContextActive(play)
  <<" focused="<<mmvr::InputFocused()<<" paused="<<mmvr::MenuPaused()<<" pause="<<int(play->pauseCtx.state)
  <<" transition="<<int(play->transitionTrigger)<<" health="<<gSaveContext.save.saveInfo.playerData.health
  <<" talk="<<(p->talkActor==&npc->actor)<<" action="<<int(p->exchangeItemAction)
  <<" selected="<<mmvrgame::SelectedItem(play)<<" inventory="<<gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED]
  <<" button="<<Player_GetItemOnButton(play,p,EQUIP_SLOT_C_DOWN)<<" trigger="<<frame.triggers[hand]<<"\n";
 if(sawReply&&npc->unk_366==0&&play->msgCtx.msgMode==MSGMODE_NONE&&!(p->stateFlags1&PLAYER_STATE1_TALKING)){
  log<<"PASS case="<<test<<" pre="<<pre<<" wrong="<<wrong<<" cancel="<<cancel<<" prompt="<<sawPrompt<<" age="<<age<<"\n";log.flush();
  if(++test==12){mmvr::SetNativeTestTracking(false);Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
  start=0;
 }
 if(age>300){log<<"FAIL timeout case="<<test<<"\n";log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}
