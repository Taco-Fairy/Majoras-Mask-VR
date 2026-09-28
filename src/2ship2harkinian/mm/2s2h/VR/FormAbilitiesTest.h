#pragma once
#include "FormPresentation.h"
#include "form_presentation.h"
extern "C" {
#include "overlays/actors/ovl_En_Boom/z_en_boom.h"
void EnBoom_Destroy(Actor*,PlayState*);
s32 MMVR_ZoraGroundSteering(Player*);
void Player_InitItemActionWithAnim(PlayState*,Player*,PlayerItemAction);
s32 Player_UpperAction_7(Player*,PlayState*);s32 Player_UpperAction_8(Player*,PlayState*);
s32 Player_UpperAction_6(Player*,PlayState*);s32 Player_UpperAction_12(Player*,PlayState*);
s32 Player_UpperAction_13(Player*,PlayState*);s32 Player_UpperAction_14(Player*,PlayState*);s32 Player_UpperAction_15(Player*,PlayState*);
void Player_Action_72(Player*,PlayState*);
void Player_Action_43(Player*,PlayState*);void Player_Action_Idle(Player*,PlayState*);void Player_Action_95(Player*,PlayState*);
}
static void NativeFormAbilitiesTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto collision=play->colChkCtx;
 // Native-scale matrices below must not inherit a saved floor calibration.
 mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
 auto prepare=[&](){*p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->currentMask=PLAYER_MASK_NONE;p->heldActor=p->actor.child=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;*CONTROLLER1(&play->state)={};gSaveContext=save;mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();mmvrgame::ClearItemSelection();};
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
 mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=16000;f.timeSeconds=16000;
 for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.hands[h].position={h?.3f:-.3f,-.25f,-.4f};f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
 {
  prepare();mmvr::SetNativeTestEye(0);mmvr::CameraFrame camera;camera.active=camera.heldActorActive=true;
  Mtx matrices[3]{};mmvr::SetHeldActorRange(&matrices[0],&matrices[1],0);mmvr::SetHeldActorRange(&matrices[1],&matrices[2],1);
  bool tracked=true;for(int i=0;i<5;++i){camera.heldActorCorrection=mmvr::YawPose(0,float(i),float(i)*2,-float(i));mmvr::SetNativeTestCamera(camera);
   for(int layer=0;layer<2;++layer){auto model=mmvr::YawPose(0,20,30,40);tracked&=mmvr::OverrideModelMatrix(&matrices[layer],model.m)&&std::abs(model.m[3][0]-20-i)<.001f&&std::abs(model.m[3][1]-30-2*i)<.001f;}
  }
  log<<",\"trackedTranslucentActor\":"<<tracked;mmvr::SetHeldActorRange(nullptr,nullptr,0);mmvr::SetHeldActorRange(nullptr,nullptr,1);mmvr::SetNativeTestCamera({});
 }
 {
  prepare();p->transformation=PLAYER_FORM_DEKU;p->stateFlags1=PLAYER_STATE1_400000;
  p->actor.shape.rot.y=(s16)0x8000;
  auto reference=mmvr::YawPose(0,0,2000+mmvrgame::FormEyeHeight(p),0);
  mmvrgame::RecordFormTracking(f,reference,head);
  auto native=mmvr::YawPose(0,0,reference.m[3][1]-8,-5);for(int k=0;k<3;++k)native.m[k][k]=.01f;
  Matrix_Push();Matrix_Put((MtxF*)&native);MMVR_RecordDekuGuard(play,p);Matrix_Pop();
  Mtx guardAddress{},bubbleAddress{};MMVR_BindDekuGuard(&guardAddress);mmvr::SetDekuBubbleMatrix(&bubbleAddress);mmvr::SetNativeTestEye(0);
  bool guard=true,bubble=true;
  for(int i=0;i<6;++i){
   auto movingHead=mmvr::PoseMatrix({{std::sin(i*.06f),0,0,std::cos(i*.06f)},{0,0,0}});
   auto movingView=reference;movingView.m[3][2]-=float(i)*2;
   mmvrgame::RecordFormTracking(f,movingView,movingHead);
   auto pose=mmvrgame::DekuGuardPose(play,p),expected=mmvr::Multiply(mmvr::Multiply(native,mmvr::InversePose(reference)),mmvrgame::FormHeadPose());
   mmvr::CameraFrame camera;camera.active=true;camera.dekuGuard=pose;camera.dekuGuardCorrection=mmvrgame::DekuGuardCorrection(play,p);camera.dekuBubble=mmvrgame::FormHeadPose();mmvr::SetNativeTestCamera(camera);
   auto output=native;guard&=mmvr::OverrideModelMatrix(&guardAddress,output.m,native.m);
   for(int row=0;row<4;++row)for(int col=0;col<4;++col)guard&=std::abs(output.m[row][col]-expected.m[row][col])<.001f;
   // Native world-space interpolation must not reintroduce stale head/root motion.
   auto intermediate=native;intermediate.m[3][1]-=3;
   auto interpolatedExpected=camera.dekuGuard;
   output=intermediate;guard&=mmvr::OverrideModelMatrix(&guardAddress,output.m,native.m);
   for(int row=0;row<4;++row)for(int col=0;col<4;++col)guard&=std::abs(output.m[row][col]-interpolatedExpected.m[row][col])<.001f;
   output=native;bubble&=mmvr::OverrideModelMatrix(&bubbleAddress,output.m,native.m);
   for(int c=0;c<3;++c)bubble&=std::abs(output.m[3][c]-(camera.dekuBubble.m[3][c]-camera.dekuBubble.m[2][c]*4.6f))<.001f;
  }
  log<<",\"dekuHeadReplay\":{\"guard\":"<<guard<<",\"bubble\":"<<bubble<<"}";
  mmvr::ResetFormEffectMatrices();mmvr::SetNativeTestCamera({});mmvrgame::ClearFormTracking();
 }
 log<<",\"formPresentation\":[";
 for(int form:{PLAYER_FORM_DEKU,PLAYER_FORM_ZORA}){
  prepare();p->transformation=form;p->actionFunc=Player_Action_43;p->upperActionFunc=form==PLAYER_FORM_DEKU?Player_UpperAction_7:Player_UpperAction_13;
  mmvrgame::RecordFormTracking(f,view,head);mmvrgame::RecordTracking(f,view,head);
  mmvr::CameraFrame frame;for(int h=0;h<2;++h)frame.hands[h]=mmvr::TrackedHandModel(f,view,head,h,h,mmvr::GetSettings());
  mmvrgame::UpdateFormPresentation(frame);bool reticle=frame.itemReticle.m[3][3]==1;
  p->speedXZ=6;p->actor.speed=6;CONTROLLER1(&play->state)->cur.stick_x=80;CONTROLLER1(&play->state)->rel.stick_x=80;
  mmvrgame::ProcessFormInput(play);bool stationary=p->speedXZ==0&&p->actor.speed==0&&CONTROLLER1(&play->state)->cur.stick_x==0;
  bool detached=true;if(form==PLAYER_FORM_ZORA){Actor fin{};fin.params=0;p->stateFlags1|=PLAYER_STATE1_ZORA_BOOMERANG_THROWN;p->zoraBoomerangActor=&fin;mmvr::CameraFrame flight;mmvrgame::UpdateFormPresentation(flight);detached=flight.formFins[0].m[3][3]==0;p->zoraBoomerangActor=nullptr;}
  auto oldState=play->pauseCtx.state;mmvr::SetNativePause(true);bool pauseHidden=!mmvrgame::FormReticleVisible(p);mmvr::SetNativePause(false);play->pauseCtx.state=oldState;
  p->actionFunc=Player_Action_Idle;p->upperActionFunc=Player_UpperAction_6;bool idleHidden=!mmvrgame::FormReticleVisible(p);
  if(form==PLAYER_FORM_ZORA)log<<",";log<<"{\"form\":"<<form<<",\"reticle\":"<<reticle<<",\"stationary\":"<<stationary<<",\"detached\":"<<detached<<",\"pauseHidden\":"<<pauseHidden<<",\"idleHidden\":"<<idleHidden<<"}";
 }
 log<<"],\"formEndShot\":[";
 for(int form:{PLAYER_FORM_DEKU,PLAYER_FORM_ZORA}){
  prepare();p->transformation=form;p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;
  mmvrgame::ResetTestCamera();
  // Commit the changed form's draw epoch before exercising its native end-shot
  // transition, just as the live renderer does after a transformation.
  p->actor.init=nullptr;Matrix_Push();
  Matrix_Translate(p->actor.world.pos.x,p->actor.world.pos.y,p->actor.world.pos.z,MTXMODE_NEW);
  Matrix_Scale(.01f,.01f,.01f,MTXMODE_APPLY);
  MMVR_PlayerDrawBegin(play,&p->actor);MMVR_PlayerDrawEnd(play,&p->actor);Matrix_Pop();
  f.epoch++;auto camera=mmvrgame::TestCameraFrame(f);
  p->actionFunc=Player_Action_43;p->unk_AA5=PLAYER_UNKAA5_3;p->stateFlags1|=PLAYER_STATE1_100000;p->stateFlags3|=PLAYER_STATE3_40;
  p->upperActionFunc=form==PLAYER_FORM_DEKU?Player_UpperAction_8:Player_UpperAction_14;
  PlayerAnimation_PlayOnce(play,&p->skelAnimeUpper,(PlayerAnimationHeader*)(form==PLAYER_FORM_DEKU?gPlayerAnim_pn_tamahaki:gPlayerAnim_pz_cutterattack));
  p->skelAnimeUpper.curFrame=p->skelAnimeUpper.endFrame;
  if(form==PLAYER_FORM_DEKU)Player_UpperAction_8(p,play);else Player_UpperAction_14(p,play);
  bool finished=p->unk_AA5==PLAYER_UNKAA5_0&&p->actionFunc!=Player_Action_43&&!(p->stateFlags1&PLAYER_STATE1_100000)&&p->upperActionFunc==(form==PLAYER_FORM_DEKU?Player_UpperAction_6:Player_UpperAction_15);
  if(form==PLAYER_FORM_ZORA)log<<",";log<<"{\"form\":"<<form<<",\"camera\":"<<camera.active<<",\"finished\":"<<finished<<"}";
 }
 log<<"]";
 {
  prepare();p->transformation=PLAYER_FORM_ZORA;p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;
  mmvrgame::ResetTestCamera();
  // A form change needs a matching draw epoch before first-person logic accepts
  // it. Changing only transformation leaves the human draw intentionally stale.
  p->actor.init=nullptr;
  Matrix_Push();Matrix_Translate(p->actor.world.pos.x,p->actor.world.pos.y,p->actor.world.pos.z,MTXMODE_NEW);
  Matrix_Scale(.01f,.01f,.01f,MTXMODE_APPLY);
  MMVR_PlayerDrawBegin(play,&p->actor);MMVR_PlayerDrawEnd(play,&p->actor);Matrix_Pop();
  f.epoch++;auto camera=mmvrgame::TestCameraFrame(f);
  p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;p->stateFlags1=0;p->currentBoots=PLAYER_BOOTS_ZORA_LAND;
  bool groundSteering=MMVR_ZoraGroundSteering(p);
  p->stateFlags1=PLAYER_STATE1_8000000;p->currentBoots=PLAYER_BOOTS_ZORA_UNDERWATER;
  groundSteering&=MMVR_ZoraGroundSteering(p);
  p->actor.bgCheckFlags=0;groundSteering&=!MMVR_ZoraGroundSteering(p);
  p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;p->currentBoots=PLAYER_BOOTS_ZORA_LAND;
  groundSteering&=!MMVR_ZoraGroundSteering(p);
  p->stateFlags1=0;p->transformation=PLAYER_FORM_HUMAN;groundSteering&=!MMVR_ZoraGroundSteering(p);
  p->transformation=PLAYER_FORM_ZORA;
  log<<",\"zoraGroundSteering\":"<<groundSteering;
  EnBoom left{},right{};left.effectIndex=right.effectIndex=0x7FFFFFFF /* no allocated trail: Effect_Destroy ignores out-of-range indices */;
  Collider_InitQuad(play,&left.collider);Collider_InitQuad(play,&right.collider);
  left.actor.child=&right.actor;right.actor.parent=&left.actor;p->zoraBoomerangActor=&left.actor;
  Actor target{};p->focusActor=&target;p->autoLockOnActor=&target;p->zTargetActiveTimer=5;
  p->stateFlags1|=PLAYER_STATE1_ZORA_BOOMERANG_THROWN|PLAYER_STATE1_PARALLEL|PLAYER_STATE1_Z_TARGETING;
  p->stateFlags2|=PLAYER_STATE2_LOCK_ON_WITH_SWITCH;
  EnBoom_Destroy(&left.actor,play);
  const bool firstKeeps=p->focusActor==&target&&p->zoraBoomerangActor==&right.actor&&
   (p->stateFlags1&PLAYER_STATE1_ZORA_BOOMERANG_THROWN);
  mmvrgame::ResetTestCamera(); // Gameplay cleanup must not require a rendered player.
  EnBoom_Destroy(&right.actor,play);
  const bool lastClears=!p->focusActor&&!p->autoLockOnActor&&!p->zoraBoomerangActor&&!p->zTargetActiveTimer&&
   !(p->stateFlags1&(PLAYER_STATE1_PARALLEL|PLAYER_STATE1_Z_TARGETING|PLAYER_STATE1_ZORA_BOOMERANG_THROWN))&&
   !(p->stateFlags2&PLAYER_STATE2_LOCK_ON_WITH_SWITCH);
  // Cleanup without a live throw must not cancel a later deliberate Y lock.
  p->focusActor=&target;p->stateFlags2|=PLAYER_STATE2_LOCK_ON_WITH_SWITCH;p->zTargetActiveTimer=5;
  EnBoom spare{};spare.effectIndex=0x7FFFFFFF /* no allocated trail: Effect_Destroy ignores out-of-range indices */;Collider_InitQuad(play,&spare.collider);EnBoom_Destroy(&spare.actor,play);
  log<<",\"zoraFinReturn\":{\"camera\":"<<camera.active<<",\"firstKeeps\":"<<firstKeeps<<",\"lastClears\":"<<lastClears
     <<",\"ordinaryLockPreserved\":"<<(p->focusActor==&target&&p->zTargetActiveTimer==5)<<"}";
 }
 log<<",\"formWinding\":[";
 for(int form=0;form<PLAYER_FORM_MAX;++form)for(int left=0;left<2;++left){
  prepare();p->transformation=form;p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
  mmvrgame::ResetTestCamera();f.epoch++;f.grips[0]=f.grips[1]=0;auto a=mmvrgame::TestCameraFrame(f);f.grips[0]=f.grips[1]=1;auto b=mmvrgame::TestCameraFrame(f);
  bool same=a.active&&b.active;for(int h=0;h<2;++h)for(int i=0;i<4;++i)for(int j=0;j<4;++j)same&=std::abs(a.hands[h].m[i][j]-b.hands[h].m[i][j])<.0001;
  if(form||left)log<<",";log<<"{\"form\":"<<form<<",\"left\":"<<left<<",\"gripStable\":"<<same<<"}";
 }
 log<<"],\"formReplay\":{";
 prepare();p->transformation=PLAYER_FORM_DEKU;p->actionFunc=Player_Action_95;mmvrgame::RecordFormTracking(f,view,head);
 Vec3f tip{},base{};MMVR_DekuSpinTrail(play,p,&tip.x,&base.x);
 float radius=std::hypot(tip.x-view.m[3][0],tip.z-view.m[3][2]);bool swirl=std::abs(radius-mmvr::GetSettings().Get(mmvr::Setting::DekuSpinRadius)*144)<.01f&&tip.y>base.y;
 p->actor.shape.rot.y=0x4000;Vec3f turn{},turnBase{};MMVR_DekuSpinTrail(play,p,&turn.x,&turnBase.x);swirl&=std::abs(turn.x-view.m[3][0]-radius)<.01f&&mmvrgame::NativeAbilityOwnsFacing(p);
 mmvr::ResetFormEffectMatrices();Mtx effectAddress{},finAddress{};auto local=mmvr::YawPose(.2f,5,0,-10);mmvr::SetFormEffectMatrix(&effectAddress,local);mmvr::SetFormFinMatrix(0,&finAddress);
 mmvr::CameraFrame replay;replay.active=true;replay.formEffectAnchor=mmvr::YawPose(.6f,100,80,-50);replay.formFins[0]=mmvr::YawPose(.4f,20,30,40);
 mmvr::SetNativeTestCamera(replay);mmvr::SetNativeTestEye(.3f);float output[4][4]{};bool late=mmvr::OverrideModelMatrix(&effectAddress,output);auto expected=mmvr::Multiply(local,replay.formEffectAnchor);
 for(int i=0;i<4;++i)for(int j=0;j<4;++j)late&=std::abs(output[i][j]-expected.m[i][j])<.001f;
 bool fin=mmvr::OverrideModelMatrix(&finAddress,output);for(int i=0;i<4;++i)for(int j=0;j<4;++j)fin&=std::abs(output[i][j]-replay.formFins[0].m[i][j])<.001f;
 Mtx shieldAddress{};mmvr::SetShieldEffectMatrix(&shieldAddress,local);replay.shieldEffectAnchor=mmvr::YawPose(-.7f,200,120,30);mmvr::SetNativeTestCamera(replay);
 bool shieldLate=mmvr::OverrideModelMatrix(&shieldAddress,output);auto shieldExpected=mmvr::Multiply(local,replay.shieldEffectAnchor);
 for(int i=0;i<4;++i)for(int j=0;j<4;++j)shieldLate&=std::abs(output[i][j]-shieldExpected.m[i][j])<.001f;
 late&=shieldLate;
 mmvr::ResetFormEffectMatrices();bool stale=!mmvr::OverrideModelMatrix(&shieldAddress,output)&&!mmvr::OverrideModelMatrix(&effectAddress,output)&&!mmvr::OverrideModelMatrix(&finAddress,output);
 log<<"\"swirl\":"<<swirl<<",\"lateEffect\":"<<late<<",\"lateFin\":"<<fin<<",\"staleCleared\":"<<stale<<"},\"physicalSticks\":[";
 for(int left=0;left<2;++left)for(int miss=0;miss<2;++miss){
  prepare();mmvr::GetSettings()=settings;mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);int hand=1-left;
  p->heldItemId=ITEM_DEKU_STICK;p->nextModelGroup=Player_ActionToModelGroup(p,PLAYER_IA_DEKU_STICK);Player_InitItemActionWithAnim(play,p,PLAYER_IA_DEKU_STICK);AMMO(ITEM_DEKU_STICK)=10;
  f.epoch++;f.grips[0]=f.grips[1]=0;f.hands[hand].position={0,-.45f,-.35f};
  auto centerModel=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());
  Vec3f center{};for(int k=0;k<3;++k)(&center.x)[k]=centerModel.m[3][k]+1800*centerModel.m[0][k]+267.2f*centerModel.m[1][k]-33.82f*centerModel.m[2][k];
  Actor target{};target.id=ACTOR_EN_KUSA;target.update=[](Actor*,PlayState*){};target.world.pos=center;
  ColliderCylinderInit init={{COL_MATERIAL_NONE,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},{ELEM_MATERIAL_UNK0,{0,0,0},{DMG_DEKU_STICK,0,0},ATELEM_NONE,ACELEM_ON,OCELEM_NONE},{4,8,0,{(s16)center.x,(s16)(center.y-4+(miss?100:0)),(s16)center.z}}};
  ColliderCylinder cylinder;Collider_InitAndSetCylinder(play,&cylinder,&target,&init);int contacts=0;float damage=0;bool animationFree=true;
  for(int sample=0;sample<105;++sample){
   f.timeSeconds=17000+left*4+miss*2+sample/90.;f.hands[hand].position.x=sample<25?-.25f:std::min(.3f,-.25f+(sample-25)*.02f);
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateSwordDiagnostics(f,model);
   if(sample%3==0){CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetCylinderAC(play,&cylinder.base);CollisionCheck_ResetDamage(&target.colChkInfo);CollisionCheck_SetAC(play,&play->colChkCtx,&cylinder.base);
    mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
    if(cylinder.base.acFlags&AC_HIT){++contacts;damage=target.colChkInfo.damage;}
   }
   animationFree&=p->meleeWeaponState==PLAYER_MELEE_WEAPON_STATE_0;
   if(!miss&&sample%20==0){const auto& d=mmvr::GetCombatDiagnostics();std::ofstream("native-stick-diagnosis.log",std::ios::app)<<"left="<<left<<" sample="<<sample<<" weapon="<<Player_GetMeleeWeaponHeld(p)<<" form="<<int(p->transformation)<<" held="<<p->heldItemAction<<" item="<<p->itemAction<<" active="<<d.active<<" blocked="<<d.blocked<<" speed="<<d.speed<<" swings="<<d.swings<<" allowed="<<mmvr::PhysicalActionsAllowed()<<" fp="<<mmvr::FirstPersonRequested()<<" msg="<<int(play->msgCtx.msgMode)<<" cs="<<int(p->csAction)<<" state="<<p->stateFlags1<<","<<p->stateFlags2<<","<<p->stateFlags3<<"\n";}
  }
  if(left||miss)log<<",";log<<"{\"left\":"<<left<<",\"miss\":"<<miss<<",\"contacts\":"<<contacts<<",\"damage\":"<<damage<<",\"ammo\":"<<int(AMMO(ITEM_DEKU_STICK))<<",\"empty\":"<<(p->heldItemAction==PLAYER_IA_NONE)<<",\"animationFree\":"<<animationFree<<"}";
  Collider_DestroyCylinder(play,&cylinder);
 }
 log<<"]";
 {
  prepare();p->transformation=PLAYER_FORM_HUMAN;p->actionFunc=Player_Action_72;
  p->stateFlags2=PLAYER_STATE2_80;mmvr::SetNativeTestTracking(true);
  int points=0;f.epoch++;f.timeSeconds+=1;
  for(int i=0;i<270;++i) {
   f.timeSeconds+=1./90.;f.hands[0].position.x=float(.06*std::sin(i/90.*6.2831853*3));
   mmvrgame::RecordGrabShake(f);mmvrgame::RecordFormTracking(f,view,head);
   points+=MMVR_PhysicalGrabEscape(p);
  }
  p->actionFunc=Player_Action_Idle;p->stateFlags2=0;
  f.timeSeconds+=1./90.;mmvrgame::RecordGrabShake(f);
  log<<",\"physicalEscape\":{\"points\":"<<points<<",\"releasedEmpty\":"<<(MMVR_PhysicalGrabEscape(p)==0)<<"}";
 }
 mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();mmvrgame::ResetTestCamera();*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;play->colChkCtx=collision;
}
