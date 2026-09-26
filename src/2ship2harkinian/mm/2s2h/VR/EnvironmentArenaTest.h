#pragma once
#include "Camera.h"
#include <fstream>
#include "NativeForms.h"
extern "C" {void Play_UpdateWaterCamera(PlayState*,Camera*);void Player_Action_Idle(Player*,PlayState*);}
struct EnvironmentArenaResult {bool head=false,exit=false,body=false,fallback=false,theater=false;};
static EnvironmentArenaResult NativeEnvironmentArenaTest(PlayState* play){
 EnvironmentArenaResult out;auto* p=GET_PLAYER(play);auto player=*p;auto* camera=GET_ACTIVE_CAM(play);auto savedCamera=*camera;auto env=play->envCtx;auto cs=play->csCtx;auto flags=play->actorCtx.flags;auto settings=mmvr::GetSettings();
 // Preserve the form from the last real player draw. Camera sampling rejects
 // an unsynchronized form switch, which this fixture cannot complete here.
 p->currentMask=PLAYER_MASK_NONE;p->csAction=PLAYER_CSACTION_NONE;p->getItemDrawIdPlusOne=0;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->actor.world.pos={650,-100,300};p->actor.prevPos=p->actor.world.pos;p->actor.draw=[](Actor*,PlayState*){};p->actor.scale.y=.01f;
 play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;camera->eye={0,200,300};camera->stateFlags&=~CAM_STATE_UNDERWATER;
 mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvrgame::ResetTestCamera();
 mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=27000;f.timeSeconds=27000;
 auto frame=mmvrgame::TestCameraFrame(f);float eye[3]{};bool sampled=frame.active&&MMVR_EnvironmentEye(play,eye);out.head=sampled&&eye[1]<-15;
 Play_UpdateWaterCamera(play,camera);out.head&=(camera->stateFlags&CAM_STATE_UNDERWATER)&&camera->eye.y==200;
 // Cross the actual surface with an 8-unit margin, independent of calibrated form height.
 if(sampled)f.head.position.y=(-7.f-eye[1])/40.f;
 f.timeSeconds+=.02;mmvrgame::TestCameraFrame(f);Play_UpdateWaterCamera(play,camera);out.exit=sampled&&!(camera->stateFlags&CAM_STATE_UNDERWATER)&&camera->eye.y==200;
 p->actor.world.pos.y-=40;out.body=MMVR_EnvironmentEye(play,eye)&&eye[1]<-15;Play_UpdateWaterCamera(play,camera);out.body&=(camera->stateFlags&CAM_STATE_UNDERWATER)!=0;
 play->actorCtx.flags|=ACTORCTX_FLAG_TELESCOPE_ON;out.theater=!MMVR_EnvironmentEye(play,eye);Play_UpdateWaterCamera(play,camera);out.theater&=!(camera->stateFlags&CAM_STATE_UNDERWATER);play->actorCtx.flags=flags;
 mmvrgame::ResetTestCamera();camera->eye={650,-40,300};Play_UpdateWaterCamera(play,camera);out.fallback=(camera->stateFlags&CAM_STATE_UNDERWATER)!=0;
 camera->eye={0,200,300};Play_UpdateWaterCamera(play,camera);out.fallback&=!(camera->stateFlags&CAM_STATE_UNDERWATER);
 *p=player;*camera=savedCamera;play->envCtx=env;play->csCtx=cs;play->actorCtx.flags=flags;mmvr::GetSettings()=settings;mmvrgame::ResetTestCamera();return out;
}

struct RoomScaleArenaResult {bool free=false,target=false,carry=false,both=false,pause=false,climb=false,wall=false,edge=false;};
static RoomScaleArenaResult NativeRoomScaleArenaTest(PlayState* play){
 RoomScaleArenaResult out;auto* p=GET_PLAYER(play);auto saved=*p;auto settings=mmvr::GetSettings();auto pause=play->pauseCtx.state;
 Actor* object=nullptr;for(Actor* a=play->actorCtx.actorLists[ACTORCAT_PROP].first;a;a=a->next)if(a->id==ACTOR_OBJ_TSUBO&&a->update){object=a;break;}
 if(!object)return out;
 mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
 for(int mode=0;mode<8;++mode){
  *p=saved;p->transformation=PLAYER_FORM_HUMAN;p->currentMask=PLAYER_MASK_NONE;p->csAction=PLAYER_CSACTION_NONE;p->getItemDrawIdPlusOne=0;
  p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;
  p->actor.world.pos={0,0,0};if(mode==6)p->actor.world.pos.x=-980;if(mode==7)p->actor.world.pos={435,0,300};
  p->actor.prevPos=p->actor.world.pos;p->actor.shape.rot.y=p->actor.world.rot.y=0;p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;p->cylinder.dim.radius=12;
  Collider_UpdateCylinder(&p->actor,&p->cylinder);p->focusActor=(mode==1||mode==3)?object:nullptr;p->heldActor=(mode==2||mode==3)?object:nullptr;
  if(p->heldActor)p->stateFlags1|=PLAYER_STATE1_CARRYING_ACTOR;
  play->pauseCtx.state=mode==4?PAUSE_STATE_MAIN:PAUSE_STATE_OFF;if(mode==5)p->stateFlags1|=PLAYER_STATE1_200000;
  mmvrgame::ResetTestCamera();mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=28000+mode;f.timeSeconds=28000+mode;
  auto first=mmvrgame::TestCameraFrame(f);float start=p->actor.world.pos.x;
  bool valid=first.active;float lastView=0;
  for(int step=0;step<10;++step){f.head.position.x+=(mode==7?-.1f:.1f);f.timeSeconds+=1.0/120.0;auto frame=mmvrgame::TestCameraFrame(f);lastView=mmvr::InversePose(frame.view).m[3][0];
   valid&=frame.active&&std::abs(float(p->cylinder.dim.pos.x)-p->actor.world.pos.x)<1.1f&&std::abs(lastView-p->actor.world.pos.x)<.01f;
  }
  float delta=p->actor.world.pos.x-start;
  bool pass=valid&&((mode<4&&std::abs(delta+40.f)<.02f)||(mode==4&&std::abs(delta)<.01f)||(mode==5&&std::abs(delta)<.01f)||
      (mode==6&&delta<0&&p->actor.world.pos.x>=-988.1f)||(mode==7&&delta>0&&p->actor.world.pos.x<=450.f&&p->actor.world.pos.y==0));
  pass&=p->actor.shape.rot.y==0&&p->focusActor==((mode==1||mode==3)?object:nullptr)&&p->heldActor==((mode==2||mode==3)?object:nullptr);
  bool* results[]={&out.free,&out.target,&out.carry,&out.both,&out.pause,&out.climb,&out.wall,&out.edge};*results[mode]=pass;
  std::ofstream("native-roomscale.log",std::ios::app)<<"mode="<<mode<<" active="<<first.active<<" dx="<<delta<<" body="<<p->actor.world.pos.x<<" view="<<lastView<<" valid="<<valid<<" pass="<<pass<<'\n';
 }
 *p=saved;play->pauseCtx.state=pause;mmvr::GetSettings()=settings;mmvrgame::ResetTestCamera();return out;
}
