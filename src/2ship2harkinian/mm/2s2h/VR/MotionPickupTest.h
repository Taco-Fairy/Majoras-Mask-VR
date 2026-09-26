#pragma once
#include "GoronCombat.h"
#include "Collectibles.h"
#include "Swimming.h"
extern "C" {void EnItem00_Init(Actor*,PlayState*);void EnItem00_Update(Actor*,PlayState*);void EnItem00_Destroy(Actor*,PlayState*);void Player_Action_59(Player*,PlayState*);void func_8083B850(PlayState*,Player*);void func_80844784(PlayState*,Player*);void Player_Action_56(Player*,PlayState*);void Player_Action_57(Player*,PlayState*);}
static void NativeMotionPickupTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto col=play->colChkCtx;auto cs=play->csCtx;auto msg=play->msgCtx.msgMode;
 play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;play->msgCtx.msgMode=MSGMODE_NONE;
 auto prepare=[&](int form){*p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->heldActor=p->actor.child=nullptr;p->transformation=PLAYER_FORM_HUMAN;mmvrgame::ProcessGoronInput(play);p->transformation=form;p->currentMask=PLAYER_MASK_NONE;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->getItemDrawIdPlusOne=0;p->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;*CONTROLLER1(&play->state)={};mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();};
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;
 for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
 log<<",\"motionSwords\":[";
 const ItemId swords[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY,ITEM_SWORD_DEITY};
 for(int w=0;w<5;++w)for(int hand=0;hand<2;++hand)for(int mode=0;mode<7;++mode){
  prepare(w==4?PLAYER_FORM_FIERCE_DEITY:PLAYER_FORM_HUMAN);mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,1-hand);MMVR_PlayerEquipSword(play,p,swords[w]);f.epoch=40000+w*14+hand*7+mode;
  EnDekubaba enemy{};enemy.actor.id=ACTOR_EN_DEKUBABA;enemy.actor.update=EnDekubaba_Update;enemy.actor.world.pos=enemy.actor.home.pos={0,2000,0};EnDekubaba_Init(&enemy.actor,play);enemy.actor.colChkInfo.health=8;enemy.collider.base.colMaterial=COL_MATERIAL_HIT0;enemy.collider.base.acFlags&=~AC_HARD;
  float length=MMVR_NativeSwordLength(p)*.01f;for(auto& e:enemy.colliderElements){e.dim.worldSphere.center={(s16)(length-1),(s16)(w==4?2025:2022),0};e.dim.worldSphere.radius=2;e.base.acElemFlags|=ACELEM_ON;}
  int contacts=0;
  for(int tick=0;tick<90;++tick){
   float x=mode==0?0:mode==1?-.3f+tick*.003f:std::min(0.f,-.6f+std::max(0,tick-20)*.025f);
   if(mode==4)x=.04f*std::sin(tick*.8f);if(mode==5)x=0;
   f.hands[hand].orientation=mode==5?XrQuaternionf{0,std::sin(tick*.08f),0,std::cos(tick*.08f)}:XrQuaternionf{0,0,0,1};
   f.timeSeconds=f.epoch+tick/90.;f.hands[hand].position={x,-.5f,0};mmvrgame::RecordTracking(f,view,head);
   auto model=mmvr::YawPose(0,x*40,2020,0);for(int k=0;k<3;++k)model.m[k][k]=.01f;mmvrgame::UpdateSwordDiagnostics(f,model);
   if(tick%3==0){CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetJntSphAC(play,&enemy.collider.base);CollisionCheck_ResetDamage(&enemy.actor.colChkInfo);enemy.collider.base.acFlags|=AC_ON;
    // Tick 45 still resolves moving XR samples 43/44. By tick 48 a full
    // simulation interval is stationary: a new target must then be safe.
    // Mode 6 requires the final moving sweep to survive the stopped sample 45.
    if((mode!=3||tick>=48)&&(mode!=6||tick==45))CollisionCheck_SetAC(play,&play->colChkCtx,&enemy.collider.base);
    mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
    if(enemy.collider.base.acFlags&AC_HIT){++contacts;EnDekubaba_UpdateDamage(&enemy,play);}
   }
  }
  if(w||hand||mode)log<<",";log<<"{\"weapon\":"<<w+1<<",\"hand\":"<<hand<<",\"mode\":"<<mode<<",\"contacts\":"<<contacts<<",\"health\":"<<int(enemy.actor.colChkInfo.health)<<"}";EnDekubaba_Destroy(&enemy.actor,play);
 }
 log<<"],\"goronPunches\":[";
 for(int hand=0;hand<2;++hand)for(int mode=0;mode<6;++mode){
  prepare(PLAYER_FORM_GORON);f.epoch=41000+hand*6+mode;mmvrgame::RecordFormTracking(f,view,head);CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessGoronInput(play);
  bool toggles=mmvrgame::GoronFists(p);mmvrgame::ProcessGoronInput(play);toggles&=!mmvrgame::GoronFists(p);
  mmvrgame::ProcessGoronInput(play);toggles&=mmvrgame::GoronFists(p);CONTROLLER1(&play->state)->press.button=0;
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
  if(hand||mode)log<<",";log<<"{\"hand\":"<<hand<<",\"mode\":"<<mode<<",\"contacts\":"<<contacts<<",\"health\":"<<int(enemy.actor.colChkInfo.health)<<",\"fire\":"<<fire<<",\"toggles\":"<<toggles<<",\"pauseSafe\":"<<pauseSafe<<"}";EnDekubaba_Destroy(&enemy.actor,play);
 }
 log<<"],\"shieldEdges\":[";
 for(int kind=0;kind<2;++kind)for(int hand=0;hand<2;++hand)for(int mode=0;mode<3;++mode){
  prepare(PLAYER_FORM_HUMAN);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,1-hand);mmvr::GetSettings().Set(mmvr::Setting::ShieldMargin,mode==0?0.f:.03f);p->currentShield=kind?PLAYER_SHIELD_MIRROR_SHIELD:PLAYER_SHIELD_HEROS_SHIELD;f.epoch=41500+kind*6+hand*3+mode;f.timeSeconds=f.epoch;f.grips[hand]=0;f.grips[1-hand]=1;mmvrgame::RecordTracking(f,view,head);
  auto model=mmvr::YawPose(0,0,2030,kind?14.03f:14.39f);for(int k=0;k<3;++k)model.m[k][k]=.01f;mmvrgame::UpdateShield(f,model);CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetCylinderAC(play,&p->cylinder.base);Collider_UpdateCylinder(&p->actor,&p->cylinder);CollisionCheck_SetAC(play,&play->colChkCtx,&p->cylinder.base);
  Actor attacker{};attacker.id=ACTOR_EN_DEKUBABA;attacker.update=[](Actor*,PlayState*){};
  ColliderCylinderInit init={{COL_MATERIAL_NONE,AT_ON|AT_TYPE_ENEMY,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},{ELEM_MATERIAL_UNK0,{DMG_SWORD,0,4},{0,0,0},ATELEM_ON,ACELEM_NONE,OCELEM_NONE},{1,4,0,{(s16)((kind?13:12)+(mode==2?8:0)),2028,10}}};
  ColliderCylinder attack;Collider_InitAndSetCylinder(play,&attack,&attacker,&init);CollisionCheck_SetAT(play,&play->colChkCtx,&attack.base);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);bool blocked=p->shieldQuad.base.acFlags&AC_BOUNCED;MMVR_AfterAttackCollision(play);bool body=p->cylinder.base.acFlags&AC_HIT;Collider_DestroyCylinder(play,&attack);
  if(kind||hand||mode)log<<",";log<<"{\"kind\":"<<kind<<",\"hand\":"<<hand<<",\"mode\":"<<mode<<",\"blocked\":"<<blocked<<",\"bodyHit\":"<<body<<"}";
 }
 f.grips[0]=f.grips[1]=0;log<<"],\"handPickups\":[";
 for(int form=0;form<PLAYER_FORM_MAX;++form)for(int hand=0;hand<2;++hand)for(int miss=0;miss<2;++miss){
  prepare(form);f.epoch=42000+form*4+hand*2+miss;f.timeSeconds=f.epoch;f.hands[hand].position={.9f,-.5f,0};f.handValid[1-hand]=f.handTracked[1-hand]=false;
  mmvrgame::RecordFormTracking(f,view,head);EnItem00 drop{};drop.actor.id=ACTOR_EN_ITEM00;drop.actor.params=ITEM00_RUPEE_GREEN;drop.actor.update=EnItem00_Update;drop.actor.world.pos={36,2020,(float)(miss?20:0)};EnItem00_Init(&drop.actor,play);drop.actor.gravity=0;drop.actor.xzDistToPlayer=36;drop.actor.playerHeightRel=20;drop.unk14C=0;
  auto before=gSaveContext.rupeeAccumulator;auto palm=mmvrgame::FormHandPose(hand);std::ofstream("native-pickup-detail.log",std::ios::app)<<form<<" "<<hand<<" ready="<<mmvrgame::FormTrackingReady(p)<<" enabled="<<mmvr::GetSettings().Get(mmvr::Setting::HandPickup)<<" pose="<<palm.m[3][0]<<","<<palm.m[3][1]<<","<<palm.m[3][2]<<","<<palm.m[3][3]<<" item="<<drop.actor.world.pos.x<<","<<drop.actor.world.pos.y<<","<<drop.actor.world.pos.z<<" alive="<<(drop.actor.update!=nullptr)<<" radius="<<drop.collider.dim.radius<<" height="<<drop.collider.dim.height<<" reach="<<mmvr::GetSettings().Get(mmvr::Setting::AimReach)<<"\n";bool touch=MMVR_HandCollectible(play,&drop.actor);EnItem00_Update(&drop.actor,play);int awarded=gSaveContext.rupeeAccumulator-before;
  for(int tick=0;tick<4&&drop.actor.update;++tick)EnItem00_Update(&drop.actor,play);bool once=gSaveContext.rupeeAccumulator-before==awarded;
  mmvr::SetNativePause(true);bool pauseSafe=!MMVR_HandCollectible(play,&drop.actor);mmvr::SetNativePause(false);
  if(form||hand||miss)log<<",";log<<"{\"form\":"<<form<<",\"hand\":"<<hand<<",\"miss\":"<<miss<<",\"touch\":"<<touch<<",\"award\":"<<awarded<<",\"once\":"<<once<<",\"pauseSafe\":"<<pauseSafe<<"}";EnItem00_Destroy(&drop.actor,play);f.handValid[1-hand]=f.handTracked[1-hand]=true;
 }
 log<<"],\"swimAdapter\":[";
 for(int form=0;form<PLAYER_FORM_MAX;++form){prepare(form);p->stateFlags1|=PLAYER_STATE1_8000000;f.epoch=43000+form;f.timeSeconds=f.epoch;auto gaze=mmvr::YawPose(.7f);float angle=.5f;gaze.m[2][1]=std::sin(angle);gaze.m[2][0]*=std::cos(angle);gaze.m[2][2]*=std::cos(angle);mmvrgame::RecordFormTracking(f,view,gaze);s16 yaw=0,pitch=0;bool aimed=MMVR_SwimAim(play,p,&yaw,&pitch);p->actionFunc=Player_Action_59;p->av1.actionVar1=0;p->av2.actionVar2=1;float horizontal=0,vertical=-3;bool dive=MMVR_SwimDive(play,p,&horizontal,&yaw,&vertical);float speed=std::hypot(horizontal,vertical);vertical=2;bool ascentSafe=!MMVR_SwimDive(play,p,&horizontal,&yaw,&vertical)&&vertical==2;
  if(form)log<<",";log<<"{\"form\":"<<form<<",\"aimed\":"<<aimed<<",\"pitch\":"<<pitch<<",\"dive\":"<<dive<<",\"speed\":"<<speed<<",\"ascentSafe\":"<<ascentSafe<<"}";}
 log<<"],\"zoraSwimLaunch\":[";
 for(int enabled=0;enabled<2;++enabled){
  prepare(PLAYER_FORM_ZORA);p->stateFlags1|=PLAYER_STATE1_8000000;p->currentBoots=PLAYER_BOOTS_ZORA_LAND;
  mmvr::GetSettings().Set(mmvr::Setting::HeadSwim,enabled);
  mmvr::GetSettings().Set(mmvr::Setting::ZoraEyeHeight,mmvr::SettingDefinitions[size_t(mmvr::Setting::ZoraEyeHeight)].initial);
  f.epoch=44000+enabled;f.timeSeconds=f.epoch;auto gaze=mmvr::YawPose(.7f);gaze.m[2][1]=std::sin(.5f);gaze.m[2][0]*=std::cos(.5f);gaze.m[2][2]*=std::cos(.5f);
  mmvrgame::RecordFormTracking(f,view,gaze);s16 yaw=0,pitch=0;MMVR_SwimAim(play,p,&yaw,&pitch);
  p->yaw=123;p->unk_AAA=456;p->speedXZ=4;p->actor.velocity.y=-3;func_8083B850(play,p);
  bool direction=enabled?(p->yaw==yaw&&p->unk_AAA==pitch):(p->yaw==123&&p->unk_AAA==456);
  bool speed=std::abs(p->unk_B48-5.f)<.001f;
  bool height=std::abs(MMVR_SwimEyeHeight(p,18.f,68.f)-18.f)<.001f;
  p->stateFlags1&=~PLAYER_STATE1_8000000;height&=MMVR_SwimEyeHeight(p,18.f,68.f)==68.f;
  p->stateFlags1|=PLAYER_STATE1_8000000;p->currentBoots=PLAYER_BOOTS_ZORA_UNDERWATER;p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
  height&=MMVR_SwimEyeHeight(p,18.f,68.f)==68.f;
  if(enabled)log<<",";log<<"{\"enabled\":"<<enabled<<",\"direction\":"<<direction<<",\"speedPreserved\":"<<speed<<",\"swimHeight\":"<<height<<"}";
 }
 log<<"],\"swimSpeed\":[";
 const float oldWind=play->envCtx.windSpeed;play->envCtx.windSpeed=0;
 for(int form : {PLAYER_FORM_HUMAN,PLAYER_FORM_ZORA}) {
  Vec3f baselineStep{};
  for(int rate : {100,50,200}) {
   prepare(form);p->stateFlags1=PLAYER_STATE1_8000000;p->currentBoots=PLAYER_BOOTS_ZORA_LAND;
   p->actionFunc=form==PLAYER_FORM_ZORA?Player_Action_56:Player_Action_57;
   p->actor.bgCheckFlags=0;p->actor.colChkInfo.displacement={};p->actor.gravity=0;p->actor.terminalVelocity=-20;
   p->yaw=0;p->speedXZ=4;p->actor.velocity={0,-3,0};p->pushedSpeed=2;p->pushedYaw=0x4000;p->windSpeed=0;
   f.epoch=45000+form*1000+rate;f.timeSeconds=f.epoch;mmvrgame::RecordFormTracking(f,view,head);
   mmvr::GetSettings().Set(mmvr::Setting::SwimSpeed,rate);
   const Vec3f initial=p->actor.world.pos;func_80844784(play,p);
   Vec3f step{p->actor.world.pos.x-initial.x,p->actor.world.pos.y-initial.y,p->actor.world.pos.z-initial.z};
   if(rate==100)baselineStep=step;
   bool proportional=std::abs(step.z-baselineStep.z*rate*.01f)<.001f&&std::abs(step.x-baselineStep.x)<.001f;
   proportional&=std::abs(step.y-baselineStep.y*(form==PLAYER_FORM_ZORA?rate*.01f:1.f))<.001f;
   bool noCompounding=std::abs(p->speedXZ-4)<.001f&&std::abs(p->actor.velocity.y+3)<.001f;
   auto previous=p->actor.world.pos;func_80844784(play,p);
   noCompounding&=std::abs(p->actor.world.pos.z-previous.z-step.z)<.001f;
   p->stateFlags1=0;bool land=MMVR_SwimMovementScale(play,p,false)==1;
   p->stateFlags1=PLAYER_STATE1_8000000;p->actionFunc=Player_Action_59;p->av1.actionVar1=1;
   bool recovery=MMVR_SwimMovementScale(play,p,true)==1;
   if(form!=PLAYER_FORM_HUMAN||rate!=100)log<<",";
   log<<"{\"form\":"<<form<<",\"rate\":"<<rate<<",\"proportional\":"<<proportional<<",\"noCompounding\":"<<noCompounding<<",\"landUnchanged\":"<<land<<",\"recoveryUnchanged\":"<<recovery<<"}";
  }
 }
 play->envCtx.windSpeed=oldWind;
 log<<"]";*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;play->colChkCtx=col;play->csCtx=cs;play->msgCtx.msgMode=msg;mmvrgame::ClearTracking();
}
