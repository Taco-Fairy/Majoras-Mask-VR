#pragma once
static void NativeTrackedBodyTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto settings=mmvr::GetSettings();auto ctx=play->colChkCtx;
 log<<",\"trackedBody\":[";
 for(int form=0;form<PLAYER_FORM_MAX;++form)for(int mode=0;mode<7;++mode){
  *p=baseline;p->transformation=form;p->currentMask=PLAYER_MASK_NONE;p->actor.world.pos={0,2000,0};p->heldActor=nullptr;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;
  mmvrgame::ClearTracking();mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
  mmvr::TrackingFrame f{};f.epoch=78000+form*7+mode;f.timeSeconds=f.epoch;f.head.orientation.w=f.origin.orientation.w=1;
  for(int h=0;h<2;++h){f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.hands[h].position={h?.6f:-.6f,-.5f,-.3f};}
  if(mode==5)f.handTracked[0]=false;
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);mmvrgame::RecordTracking(f,view,head);
  mmvrgame::UpdateShield(f,mmvr::Matrix{});CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetCylinderAC(play,&p->cylinder.base);Collider_UpdateCylinder(&p->actor,&p->cylinder);CollisionCheck_SetAC(play,&play->colChkCtx,&p->cylinder.base);
  const Vec3s points[]={{0,2044,0},{0,2020,0},{-24,2025,-12},{24,2025,-12},{0,2044,12},{-24,2025,-12},{16,2020,0}};
  Actor enemy{};enemy.id=ACTOR_EN_DEKUBABA;enemy.update=[](Actor*,PlayState*){};
  ColliderSphere attack{};attack.base.actor=&enemy;attack.base.shape=COLSHAPE_SPHERE;attack.base.atFlags=AT_ON|AT_TYPE_ENEMY;attack.elem.atElemFlags=ATELEM_ON;attack.elem.atDmgInfo.dmgFlags=DMG_SWORD;attack.elem.atDmgInfo.damage=4;
  attack.dim.worldSphere.center=points[mode];attack.dim.worldSphere.radius=2;CollisionCheck_SetAT(play,&play->colChkCtx,&attack.base);
  MMVR_FilterAttackCollisions(play);int proxies=play->colChkCtx.colACCount;CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
  if(form||mode)log<<",";log<<"{\"form\":"<<form<<",\"mode\":"<<mode<<",\"hit\":"<<bool(p->cylinder.base.acFlags&AC_HIT)<<",\"proxies\":"<<proxies<<"}";
 }
 log<<"],\"physicalFormGuards\":[";
 for(int form:{PLAYER_FORM_HUMAN,PLAYER_FORM_ZORA,PLAYER_FORM_DEKU})for(int exposed=0;exposed<2;++exposed){
  *p=baseline;p->transformation=form;p->currentMask=PLAYER_MASK_NONE;p->actor.world.pos={0,2000,0};p->heldActor=nullptr;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=PLAYER_STATE1_400000;p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->getItemDrawIdPlusOne=0;p->currentShield=PLAYER_SHIELD_HEROS_SHIELD;
  mmvrgame::ClearTracking();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);mmvr::GetSettings().Set(mmvr::Setting::PhysicalShield,1);
  mmvr::TrackingFrame f{};f.epoch=79000+form*2+exposed;f.timeSeconds=f.epoch;f.origin.orientation.w=f.head.orientation.w=1;
  for(int h=0;h<2;++h){f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.hands[h].position={h?1.f:-1.f,0,0};}f.grips[0]=1;
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);mmvrgame::RecordTracking(f,view,head);
  auto model=mmvr::YawPose(0,0,2030,14.39f);for(int k=0;k<3;++k)model.m[k][k]=.01f;
  if(form==PLAYER_FORM_ZORA){auto desired=mmvr::YawPose(0,0,2030,7.12f);desired.m[0][0]=.01f;desired.m[1][1]=desired.m[2][2]=0;desired.m[1][2]=.01f;desired.m[2][1]=-.01f;
   desired=mmvr::Multiply(mmvr::ZoraShieldToFin(),desired);
   mmvr::Matrix inverse;mmvr::InverseAffine(mmvr::AttachedZoraShield(mmvr::YawPose(0),0,mmvr::GetSettings()),inverse);model=mmvr::Multiply(inverse,desired);}
  mmvrgame::UpdateShield(f,model);
  if(form==PLAYER_FORM_DEKU){auto guard=mmvr::YawPose(0,0,2010,0);for(int k=0;k<3;++k)guard.m[k][k]=.01f;Matrix_Push();Matrix_Put((MtxF*)&guard);MMVR_RecordDekuGuard(play,p);Matrix_Pop();}
  CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetCylinderAC(play,&p->cylinder.base);Collider_UpdateCylinder(&p->actor,&p->cylinder);CollisionCheck_SetAC(play,&play->colChkCtx,&p->cylinder.base);
  Actor enemy{};enemy.update=[](Actor*,PlayState*){};ColliderSphere attack{};attack.base.actor=&enemy;attack.base.shape=COLSHAPE_SPHERE;attack.base.atFlags=AT_ON|AT_TYPE_ENEMY;attack.elem.atElemFlags=ATELEM_ON;attack.elem.atDmgInfo.dmgFlags=DMG_SWORD;attack.elem.atDmgInfo.damage=4;
  attack.dim.worldSphere.center=exposed?Vec3s{0,2044,0}:Vec3s{0,(s16)(form==PLAYER_FORM_DEKU?2015:2030),10};attack.dim.worldSphere.radius=exposed?2:6;
  CollisionCheck_SetAT(play,&play->colChkCtx,&attack.base);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);bool bounced=attack.base.atFlags&AT_BOUNCED;MMVR_AfterAttackCollision(play);
  if(form!=PLAYER_FORM_HUMAN||exposed)log<<",";log<<"{\"form\":"<<form<<",\"exposed\":"<<exposed<<",\"blocked\":"<<bounced<<",\"bodyHit\":"<<bool(p->cylinder.base.acFlags&AC_HIT)<<"}";
 }
 log<<"]";*p=saved;mmvr::GetSettings()=settings;play->colChkCtx=ctx;mmvrgame::ClearTracking();
}
