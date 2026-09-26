#pragma once
#include "GoronCombat.h"
static void NativeGoronRayReview(PlayState* play, const Player& baseline, std::ostream& log) {
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto col=play->colChkCtx;auto cs=play->csCtx;auto msg=play->msgCtx.msgMode;
 play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;play->msgCtx.msgMode=MSGMODE_NONE;
 auto prepare=[&](int form){*p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->heldActor=p->actor.child=nullptr;p->transformation=PLAYER_FORM_HUMAN;mmvrgame::ProcessGoronInput(play);p->transformation=form;p->currentMask=PLAYER_MASK_NONE;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->getItemDrawIdPlusOne=0;p->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;*CONTROLLER1(&play->state)={};mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();};
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;
 for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
 log<<"{\"cases\":[";
 for(int pass=0;pass<2;++pass){
 for(int hand=0;hand<2;++hand)for(int mode=0;mode<6;++mode){
  mmvrgame::SetGoronRayReview(pass!=0);
  prepare(PLAYER_FORM_GORON);f.epoch=41000+hand*6+mode;mmvrgame::RecordFormTracking(f,view,head);CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessGoronInput(play);
  bool toggles=mmvrgame::GoronFists(p);CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessGoronInput(play);toggles&=!mmvrgame::GoronFists(p);
  CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessGoronInput(play);toggles&=mmvrgame::GoronFists(p);CONTROLLER1(&play->state)->press.button=0;
  EnDekubaba enemy{};enemy.actor.id=ACTOR_EN_DEKUBABA;enemy.actor.update=EnDekubaba_Update;enemy.actor.world.pos=enemy.actor.home.pos={0,2000,0};EnDekubaba_Init(&enemy.actor,play);enemy.actor.colChkInfo.health=8;enemy.collider.base.colMaterial=COL_MATERIAL_HIT0;enemy.collider.base.acFlags&=~AC_HARD;
  for(auto& e:enemy.colliderElements){e.dim.worldSphere.center={(s16)(mode==3?60:0),2025,-12};e.dim.worldSphere.radius=3;e.base.acElemFlags|=ACELEM_ON;}
  int contacts=0;bool fire=false;
  for(int tick=0;tick<90;++tick){f.timeSeconds=f.epoch+tick/90.;float z=mode==0?-.3f:mode==1?-.1f-tick*.002f:std::max(-.6f,.6f-std::max(0,tick-20)*.025f);
   if(mode>=4)for(auto& e:enemy.colliderElements)e.dim.worldSphere.center={0,2025,(s16)(z*40-(mode==4?12:20))};
   f.hands[hand].position={0,-.5f,z};f.hands[1-hand].position={.7f,-.5f,0};mmvrgame::RecordFormTracking(f,view,head);mmvrgame::UpdateGoronCombat(f,view,head);mmvrgame::UpdateGoronCombat(f,view,head);fire|=mmvrgame::GoronPunchFire(hand)>0;
   if(tick%3==0){CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetJntSphAC(play,&enemy.collider.base);CollisionCheck_ResetDamage(&enemy.actor.colChkInfo);enemy.collider.base.acFlags|=AC_ON;CollisionCheck_SetAC(play,&play->colChkCtx,&enemy.collider.base);
    mmvrgame::ResolveGoronCombat(play);CollisionCheck_Damage(play,&play->colChkCtx);if(enemy.collider.base.acFlags&AC_HIT){++contacts;EnDekubaba_UpdateDamage(&enemy,play);}}
  }
  mmvr::SetNativePause(true);mmvrgame::UpdateGoronCombat(f,view,head);bool pauseSafe=!mmvrgame::GoronDebugCollider(hand);mmvr::SetNativePause(false);
  if(pass||hand||mode)log<<",";log<<"{\"legacy\":"<<pass<<",\"rays\":"<<mmvrgame::GoronRayReviewCount()<<",\"hand\":"<<hand<<",\"mode\":"<<mode<<",\"contacts\":"<<contacts<<",\"health\":"<<int(enemy.actor.colChkInfo.health)<<",\"fire\":"<<fire<<",\"toggles\":"<<toggles<<",\"pauseSafe\":"<<pauseSafe<<"}";EnDekubaba_Destroy(&enemy.actor,play);
 }
 }
 log<<"]}";
 mmvrgame::SetGoronRayReview(false);
 *p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;play->colChkCtx=col;play->csCtx=cs;play->msgCtx.msgMode=msg;
}
