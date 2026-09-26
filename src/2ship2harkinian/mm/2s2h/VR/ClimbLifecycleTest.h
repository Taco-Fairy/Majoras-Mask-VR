#pragma once
#include "NativeClimbing.h"
#include "PlayerBody.h"
#include <limits>
extern "C" {
void Player_Action_50(Player*,PlayState*);
void Player_Action_51(Player*,PlayState*);
void Player_Action_49(Player*,PlayState*);
void func_80839E74(Player*,PlayState*);
}

// Only fixture placement is synthetic. Action changes use the ordinary native
// idle setup; root flags, joints and queued actor movement are never injected.
static void NativeClimbPlace(PlayState* play,float x,float y,float z){
 auto* p=GET_PLAYER(play);mmvrgame::ClearClimbing();mmvrgame::StowItem(play);func_80839E74(p,play);
 p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->heldActor=nullptr;
 // Player_UpdateCommon reloads prevPos from home.pos before collision. A
 // deliberate fixture teleport must initialize that native history too.
 mmvrgame::MovePlayerBody(play,p,{x,y,z});
 p->actor.world.pos=p->actor.prevPos=p->actor.home.pos={x,y,z};p->actor.velocity={};p->actor.speed=p->speedXZ=0;
 p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=(s16)0x8000;p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;
 p->actor.floorHeight=0;p->actor.wallPoly=nullptr;mmvrgame::ResetTestCamera();
}

static mmvr::TrackingFrame NativeClimbFrame(int epoch){
 mmvr::TrackingFrame frame{};frame.head.orientation.w=frame.origin.orientation.w=1;frame.epoch=epoch;
 for(int hand=0;hand<2;++hand){
  frame.hands[hand].orientation.w=frame.aims[hand].orientation.w=1;
  frame.handValid[hand]=frame.handTracked[hand]=frame.aimValid[hand]=true;
  frame.hands[hand].position={hand?.12f:-.12f,-.1f,-.8f};
 }
 return frame;
}

static mmvr::Pad NativeClimbRecovery(PlayState* play,int tick,std::ofstream& trace){
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
 static mmvr::TrackingFrame frame;
 static const char* names[]={"gap","backward","nonfinite","recenter","pause","disabled"};
 struct Recovery {
  bool attached=false,cleared=false,heldDisarmed=true,rootStable=true,directStable=true;
  bool firstDelta=true,nativeMoved=false,rearmed=false,dropped=false,finite=true;
  int ownedTicks=0,nativeTicks=0;float pullStart=0,partialRise=0,nativeStart=0,nativeRise=0,rearmStart=0,rearmRise=0;
  float afterSamples=0,frozenFrame=0,boundaryY=0,observedDelta=0,expectedDelta=0,maxDirectDrift=0;
  Vec3s frozenRoot{},boundaryRoot{};float boundaryScale=0;unsigned boundaryFlags=0;double timeOffset=0;
 };
 static Recovery state;
 if(tick>=800&&tick<1160){
  int index=(tick-800)/60,age=(tick-800)%60;const int activeHand=index%2;
  if(age==0){
   state={};mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,1);
   CVarSetFloat("gVR.PhysicalClimbing",1);CVarSetFloat("gVR.StickClimbing",1);
   NativeClimbPlace(play,700,30,-615);frame=NativeClimbFrame(91000+index);
  }
  // This observes the result of the previous real Player_Update and its normal
  // animation queue, before this tick's synthetic controller samples are sent.
  if(age>=6&&age<=11){
   float drift=std::abs(p->actor.world.pos.y-state.afterSamples);state.maxDirectDrift=std::max(state.maxDirectDrift,drift);
   state.directStable&=MMVR_DirectClimbMode(play,p)&&p->actionFunc==Player_Action_50&&drift<.05f;
   state.rootStable&=p->skelAnime.curFrame==state.frozenFrame&&p->skelAnime.prevTransl.x==state.frozenRoot.x&&
       p->skelAnime.prevTransl.y==state.frozenRoot.y&&p->skelAnime.prevTransl.z==state.frozenRoot.z;
   ++state.ownedTicks;
  }
  if(age==5){state.frozenRoot=p->skelAnime.prevTransl;state.frozenFrame=p->skelAnime.curFrame;}
  if(age==12){
   state.partialRise=p->actor.world.pos.y-state.pullStart;state.boundaryY=p->actor.world.pos.y;
   state.boundaryRoot=p->skelAnime.prevTransl;state.boundaryFlags=p->skelAnime.movementFlags;
   state.boundaryScale=p->actor.scale.y*((state.boundaryFlags&ANIM_FLAG_4)?1.f:p->ageProperties->unk_08);
  }
  if(age==13){
   state.observedDelta=p->actor.world.pos.y-state.boundaryY;
   state.expectedDelta=(state.boundaryFlags&ANIM_FLAG_NOMOVE)?0.f:
       (p->skelAnime.prevTransl.y-state.boundaryRoot.y)*state.boundaryScale;
   state.firstDelta=p->actionFunc==Player_Action_50&&std::abs(state.observedDelta-state.expectedDelta)<.1f&&std::abs(state.observedDelta)<12.f;
  }
  if(age==17)state.nativeStart=p->actor.world.pos.y;
  if(age>=17&&age<26){pad.y=70;++state.nativeTicks;}
  if(age==26){
   state.nativeRise=p->actor.world.pos.y-state.nativeStart;
   state.nativeMoved=p->actionFunc==Player_Action_50&&!MMVR_DirectClimbMode(play,p)&&state.nativeRise>3.f;
   mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);CVarSetFloat("gVR.PhysicalClimbing",1);
   for(auto& hand:frame.hands){hand.position.y=-.1f;hand.position.z=(-650-p->actor.world.pos.z)/40.f;}
  }
  if(age==28)state.rearmStart=p->actor.world.pos.y;
  if(age==35){state.rearmRise=p->actor.world.pos.y-state.rearmStart;state.rearmed=MMVR_DirectClimbMode(play,p)&&state.rearmRise>2.f;}
  if(age<40)for(int sub=0;sub<4;++sub){
   frame.timeSeconds=93000+index*10+age/30.+sub/120.+state.timeOffset;
   frame.triggers[activeHand]=((age>=3&&age<26)||(age>=28&&age<35))?1:0;
   if((age>=4&&age<12)||(age>=29&&age<35))frame.hands[activeHand].position.y-=.005f;
   if(age==12&&sub==0){
    if(index==0){state.timeOffset=.3;frame.timeSeconds+=.3;}
    if(index==1)frame.timeSeconds-=.1;
    if(index==2)frame.timeSeconds=std::numeric_limits<double>::quiet_NaN();
    if(index==3)++frame.originEpoch;
    if(index==5){mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,0);CVarSetFloat("gVR.PhysicalClimbing",0);}
   }
   auto pause=play->pauseCtx.state;
   if(age==12&&sub==0&&index==4)play->pauseCtx.state=PAUSE_STATE_MAIN;
   if(age==12&&sub==0&&index==2){
    // Deliberately corrupt only the climb timestamp. Camera height has its own
    // time consumer, outside this lifecycle regression's ownership boundary.
    mmvrgame::UpdateClimbing(frame,mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+mmvrgame::FormEyeHeight(p),p->actor.world.pos.z),mmvr::YawPose(0));
   }else mmvrgame::TestCameraFrame(frame);
   play->pauseCtx.state=pause;
   if(age==3&&sub==3){state.attached=MMVR_DirectClimbMode(play,p)&&p->actionFunc==Player_Action_50;state.pullStart=p->actor.world.pos.y;}
   if(age==12&&sub==0)state.cleared=!MMVR_DirectClimbMode(play,p)&&p->actionFunc==Player_Action_50&&std::abs(p->actor.world.pos.y-state.boundaryY)<.001f;
   if(age>=12&&age<26)state.heldDisarmed&=!MMVR_DirectClimbMode(play,p);
   if(age==35&&sub==3)state.dropped=!MMVR_DirectClimbMode(play,p)&&p->actionFunc!=Player_Action_50;
   state.finite&=std::isfinite(p->actor.world.pos.y);
  }
  state.afterSamples=p->actor.world.pos.y;
  if(age==40){
   bool ok=state.attached&&state.partialRise>4.f&&state.cleared&&state.heldDisarmed&&state.rootStable&&state.directStable&&
       state.firstDelta&&state.nativeMoved&&state.rearmed&&state.dropped&&state.finite&&state.ownedTicks==6&&state.nativeTicks==9;
   std::ofstream("native-room-scenario.log",std::ios::app)<<"live-climb-"<<names[index]<<"-recovery="<<ok<<"\n";
   trace<<"recovery {\"name\":\""<<names[index]<<"\",\"hand\":"<<activeHand<<",\"ok\":"<<ok<<",\"attached\":"<<state.attached
       <<",\"partialRise\":"<<state.partialRise<<",\"cleared\":"<<state.cleared<<",\"heldDisarmed\":"<<state.heldDisarmed
       <<",\"rootStable\":"<<state.rootStable<<",\"directStable\":"<<state.directStable<<",\"maxDirectDrift\":"<<state.maxDirectDrift
       <<",\"firstDelta\":"<<state.firstDelta<<",\"observedDelta\":"<<state.observedDelta<<",\"expectedDelta\":"<<state.expectedDelta
       <<",\"nativeRise\":"<<state.nativeRise<<",\"rearmRise\":"<<state.rearmRise<<",\"dropped\":"<<state.dropped
       <<",\"ownedTicks\":"<<state.ownedTicks<<",\"nativeTicks\":"<<state.nativeTicks<<"}\n"<<std::flush;
  }
  return pad;
 }
 struct Ladder {bool mounted=false,sawTransition=false,finished=false;float startY=0,minY=10000,maxY=-10000,rootTravel=0,afterY=0;bool previousTransition=false;int rootTicks=0;};
 static Ladder ladder;
 if(tick>=1160&&tick<1520){
  bool vine=tick>=1400;bool top=tick<1280||vine;int age=tick-(vine?1400:top?1160:1280);const int activeHand=top?0:1;
  if(age==0){
   ladder={};mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,1);
   CVarSetFloat("gVR.PhysicalClimbing",1);CVarSetFloat("gVR.StickClimbing",1);
   NativeClimbPlace(play,vine?700:1120,top?126:28,-615);frame=NativeClimbFrame(vine?92002:top?92000:92001);
  }
  if(ladder.previousTransition){ladder.rootTravel+=std::abs(p->actor.world.pos.y-ladder.afterY);++ladder.rootTicks;}
  ladder.sawTransition|=p->actionFunc==(vine?Player_Action_49:Player_Action_51);
  ladder.minY=std::min(ladder.minY,p->actor.world.pos.y);ladder.maxY=std::max(ladder.maxY,p->actor.world.pos.y);
  if(age<100)for(int sub=0;sub<4;++sub){
   frame.timeSeconds=(top?94000:95000)+age/30.+sub/120.;frame.triggers[activeHand]=age>=3?1:0;
   if(age>=4&&p->actionFunc==Player_Action_50)frame.hands[activeHand].position.y+=top?-.006f:.015f;
   mmvrgame::TestCameraFrame(frame);
   if(age==3&&sub==3){ladder.mounted=p->actionFunc==Player_Action_50&&(vine?p->av1.actionVar1!=0:p->av1.actionVar1==0)&&MMVR_DirectClimbMode(play,p);ladder.startY=p->actor.world.pos.y;}
  }
  ladder.previousTransition=p->actionFunc==(vine?Player_Action_49:Player_Action_51);ladder.afterY=p->actor.world.pos.y;
  if(age==110){
   ladder.finished=p->actionFunc!=Player_Action_50&&p->actionFunc!=Player_Action_51&&p->actionFunc!=Player_Action_49&&(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND);
   bool reached=top?(p->actor.world.pos.y>165&&p->actor.world.pos.z<-645):(std::abs(p->actor.world.pos.y)<8);
   bool ok=ladder.mounted&&ladder.sawTransition&&ladder.finished&&reached&&(vine||ladder.rootTravel>5)&&ladder.rootTicks>=4;
   std::ofstream("native-room-scenario.log",std::ios::app)<<"live-ladder-"<<(vine?"vine-top":top?"top":"bottom")<<"="<<ok<<"\n";
   trace<<"ladder {\"name\":\""<<(vine?"vine-top":top?"top":"bottom")<<"\",\"hand\":"<<activeHand<<",\"ok\":"<<ok<<",\"mounted\":"<<ladder.mounted
       <<",\"transition\":"<<ladder.sawTransition<<",\"finished\":"<<ladder.finished<<",\"startY\":"<<ladder.startY
       <<",\"endY\":"<<p->actor.world.pos.y<<",\"endZ\":"<<p->actor.world.pos.z<<",\"rootTravel\":"<<ladder.rootTravel
       <<",\"rootTicks\":"<<ladder.rootTicks<<"}\n"<<std::flush;
  }
 }
 return pad;
}

// Controller samples run before actual native Player_Update/animation/bg checks.
// Unlike the isolated helper fixture, no Action_50 calls replace the game loop.
static mmvr::Pad NativeClimbLifecycle(PlayState* play,int tick){
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
 static float savedPhysical=0,savedStick=1;static mmvr::Settings saved;static mmvr::TrackingFrame f;static float peak=0;static bool attached=false;
 static std::ofstream trace("native-climb-lifecycle.log");
 auto prepare=[&](){
  NativeClimbPlace(play,700,0,-615);
  f={};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=88000+tick;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].position={h?.12f:-.12f,-.1f,-.8f};}
  peak=0;attached=false;
 };
 if(tick==690){const char* session=std::getenv("MMVR_SESSION_TOKEN");trace<<"session "<<(session?session:"")<<"\n"<<std::flush;savedPhysical=CVarGetFloat("gVR.PhysicalClimbing",0);savedStick=CVarGetFloat("gVR.StickClimbing",1);CVarSetFloat("gVR.PhysicalClimbing",1);CVarSetFloat("gVR.StickClimbing",0);saved=mmvr::GetSettings();mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,0);prepare();}
 if(tick>=690&&tick<=715){
  mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,0);
  for(int sub=0;sub<4;++sub){f.timeSeconds=88000+(tick-690)*4./120+sub/120.;f.triggers[0]=(tick>=693&&tick<711)?1:0;
   if(tick>=695&&tick<709)f.hands[0].position.y-=.005f;
   auto camera=mmvrgame::TestCameraFrame(f);
   if(sub==0){auto pose=mmvr::InversePose(camera.view);trace<<"camera tick="<<tick<<" active="<<camera.active<<" physical="<<MMVR_PhysicalClimbEnabled(play,p)<<" form="<<int(p->transformation)<<" pose="<<pose.m[3][0]<<","<<pose.m[3][1]<<","<<pose.m[3][2]<<" yaw="<<mmvr::PoseYaw(pose)<<" trigger="<<f.triggers[0]<<"\n"<<std::flush;}
  }
  attached|=MMVR_DirectClimbMode(play,p)!=0;peak=std::max(peak,p->actor.world.pos.y);
  if(tick==715)std::ofstream("native-room-scenario.log",std::ios::app)<<"live-trigger-climb="<<(attached&&peak>8&&p->actionFunc!=Player_Action_50)<<"\n";
 }
 if(tick==720){CVarSetFloat("gVR.StickClimbing",1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,1);prepare();}
 if(tick>=720&&tick<795){
  mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,1);
  f.timeSeconds=89000+(tick-720)/30.;mmvrgame::TestCameraFrame(f);pad.y=70;
  attached|=p->actionFunc==Player_Action_50;peak=std::max(peak,p->actor.world.pos.y);
 }
 if(tick==795)std::ofstream("native-room-scenario.log",std::ios::app)<<"live-stick-climb="<<(attached&&peak>20)<<"\n";
 if(tick>=800&&tick<1520)pad=NativeClimbRecovery(play,tick,trace);
 if(tick==1520){
  NativeClimbPlace(play,700,0,-615);CVarSetFloat("gVR.PhysicalClimbing",savedPhysical);CVarSetFloat("gVR.StickClimbing",savedStick);
  mmvr::GetSettings()=saved;mmvrgame::ClearClimbing();mmvrgame::ResetTestCamera();mmvr::SetNativeTestTracking(false);
 }
 if(tick%2==0)trace<<tick<<" y="<<p->actor.world.pos.y<<" z="<<p->actor.world.pos.z<<" action="<<(void*)p->actionFunc<<" wall="<<(void*)p->actor.wallPoly<<" flags="<<p->stateFlags1<<" bg="<<p->actor.bgCheckFlags<<" attached="<<attached<<" peak="<<peak<<"\n"<<std::flush;
 if(tick==1520)trace<<"COMPLETE\n"<<std::flush;
 return pad;
}
