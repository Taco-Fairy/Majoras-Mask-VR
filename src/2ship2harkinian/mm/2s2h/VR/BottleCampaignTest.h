#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Zoraegg/z_en_zoraegg.h"
#include "overlays/actors/ovl_En_Dnp/z_en_dnp.h"
void EnDnp_Init(Actor*,PlayState*);void EnDnp_Update(Actor*,PlayState*);void EnDnp_Destroy(Actor*,PlayState*);
void EnZoraegg_Init(Actor*,PlayState*);void EnZoraegg_Update(Actor*,PlayState*);void EnZoraegg_Destroy(Actor*,PlayState*);
}
static void NativeBottleCampaignTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto flags=play->actorCtx.sceneFlags;
 auto collision=play->colChkCtx;auto tasks=play->animTaskQueue;auto fullMsg=play->msgCtx;
 auto msg=play->msgCtx.msgMode;auto input=*CONTROLLER1(&play->state);
 log<<",\"underwaterEggs\":[";
 int count=0;
 for(int form:{PLAYER_FORM_HUMAN,PLAYER_FORM_ZORA})for(int left=0;left<2;++left){
  *p=baseline;gSaveContext=save;play->actorCtx.sceneFlags=flags;play->msgCtx=fullMsg;play->msgCtx.msgMode=MSGMODE_NONE;*CONTROLLER1(&play->state)={};
  p->actor.world.pos={0,2000,0};p->actor.velocity={};p->actor.depthInWater=80;p->transformation=form;gSaveContext.save.playerForm=form;
  p->stateFlags1=PLAYER_STATE1_8000000;p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->currentMask=PLAYER_MASK_NONE;
  p->heldActor=p->actor.child=p->interactRangeActor=nullptr;p->heldItemId=ITEM_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
  mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvr::GetSettings().Set(mmvr::Setting::PhysicalBottle,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
  mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
  gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;p->heldItemButton=EQUIP_SLOT_C_DOWN;
  mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_BOTTLE);
  bool equipped=MMVR_IndependentBottle(p)&&p->leftHandType==PLAYER_MODELTYPE_LH_BOTTLE;
  auto swimAction=p->actionFunc;auto swimFlags=p->stateFlags1;mmvrgame::StowItem(play);
  bool stowed=p->heldItemAction==PLAYER_IA_NONE&&mmvrgame::SelectedItem(play)==ITEM_NONE&&p->actionFunc==swimAction&&p->stateFlags1==swimFlags&&p->actor.depthInWater==80;
  mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_BOTTLE);equipped&=MMVR_IndependentBottle(p)!=0;
  EnZoraegg egg{};egg.actor.id=ACTOR_EN_ZORAEGG;egg.actor.params=ZORA_EGG_PARAMS(ZORA_EGG_TYPE_00,45);egg.actor.update=EnZoraegg_Update;egg.actor.csId=CS_ID_NONE;
  Flags_UnsetSwitch(play,45);EnZoraegg_Init(&egg.actor,play);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=70000+count;int hand=1-left;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);bool caught=false,restSafe=true;
  for(int i=0;i<80&&!caught;++i){
   f.timeSeconds=70000+count*10+i/90.;f.hands[hand].position={i<20?0.f:(i-20)*.006f,-.5f,-.3f};
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateBottle(f,model);
   auto opening=mmvrgame::BottleMouthForForm(p);Vec3f mouth{};
   for(int k=0;k<3;++k)(&mouth.x)[k]=opening.x*model.m[0][k]+opening.y*model.m[1][k]+opening.z*model.m[2][k]+model.m[3][k];
   if(i%3==0){egg.actor.world.pos=mouth;EnZoraegg_Update(&egg.actor,play);caught=egg.actor.parent==&p->actor;if(i<20)restSafe&=!caught;}
  }
  bool inventory=gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_ZORA_EGG;
  bool textPending=p->av1.actionVar1!=0&&p->av2.actionVar2==0; // Water animation must not skip native capture text.
  if(caught){p->actionFunc(p,play);f.timeSeconds+=1./90;mmvrgame::RecordTracking(f,view,head);}
  bool notice=caught&&p->av2.actionVar2==1&&play->msgCtx.msgMode!=MSGMODE_NONE&&mmvrgame::SelectedItem(play)==ITEM_ZORA_EGG;

  if(caught)EnZoraegg_Update(&egg.actor,play);
  bool once=egg.actor.update==nullptr&&Flags_GetSwitch(play,45)&&!MMVR_IndependentBottle(p);
  if(count++)log<<",";log<<"{\"form\":"<<form<<",\"left\":"<<left<<",\"equipped\":"<<equipped<<",\"stowed\":"<<stowed<<",\"caught\":"<<caught<<",\"restSafe\":"<<restSafe<<",\"inventory\":"<<inventory<<",\"textPending\":"<<textPending<<",\"notice\":"<<notice<<",\"once\":"<<once<<"}";
  EnZoraegg_Destroy(&egg.actor,play);
 }
 log<<"],\"princessScoops\":[";
 for(int left=0;left<2;++left){
  *p=baseline;gSaveContext=save;play->msgCtx=fullMsg;play->msgCtx=fullMsg;play->msgCtx.msgMode=MSGMODE_NONE;*CONTROLLER1(&play->state)={};
  p->actor.world.pos={0,2000,0};p->actor.velocity={};p->transformation=PLAYER_FORM_HUMAN;gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
  p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->currentMask=PLAYER_MASK_NONE;
  p->heldActor=p->actor.child=p->interactRangeActor=nullptr;p->heldItemId=ITEM_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
  for(int slot=SLOT_BOTTLE_1;slot<=SLOT_BOTTLE_6;++slot)gSaveContext.save.saveInfo.inventory.items[slot]=ITEM_BOTTLE;
  CLEAR_WEEKEVENTREG(WEEKEVENTREG_23_20);
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;p->heldItemButton=EQUIP_SLOT_C_DOWN;
  mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_BOTTLE);
  EnDnp princess{};princess.actor.id=ACTOR_EN_DNP;princess.actor.params=DEKU_PRINCESS_TYPE_WOODFALL_TEMPLE;princess.actor.update=EnDnp_Update;princess.actor.csId=CS_ID_NONE;
  EnDnp_Init(&princess.actor,play);bool nativeOffer=(princess.unk_322&0x400)&&princess.actor.update;
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=71000+left;int hand=1-left;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);bool caught=false,restSafe=true;
  for(int i=0;i<80&&!caught;++i){
   f.timeSeconds=71000+left*10+i/90.;f.hands[hand].position={i<20?0.f:(i-20)*.006f,-.5f,-.3f};
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateBottle(f,model);
   auto opening=mmvrgame::BottleMouthForForm(p);Vec3f mouth{};
   for(int k=0;k<3;++k)(&mouth.x)[k]=opening.x*model.m[0][k]+opening.y*model.m[1][k]+opening.z*model.m[2][k]+model.m[3][k];
   if(i%3==0){princess.actor.world.pos={mouth.x+20,mouth.y-princess.collider.dim.height*.5f,mouth.z};princess.actor.velocity={};
    EnDnp_Update(&princess.actor,play);caught=princess.actor.parent==&p->actor;if(i<20)restSafe&=!caught;}
  }
  bool inventory=gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_DEKU_PRINCESS;
  if(caught)EnDnp_Update(&princess.actor,play);
  bool once=caught&&!(princess.unk_322&0x400)&&!princess.actor.parent;
  if(left)log<<",";log<<"{\"left\":"<<left<<",\"nativeOffer\":"<<nativeOffer<<",\"caught\":"<<caught<<",\"restSafe\":"<<restSafe<<",\"inventory\":"<<inventory<<",\"once\":"<<once<<"}";
  EnDnp_Destroy(&princess.actor,play);play->colChkCtx=collision;play->animTaskQueue=tasks;
 }
 log<<"]";play->msgCtx=fullMsg;*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;play->actorCtx.sceneFlags=flags;play->msgCtx.msgMode=msg;*CONTROLLER1(&play->state)=input;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
}
