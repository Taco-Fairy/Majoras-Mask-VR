#pragma once
#include "ItemUse.h"
extern "C" {
#include "overlays/actors/ovl_En_M_Fire1/z_en_m_fire1.h"
void EnBom_Update(Actor*,PlayState*);
Actor* Actor_Delete(ActorContext*,Actor*,PlayState*);
}
static void NativeItemUseTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto savedPlayer=*p;auto savedSave=gSaveContext;auto savedInput=*CONTROLLER1(&play->state);auto settings=mmvr::GetSettings();
 auto frame=mmvr::TrackingFrame{};frame.head.orientation.w=frame.origin.orientation.w=1;
 for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.hands[h].position={0,-.4f,-.3f};frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;}
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
 auto prepare=[&](int item,int slot,int form=-1){
  *p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->heldActor=p->actor.child=nullptr;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
  p->heldItemId=ITEM_NONE;p->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;
  *CONTROLLER1(&play->state)={};mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
  gSaveContext.save.saveInfo.inventory.items[slot]=item;BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;
  p->transformation=form>=0?form:item==ITEM_POWDER_KEG?PLAYER_FORM_GORON:PLAYER_FORM_HUMAN;gSaveContext.save.playerForm=p->transformation;
  mmvrgame::SelectItem(play,slot,item);
 };
 log<<",\"triggerItems\":[";
 int cases=0;
 for(int kind=0;kind<3;++kind)for(int form=0;form<PLAYER_FORM_MAX;++form)for(int left=0;left<2;++left){
  int item=kind==2?ITEM_POWDER_KEG:kind?ITEM_DEKU_NUT:ITEM_BOMB,slot=kind==2?SLOT_POWDER_KEG:kind?SLOT_DEKU_NUT:SLOT_BOMB;
  if(!gPlayerFormItemRestrictions[form][item])continue;
  mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int dominant=1-left;
  int stock=kind==2?1:10;AMMO(item)=stock;prepare(item,slot,form);frame.epoch=700+kind*10+form*2+left;frame.hands[dominant].position={0,-.4f,-.3f};
  bool selectIdle=!p->heldActor&&AMMO(item)==stock,held=true,follow=true;Actor* actor=nullptr;
  for(int sample=0;sample<36;++sample){
   frame.timeSeconds=500+kind*4+left*2+sample/90.;frame.triggers[0]=frame.triggers[1]=0;
   if(sample>=5&&sample<25)frame.triggers[dominant]=1;
   frame.hands[dominant].position.x=left?0.f:sample*.01f;
   mmvrgame::RecordTracking(frame,view,head);
   if(sample%3==0){
    MMVR_ProcessInteractions(play);MMVR_UpdateHeldItem(play,p);
    if(p->heldActor){actor=p->heldActor;if(actor->init){actor->init(actor,play);actor->init=nullptr;}}
    if(sample>=6&&sample<25){held&=actor&&actor==p->heldActor&&actor->parent==&p->actor&&AMMO(item)==stock-1;
     if(actor){follow&=std::abs(actor->world.pos.x-frame.hands[dominant].position.x*40)<.01f;
      if(kind==1)EnArrow_Update(actor,play);else EnBom_Update(actor,play);
      held&=actor->parent==&p->actor;}}
   }
  }
  bool released=actor&&actor->parent==nullptr&&p->heldActor==nullptr&&AMMO(item)==stock-1;
  float speed=0;bool impact=true;
  if(actor){
   if(kind==1)EnArrow_Update(actor,play);speed=actor->speed;
   if(kind==1){auto count=play->actorCtx.actorLists[ACTORCAT_MISC].length;((EnArrow*)actor)->unk_262=1;EnArrow_Update(actor,play);
    impact=actor->update==nullptr&&play->actorCtx.actorLists[ACTORCAT_MISC].length>count;
    bool stun=false;for(auto* a=play->actorCtx.actorLists[ACTORCAT_MISC].first;a;a=a->next)if(a->id==ACTOR_EN_M_FIRE1&&a->update&&a->params==0){
     if(a->init){a->init(a,play);a->init=nullptr;}stun|=((EnMFire1*)a)->collider.elem.atDmgInfo.dmgFlags==DMG_DEKU_NUT;Actor_Kill(a);}
    impact&=stun;}
   if(kind==2)Actor_Delete(&play->actorCtx,actor,play);else Actor_Kill(actor);
  }
  if(cases++)log<<",";
  log<<"{\"form\":"<<form<<",\"nut\":"<<kind<<",\"left\":"<<left<<",\"selectionIdle\":"<<selectIdle<<",\"held\":"<<held<<",\"follow\":"<<follow<<",\"released\":"<<released<<",\"speed\":"<<speed<<",\"impact\":"<<impact<<"}";
 }
 log<<"]";
 // A full tap between native ticks must consume once, hold once, and release once.
 AMMO(ITEM_DEKU_NUT)=10;prepare(ITEM_DEKU_NUT,SLOT_DEKU_NUT);int dominant=mmvr::SwordController(mmvr::GetSettings());frame.epoch=800;
 for(int i=0;i<4;++i){frame.timeSeconds=520+i/90.;frame.triggers[0]=frame.triggers[1]=0;frame.triggers[dominant]=i==2?1:0;mmvrgame::RecordTracking(frame,view,head);}
 MMVR_ProcessInteractions(play);bool tap=AMMO(ITEM_DEKU_NUT)==9&&!p->heldActor;
 // Held input across a selection/focus reset never creates another item.
 mmvrgame::ClearItemTrigger();frame.timeSeconds+=.01;frame.triggers[dominant]=1;mmvrgame::RecordTracking(frame,view,head);MMVR_ProcessInteractions(play);tap&=AMMO(ITEM_DEKU_NUT)==9;
 log<<",\"quickItemTap\":"<<tap;
 // Both handedness choices fire the hookshot with their primary trigger.
 bool hooks=true;
 for(int left=0;left<2;++left){
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);dominant=1-left;prepare(ITEM_HOOKSHOT,SLOT_HOOKSHOT);auto* hook=p->heldActor;
  if(hook&&hook->init){hook->init(hook,play);hook->init=nullptr;}
  hooks&=!MMVR_HookshotInFlight(p);
  frame.epoch=810+left;
  for(int i=0;i<6;++i){frame.timeSeconds=525+left+i/90.;frame.triggers[0]=frame.triggers[1]=0;frame.triggers[dominant]=i>=3?1:0;mmvrgame::RecordTracking(frame,view,head);if(i%3==0)MMVR_ProcessInteractions(play);}
  hooks&=hook&&hook->parent==nullptr&&p->heldActor==nullptr&&MMVR_HookshotInFlight(p);
  auto& in=*CONTROLLER1(&play->state);in.cur.stick_x=60;in.cur.stick_y=60;p->speedXZ=p->actor.speed=4;
  auto before=p->actor.world.pos;MMVR_ProcessInteractions(play);
  hooks&=in.cur.stick_x==0&&in.cur.stick_y==0&&p->speedXZ==0&&p->actor.speed==0&&p->actor.world.pos.x==before.x&&p->actor.world.pos.z==before.z;
  // Transport is still native: no absolute position restoration.
  p->actor.world.pos.x+=5;MMVR_ProcessInteractions(play);hooks&=p->actor.world.pos.x==before.x+5;
  if(hook)Actor_Kill(hook);p->heldActor=p->actor.child=nullptr;
 }
 log<<",\"triggerHookshot\":"<<hooks;
 // With tracked aim disabled, the trigger must reach the native C-button hookshot action.
 mmvr::GetSettings().Set(mmvr::Setting::TrackedAim,0);prepare(ITEM_HOOKSHOT,SLOT_HOOKSHOT);
 frame.epoch=812;dominant=mmvr::SwordController(mmvr::GetSettings());
 for(int i=0;i<6;++i){frame.timeSeconds=527+i/90.;frame.triggers[0]=frame.triggers[1]=0;frame.triggers[dominant]=i>=3?1:0;
  mmvrgame::RecordTracking(frame,view,head);if(i%3==0)MMVR_ProcessInteractions(play);}
 bool nativeHookshotTrigger=(CONTROLLER1(&play->state)->press.button&BTN_CDOWN)!=0;
 log<<",\"nativeHookshotTrigger\":"<<nativeHookshotTrigger;
 if(p->heldActor)Actor_Kill(p->heldActor);p->heldActor=p->actor.child=nullptr;
 mmvr::GetSettings().Set(mmvr::Setting::TrackedAim,1);
 // B stows a bow. Its next B press draws the sword; one more B stows that sword.
 prepare(ITEM_BOW,SLOT_BOW);frame.epoch=820;frame.timeSeconds=530;frame.triggers[0]=frame.triggers[1]=0;mmvrgame::RecordTracking(frame,view,head);
 mmvrgame::ProcessSwordEquip(play,false);CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessCombatInput(play);bool stow=p->heldItemAction==PLAYER_IA_NONE;
 CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessCombatInput(play);bool drew=Player_GetMeleeWeaponHeld(p)>0;
 CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessCombatInput(play);stow&=p->heldItemAction==PLAYER_IA_NONE;
 log<<",\"bControls\":{\"stowed\":"<<stow<<",\"doubleDraw\":"<<drew<<"}";
 mmvrgame::ProcessSwordEquip(play,false);mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();*p=savedPlayer;gSaveContext=savedSave;*CONTROLLER1(&play->state)=savedInput;mmvr::GetSettings()=settings;
}
