#pragma once
#include "Masks.h"
#include "ScenePresentation.h"
#include "MaskModels.h"
#include "Holster.h"
#include "FormAim.h"
#include "NativeForms.h"
extern "C" void Player_Action_86(Player*,PlayState*);
extern "C" void Player_Action_93(Player*,PlayState*);
extern "C" void Player_Action_Idle(Player*,PlayState*);
extern "C" void Player_Action_26(Player*,PlayState*);
extern "C" void KaleidoScope_HandlePageToggles(PlayState*,Input*);
static void NativeGestureTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);
 mmvr::GetSettings().Set(mmvr::Setting::VrCameraCutscenes,0);
 auto prepare=[&](){*p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->currentMask=PLAYER_MASK_NONE;p->heldActor=p->actor.child=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;*CONTROLLER1(&play->state)={};gSaveContext=save;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();};
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
 {auto cs=play->csCtx;auto flags=play->actorCtx.flags;prepare();p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;p->csAction=PLAYER_CSACTION_WAIT;
 mmvr::TrackingFrame cameraTracking{};cameraTracking.head.orientation.w=cameraTracking.origin.orientation.w=1;cameraTracking.epoch=9900;cameraTracking.timeSeconds=9900;
 auto firstView=mmvrgame::TestCameraFrame(cameraTracking);p->actor.world.pos.x+=100;p->actor.world.pos.y+=25;p->actor.shape.rot.y+=10000;cameraTracking.timeSeconds+=.02;
 auto frozenView=mmvrgame::TestCameraFrame(cameraTracking);bool anchored=firstView.active&&frozenView.active;
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)anchored&=std::abs(firstView.view.m[i][j]-frozenView.view.m[i][j])<.001f;
 auto aPose=mmvr::InversePose(firstView.view),bPose=mmvr::InversePose(frozenView.view);anchored&=std::abs(bPose.m[3][0]-aPose.m[3][0]-100)<.001f&&std::abs(bPose.m[3][1]-aPose.m[3][1]-25)<.001f;
 float itemPos[3]{};bool overhead=MMVR_ItemPresentationPosition(itemPos)&&itemPos[1]>mmvr::InversePose(firstView.view).m[3][1];
 cameraTracking.head.position.x=.1f;cameraTracking.timeSeconds+=.02;auto leanView=mmvrgame::TestCameraFrame(cameraTracking);bool lean=std::abs(leanView.view.m[3][0]-frozenView.view.m[3][0])>3.9f;
 play->actorCtx.flags|=ACTORCTX_FLAG_TELESCOPE_ON;bool theaterClean=!MMVR_FirstPersonBody()&&!MMVR_ItemPresentationPosition(itemPos);play->actorCtx.flags=flags;
 mmvrgame::ResetTestCamera();
 bool local=mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));play->csCtx.state=CS_STATE_RUN;play->csCtx.playerCue=nullptr;p->getItemDrawIdPlusOne=0;
 bool remote=!mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));CsCmdActorCue cue{};play->csCtx.playerCue=&cue;bool cueLocal=mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));
 int gameMode=gSaveContext.gameMode;gSaveContext.gameMode=GAMEMODE_END_CREDITS;bool creditsWithLink=mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));gSaveContext.gameMode=gameMode;
 play->actorCtx.flags|=ACTORCTX_FLAG_TELESCOPE_ON;bool telescope=!mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));play->actorCtx.flags=flags;play->csCtx.playerCue=nullptr;
 Actor scrub{};scrub.id=ACTOR_EN_SELLNUTS;scrub.update=[](Actor*,PlayState*){};auto* oldNpc=play->actorCtx.actorLists[ACTORCAT_NPC].first;scrub.next=oldNpc;play->actorCtx.actorLists[ACTORCAT_NPC].first=&scrub;p->csActor=&scrub;bool scrubLocal=mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));p->csActor=nullptr;play->actorCtx.actorLists[ACTORCAT_NPC].first=oldNpc;
 p->getItemDrawIdPlusOne=GID_MASK_TRUTH+1;bool reward=mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));p->actor.draw=nullptr;bool hidden=mmvr::ImmersiveScene(mmvrgame::SceneFacts(play));
 log<<",\"sceneRouting\":{\"local\":"<<local<<",\"remote\":"<<remote<<",\"creditsWithLink\":"<<creditsWithLink<<",\"cue\":"<<cueLocal<<",\"telescope\":"<<telescope<<",\"scrub\":"<<scrubLocal<<",\"reward\":"<<reward<<",\"theaterClean\":"<<theaterClean<<",\"anchored\":"<<anchored<<",\"overhead\":"<<overhead<<",\"lean\":"<<lean<<",\"hidden\":"<<hidden<<"}";
 play->csCtx=cs;play->actorCtx.flags=flags;}
 {
  prepare();p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;
  mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=9950;f.timeSeconds=9950;
  Mtx submitted{};auto oldView=play->view;play->view.viewingPtr=&submitted;MMVR_CaptureWorldView(&submitted);
  auto before=mmvrgame::TestCameraFrame(f);p->csAction=PLAYER_CSACTION_WAIT;View_UpdateViewingMatrix(&play->view);f.timeSeconds+=.02;
  auto during=mmvrgame::TestCameraFrame(f);bool pointer=before.active&&during.active&&during.viewAddress==&submitted&&play->view.viewingPtr!=&submitted;
  mmvr::SetNativeTestCamera(during);mmvr::SetNativeTestEye(0);float actual[4][4]{};pointer&=mmvr::OverrideViewMatrix(&submitted,actual);
  bool stable=true;for(int i=0;i<4;++i)for(int j=0;j<4;++j)stable&=std::abs(before.view.m[i][j]-during.view.m[i][j])<.001f;
  auto oldAction=p->actionFunc;p->actionFunc=Player_Action_86;p->actor.draw=nullptr;
  bool forms=true;for(int form=0;form<PLAYER_FORM_MAX;++form){p->transformation=form;f.timeSeconds+=.02;forms&=mmvrgame::TestCameraFrame(f).active;}
  log<<",\"cameraTransitions\":{\"submittedPointer\":"<<pointer<<",\"stableEntry\":"<<stable<<",\"hiddenForms\":"<<forms<<"}";
  p->actionFunc=oldAction;play->view=oldView;mmvrgame::ResetTestCamera();
  prepare();p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;
  f.timeSeconds+=.02;auto entry=mmvrgame::TestCameraFrame(f);
  p->stateFlags1|=PLAYER_STATE1_20000000;p->actor.shape.rot.y=1234;p->actor.world.rot.y=1234;
  f.head.orientation={0,.5f,0,.8660254f};f.timeSeconds+=.02;mmvrgame::TestCameraFrame(f);
  const bool entryFacing=entry.active&&p->actor.shape.rot.y==1234&&MMVR_InputYaw(4321)==4321;
  p->stateFlags1&=~PLAYER_STATE1_20000000;
  const auto priorScene=play->sceneId;play->sceneId^=1;
  const bool invalidated=!MMVR_FirstPersonBody()&&MMVR_InputYaw(4321)==4321;
  play->sceneId=priorScene;
  log<<",\"sceneOwnership\":{\"nativeEntryFacing\":"<<entryFacing<<",\"staleCameraInvalidated\":"<<invalidated<<"}";
  mmvrgame::ResetTestCamera();
 }
 {
  auto oldTrigger=play->transitionTrigger;auto oldMode=play->transitionMode;
  bool retained=true,nativeFacing=true,nativePosition=true;
  for(int camera=0;camera<2;++camera)for(int stage=0;stage<2;++stage){
   prepare();p->csAction=PLAYER_CSACTION_NONE;p->getItemDrawIdPlusOne=0;
   p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;
   mmvr::GetSettings().Set(mmvr::Setting::VrCameraCutscenes,camera);
   play->transitionTrigger=TRANS_TRIGGER_OFF;play->transitionMode=TRANS_MODE_OFF;
   mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;
   f.epoch=9955+camera*2+stage;f.timeSeconds=9955+camera*2+stage;
   auto before=mmvrgame::TestCameraFrame(f);
   play->transitionTrigger=stage?TRANS_TRIGGER_OFF:TRANS_TRIGGER_START;
   play->transitionMode=stage?TRANS_MODE_FILL_IN:TRANS_MODE_OFF;
   p->actor.shape.rot.y=p->actor.world.rot.y=1234;auto start=p->actor.world.pos;
   f.head.orientation={0,.5f,0,.8660254f};f.head.position.x=.03f;f.timeSeconds+=.02;
   auto during=mmvrgame::TestCameraFrame(f);retained&=before.active&&during.active;
   nativeFacing&=p->actor.shape.rot.y==1234&&p->actor.world.rot.y==1234;
   nativePosition&=std::abs(p->actor.world.pos.x-start.x)<.001f&&std::abs(p->actor.world.pos.z-start.z)<.001f;
  }
  log<<",\"areaFadeCamera\":{\"firstPersonBothPolicies\":"<<retained<<",\"nativeFacing\":"<<nativeFacing<<",\"nativePosition\":"<<nativePosition<<"}";
  play->transitionTrigger=oldTrigger;play->transitionMode=oldMode;mmvrgame::ResetTestCamera();
  mmvr::GetSettings().Set(mmvr::Setting::VrCameraCutscenes,0);
 }
 {
  prepare();auto oldCs=play->csCtx;play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;
  auto* cam=GET_ACTIVE_CAM(play);auto* oldTarget=cam->target;Actor scrub{};scrub.id=ACTOR_EN_SELLNUTS;scrub.update=[](Actor*,PlayState*){};
  auto* oldNpc=play->actorCtx.actorLists[ACTORCAT_NPC].first;scrub.next=oldNpc;play->actorCtx.actorLists[ACTORCAT_NPC].first=&scrub;p->csActor=cam->target=&scrub;p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;p->csAction=PLAYER_CSACTION_NONE;p->getItemDrawIdPlusOne=0;
  mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=9960;f.timeSeconds=9960;
  bool stale=!mmvrgame::SceneFacts(play).cinematic&&!mmvrgame::InWorldCinematic(play),follow=true,hold=true,exit=true;
  for(int cycle=0;cycle<4;++cycle){
   auto before=mmvrgame::TestCameraFrame(f);p->actor.world.pos.x+=10;f.timeSeconds+=.02;
   auto moved=mmvrgame::TestCameraFrame(f);follow&=before.active&&moved.active&&std::abs(mmvr::InversePose(moved.view).m[3][0]-mmvr::InversePose(before.view).m[3][0]-10)<.001f;
   p->csAction=PLAYER_CSACTION_WAIT;f.timeSeconds+=.02;auto locked=mmvrgame::TestCameraFrame(f);
   p->actor.world.pos.x+=3;f.timeSeconds+=.02;auto cut=mmvrgame::TestCameraFrame(f);hold&=mmvrgame::InWorldCinematic(play)&&std::abs(mmvr::InversePose(cut.view).m[3][0]-mmvr::InversePose(locked.view).m[3][0]-3)<.001f;
   p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=0;f.timeSeconds+=.02;auto released=mmvrgame::TestCameraFrame(f);
   exit&=!mmvrgame::InWorldCinematic(play)&&std::abs(mmvr::InversePose(released.view).m[3][0]-p->actor.world.pos.x)<.001f;
  }
  log<<",\"scrubLifecycle\":{\"staleReferences\":"<<stale<<",\"movementFollows\":"<<follow<<",\"dialogueAnchors\":"<<hold<<",\"controlReturns\":"<<exit<<"}";
  play->actorCtx.actorLists[ACTORCAT_NPC].first=oldNpc;cam->target=oldTarget;play->csCtx=oldCs;p->csActor=nullptr;mmvrgame::ResetTestCamera();
 }
 {
  prepare();Actor actor{};actor.update=[](Actor*,PlayState*){};actor.world.pos={100,2045,0};actor.focus.pos=actor.world.pos;
  bool nearbyWitness=mmvrgame::WitnessAction(play,p,&actor)==mmvrgame::ActionWitness::Nearby;
  actor.world.pos.x=2000;actor.focus.pos=actor.world.pos;
  bool remoteWitness=mmvrgame::WitnessAction(play,p,&actor)==mmvrgame::ActionWitness::Remote;
  actor.update=nullptr;bool dead=mmvrgame::WitnessAction(play,p,&actor)==mmvrgame::ActionWitness::Unknown;
  bool notListed=!mmvrgame::LiveSceneActor(play,&actor);
  log<<",\"cutsceneWitness\":{\"nearActor\":"<<nearbyWitness<<",\"remoteActor\":"<<remoteWitness<<",\"deadIgnored\":"<<dead<<",\"staleIgnored\":"<<notListed<<"}";
 }
 {
  bool lower=true,locked=true,gaze=true,spinOff=true,headAligned=true;
  for(int spin=0;spin<2;++spin){
   prepare();p->transformation=PLAYER_FORM_DEKU;p->actionFunc=Player_Action_93;p->av1.actionVar1=1;
   p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;p->csAction=PLAYER_CSACTION_NONE;
   mmvr::GetSettings().Set(mmvr::Setting::FlowerCameraSpin,spin);mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
   mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=9970+spin;f.timeSeconds=9970+spin;
   auto first=mmvrgame::TestCameraFrame(f);auto start=mmvr::InversePose(first.view);
   for(int i=0;i<180;++i){f.timeSeconds+=1./120;f.head.position.x=float(i)/180;f.head.position.z=-float(i)/360;
    auto frame=mmvrgame::TestCameraFrame(f);auto pose=mmvr::InversePose(frame.view);
    locked&=frame.active&&std::abs(pose.m[3][0]-start.m[3][0])<.01f&&std::abs(pose.m[3][2]-start.m[3][2])<.01f;
    if(!spin)for(int a=0;a<3;++a)for(int b=0;b<3;++b)spinOff&=std::abs(pose.m[a][b]-start.m[a][b])<.001f;
    auto head=mmvrgame::FormHeadPose();headAligned&=std::abs(head.m[3][0]-pose.m[3][0])<.01f&&std::abs(head.m[3][1]-pose.m[3][1])<.01f;
    if(i==179)lower&=std::abs(start.m[3][1]-pose.m[3][1]-12)<.02f;
   }
   p->av1.actionVar1=2;f.head.orientation={0,.5f,0,.8660254f};f.timeSeconds+=1./120;
   auto held=mmvrgame::TestCameraFrame(f);gaze&=std::abs(p->yaw-(s16)MMVR_InputYaw(0))<3;
   f.originEpoch++;f.timeSeconds+=1./120;auto recentered=mmvrgame::TestCameraFrame(f);
   lower&=std::abs(mmvr::InversePose(held.view).m[3][1]-mmvr::InversePose(recentered.view).m[3][1])<.01f;
  }
  log<<",\"flowerCamera\":{\"lowering\":"<<lower<<",\"locked\":"<<locked<<",\"gaze\":"<<gaze<<",\"spinOff\":"<<spinOff<<",\"headAligned\":"<<headAligned<<"}";
  mmvr::GetSettings().Set(mmvr::Setting::FlowerCameraSpin,settings.Get(mmvr::Setting::FlowerCameraSpin));mmvrgame::ResetTestCamera();
 }
 {
  bool systemForward=true,manualPreserved=true,positionSafe=true;
  for(int system=0;system<2;++system){
   prepare();p->transformation=PLAYER_FORM_HUMAN;p->actor.draw=[](Actor*,PlayState*){};
   p->actor.scale.y=.01f;p->csAction=PLAYER_CSACTION_NONE;p->actor.shape.rot.y=p->actor.world.rot.y=0;
   mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;
   f.epoch=12000+system;f.timeSeconds=12000+system;
   auto initial=mmvr::InversePose(mmvrgame::TestCameraFrame(f).view);
   f.head.orientation={0,.38268343f,0,.92387953f};f.timeSeconds+=.02;
   mmvrgame::TestCameraFrame(f);p->actor.shape.rot.y=0x2000;p->actor.world.rot.y=0x2000;
   f.head.orientation={0,0,0,1};++f.epoch;++f.originEpoch;if(system)++f.systemRecenterEpoch;f.timeSeconds+=.02;
   auto after=mmvr::InversePose(mmvrgame::TestCameraFrame(f).view);
   float delta=std::remainder(mmvr::PoseYaw(after)-mmvr::PoseYaw(initial),6.2831853f);
   if(system)systemForward&=std::abs(delta)<.001f;else manualPreserved&=std::abs(delta-.78539816f)<.001f;
   positionSafe&=std::abs(after.m[3][0]-initial.m[3][0])<.01f&&std::abs(after.m[3][2]-initial.m[3][2])<.01f;
  }
  log<<",\"systemRecenter\":{\"forward\":"<<systemForward<<",\"manualPreserved\":"<<manualPreserved<<",\"positionSafe\":"<<positionSafe<<"}";
  mmvrgame::ResetTestCamera();
 }
 {
  prepare();p->transformation=PLAYER_FORM_HUMAN;p->actionFunc=Player_Action_Idle;
  p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actor.scale={.01f,.01f,.01f};p->csAction=PLAYER_CSACTION_NONE;
  p->actor.world.pos={0,2000,0};p->actor.floorHeight=2000;p->actor.floorBgId=BGCHECK_SCENE;
  p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;p->actor.draw=[](Actor*,PlayState*){};
  mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=12250;f.timeSeconds=12250;
  auto standingFrame=mmvrgame::TestCameraFrame(f);auto standing=mmvr::InversePose(standingFrame.view);
  p->actionFunc=Player_Action_26;p->stateFlags3|=PLAYER_STATE3_8000000;
  p->actor.world.pos.y+=15;p->actor.floorHeight+=15;f.timeSeconds+=.02;
  auto rollingFrame=mmvrgame::TestCameraFrame(f);auto rolling=mmvr::InversePose(rollingFrame.view);
  bool rollHeight=standingFrame.active&&rollingFrame.active&&std::abs((rolling.m[3][1]-standing.m[3][1])-15.f)<.1f;
  log<<",\"linkRollCamera\":{\"rollDoesNotRetainFloorEase\":"<<rollHeight<<"}";
  mmvrgame::ResetTestCamera();
 }
 log<<",\"maskReplacement\":[";
 for(int left=0;left<2;++left){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int hand=1-left;p->currentMask=PLAYER_MASK_TRUTH;
  gSaveContext.save.equippedMask=PLAYER_MASK_TRUTH;int item=ITEM_MASK_BUNNY,slot=SLOT_MASK_BUNNY;
  gSaveContext.save.saveInfo.inventory.items[slot]=item;BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;
  mmvrgame::SelectItem(play,slot,item);mmvrgame::UpdateMaskContext(play);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=9980+left;
  f.hands[hand].orientation.w=f.aims[hand].orientation.w=1;f.handTracked[hand]=f.handValid[hand]=f.aimValid[hand]=true;
  // Start beside the face, off-center and lower than the old activation box.
  f.hands[hand].position={left?-.42f:.42f,-.48f,-.18f};
  auto sample=[&](int tick,float t){f.timeSeconds=9980+left+tick*.02;f.triggers[hand]=t;mmvr::UpdateMaskTracking(f,true);mmvrgame::ProcessMasks(play);};
  sample(0,0);sample(1,0);sample(2,1);bool held=mmvr::HeldMaskItem()==item;sample(3,0);
  bool outsideRejected=p->currentMask==PLAYER_MASK_TRUTH;
  // Grab the replacement away from the worn face slot, carry it to the
  // face, then release. A new press at the face targets the worn mask.
  sample(4,1);f.hands[hand].position={left?-.28f:.28f,-.36f,-.17f};sample(5,1);
  f.hands[hand].position={left?-.14f:.14f,-.24f,-.15f};sample(6,1);
  f.hands[hand].position={0,-.12f,-.14f};sample(7,1);sample(8,0);
  bool replaced=p->currentMask==PLAYER_MASK_BUNNY;
  if(left)log<<",";log<<"{\"heldReplacement\":"<<held<<",\"outsideRejected\":"<<outsideRejected<<",\"nativeWornReplacement\":"<<replaced<<"}";
 }
 log<<"],\"formAim\":[";
 for(int form:{PLAYER_FORM_DEKU,PLAYER_FORM_ZORA})for(int turn=0;turn<4;++turn){
  prepare();p->transformation=form;mmvr::TrackingFrame f{};f.epoch=9990+turn;f.timeSeconds=9990+turn;
  f.head.orientation={0,.258819f,0,.965926f};f.origin.orientation.w=1;
  for(int h=0;h<2;++h){f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;f.aims[h].orientation={h?.258819f:-.258819f,0,0,.965926f};f.aims[h].position={h?.3f:-.3f,-.2f,-.4f};f.hands[h]=f.aims[h];}
  auto basis=mmvr::YawPose(turn*1.570796327f,0,2045,0);mmvrgame::RecordFormTracking(f,basis,mmvr::PoseMatrix(f.head));
  int start=form==PLAYER_FORM_DEKU?-1:0,end=form==PLAYER_FORM_DEKU?0:2;
  for(int h=start;h<end;++h){Vec3f pos{};Vec3s rot{};bool aimed=MMVR_FormProjectilePose(play,p,h,&pos.x,&rot.x);
   auto pose=mmvr::Multiply(mmvr::PoseMatrix(f.head),basis);
   float x=-pose.m[2][0],y=-pose.m[2][1],z=-pose.m[2][2];
   Vec3f direction{Math_SinS(rot.y)*Math_CosS(rot.x),-Math_SinS(rot.x),Math_CosS(rot.y)*Math_CosS(rot.x)};
   float dot=direction.x*x+direction.y*y+direction.z*z;bool expected=dot>.999f;
   mmvr::SetNativePause(true);bool paused=!MMVR_FormProjectilePose(play,p,h,&pos.x,&rot.x);mmvr::SetNativePause(false);
   if(form!=PLAYER_FORM_DEKU||turn||h!=start)log<<",";log<<"{\"form\":"<<form<<",\"hand\":"<<h<<",\"aimed\":"<<aimed<<",\"direction\":"<<expected<<",\"pauseBlocked\":"<<paused<<"}";
  }
 }
 log<<"]";mmvrgame::ClearFormTracking();

 log<<",\"physicalMasks\":[";
 for(int left=0;left<2;++left){
  int n=0;
  for(const auto& mask:mmvrgame::MaskModels){
   prepare();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::PhysicalMasks,1);int hand=1-left;
   gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU]=mask.item;BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=mask.item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_MASK_DEKU;
   mmvrgame::SelectItem(play,SLOT_MASK_DEKU,mask.item);mmvr::SetMaskContext(mask.item,false);
   mmvr::TrackingFrame f{};f.origin.orientation.w=f.head.orientation.w=1;f.epoch=3000+left*24+n;
   for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
   f.hands[hand].position={left?-.2f:.2f,-.13f,-.4f};double time=2000+left*30+n;
   auto sample=[&](int step,float trigger){f.timeSeconds=time+step*.02;f.triggers[hand]=trigger;mmvrgame::RecordTracking(f,view,head);bool wear=mmvr::UpdateMaskTracking(f,true);mmvrgame::ProcessItemTrigger(play);return wear;};
   sample(0,0);sample(1,0);sample(2,1);bool held=mmvr::HeldMaskItem()==mask.item;bool noPress=!(CONTROLLER1(&play->state)->press.button&BTN_CDOWN);
   auto model=mmvrgame::HeldMaskPose(f,view,head);float error=0;
   for(int k=0;k<3;++k){float bottom=model.m[3][k];for(int j=0;j<3;++j)bottom+=(&mask.bottom.x)[j]*model.m[j][k];float expected=(&f.hands[hand].position.x)[k]*40+view.m[3][k];error=std::max(error,std::abs(bottom-expected));}
   f.hands[hand].position.x=0;f.hands[hand].position.z=-.28f;sample(3,1);f.hands[hand].position.z=-.14f;sample(4,1);bool wear=sample(5,0);
   float visualHeight=mask.height*std::sqrt(SQ(model.m[1][0])+SQ(model.m[1][1])+SQ(model.m[1][2]))/40;
   bool use=mmvr::TakeMaskUse()==mask.item;bool ended=mmvr::HeldMaskItem()<0;
   if(left||n)log<<",";log<<"{\"left\":"<<left<<",\"item\":"<<mask.item<<",\"held\":"<<held<<",\"noPress\":"<<noPress<<",\"height\":"<<visualHeight<<",\"anchorError\":"<<error<<",\"wear\":"<<wear<<",\"use\":"<<use<<",\"ended\":"<<ended<<"}";++n;
  }
 }
 log<<"],\"maskRemoval\":[";
 for(int left=0;left<2;++left)for(int chosen=0;chosen<2;++chosen)for(int maskIndex=0;maskIndex<2;++maskIndex)for(int grabHand=0;grabHand<2;++grabHand){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int hand=1-left,item=maskIndex?ITEM_MASK_BUNNY:ITEM_MASK_TRUTH,slot=maskIndex?SLOT_MASK_BUNNY:SLOT_MASK_TRUTH;
  gSaveContext.save.saveInfo.inventory.items[slot]=item;BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;
  mmvrgame::SelectItem(play,slot,item);mmvrgame::UpdateMaskContext(play);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=4000+left*4+chosen*2+maskIndex;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
  int step=0;auto sample=[&](float z,float trigger){f.timeSeconds=3000+f.epoch+step++*.02;f.hands[hand].position={0,-.17f,z};f.triggers[hand]=trigger;mmvr::UpdateMaskTracking(f,true);mmvrgame::RecordTracking(f,view,head);mmvrgame::ProcessMasks(play);mmvrgame::ProcessItemTrigger(play);};
  sample(-.5f,0);sample(-.5f,0);sample(-.5f,1);sample(-.3f,1);sample(-.12f,1);sample(-.12f,0);
  bool worn=Player_GetCurMaskItemId(play)==item&&mmvr::WornMaskItem()==item;
  mmvrgame::ClearItemSelection();gSaveContext.save.saveInfo.inventory.items[SLOT_BOMB]=ITEM_BOMB;AMMO(ITEM_BOMB)=10;
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOMB;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOMB;
  if(chosen)mmvrgame::SelectItem(play,SLOT_BOMB,ITEM_BOMB);mmvrgame::UpdateMaskContext(play);
  hand=grabHand;f.triggers[0]=f.triggers[1]=0;
  if(!chosen){sample(-.5f,0);sample(-.5f,0);sample(-.5f,1);sample(-.3f,1);sample(-.12f,1);}
  else{sample(-.12f,0);sample(-.12f,0);}
  bool missed=mmvr::HeldMaskItem()<0;
  sample(-.12f,0);sample(-.12f,1);bool grabbed=missed&&mmvr::HeldMaskItem()==item&&mmvr::HeldMaskController()==hand;
  sample(-.3f,1);sample(-.5f,1);sample(-.5f,0);
  bool removed=Player_GetCurMaskItemId(play)==ITEM_NONE&&mmvr::WornMaskItem()<0;
  bool untouched=BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)==ITEM_BOMB&&C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)==SLOT_BOMB&&AMMO(ITEM_BOMB)==10&&!p->heldActor;
  if(left||chosen||maskIndex||grabHand)log<<",";log<<"{\"left\":"<<left<<",\"selectedBomb\":"<<chosen<<",\"item\":"<<item<<",\"worn\":"<<worn<<",\"grabbed\":"<<grabbed<<",\"removed\":"<<removed<<",\"untouched\":"<<untouched<<"}";
 }
 log<<"],\"shoulderHolster\":[";
 for(int left=0;left<2;++left)for(int mode=0;mode<3;++mode){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::ShoulderHolster,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int hand=1-left;
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_B)=ITEM_SWORD_GILDED;
  if(mode==1){p->currentMask=PLAYER_MASK_TRUTH;mmvrgame::SelectItem(play,SLOT_MASK_TRUTH,ITEM_MASK_TRUTH);}
  if(mode==2)mmvrgame::SelectItem(play,SLOT_BOMB,ITEM_BOMB);
  mmvr::TrackingFrame f{};f.origin.orientation.w=f.head.orientation.w=1;f.epoch=3300+left*3+mode;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
  f.hands[hand].position={left?-.25f:.25f,-.2f,.2f};double time=2400+left*3+mode;
  auto sample=[&](int step,float trigger){f.timeSeconds=time+step*.02;f.triggers[hand]=trigger;mmvrgame::RecordTracking(f,view,head);mmvrgame::ProcessHolster(play);};
  sample(0,0);sample(1,0);sample(2,1);bool before=Player_GetMeleeWeaponHeld(p)==0;
  for(int i=1;i<=5;++i){f.hands[hand].position.z=.2f-i*.065f;sample(2+i,1);}
  bool drew=Player_GetMeleeWeaponHeld(p)==PLAYER_MELEEWEAPON_SWORD_GILDED;
  sample(8,0);for(int i=1;i<=4;++i){f.hands[hand].position.z=-.125f+i*.08f;sample(8+i,0);}sample(13,1);
  bool stowed=p->heldItemAction==PLAYER_IA_NONE;bool maskPreserved=mode!=1||p->currentMask==PLAYER_MASK_TRUTH;
  if(left||mode)log<<",";log<<"{\"left\":"<<left<<",\"mode\":"<<mode<<",\"before\":"<<before<<",\"drew\":"<<drew<<",\"stowed\":"<<stowed<<",\"maskPreserved\":"<<maskPreserved<<"}";
 }
 log<<"]";
 {
  auto oldPause=play->pauseCtx;auto oldInterface=play->interfaceCtx;
  bool pageLeft=true,pageRight=true;
  for(int page=0;page<4;++page){
   for(int direction=0;direction<2;++direction){
    play->pauseCtx={};play->pauseCtx.pageIndex=page;play->pauseCtx.state=PAUSE_STATE_MAIN;
    mmvr::PauseTriggers trigger;trigger.Update(0,0,true);Input testInput{};
    testInput.cur.button=testInput.press.button=trigger.Update(direction==0?1.f:0.f,direction==1?1.f:0.f,true);
    KaleidoScope_HandlePageToggles(play,&testInput);
    const bool passed=play->pauseCtx.mainState==PAUSE_MAIN_STATE_SWITCHING_PAGE&&play->pauseCtx.nextPageMode==page*2+(direction==0?1:0);
    (direction==0?pageLeft:pageRight)&=passed;
   }
  }
  play->pauseCtx=oldPause;play->interfaceCtx=oldInterface;
  log<<",\"nativeTriggerPages\":{\"left\":"<<pageLeft<<",\"right\":"<<pageRight<<"}";
 }
 mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvr::SetMaskContext(-1,false);*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;
}
