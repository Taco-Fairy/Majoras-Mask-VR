#pragma once
#include "Camera.h"
#include "NativeForms.h"
#include "ScenePresentation.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
extern "C" { void Player_Action_89(Player*,PlayState*); void Player_Action_90(Player*,PlayState*); }

// Run after the regular combat fixtures. Exercise the real draw boundary and
// collision pipeline: a coordinate change must not become an enormous sword sweep.
static void NativeGiantTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);const auto saved=*p;const auto save=gSaveContext;const auto settings=mmvr::GetSettings();
 const auto input=*CONTROLLER1(&play->state);const auto collision=play->colChkCtx;const auto cs=play->csCtx;
 const auto pause=play->pauseCtx.state;const auto msg=play->msgCtx.msgMode;
 const auto transition=play->transitionTrigger;const auto mode=play->transitionMode;const auto roomStatus=play->roomCtx.status;
 const auto setup=play->numSetupActors;auto* gfx=play->state.gfxCtx;const auto opa=gfx->polyOpa,xlu=gfx->polyXlu;
 const int fps=CVarGetInteger("gInterpolationFPS",20);CVarSetInteger("gInterpolationFPS",120);
 play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;play->pauseCtx.state=PAUSE_STATE_OFF;
 play->msgCtx.msgMode=MSGMODE_NONE;play->transitionTrigger=TRANS_TRIGGER_OFF;play->transitionMode=TRANS_MODE_OFF;play->roomCtx.status=0;play->numSetupActors=0;
 mmvr::SetFirstPersonEligibility(true);mmvr::SetNativePause(false);
 auto& tuning=mmvr::GetSettings();
 for(auto setting:{mmvr::Setting::EyeHeight,mmvr::Setting::HandScale,mmvr::Setting::SwingSpeed,mmvr::Setting::SwingDistance,mmvr::Setting::SwingResetSpeed,mmvr::Setting::SwingCooldown,mmvr::Setting::SwordWindow,mmvr::Setting::AimReach})
  tuning.Set(setting,mmvr::SettingDefinitions[size_t(setting)].initial);
 tuning.Set(mmvr::Setting::PhysicalSword,1);tuning.Set(mmvr::Setting::DisableButtonMelee,1);tuning.Set(mmvr::Setting::FormFirstPerson,1);
 tuning.Set(mmvr::Setting::ItemSmoothing,0);tuning.Set(mmvr::Setting::PhysicalMasks,1);tuning.Set(mmvr::Setting::VrCameraCutscenes,0);
 auto draw=[&](){
  p->actor.focus.pos={p->actor.world.pos.x,p->actor.world.pos.y+48,p->actor.world.pos.z};
  p->bodyPartsPos[PLAYER_BODYPART_HEAD]=p->actor.focus.pos;
  Matrix_Push();Matrix_Translate(p->actor.world.pos.x,p->actor.world.pos.y,p->actor.world.pos.z,MTXMODE_NEW);Matrix_Scale(.01f,.01f,.01f,MTXMODE_APPLY);
  MMVR_PlayerDrawBegin(play,&p->actor);MMVR_PlayerDrawEnd(play,&p->actor);Matrix_Pop();
 };
 auto prepare=[&](int hand){
  *p=baseline;gSaveContext=save;p->transformation=PLAYER_FORM_HUMAN;gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
  p->currentMask=PLAYER_MASK_GIANT;gSaveContext.save.equippedMask=PLAYER_MASK_GIANT;
  p->actor.world.pos=p->actor.prevPos={0,2000,0};p->actor.scale={.01f,.01f,.01f};p->actor.shape.rot={};p->actor.world.rot={};p->actor.velocity={};
  p->actor.update=[](Actor*,PlayState*){};p->actor.draw=[](Actor*,PlayState*){};p->actor.bgCheckFlags=0;
  p->actionFunc=Player_Action_Idle;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
  p->heldActor=p->actor.child=nullptr;p->focusActor=nullptr;p->getItemDrawIdPlusOne=0;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;
  *CONTROLLER1(&play->state)={};mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();tuning.Set(mmvr::Setting::SwordLeftHanded,1-hand);
  MMVR_PlayerEquipSword(play,p,ITEM_SWORD_GILDED);MMVR_CameraCoordinateBoundary(play);draw();
 };
 log<<",\"giantPhysical\":[";
 for(int hand=0;hand<2;++hand){
  prepare(hand);mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=96000+hand;f.timeSeconds=f.epoch;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].position={0,-.5f,0};}
  auto camera=mmvrgame::TestCameraFrame(f);auto eye=mmvr::InversePose(camera.view);
  bool eligible=mmvrgame::FirstPersonFormAllowed(p)&&!mmvrgame::GiantTransformationActive(p)&&MMVR_IndependentSword(p)&&MMVR_DisableJumpAttack(p)&&MMVR_FirstPersonBody()&&camera.active;
  bool scale=std::abs(eye.m[3][1]-p->actor.world.pos.y-mmvrgame::FormEyeHeight(p))<.01f;
  float handScale=0;for(int k=0;k<3;++k)handScale+=SQ(camera.hands[0].m[0][k]);
  scale&=std::abs(std::sqrt(handScale)-.01f*tuning.Get(mmvr::Setting::HandScale))<.00001f;
  const bool mesh=mmvrgame::FormHandMesh(p,0)==MMVR_TrackedLeftHandMesh(p)&&mmvrgame::FormHandMesh(p,1)==MMVR_TrackedRightHandMesh(p);
  bool nativeFallback=true,theater=true;
  for(int cut=0;cut<2;++cut)for(int transient=0;transient<4;++transient){
   tuning.Set(mmvr::Setting::VrCameraCutscenes,cut);p->actor.scale={.01f,.01f,.01f};p->stateFlags1=0;p->actionFunc=Player_Action_Idle;
   if(transient==0)p->actor.scale={.1f,.1f,.1f};
   if(transient==1)p->stateFlags1|=PLAYER_STATE1_100;
   if(transient==2)p->actionFunc=Player_Action_89;
   if(transient==3){p->actionFunc=Player_Action_90;p->currentMask=PLAYER_MASK_NONE;}
   auto& controls=*CONTROLLER1(&play->state);controls={};controls.cur.button=controls.press.button=BTN_B|BTN_R;
   mmvrgame::ProcessCombatInput(play);
   nativeFallback&=mmvrgame::GiantTransformationActive(p)&&!mmvrgame::FirstPersonFormAllowed(p)&&!MMVR_IndependentSword(p)&&!MMVR_DisableButtonMelee(p)&&!MMVR_DisableJumpAttack(p)&&controls.cur.button==(BTN_B|BTN_R)&&controls.press.button==(BTN_B|BTN_R);
   theater&=mmvrgame::SceneView(play)==mmvr::SceneView::Theater;p->currentMask=PLAYER_MASK_GIANT;
  }
  tuning.Set(mmvr::Setting::VrCameraCutscenes,0);
  auto cameraOnly=[&](){
   auto frame=mmvrgame::TestCameraFrame(f);const auto eye=mmvr::InversePose(frame.view);
   return frame.active&&!MMVR_FirstPersonBody()&&!frame.heldActorActive&&
    frame.hands[0].m[3][3]==0&&frame.hands[1].m[3][3]==0&&
    std::abs(eye.m[3][0]-p->actor.world.pos.x)<.01f&&
    std::abs(eye.m[3][1]-p->actor.world.pos.y-mmvrgame::FormEyeHeight(p))<.01f&&
    std::abs(eye.m[3][2]-p->actor.world.pos.z)<.01f;
  };
  int contacts[3]{};bool boundaryBlocked=true,restored=true,noOldDamage=true,unmatchedEndBlocked=true;
  for(int scenario=0;scenario<3;++scenario){
   prepare(hand);f.epoch=96100+hand*10+scenario;f.timeSeconds=f.epoch;
   EnDekubaba enemy{};enemy.actor.id=ACTOR_EN_DEKUBABA;enemy.actor.update=EnDekubaba_Update;enemy.actor.world.pos=enemy.actor.home.pos=p->actor.world.pos;
   EnDekubaba_Init(&enemy.actor,play);enemy.actor.colChkInfo.health=8;enemy.collider.base.colMaterial=COL_MATERIAL_HIT0;enemy.collider.base.acFlags&=~AC_HARD;
   const float bladeLength=MMVR_NativeSwordLength(p)*.01f;float y=2000;
   for(int tick=0;tick<90;++tick){
    float x=scenario==0?0:std::min(0.f,-.6f+std::max(0,tick-20)*.025f);
    f.timeSeconds=f.epoch+tick/90.;f.hands[hand].position={x,-.5f,0};
    if(scenario==2&&tick==42){
     y+=3150;p->actor.world.pos.y=y;p->actor.prevPos=p->actor.world.pos;
     MMVR_CameraCoordinateBoundary(play);
     boundaryBlocked&=cameraOnly()&&!mmvrgame::MeleeDebugCollider();
     MMVR_PlayerDrawEnd(play,&p->actor);unmatchedEndBlocked&=cameraOnly();
     draw();f.timeSeconds+=.00001;auto resumed=mmvrgame::TestCameraFrame(f);
     restored&=resumed.active&&MMVR_FirstPersonBody()&&std::abs(mmvr::InversePose(resumed.view).m[3][1]-y-mmvrgame::FormEyeHeight(p))<.01f;
    }
    if(scenario==2&&tick>=42){x=-.05f;f.hands[hand].position.x=x;}
    auto view=mmvr::YawPose(0,0,y+45,0),head=mmvr::YawPose(0);mmvrgame::RecordTracking(f,view,head);
    auto model=mmvr::YawPose(0,x*40,y+20,0);for(int k=0;k<3;++k)model.m[k][k]=.01f;
    mmvrgame::UpdateSwordDiagnostics(f,model);
    for(auto& e:enemy.colliderElements){e.dim.worldSphere.center={(s16)(bladeLength-1),(s16)(y+22),0};e.dim.worldSphere.radius=2;e.base.acElemFlags|=ACELEM_ON;}
    if(tick%3==0){
     CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetJntSphAC(play,&enemy.collider.base);CollisionCheck_ResetDamage(&enemy.actor.colChkInfo);enemy.collider.base.acFlags|=AC_ON;
     if(scenario!=2||tick>=42)CollisionCheck_SetAC(play,&play->colChkCtx,&enemy.collider.base);
     mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
     if(enemy.collider.base.acFlags&AC_HIT){++contacts[scenario];EnDekubaba_UpdateDamage(&enemy,play);}
    }
   }
   if(scenario==2)noOldDamage=enemy.actor.colChkInfo.health==8;
   EnDekubaba_Destroy(&enemy.actor,play);play->colChkCtx=collision;
  }
  // Ordinary scene entry uses the same invalidation path, then a real draw reopens it.
  prepare(hand);MMVR_CameraSceneBoundary(play);draw();const auto oldScene=play->sceneId;play->sceneId^=1;
  MMVR_CameraSceneBoundary(play);bool sceneReset=cameraOnly();
  play->sceneId=oldScene;MMVR_CameraSceneBoundary(play);draw();f.timeSeconds+=.02;
  sceneReset&=MMVR_FirstPersonBody()&&mmvrgame::TestCameraFrame(f).active;
  // The same held trigger must not silently acquire a mask after cancellation.
  prepare(hand);MMVR_PlayerEmptyHands(play,p);
  const bool maskHandEmpty=p->heldItemAction==PLAYER_IA_NONE;
  mmvr::SetMaskContext(ITEM_MASK_GIANT,false);f.epoch=96200+hand;f.timeSeconds=f.epoch;f.hands[hand].position={.3f,-.2f,-.4f};
  auto maskSample=[&](float trigger){f.timeSeconds+=.02;f.triggers[hand]=trigger;mmvr::UpdateMaskTracking(f,true);};
  maskSample(0);maskSample(0);maskSample(1);const bool wasHeld=mmvr::HeldMaskItem()==ITEM_MASK_GIANT;
  MMVR_CameraCoordinateBoundary(play);maskSample(1);maskSample(1);
  bool rearm=maskHandEmpty&&wasHeld&&mmvr::HeldMaskItem()<0&&mmvr::TakeMaskUse()<0;
  maskSample(0);maskSample(1);rearm&=mmvr::HeldMaskItem()==ITEM_MASK_GIANT;mmvr::CancelHeldMask();mmvr::SetMaskContext(-1,false);
  if(hand)log<<",";
  log<<"{\"hand\":"<<hand<<",\"eligible\":"<<eligible<<",\"humanScale\":"<<scale<<",\"humanMesh\":"<<mesh<<",\"transientNative\":"<<nativeFallback<<",\"transientTheater\":"<<theater<<",\"stationaryContacts\":"<<contacts[0]<<",\"swingContacts\":"<<contacts[1]<<",\"boundaryContacts\":"<<contacts[2]<<",\"boundaryBlocked\":"<<boundaryBlocked<<",\"freshDrawRestored\":"<<restored<<",\"noOldDamage\":"<<noOldDamage<<",\"maskHandEmpty\":"<<maskHandEmpty<<",\"maskInitiallyHeld\":"<<wasHeld<<",\"triggerRearm\":"<<rearm<<",\"unmatchedEndBlocked\":"<<unmatchedEndBlocked<<",\"ordinarySceneReset\":"<<sceneReset<<"}";
 }
 log<<"]";
 // Use the native interpolation recorder, not a parallel copy of its math.
 Mtx destination{};int label=0;char recordSource[]="mmvr-giant-lifecycle";auto record=[&](float y){
  FrameInterpolation_ShouldInterpolateFrame(true);FrameInterpolation_StartRecord();FrameInterpolation_RecordOpenChild(&label,0);
  Matrix_Push();Matrix_Translate(0,y,0,MTXMODE_NEW);FrameInterpolation_RecordMatrixToMtx(&destination,recordSource,__LINE__);Matrix_Pop();
  FrameInterpolation_RecordCloseChild();FrameInterpolation_StopRecord();
 };
 record(2000);record(2010);auto before=FrameInterpolation_Interpolate(.5f);
 const bool actuallyInterpolated=before.contains(&destination)&&std::abs(before.at(&destination).mf[3][1]-2005)<.01f;
 MMVR_CameraCoordinateBoundary(play);const bool pendingSuppressed=FrameInterpolation_Interpolate(.5f).empty();record(5160);bool cut=true;
 for(float alpha:{0.f,.25f,.5f,1.f}){auto frames=FrameInterpolation_Interpolate(alpha);cut&=frames.contains(&destination)&&std::abs(frames.at(&destination).mf[3][1]-5160)<.01f;}
 log<<",\"giantInterpolation\":{\"primed\":"<<actuallyInterpolated<<",\"boundaryCut\":"<<cut<<",\"pendingSuppressed\":"<<pendingSuppressed<<"}";
 FrameInterpolation_ResetHistory();CVarSetInteger("gInterpolationFPS",fps);
 *p=saved;gSaveContext=save;tuning=settings;*CONTROLLER1(&play->state)=input;play->colChkCtx=collision;play->csCtx=cs;play->pauseCtx.state=pause;play->msgCtx.msgMode=msg;
 play->transitionTrigger=transition;play->transitionMode=mode;play->roomCtx.status=roomStatus;play->numSetupActors=setup;
 MMVR_CameraCoordinateBoundary(play);draw();*p=saved;gfx->polyOpa=opa;gfx->polyXlu=xlu;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
}