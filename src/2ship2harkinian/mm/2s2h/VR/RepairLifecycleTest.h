#pragma once
#include "Bottle.h"
extern "C" {
#include "overlays/actors/ovl_En_Test5/z_en_test5.h"
}
static void NativeWaterScoopTest(PlayState* play){
 auto* p=GET_PLAYER(play);auto player=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto msg=play->msgCtx;auto tasks=play->animTaskQueue;auto input=*CONTROLLER1(&play->state);
 std::ofstream log("native-room-scenario.log",std::ios::app);
 for(int sourceIndex=0;sourceIndex<2;++sourceIndex)for(int hand=0;hand<2;++hand){
  EnTest5* source=nullptr;
  for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)
   if(a->id==ACTOR_EN_TEST5&&a->update&&!a->init&&(a->world.pos.x>0)==(sourceIndex==0))source=reinterpret_cast<EnTest5*>(a);
  bool caught=false,restSafe=true;
  if(source){
   *p=player;gSaveContext=save;play->msgCtx=msg;play->msgCtx.msgMode=MSGMODE_NONE;
   mmvr::GetSettings().Set(mmvr::Setting::PhysicalBottle,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,1-hand);
   float x=source->minPos.x+source->xLength*.5f,z=source->minPos.z+source->zLength*.5f;
   NativeClimbPlace(play,x,source->minPos.y-30,z+18);
   p->actor.depthInWater=30;p->stateFlags1=PLAYER_STATE1_8000000;p->heldItemButton=EQUIP_SLOT_C_DOWN;
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
   gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_BOTTLE);
   auto frame=NativeClimbFrame(180000+sourceIndex*2+hand);auto view=mmvr::YawPose(0,x,source->minPos.y+22,z+18),head=mmvr::YawPose(0);
   mmvrgame::ClearBottle();source->actor.parent=nullptr;
   for(int i=0;i<75&&!caught;++i){
    frame.timeSeconds=180000+sourceIndex*10+hand*2+i/90.;frame.hands[hand].position={i<20?-.2f:-.2f+(i-20)*.006f,-.65f,-.4f};
    mmvrgame::RecordTracking(frame,view,head);
    auto model=mmvr::TrackedHandModel(frame,view,head,0,hand,mmvr::GetSettings());
    mmvrgame::UpdateBottle(frame,model);
    source->actor.update(&source->actor,play);caught=gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_SPRING_WATER;
    if(i<20)restSafe&=!caught;
   }
   source->actor.parent=nullptr;
  }
  log<<"bottle-water-"<<sourceIndex<<"-"<<hand<<"="<<(caught&&restSafe)<<"\n";
 }
 *p=player;gSaveContext=save;play->msgCtx=msg;play->animTaskQueue=tasks;*CONTROLLER1(&play->state)=input;mmvr::GetSettings()=settings;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvrgame::ResetTestCamera();
}
static mmvr::Pad NativeRepairLifecycle(PlayState* play,int tick){
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
 static mmvr::Settings saved;static mmvr::TrackingFrame frame;static Actor* block=nullptr;static float startZ=0,pushedZ=0,startX=0,minPull=0;static bool grabbed=false,stable=true;static float distance1=0,savedSpeed=1;static std::ofstream trace("native-repair-lifecycle.log");
 auto check=[&](const char* name,bool pass){std::ofstream("native-room-scenario.log",std::ios::app)<<name<<"="<<pass<<"\n";trace<<name<<"="<<pass<<"\n"<<std::flush;};
 if(tick==1521){saved=mmvr::GetSettings();savedSpeed=CVarGetFloat("gVR.MovementSpeed",1);mmvr::SetNativeTestTracking(true);NativeWaterScoopTest(play);}
 if(tick==1540){
  for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)if(a->id==ACTOR_OBJ_OSHIHIKI&&a->update)block=a;
  if(block){startZ=block->world.pos.z;startX=block->world.pos.x;NativeClimbPlace(play,startX,0,startZ+45);}
  frame=NativeClimbFrame(180100);
 }
 if(tick>=1540&&tick<1680){
  frame.timeSeconds=180100+(tick-1540)/30.;mmvrgame::TestCameraFrame(frame);
  pad.buttons=tick>=1545?BTN_A:0;pad.y=tick<1545?0:tick<1620?75:-75;
  grabbed|=(p->stateFlags2&PLAYER_STATE2_100)!=0;
  stable&=std::abs(p->actor.world.pos.x-startX)<5;
  if(tick==1620){pushedZ=block?block->world.pos.z:0;minPull=pushedZ;check("live-push-block",block&&grabbed&&stable&&startZ-pushedZ>20);}
  if(tick>=1620&&block)minPull=std::min(minPull,block->world.pos.z);
  if(tick==1679)check("live-pull-block",block&&stable&&block->world.pos.z-minPull>20);
 }
 if(tick==1680){NativeClimbPlace(play,-1760,0,660);p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=(s16)0xC000;frame=NativeClimbFrame(180200);}
 if(tick>=1680&&tick<1870){frame.timeSeconds=180200+(tick-1680)/30.;mmvrgame::TestCameraFrame(frame);pad.y=tick<1780?75:0;
  if(tick==1869)check("live-cucco-ramp",p->actor.world.pos.x<-2490&&p->actor.world.pos.y>210&&(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND));}
 if(tick==1870||tick==1930){NativeClimbPlace(play,0,0,450);frame=NativeClimbFrame(180300+tick);mmvr::GetSettings().Set(mmvr::Setting::MovementSpeed,tick==1870?1:2);CVarSetFloat("gVR.MovementSpeed",tick==1870?1:2);}
 if(tick>=1870&&tick<1990){frame.timeSeconds=180300+tick/30.;mmvrgame::TestCameraFrame(frame);pad.y=55;
  if(tick==1929)distance1=450-p->actor.world.pos.z;
  if(tick==1989){float distance2=450-p->actor.world.pos.z;trace<<"speed distances="<<distance1<<","<<distance2<<"\n";check("live-movement-speed",distance1>30&&distance2>distance1*1.5f&&distance2<distance1*2.5f);}}
 if(tick==1990){CVarSetFloat("gVR.MovementSpeed",savedSpeed);mmvr::GetSettings()=saved;mmvrgame::ClearTracking();mmvrgame::ResetTestCamera();mmvr::SetNativeTestTracking(false);trace<<"COMPLETE\n"<<std::flush;}
 if(tick%5==0)trace<<tick<<" pos="<<p->actor.world.pos.x<<","<<p->actor.world.pos.y<<","<<p->actor.world.pos.z<<" yaw="<<p->actor.shape.rot.y<<" action="<<(void*)p->actionFunc<<" flags2="<<p->stateFlags2<<" block="<<(block?block->world.pos.z:0)<<"\n"<<std::flush;
 return pad;
}
