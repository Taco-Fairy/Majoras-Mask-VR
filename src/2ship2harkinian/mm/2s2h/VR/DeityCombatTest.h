#pragma once
#include "GiantLifecycleTest.h"
extern "C" {
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
void EnMThunder_Update(Actor*,PlayState*);
}
static void NativeDeityTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto col=play->colChkCtx;
 log<<",\"deityBeams\":[";
 for(int left=0;left<2;++left)for(int mode=0;mode<5;++mode){
  *p=baseline;gSaveContext=save;p->transformation=PLAYER_FORM_FIERCE_DEITY;gSaveContext.save.playerForm=PLAYER_FORM_FIERCE_DEITY;
  p->currentMask=PLAYER_MASK_NONE;p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->heldActor=p->actor.child=nullptr;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->csAction=PLAYER_CSACTION_NONE;
  p->actionFunc=Player_Action_Idle;p->stateFlags1=PLAYER_STATE1_Z_TARGETING;p->stateFlags2=0;p->stateFlags3=mode==4?0:PLAYER_STATE3_HOSTILE_LOCK_ON;p->getItemDrawIdPlusOne=0;*CONTROLLER1(&play->state)={};
  mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
  mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_B)=ITEM_SWORD_DEITY;MMVR_PlayerEquipSword(play,p,ITEM_SWORD_DEITY);
  gSaveContext.save.saveInfo.playerData.isMagicAcquired=true;gSaveContext.save.saveInfo.playerData.magic=mode==2?0:12;gSaveContext.magicState=MAGIC_STATE_IDLE;
  CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=71000+left*5+mode;int hand=1-left;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);Actor target{};target.update=[](Actor*,PlayState*){};target.focus.pos={300,2150,-300};p->focusActor=&target;
  int beams=0;bool flight=true,damage=true,animationFree=true;std::vector<Actor*> owned;
  for(int tick=0;tick<90;++tick){float x=mode==0?0.f:std::min(.6f,-.6f+std::max(0,tick-20)*(mode==3?.025f:.06f));f.timeSeconds=f.epoch+tick/90.;f.hands[hand].position={x,-.5f,0};
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateSwordDiagnostics(f,model);
   if(tick%3==0){mmvrgame::ProcessCombatInput(play);animationFree&=p->actionFunc==Player_Action_Idle&&!(p->stateFlags1&PLAYER_STATE1_CHARGING_SPIN_ATTACK)&&!(p->stateFlags2&PLAYER_STATE2_20000);
    for(Actor* actor=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;actor;actor=actor->next)if(actor->id==ACTOR_EN_M_THUNDER&&actor->update&&std::find(owned.begin(),owned.end(),actor)==owned.end()){
     owned.push_back(actor);++beams;auto* beam=(EnMThunder*)actor;damage&=beam->collider.elem.atDmgInfo.dmgFlags==DMG_SWORD_BEAM&&beam->collider.elem.atDmgInfo.damage==3;
     auto before=actor->world.pos;CollisionCheck_ClearContext(play,&play->colChkCtx);EnMThunder_Update(actor,play);Vec3f travel{actor->world.pos.x-before.x,actor->world.pos.y-before.y,actor->world.pos.z-before.z};
     Vec3f aim{target.focus.pos.x-before.x,target.focus.pos.y-before.y,target.focus.pos.z-before.z};float distance=std::sqrt(SQ(travel.x)+SQ(travel.y)+SQ(travel.z)),a=std::sqrt(SQ(aim.x)+SQ(aim.y)+SQ(aim.z));
     flight&=distance>79&&distance<81&&(travel.x*aim.x+travel.y*aim.y+travel.z*aim.z)/(distance*a)>.999f;
    }
   }
  }
  bool mesh=mmvrgame::FormHandMesh(p,0)==MMVR_TrackedLeftHandMesh(p);float length=MMVR_NativeSwordLength(p);
  if(left||mode)log<<",";log<<"{\"left\":"<<left<<",\"mode\":"<<mode<<",\"beams\":"<<beams<<",\"magic\":"<<int(gSaveContext.save.saveInfo.playerData.magic)<<",\"flight\":"<<flight<<",\"damage\":"<<damage<<",\"animationFree\":"<<animationFree<<",\"mesh\":"<<mesh<<",\"length\":"<<length<<"}";
  for(auto* actor:owned)Actor_Delete(&play->actorCtx,actor,play);play->colChkCtx=col;
 }
 log<<"]";
 *p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;play->colChkCtx=col;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
 NativeGiantTest(play,baseline,log);
}

// Drive the actual trigger, native projectile and magic paths across render/game ticks.
static void NativeDeityTriggerTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto col=play->colChkCtx;auto pause=play->pauseCtx.state;
 log<<",\"deityTrigger\":[";
 for(int left=0;left<2;++left)for(int mode=0;mode<17;++mode){
  *p=baseline;gSaveContext=save;play->pauseCtx.state=PAUSE_STATE_OFF;p->transformation=PLAYER_FORM_FIERCE_DEITY;gSaveContext.save.playerForm=PLAYER_FORM_FIERCE_DEITY;
  p->currentMask=PLAYER_MASK_NONE;p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->heldActor=p->actor.child=nullptr;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->csAction=PLAYER_CSACTION_NONE;
  p->actionFunc=Player_Action_Idle;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->getItemDrawIdPlusOne=0;p->focusActor=nullptr;*CONTROLLER1(&play->state)={};
  mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::DeityBeamInterval,.35f);
  mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_B)=ITEM_SWORD_DEITY;MMVR_PlayerEquipSword(play,p,ITEM_SWORD_DEITY);
  gSaveContext.save.saveInfo.playerData.isMagicAcquired=true;gSaveContext.save.saveInfo.playerData.magic=mode==2?0:12;gSaveContext.magicState=MAGIC_STATE_IDLE;CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=73000+left*17+mode;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.hands[h].position={h?.25f:-.25f,-.5f,-.2f};f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(.65f,0,2045,0),head=mmvr::YawPose(mode==8?-.7f:.3f);
  if(mode==8){XrPosef pitch{};pitch.orientation={std::sin(.2f),0,0,std::cos(.2f)};head=mmvr::Multiply(mmvr::PoseMatrix(pitch),head);}
  auto eye=mmvr::Multiply(head,view);Vec3f target{eye.m[3][0]-4000*eye.m[2][0],eye.m[3][1]-4000*eye.m[2][1],eye.m[3][2]-4000*eye.m[2][2]};
  Actor lockTarget{};lockTarget.update=[](Actor*,PlayState*){};lockTarget.focus.pos={-300,2100,200};
  if(mode==10){p->focusActor=&lockTarget;p->stateFlags1=PLAYER_STATE1_Z_TARGETING;p->stateFlags3=PLAYER_STATE3_HOSTILE_LOCK_ON;}
  int beams=0;double last=-100;bool cadence=true,flight=true,animationFree=true,damage=true;std::vector<Actor*> owned;
  for(int tick=0;tick<120;++tick){
   f.timeSeconds=f.epoch+tick/90.;f.triggers[0]=f.triggers[1]=0;f.triggers[mode==1?left:1-left]=(mode==7||tick>=4)&&(mode!=5||tick<20)&&tick<85?1.f:0.f;
   if(mode==3&&tick>=20)play->pauseCtx.state=PAUSE_STATE_MAIN;
   if((mode==4&&tick>=20)||(mode==9&&tick>=20&&tick<40))f.handTracked[1-left]=false;else f.handTracked[1-left]=true;
   if(mode==11&&tick>=20)f.handTracked[1-left]=false;
   if(mode==14)f.handTracked[left]=f.handValid[left]=false; // Off-hand loss must not disable a tracked sword.
   if(mode==15)f.handValid[1-left]=false;
   if(mode==16)f.triggers[left]=1; // Both triggers still produce only one beam per qualified stroke.
   float stroke=tick<25?-.5f:tick<45?-.5f+(tick-25)*.05f:tick<65?.5f:std::max(-.5f,.5f-(tick-65)*.05f);
   f.hands[1-left].position.x=mode==0?0:mode==12?stroke*.08f:mode==13?0:stroke;
   if(mode==13){float angle=stroke*2;f.hands[1-left].orientation={0,std::sin(angle*.5f),0,std::cos(angle*.5f)};}
   if(mode==6&&tick==20)mmvrgame::StowItem(play);
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,1-left,mmvr::GetSettings());mmvrgame::UpdateSwordDiagnostics(f,model);
   if(tick%3==0){mmvrgame::ProcessCombatInput(play);mmvrgame::ProcessItemTrigger(play);animationFree&=p->actionFunc==Player_Action_Idle&&!(p->stateFlags1&PLAYER_STATE1_CHARGING_SPIN_ATTACK)&&!(p->stateFlags2&PLAYER_STATE2_20000);
    for(Actor* actor=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;actor;actor=actor->next)if(actor->id==ACTOR_EN_M_THUNDER&&actor->update&&std::find(owned.begin(),owned.end(),actor)==owned.end()){
     owned.push_back(actor);++beams;cadence&=f.timeSeconds-last>=.349;last=f.timeSeconds;auto* beam=(EnMThunder*)actor;damage&=beam->collider.elem.atDmgInfo.dmgFlags==DMG_SWORD_BEAM&&beam->collider.elem.atDmgInfo.damage==3;
     auto before=actor->world.pos;CollisionCheck_ClearContext(play,&play->colChkCtx);EnMThunder_Update(actor,play);Vec3f travel{actor->world.pos.x-before.x,actor->world.pos.y-before.y,actor->world.pos.z-before.z};
     Vec3f aim{target.x-before.x,target.y-before.y,target.z-before.z};float distance=std::sqrt(SQ(travel.x)+SQ(travel.y)+SQ(travel.z)),a=std::sqrt(SQ(aim.x)+SQ(aim.y)+SQ(aim.z));
     flight&=distance>79&&distance<81&&(travel.x*aim.x+travel.y*aim.y+travel.z*aim.z)/(distance*a)>.999f;
    }
   }
  }
  if(left||mode)log<<",";log<<"{\"left\":"<<left<<",\"mode\":"<<mode<<",\"beams\":"<<beams<<",\"magic\":"<<int(gSaveContext.save.saveInfo.playerData.magic)<<",\"flight\":"<<flight<<",\"damage\":"<<damage<<",\"cadence\":"<<cadence<<",\"animationFree\":"<<animationFree<<"}";
  for(auto* actor:owned)Actor_Delete(&play->actorCtx,actor,play);play->colChkCtx=col;
 }
 log<<"]";*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;play->colChkCtx=col;play->pauseCtx.state=pause;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
}
