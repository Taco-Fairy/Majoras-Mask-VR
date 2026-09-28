#pragma once
#include "DebugNativeTriggers.h"
// NativeStateTest.cpp uses this same C-header compatibility wrapper.
extern "C" {
#define this nativeThis
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#undef this
}

// Opt-in isolated test: load the authored Woodfall room and let its actual
// crystal actor start csId 1 after a simulated AC collision. No script is
// injected into the cutscene context.
static mmvr::Pad NativeWoodfallCrystalTest(PlayState* play, unsigned tick) {
    static bool started = false, hit = false, passed = false;
    static unsigned sceneTicks = 0;
    static bool requestedRoom=false, readyRoom=false;
    static std::ofstream log("native-woodfall-crystal.log");
    mmvr::Pad pad;
    pad.active = true;
    if (!started && tick >= 60) {
        constexpr int woodfallIndex = ARRAY_COUNT(debugNativeTriggers) - 1;
        static_assert(debugNativeTriggers[woodfallIndex].kind == DebugNativeTriggerKind::WoodfallCrystal);
        started = MMVR_DebugNativeTriggerBegin(play, woodfallIndex) != 0;
        log << "portal=" << started << " sourceScene=" << play->sceneId << "\n" << std::flush;
        if (!started) Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    if (!started || play->sceneId != SCENE_MITURIN || play->transitionTrigger != TRANS_TRIGGER_OFF)
        return pad;
    if (play->transitionMode != TRANS_MODE_OFF || play->roomCtx.status) return pad;
    if (!readyRoom) {
        if (!requestedRoom && play->roomCtx.curRoom.num != 0) {
            requestedRoom = Room_RequestNewRoom(play, &play->roomCtx, 0) != 0;
            if (!requestedRoom) throw std::runtime_error("Woodfall room 0 load rejected");
            return pad;
        }
        if (requestedRoom) Room_FinishRoomChange(play, &play->roomCtx);
        readyRoom=true;
        log << "room=" << int(play->roomCtx.curRoom.num) << '\n' << std::flush;
    }
    ++sceneTicks;
    ObjSwitch* crystal = nullptr;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_SWITCH].first; actor; actor = actor->next) {
        if (actor->id == ACTOR_OBJ_SWITCH && (actor->params & 7) == 3 &&
            ((actor->params >> 8) & 0x7F) == 0x60 && actor->csId == 1) {
            crystal = reinterpret_cast<ObjSwitch*>(actor);
            break;
        }
    }
    if (crystal && !hit && sceneTicks > 30) {
        crystal->colliderJntSph.base.acFlags |= AC_HIT;
        hit = true;
        log << "actor-found=1 csId=" << crystal->dyna.actor.csId << " room=" << int(crystal->dyna.actor.room)
            << " sceneTicks=" << sceneTicks << "\n" << std::flush;
    }
    if (hit && sceneTicks % 15 == 0) {
        const int csId = CutsceneManager_GetCurrentCsId();
        passed |= csId == 1;
        log << "sceneTicks=" << sceneTicks << " csId=" << csId << " csState=" << int(play->csCtx.state)
            << " switch=" << Flags_GetSwitch(play, 0x60) << " passed=" << passed << "\n" << std::flush;
    }
    if (passed || tick > 1200) {
        log << "PASS=" << passed << " actor=" << (crystal != nullptr) << " hit=" << hit << "\n" << std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return pad;
}

// Private regression: exercise the real debug portal, settled floor and movement.
static mmvr::Pad NativeWoodfallWebLandingTest(PlayState* play, unsigned tick) {
 static bool started=false,landed=false,pathClear=true,moved=false;
 static unsigned settled=0;static Vec3f before{};
 static std::ofstream log("native-woodfall-landing.log");
 mmvr::Pad pad;pad.active=true;
 if(!started && tick>=60){started=MMVR_DebugLocationBegin(play,101)!=0;log<<"portal="<<started<<std::endl;}
 if(started && play->sceneId==SCENE_MITURIN && play->roomCtx.curRoom.num==3 && !play->roomCtx.status){
  auto* p=GET_PLAYER(play);++settled;
  if(settled==20){
   before=p->actor.world.pos;
   landed=fabsf(before.x+1110)<2 && fabsf(before.z+750)<2 && fabsf(before.y+1185)<2;
   // The approach from spawn to the web must stay on the same walkable floor.
   for(float z=-750;z<=-630;z+=10){
    Vec3f probe{-1110,-1100,z};CollisionPoly* poly=nullptr;int bg=0;
    float floor=BgCheck_EntityRaycastFloor5(&play->colCtx,&poly,&bg,&p->actor,&probe);
    Vec3f center{-1110,floor+30,z};
    pathClear &= poly && fabsf(floor+1185)<2 && !BgCheck_SphVsFirstWall(&play->colCtx,&center,20);
   }
   log<<"landed="<<landed<<" pathClear="<<pathClear<<std::endl;
  }
  // Ordinary native stick input, not position injection, proves the player is free.
  if(settled>=25 && settled<40)pad.y=60;
  if(settled==45){
   float dx=p->actor.world.pos.x-before.x,dz=p->actor.world.pos.z-before.z;
   moved=dx*dx+dz*dz>100 && fabsf(p->actor.world.pos.y+1185)<2;
  }
  if(settled%10==0)log<<"position "<<settled<<" "<<p->actor.world.pos.x<<" "<<p->actor.world.pos.y<<" "<<p->actor.world.pos.z<<std::endl;
  if(settled>=60){
   log<<(landed&&pathClear&&moved?"PASS":"FAIL")<<" landing and native movement moved="<<moved<<std::endl;
   Ship::Context::GetRawInstance()->GetWindow()->Close();
  }
 }
 if(tick>1200){log<<"FAIL timeout"<<std::endl;Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}

#include "DebugLocations.h"
// Use the same portal as the user, then verify an actual usable hot-spring actor
// and native floor/movement. Bottle contents have separate gesture checks.
static mmvr::Pad NativeHotSpringLandingTest(PlayState* play,unsigned tick){
 static bool started=false,water=false,ground=false,moved=false;static unsigned settled=0;
 static Vec3f origin{};static std::ofstream log("native-hot-spring-landing.log");
 mmvr::Pad pad;pad.active=true;
 if(!started&&tick>=60){
  for(int i=0;i<ARRAY_COUNT(debugLocations);++i)if(debugLocations[i].scene==SCENE_GORON_HAKA&&debugLocations[i].interactionPreset==5){started=MMVR_DebugLocationBegin(play,i)!=0;break;}
 }
 if(started&&play->sceneId==SCENE_GORON_HAKA&&!play->roomCtx.status&&play->transitionMode==TRANS_MODE_OFF){
  ++settled;auto* p=GET_PLAYER(play);
  for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)
   if(a->id==ACTOR_BG_GORON_OYU&&a->update&&!a->init&&a->params==1)water=true;
  if(settled==30){origin=p->actor.world.pos;Vec3f probe=origin;probe.y+=60;CollisionPoly* poly=nullptr;int bg=0;
   float y=BgCheck_EntityRaycastFloor5(&play->colCtx,&poly,&bg,&p->actor,&probe);ground=poly&&fabsf(y-origin.y)<10;
  }
  if(settled>=35&&settled<45)pad.y=60;
  if(settled==50){float dx=p->actor.world.pos.x-origin.x,dz=p->actor.world.pos.z-origin.z;moved=dx*dx+dz*dz>25;}
  if(settled>=60){log<<(water&&ground&&moved?"PASS":"FAIL")<<" native-hot-spring water="<<water<<" ground="<<ground<<" moved="<<moved<<std::endl;Ship::Context::GetRawInstance()->GetWindow()->Close();}
 }
 if(tick>1200){log<<"FAIL native-hot-spring-timeout"<<std::endl;Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}
