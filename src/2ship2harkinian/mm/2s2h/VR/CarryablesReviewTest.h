#pragma once
#include "Carry.h"
extern "C" {
#include "overlays/actors/ovl_Obj_Kibako/z_obj_kibako.h"
#include "overlays/actors/ovl_En_Bombf/z_en_bombf.h"
void func_808AEF68(EnBombf*,PlayState*);
void func_808AEE3C(EnBombf*,PlayState*);
#include "overlays/actors/ovl_Obj_Flowerpot/z_obj_flowerpot.h"
#include "overlays/actors/ovl_Obj_Snowball2/z_obj_snowball2.h"
void func_80A1C818(ObjFlowerpot*);
void func_80B39C78(ObjSnowball2*);
void ObjKibako_SetupIdle(ObjKibako*);
void func_80926318(ObjKibako*,PlayState*);
void Player_Action_Idle(Player*,PlayState*);
}
// Native actor offers, attachment/release, and loaded visual-mesh contact.
// These run only in the isolated private fixture process, never a user session.
static void NativeCarryablesReviewTest(PlayState* play,std::ostream& out) {
 auto* p=GET_PLAYER(play);const Player baseline=*p;const auto save=gSaveContext;
 const auto settings=mmvr::GetSettings();const auto input=*CONTROLLER1(&play->state);
 mmvr::GetSettings().Set(mmvr::Setting::PhysicalCarry,1);
 mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
 mmvr::GetSettings().Set(mmvr::Setting::CarryGrabDistance,.25f);
 int meshes=0,rotation=0,attempts=0,grabs=0,releases=0,denied=0,anchors=0,snowThrows=0;bool exclusions=true;
 for(int id:{ACTOR_OBJ_KIBAKO2,ACTOR_EN_ZOG}) {Actor a{};a.id=id;exclusions&=!MMVR_CarryActorSupported(&a);}
 for(int kind=0;kind<4;++kind) {
  Actor a{};a.id=kind<2?ACTOR_OBJ_KIBAKO:kind==2?ACTOR_OBJ_FLOWERPOT:ACTOR_OBJ_SNOWBALL2;
  a.params=kind==1?s16(0x8000):0;a.scale={.15f,.15f,.15f};Vec3f closest;bool inside=false;
  if(mmvrgame::CarryPropSurface(&a,{100,10,0},closest,inside)&&!inside) {
   ++meshes;const Vec3f original=closest;a.shape.rot.y=0x4000;
   Vec3f rotated;bool rotatedInside=false;
   if(mmvrgame::CarryPropSurface(&a,{0,10,-100},rotated,rotatedInside)&&!rotatedInside&&
      std::abs(rotated.x-original.z)<.01f&&std::abs(rotated.y-original.y)<.01f&&std::abs(rotated.z+original.x)<.01f)++rotation;
  }
 }
 ObjKibako* crates[2]{};
 for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)
  if(a->id==ACTOR_OBJ_KIBAKO&&a->update&&!a->init)crates[KIBAKO_BANK_INDEX(a)]=reinterpret_cast<ObjKibako*>(a);
 out<<"{\"meshes\":"<<meshes<<",\"rotatedMeshes\":"<<rotation<<",\"exclusions\":"<<(exclusions?"true":"false")<<",\"cases\":[";
 bool first=true;
 auto check=[&](auto* box,auto setup) {if(!box)return;
  const auto original=*box;
  for(int form=0;form<PLAYER_FORM_MAX;++form)for(int hand=0;hand<2;++hand) {
   play->colChkCtx.colATCount=play->colChkCtx.colACCount=play->colChkCtx.colOCCount=0;
   *p=baseline;gSaveContext=save;*CONTROLLER1(&play->state)={};*box=original;
   p->actor.world.pos=p->actor.prevPos={0,0,230};p->actor.velocity={};p->actor.speed=p->speedXZ=0;
   p->actor.shape.rot.y=p->actor.world.rot.y=0;p->transformation=form;gSaveContext.save.playerForm=form;
   p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
   p->heldActor=p->actor.child=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;
   p->actionFunc=Player_Action_Idle;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
   box->actor.world.pos=box->actor.prevPos={0,0,200};box->actor.parent=nullptr;
   box->actor.xzDistToPlayer=30;box->actor.playerHeightRel=0;box->actor.yawTowardsPlayer=0;
   box->actor.bgCheckFlags=BGCHECKFLAG_GROUND;setup(box);
   mmvr::TrackingFrame f{};f.epoch=90000+attempts;f.timeSeconds=5000+attempts;
   f.head.orientation.w=f.origin.orientation.w=1;
   for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;
    f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].position={0,-.75f,-.75f};}
   auto view=mmvr::YawPose(0,0,45,230),head=mmvr::YawPose(0);
   mmvrgame::RecordFormTracking(f,view,head);mmvrgame::RecordTracking(f,view,head);
   // Native body-facing check deliberately fails: the physical hand still touches.
   box->actor.update(&box->actor,play);bool grabbed=mmvrgame::TryGrabCarry(play,p,hand);
   ++attempts;if(grabbed)++grabs;
   bool released=false,anchored=false;
   if(grabbed){
    auto palm=mmvr::YawPose(.4f,0,40,200);mmvr::Matrix before,after;
    mmvrgame::CarryPose(play,p,palm,before);
    float visible[3];for(int c=0;c<3;++c)visible[c]=before.m[3][c]+before.m[1][c]*box->actor.shape.yOffset*box->actor.scale.y;
    box->actor.update(&box->actor,play);++play->gameplayFrames;
    anchored=mmvrgame::CarryPose(play,p,palm,after);
    for(int c=0;c<3;++c)anchored&=std::abs(visible[c]-(after.m[3][c]+after.m[1][c]*box->actor.shape.yOffset*box->actor.scale.y))<.01f;
    if(anchored)++anchors;
    auto sample=mmvrgame::SampleHandThrow(play,p,hand);sample.moving=true;sample.velocity={40,80,-60};
    released=mmvrgame::ReleaseThrowable(play,p,sample,false)&&!p->heldActor&&!box->actor.parent&&box->actor.velocity.y>0;
    if(released){++releases;
     if(box->actor.id==ACTOR_OBJ_SNOWBALL2) {
      const float speed=box->actor.speed,vy=box->actor.velocity.y,gravity=box->actor.gravity;
      box->actor.update(&box->actor,play);
      auto* snow=reinterpret_cast<ObjSnowball2*>(box);
      if(std::abs(box->actor.speed-speed)<.001f&&std::abs(box->actor.velocity.y-(vy+gravity))<.001f&&
         box->actor.gravity==gravity&&!snow->vrPhysicalRelease)++snowThrows;
     }
    }
   }else if(form==PLAYER_FORM_DEKU)++denied;
   if(!first)out<<",";first=false;
   out<<"{\"actor\":"<<box->actor.id<<",\"bank\":"<<KIBAKO_BANK_INDEX(&box->actor)<<",\"form\":"<<form<<",\"hand\":"<<hand
      <<",\"grab\":"<<(grabbed?"true":"false")<<",\"release\":"<<(released?"true":"false")<<"}";
  }*box=original;
 };
 for(auto* box:crates)check(box,ObjKibako_SetupIdle);
 ObjFlowerpot* flower=nullptr;ObjSnowball2* snow=nullptr;
 for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)if(a->update&&!a->init){
  if(a->id==ACTOR_OBJ_FLOWERPOT)flower=reinterpret_cast<ObjFlowerpot*>(a);
  if(a->id==ACTOR_OBJ_SNOWBALL2)snow=reinterpret_cast<ObjSnowball2*>(a);
 }
 check(flower,func_80A1C818);check(snow,func_80B39C78);
 // Real bomb-flower release action: upside-down palm poses must not survive
 // into free flight, while yaw, position and throw velocity remain untouched.
 bool bombFlowerRelease=true;
 for(int tilt:{-0x7000,-0x4000,0x4000,0x7000}) {
  EnBombf bomb{};bomb.actor.id=ACTOR_EN_BOMBF;bomb.actor.params=0;
  bomb.actor.shape.rot=bomb.actor.world.rot={s16(tilt),0x2345,s16(-tilt)};
  bomb.actor.world.pos={30,80,90};bomb.actor.velocity={1,7,3};bomb.actor.speed=8;
  bomb.actionFunc=func_808AEF68;
  func_808AEF68(&bomb,play);
  bombFlowerRelease&=bomb.actionFunc==func_808AEE3C&&bomb.actor.shape.rot.x==0&&bomb.actor.shape.rot.z==0&&
   bomb.actor.world.rot.x==0&&bomb.actor.world.rot.z==0&&bomb.actor.shape.rot.y==0x2345&&bomb.actor.world.rot.y==0x2345&&
   bomb.actor.world.pos.x==30&&bomb.actor.world.pos.y==80&&bomb.actor.world.pos.z==90&&
   bomb.actor.velocity.x==1&&bomb.actor.velocity.y==7&&bomb.actor.velocity.z==3;
 }
 *p=baseline;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;mmvrgame::ClearTracking();
 out<<"],\"attempts\":"<<attempts<<",\"grabs\":"<<grabs<<",\"releases\":"<<releases<<",\"dekuDenied\":"<<denied<<",\"anchors\":"<<anchors<<",\"snowThrows\":"<<snowThrows<<",\"bombFlowerRelease\":"<<(bombFlowerRelease?"true":"false")<<"}";
}
