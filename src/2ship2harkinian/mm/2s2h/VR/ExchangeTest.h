#pragma once
#include "ItemUse.h"
extern "C" {
#include "overlays/actors/ovl_En_Sellnuts/z_en_sellnuts.h"
PlayerItemAction Player_ItemToItemAction(Player*,ItemId);
void func_80ADB924(EnSellnuts*,PlayState*);
void func_80ADBAB8(EnSellnuts*,PlayState*);
void func_80ADB0D8(EnSellnuts*,PlayState*);
}
// Exercise real input edges and the native NPC exchange reader. These are isolated
// fixtures, not a claim that every quest chain has been completed in a headset.
static void NativeExchangeTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto savedPlayer=*p;auto savedSave=gSaveContext;
 auto savedMsg=play->msgCtx;auto savedInterface=play->interfaceCtx;auto savedPause=play->pauseCtx.state;
 auto savedTransition=play->transitionTrigger;auto savedInput=*CONTROLLER1(&play->state);
 auto settings=mmvr::GetSettings();auto& in=*CONTROLLER1(&play->state);
 EnSellnuts npc{};npc.actor.id=ACTOR_EN_SELLNUTS;
 mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;
 for(int h=0;h<2;++h){f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].orientation.w=f.aims[h].orientation.w=1;}
 double clock=9000;int caseId=0;
 auto sample=[&](int hand,float value){in={};f.timeSeconds=clock+=.02;f.triggers[0]=f.triggers[1]=0;f.triggers[hand]=value;mmvrgame::UpdateItemTrigger(f);mmvrgame::ProcessItemTrigger(play);return bool(in.press.button&BTN_CDOWN);};
 auto prepare=[&](int item,int left){
  play->msgCtx.msgMode=MSGMODE_NONE;play->msgCtx.msgLength=0;mmvrgame::ProcessItemTrigger(play);
  *p=baseline;p->stateFlags1=PLAYER_STATE1_TALKING;p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
  p->heldActor=p->actor.child=nullptr;p->talkActor=&npc.actor;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;
  p->transformation=PLAYER_FORM_HUMAN;gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
  gSaveContext.save.saveInfo.playerData.health=0x30;gSaveContext.save.unk_06=0;
  play->pauseCtx.state=PAUSE_STATE_OFF;play->transitionTrigger=TRANS_TRIGGER_OFF;
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvrgame::ClearItemSelection();
  play->msgCtx.msgLength=1;play->msgCtx.msgMode=MSGMODE_TEXT_DONE;play->msgCtx.nextTextId=0xFFFF;
  play->msgCtx.textboxEndType=TEXTBOX_ENDTYPE_PAUSE_MENU;play->msgCtx.currentTextId=0xFF;
  const int slot=SLOT_TRADE_DEED;gSaveContext.save.saveInfo.inventory.items[slot]=item;
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
  f.epoch=9000+(++caseId);in={};auto action=p->actionFunc;auto held=p->heldItemAction;
  bool selected=mmvrgame::SelectItem(play,slot,item)&&mmvrgame::SelectedItem(play)==item;
  return selected&&p->actionFunc==action&&p->heldItemAction==held&&!p->heldActor;
 };
 const int items[]={ITEM_MOONS_TEAR,ITEM_DEED_LAND,ITEM_DEED_SWAMP,ITEM_DEED_MOUNTAIN,ITEM_DEED_OCEAN,
  ITEM_ROOM_KEY,ITEM_LETTER_MAMA,ITEM_LETTER_TO_KAFEI,ITEM_PENDANT_OF_MEMORIES,ITEM_POTION_RED,
  ITEM_MILK_BOTTLE,ITEM_DEKU_PRINCESS,ITEM_BOTTLE,ITEM_MAGIC_BEANS,ITEM_BOMB,ITEM_MASK_KEATON};
 log<<",\"npcExchangeOffers\":[";int count=0;
 for(int item:items)for(int left=0;left<2;++left){
  bool safe=prepare(item,left);const int hand=1-left;
  bool heldSafe=!sample(hand,1);bool offhandSafe=!sample(1-hand,0)&&!sample(1-hand,1);
  sample(hand,0);bool offered=sample(hand,1);
  const auto result=func_80123810(play);
  const bool native=result==Player_ItemToItemAction(p,static_cast<ItemId>(item))&&p->exchangeItemAction==result;
  bool intact=gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED]==item&&!p->heldActor;
  bool once=!sample(hand,1);sample(hand,0);once&=!sample(hand,1);
  if(count++)log<<",";
  log<<"{\"item\":"<<item<<",\"left\":"<<left<<",\"selectionSafe\":"<<safe<<",\"heldSafe\":"<<heldSafe<<",\"offhandSafe\":"<<offhandSafe<<",\"offered\":"<<offered<<",\"native\":"<<native<<",\"inventoryIntact\":"<<intact<<",\"once\":"<<once<<"}";
 }
 log<<"],\"npcExchangeGuards\":[";
 for(int mode=0;mode<10;++mode){
  prepare(ITEM_MOONS_TEAR,0);sample(1,0);
  switch(mode){
   case 0:play->msgCtx.textboxEndType=TEXTBOX_ENDTYPE_EVENT;break;
   case 1:p->talkActor=nullptr;break;
   case 2:p->stateFlags1&=~PLAYER_STATE1_TALKING;break;
   case 3:play->pauseCtx.state=PAUSE_STATE_MAIN;break;
   case 4:play->transitionTrigger=TRANS_TRIGGER_START;break;
   case 5:gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED]=ITEM_NONE;break;
   case 6:BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOMB;break;
   case 7:f.handTracked[1]=false;break;
   case 8:p->stateFlags2|=PLAYER_STATE2_USING_OCARINA;break;
   case 9:gSaveContext.save.saveInfo.playerData.health=0;break;
  }
  bool blocked=!sample(1,1);f.handTracked[1]=true;
  if(mode)log<<",";log<<"{\"mode\":"<<mode<<",\"blocked\":"<<blocked<<"}";
 }
 // Call the actual Clock Town scrub request handler: right item accepted, wrong
 // item rejected. Bomb presentation must not spawn a bomb or spend ammunition.
 log<<"],\"clockTownExchange\":[";
 for(int wrong=0;wrong<2;++wrong){
  prepare(wrong?ITEM_BOMB:ITEM_MOONS_TEAR,0);AMMO(ITEM_BOMB)=10;sample(1,0);bool offered=sample(1,1);
  npc.unk_33A=0;func_80ADB924(&npc,play);
  bool routed=npc.actionFunc==(wrong?func_80ADB0D8:func_80ADBAB8);
  bool safe=!p->heldActor&&AMMO(ITEM_BOMB)==10&&play->msgCtx.msgMode==MSGMODE_TEXT_CLOSING;
  bool action=p->exchangeItemAction==(wrong?PLAYER_IA_BOMB:PLAYER_IA_MOONS_TEAR);
  if(wrong)log<<",";log<<"{\"wrong\":"<<wrong<<",\"offered\":"<<offered<<",\"routed\":"<<routed<<",\"safe\":"<<safe<<",\"action\":"<<action<<"}";
 }
 // A press queued at render rate cannot leak into ordinary dialogue when the
 // NPC closes its request before the next native update.
 prepare(ITEM_MOONS_TEAR,0);sample(1,0);f.timeSeconds=clock+=.02;f.triggers[1]=1;in={};mmvrgame::UpdateItemTrigger(f);
 play->msgCtx.textboxEndType=TEXTBOX_ENDTYPE_EVENT;mmvrgame::ProcessItemTrigger(play);
 log<<"],\"npcExchangeCanceledEdge\":"<<!(in.press.button&BTN_CDOWN);
 // Preserve native A/B cancellation: the trigger exception doesn't swallow it.
 prepare(ITEM_MOONS_TEAR,0);in.press.button=BTN_B;
 bool cancel=func_80123810(play)==PLAYER_IA_MINUS1;
 log<<",\"npcExchangeCancel\":"<<cancel;
 mmvrgame::ClearItemSelection();play->msgCtx.msgLength=0;mmvrgame::ProcessItemTrigger(play);
 *p=savedPlayer;gSaveContext=savedSave;play->msgCtx=savedMsg;play->interfaceCtx=savedInterface;
 play->pauseCtx.state=savedPause;play->transitionTrigger=savedTransition;in=savedInput;mmvr::GetSettings()=settings;
}
