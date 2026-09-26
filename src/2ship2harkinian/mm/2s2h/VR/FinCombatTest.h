#pragma once
#include "FinCombat.h"
#include "form_presentation.h"
static void NativeFinCombatTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto collision=play->colChkCtx;auto settings=mmvr::GetSettings();auto cs=play->csCtx;auto msg=play->msgCtx.msgMode;
 mmvr::GetSettings().Set(mmvr::Setting::PhysicalFins,1);play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;play->msgCtx.msgMode=MSGMODE_NONE;
 log<<",\"physicalFins\":[";
 for(int left=0;left<2;++left)for(int hand=0;hand<2;++hand)for(int miss=0;miss<2;++miss){
  *p=baseline;p->transformation=PLAYER_FORM_ZORA;p->currentMask=PLAYER_MASK_NONE;p->actor.world.pos={0,2000,0};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->getItemDrawIdPlusOne=0;p->actionFunc=Player_Action_Idle;p->heldActor=nullptr;p->upperActionFunc=Player_UpperAction_15;
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvrgame::ClearFinCombat();mmvr::TrackingFrame f{};
  f.head.orientation.w=f.origin.orientation.w=1;f.hands[hand].orientation.w=1;f.handTracked[hand]=f.handValid[hand]=true;f.epoch=30000+left*4+hand*2+miss;
  auto view=mmvr::YawPose(left?1.57079632679f:0,0,2045,0),head=mmvr::YawPose(0);mmvr::Matrix models[2]{};
  auto sample=[&](int tick,float x){f.timeSeconds=f.epoch+tick/90.0;f.hands[hand].position={x,-.5f,-.2f};
   mmvrgame::RecordFormTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,hand,hand,mmvr::GetSettings());models[hand]=mmvr::AttachedFin(model,hand,mmvr::GetSettings());
   mmvrgame::UpdateFinCombat(f,view,head,models);
  };
  sample(0,0);auto* debug=reinterpret_cast<ColliderQuad*>(mmvrgame::FinDebugCollider(hand));bool geometry=debug!=nullptr;Vec3f target{};
  if(debug){auto& q=debug->dim.quad;Vec3f a{(q[0].x+q[2].x)/2,(q[0].y+q[2].y)/2,(q[0].z+q[2].z)/2},b{(q[1].x+q[3].x)/2,(q[1].y+q[3].y)/2,(q[1].z+q[3].z)/2};
   float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;geometry&=std::abs(std::sqrt(SQ(dx)+SQ(dy)+SQ(dz))-mmvr::GetSettings().Get(mmvr::Setting::FinReach)*40)<.01f&&dx*(-view.m[2][0])+dy*(-view.m[2][1])+dz*(-view.m[2][2])>=-.001f;
   target={a.x+dx*.8f,a.y+dy*.8f,a.z+dz*.8f};
  }
  EnDekubaba enemy{};enemy.actor.id=ACTOR_EN_DEKUBABA;enemy.actor.update=EnDekubaba_Update;enemy.actor.world.pos=enemy.actor.home.pos=target;EnDekubaba_Init(&enemy.actor,play);enemy.actor.colChkInfo.health=8;enemy.collider.base.colMaterial=COL_MATERIAL_HIT0;enemy.collider.base.acFlags&=~AC_HARD;
  for(auto& element:enemy.colliderElements){element.dim.worldSphere.center={(s16)(target.x+(miss?60:0)),(s16)target.y,(s16)target.z};element.dim.worldSphere.radius=3;element.base.acElemFlags|=ACELEM_ON;}
  mmvrgame::ClearFinCombat();int contacts=0;bool restingSafe=true,cooldown=true;double lastHit=-100;int damage=0;
  for(int tick=0;tick<95;++tick){float x=tick<20?-.6f:std::min(.6f,-.6f+(tick-20)*.024f);sample(tick+1,x);
   if(tick%3==0){CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetJntSphAC(play,&enemy.collider.base);CollisionCheck_ResetDamage(&enemy.actor.colChkInfo);enemy.collider.base.acFlags|=AC_ON;CollisionCheck_SetAC(play,&play->colChkCtx,&enemy.collider.base);
    MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);CollisionCheck_Damage(play,&play->colChkCtx);
    if(enemy.collider.base.acFlags&AC_HIT){++contacts;damage=enemy.actor.colChkInfo.damage;restingSafe&=tick>=20;cooldown&=f.timeSeconds-lastHit>=mmvr::GetSettings().Get(mmvr::Setting::SwingCooldown);lastHit=f.timeSeconds;EnDekubaba_UpdateDamage(&enemy,play);}
   }
  }
  mmvr::SetNativePause(true);sample(100,0);bool pauseHidden=!mmvrgame::FinDebugCollider(hand);mmvr::SetNativePause(false);
  models[hand]={};f.timeSeconds+=.02;mmvrgame::RecordFormTracking(f,view,head);mmvrgame::UpdateFinCombat(f,view,head,models);bool awayHidden=!mmvrgame::FinDebugCollider(hand);
  if(left||hand||miss)log<<",";log<<"{\"left\":"<<left<<",\"hand\":"<<hand<<",\"miss\":"<<miss<<",\"geometry\":"<<geometry<<",\"contacts\":"<<contacts<<",\"damage\":"<<damage<<",\"health\":"<<int(enemy.actor.colChkInfo.health)<<",\"restingSafe\":"<<restingSafe<<",\"cooldown\":"<<cooldown<<",\"pauseHidden\":"<<pauseHidden<<",\"awayHidden\":"<<awayHidden<<"}";
  EnDekubaba_Destroy(&enemy.actor,play);
 }
 log<<"]";mmvrgame::ClearFinCombat();*p=saved;play->colChkCtx=collision;play->csCtx=cs;play->msgCtx.msgMode=msg;mmvr::GetSettings()=settings;
}
