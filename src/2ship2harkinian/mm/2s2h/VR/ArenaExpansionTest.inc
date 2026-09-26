#include "Camera.h"
#include "NativeForms.h"
#include "ScenePresentation.h"
extern "C" { void Player_Action_89(Player*,PlayState*); void Player_Action_90(Player*,PlayState*); }
// Read the last real native draw in the upper/lower arena for each handedness.
// Never synthesize a draw here: missing production draw eligibility must fail.
static bool NativeArenaGiantView(PlayState* play,bool giant,std::ostream& log){
 auto* p=GET_PLAYER(play);if(!p){log<<"giant-view missing-player=1\n"<<std::flush;return false;}
 const auto settings=mmvr::GetSettings();const int viewMode=int(std::lround(settings.Get(mmvr::Setting::ViewMode)));
 const bool selectedBefore=mmvr::FirstPersonSelected(),requestedBefore=mmvr::FirstPersonRequested();
 // This isolated flat harness has no headset session. Select its explicit camera
 // mode as well as synthetic tracking; eligibility still requires a real draw.
 mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);mmvr::SetFirstPersonEligibility(true);
 const bool maskOk=((p->currentMask==PLAYER_MASK_GIANT)==giant);
 const bool transition=mmvrgame::GiantTransformationActive(p);bool ok=maskOk&&!transition;
 for(int hand=0;hand<2;++hand){
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,1-hand);mmvr::GetSettings().Set(mmvr::Setting::VrCameraCutscenes,0);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=97000+int(giant)*2+hand;f.timeSeconds=f.epoch;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].position={h?.25f:-.25f,-.5f,-.2f};}
  auto frame=mmvrgame::TestCameraFrame(f);const auto eye=mmvr::InversePose(frame.view);
  const bool body=MMVR_FirstPersonBody(),form=mmvrgame::FirstPersonFormAllowed(p);
  const float expectedHeight=mmvrgame::FormEyeHeight(p),actualHeight=eye.m[3][1]-p->actor.world.pos.y;
  const bool heightOk=std::isfinite(actualHeight)&&std::abs(actualHeight-expectedHeight)<.1f;
  const bool scaleOk=std::abs(p->actor.scale.x-.01f)<.0001f&&std::abs(p->actor.scale.y-.01f)<.0001f&&std::abs(p->actor.scale.z-.01f)<.0001f;
  const auto facts=mmvrgame::SceneFacts(play);
  ok&=frame.active&&body&&form&&heightOk&&scaleOk;
  log<<"giant-view wanted="<<giant<<" hand="<<hand<<" frame="<<play->gameplayFrames<<" scene="<<play->sceneId
     <<" mask="<<int(p->currentMask)<<" equippedMask="<<int(gSaveContext.save.equippedMask)<<" form="<<int(p->transformation)
     <<" maskOk="<<maskOk<<" transient="<<transition<<" action89="<<(p->actionFunc==Player_Action_89)<<" action90="<<(p->actionFunc==Player_Action_90)
     <<" flag100="<<bool(p->stateFlags1&PLAYER_STATE1_100)<<" flags1="<<p->stateFlags1<<" csAction="<<int(p->csAction)<<" csState="<<int(play->csCtx.state)<<" csId="<<CutsceneManager_GetCurrentCsId()
     <<" priorMode="<<viewMode<<" priorSelected="<<selectedBefore<<" priorRequested="<<requestedBefore<<" selected="<<mmvr::FirstPersonSelected()<<" requested="<<mmvr::FirstPersonRequested()
     <<" active="<<frame.active<<" body="<<body<<" formAllowed="<<form<<" bodyAnchor="<<(mmvr::BodyAnchor()!=nullptr)<<" sceneView="<<int(mmvrgame::SceneView(play))
     <<" eyeY="<<eye.m[3][1]<<" rootY="<<p->actor.world.pos.y<<" focusY="<<p->actor.focus.pos.y<<" height="<<actualHeight<<" expectedHeight="<<expectedHeight<<" heightOk="<<heightOk
     <<" scaleX="<<p->actor.scale.x<<" scaleY="<<p->actor.scale.y<<" scaleZ="<<p->actor.scale.z<<" scaleOk="<<scaleOk
     <<" alive="<<facts.alive<<" player="<<facts.playerPresent<<" transition="<<facts.transition<<" cinematic="<<facts.cinematic<<" locked="<<facts.playerLocked<<" remote="<<facts.remote<<" title="<<facts.titleSequence
     <<" health="<<gSaveContext.save.saveInfo.playerData.health<<" magic="<<int(gSaveContext.save.saveInfo.playerData.magic)<<" magicState="<<gSaveContext.magicState;
  for(int slot=EQUIP_SLOT_C_LEFT;slot<=EQUIP_SLOT_C_RIGHT;++slot)log<<" c"<<slot<<"Item="<<int(BUTTON_ITEM_EQUIP(0,slot))<<" c"<<slot<<"Slot="<<int(C_SLOT_EQUIP(0,slot));
  log<<"\n"<<std::flush;
 }
 mmvr::GetSettings()=settings;mmvr::ApplyViewMode(viewMode);mmvrgame::ClearTracking();mmvr::SetNativeTestTracking(false);return ok;
}
// Drives production portal and reflection paths in the isolated native harness only.
extern "C" int MMVR_DebugExpansionScenario(PlayState* play){
 static std::ofstream result("native-arena-expansion.log");static int stage=0,age=0,index=0,total=0;static Save before{};static bool success=true,giantSeen=false;
 auto check=[&](const std::string& name,bool ok){result<<name<<"="<<ok<<"\n"<<std::flush;success&=ok;};
 if(total==0){const char* token=std::getenv("MMVR_SESSION_TOKEN");result<<"session "<<(token?token:"")<<"\n"<<std::flush;}
 auto* p=GET_PLAYER(play);++total;++age;
 auto count=[&](int id){int n=0;for(int c=0;c<ACTORCAT_MAX;++c)for(auto* a=play->actorCtx.actorLists[c].first;a;a=a->next)n+=a->id==id&&a->update;return n;};
 auto next=[&](int value){stage=value;age=0;};
 if(stage==0&&MMVR_DebugRoomActive(play)&&age>30){
  check("annex-collision-bounds",header.maxBounds.x==12000);check("reflection-fixtures",count(ACTOR_MIR_RAY2)==1&&count(ACTOR_MIR_RAY3)==1&&count(ACTOR_OBJ_LIGHTSWITCH)==2);
  p->actor.world.pos={1450,0,-800};p->actor.prevPos=p->actor.world.pos;p->actor.velocity={};SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD,EQUIP_VALUE_SHIELD_MIRROR);Player_SetEquipmentData(play,p);next(1);
 }
 if(stage==1){
  mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);mmvr::GetSettings().Set(mmvr::Setting::PhysicalShield,1);
  mmvr::TrackingFrame f{};f.epoch=78000;f.timeSeconds=total*.05;f.head.orientation.w=f.origin.orientation.w=1;f.head.position.y=1.125f;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  f.grips[0]=1;auto view=mmvr::YawPose(0,1450,0,-800),head=mmvr::YawPose(0,0,1.125f,0),shield=mmvr::YawPose(0,1450,65,-800);
  for(int i=0;i<3;++i)for(int k=0;k<3;++k)shield.m[i][k]*=.01f;
  mmvrgame::RecordTracking(f,view,head);mmvrgame::UpdateShield(f,shield);
  if(age>=65){check("native-light-reflection-switch",Flags_GetSwitch(play,47));mmvr::RequestNativeCapture("native-arena-reflection");mmvrgame::ClearTracking();mmvr::SetNativeTestTracking(false);next(2);}
 }
 if(stage==2&&age>5&&MMVR_DebugRoomActive(play)&&play->transitionTrigger==TRANS_TRIGGER_OFF){
  before=gSaveContext.save;check(std::string(trials[index].name)+"-portal",MMVR_DebugTrialBegin(play,index));next(3);
 }
 if(stage==3&&play->sceneId==trials[index].scene&&play->transitionTrigger==TRANS_TRIGGER_OFF&&age>80){
  check(std::string(trials[index].name)+"-live",count(trials[index].actor)>0);
  check(std::string(trials[index].name)+"-intro-skipped",p->csAction==PLAYER_CSACTION_NONE&&play->csCtx.state==CS_STATE_IDLE);
  Save copy=gSaveContext.save;MMVR_DebugNormalizeSave(&copy);check(std::string(trials[index].name)+"-save-isolated",std::memcmp(copy.saveInfo.weekEventReg,before.saveInfo.weekEventReg,sizeof(copy.saveInfo.weekEventReg))==0);
  mmvr::RequestNativeCapture((std::string("native-arena-boss-")+std::to_string(index)).c_str());
  if(index==5){
   // Native Human mask processing removes worn masks absent from C/D-pad slots.
   // Use a real assignment; the trial snapshot restores it when returning.
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_MASK_GIANT;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_MASK_GIANT;
   check("giant-native-mask-assigned",BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)==ITEM_MASK_GIANT&&C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)==SLOT_MASK_GIANT);
   Player_UseItem(play,p,ITEM_MASK_GIANT);next(5);
  }else next(4);
 }
 if(stage==5){
  giantSeen|=p->currentMask==PLAYER_MASK_GIANT&&p->actor.world.pos.y>3000;
  if(age>200){check("giant-native-world-scale",giantSeen);check("giant-settled-first-person-both-hands",NativeArenaGiantView(play,true,result));check("giant-practice-flag",MMVR_DebugGiantPractice(play));mmvr::RequestNativeCapture("native-arena-giant");Player_UseItem(play,p,ITEM_MASK_GIANT);next(6);}
 }
 if(stage==6&&age>160){check("giant-scale-restored",p->currentMask!=PLAYER_MASK_GIANT&&p->actor.world.pos.y<1000);check("giant-removal-first-person-restored",NativeArenaGiantView(play,false,result));next(4);}
 if(stage==4&&age>5){check(std::string(trials[index].name)+"-return-request",MMVR_DebugTrialReturn(play));next(7);}
 if(stage==7&&MMVR_DebugRoomActive(play)&&play->transitionTrigger==TRANS_TRIGGER_OFF&&age>60){
  check(std::string(trials[index].name)+"-progress-restored",!trialSnapshot&&std::memcmp(gSaveContext.save.saveInfo.weekEventReg,before.saveInfo.weekEventReg,sizeof(before.saveInfo.weekEventReg))==0);
  if(index==5)check("giant-mask-assignment-restored",std::memcmp(&gSaveContext.save.saveInfo.equips,&before.saveInfo.equips,sizeof(before.saveInfo.equips))==0);
  if(++index==ARRAY_COUNT(trials)){check("all-trials-complete",true);result<<"PASS="<<success<<"\n"<<std::flush;return 1;}next(2);
 }
 if(total>2400||age>450){result<<"timeout stage="<<stage<<" trial="<<index<<" scene="<<play->sceneId<<" cs="<<int(p->csAction)<<"\nPASS=0\n"<<std::flush;return 1;}
 return 0;
}
