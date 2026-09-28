#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Jg/z_en_jg.h"
#include "overlays/actors/ovl_Obj_Snowball/z_obj_snowball.h"
void func_80B02DB0(ObjSnowball*,PlayState*);
s16 EnJg_GetCsIdForTeachingLullabyIntro(EnJg*);
void EnJg_TeachLullabyIntro(EnJg*,PlayState*);
void EnJg_FrozenIdle(EnJg*,PlayState*);
}
// Private reproduction: authored winter scene and the actor-owned lesson.
static mmvr::Pad NativeElderLessonTest(PlayState* play,unsigned tick) {
 static bool started=false,triggered=false,sawDrum=false,sawCue=false,released=false;
 static unsigned settled=0,frames=0,promptTick=0,recovered=0;
 static const bool contextual=std::getenv("MMVR_ELDER_FULL_CONTEXT")!=nullptr;
 static std::ofstream log("native-elder-lesson.log");
 mmvr::Pad pad;pad.active=true;mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
 if(!started&&tick>=60){
  EnJg absentIce{};EnJg_FrozenIdle(&absentIce,play);log<<"absent-ice-safe=1\n"<<std::flush;
  for(int i=0;i<ARRAY_COUNT(debugLocations);++i)if(debugLocations[i].scene==SCENE_17SETUGEN){started=MMVR_DebugLocationBegin(play,i)!=0;break;}
  gSaveContext.save.day=1;gSaveContext.save.eventDayCount=1;gSaveContext.save.time=CLOCK_TIME(10,0);gSaveContext.skyboxTime=gSaveContext.save.time;
  gSaveContext.save.playerForm=PLAYER_FORM_GORON;
  CLEAR_WEEKEVENTREG(WEEKEVENTREG_24_40);
  if(contextual){
   SET_WEEKEVENTREG(WEEKEVENTREG_24_80); // Heard the crying child, before entering this encounter.
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_24_10);
   gSaveContext.save.saveInfo.inventory.questItems &= ~((1u<<QUEST_SONG_LULLABY)|(1u<<QUEST_SONG_LULLABY_INTRO));
  }
 }
 if(!started||play->sceneId!=SCENE_17SETUGEN||play->transitionMode!=TRANS_MODE_OFF||play->transitionTrigger!=TRANS_TRIGGER_OFF||play->roomCtx.status)return pad;
 if(++settled<60)return pad;
 EnJg* elder=nullptr;
 for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next){
  if(!released&&a->id==ACTOR_OBJ_SNOWBALL&&a->home.rot.y==1&&a->update&&!a->init){
   func_80B02DB0(reinterpret_cast<ObjSnowball*>(a),play);Actor_Kill(a);released=true;
   log<<"released elder from authored snowball\n"<<std::flush;
  }
  if(a->id==ACTOR_EN_JG&&a->update&&!a->init)elder=reinterpret_cast<EnJg*>(a);
  if(a->id==ACTOR_OBJ_JG_GAKKI&&a->update)sawDrum=true;
 }
 if(elder&&!triggered){
  auto* p=GET_PLAYER(play);p->actor.world.pos=elder->actor.world.pos;p->actor.world.pos.z+=120;
  p->actor.prevPos=p->actor.home.pos=p->actor.world.pos;p->actor.velocity={};p->speedXZ=0;
  if(elder->icePoly){Actor_Kill(elder->icePoly);elder->icePoly=nullptr;}
  elder->actor.yawTowardsPlayer=elder->actor.shape.rot.y+(std::getenv("MMVR_ELDER_LESSON_SIDE")?1:-1);
  if(contextual){
   // Thawed encounter prerequisite only: allow the native actor to leave frozen
   // idle and offer its conversation. Do not jump to the teaching action.
   p->actor.world.pos.x=elder->actor.world.pos.x+Math_SinS(elder->actor.shape.rot.y)*70.0f;
   p->actor.world.pos.z=elder->actor.world.pos.z+Math_CosS(elder->actor.shape.rot.y)*70.0f;
   p->actor.prevPos=p->actor.home.pos=p->actor.world.pos;
   p->actor.shape.rot.y=p->actor.world.rot.y=elder->actor.shape.rot.y+0x8000;
  }else{
   elder->csId=EnJg_GetCsIdForTeachingLullabyIntro(elder);
   elder->actionFunc=EnJg_TeachLullabyIntro;elder->actor.speed=0;
  }
  triggered=true;log<<"trigger csId="<<elder->csId<<"\n"<<std::flush;
 }
 ++frames;
 sawCue|=Cutscene_IsCueInChannel(play,CS_CMD_ACTOR_CUE_470)!=0;
 if(frames%30==0)log<<"frame="<<frames<<" cue="<<sawCue<<" drum="<<sawDrum<<" cs="<<int(play->csCtx.state)<<" msg="<<int(play->msgCtx.msgMode)<<"\n"<<std::flush;
 if(play->msgCtx.msgMode==MSGMODE_SONG_PROMPT){
  static const unsigned short notes[]={BTN_A,BTN_CRIGHT,BTN_CLEFT,BTN_A,BTN_CRIGHT,BTN_CLEFT};
  unsigned index=promptTick/20;if(index<6&&promptTick%20<8)pad.buttons=notes[index];++promptTick;
  if(promptTick%20==0){auto* staff=AudioOcarina_GetPlayingStaff();log<<"prompt="<<promptTick<<" staff="<<int(staff->pos)<<","<<int(staff->state)<<" action="<<play->msgCtx.ocarinaAction<<"\n"<<std::flush;}
 }else if(play->msgCtx.msgMode==MSGMODE_SONG_PROMPT_FAIL){promptTick=0;
 }else if(frames%12<3)pad.buttons=BTN_A;
 bool done=triggered&&sawCue&&sawDrum&&CHECK_WEEKEVENTREG(WEEKEVENTREG_24_40);
 if(contextual){
  auto* p=GET_PLAYER(play);
  bool free=done&&CHECK_QUEST_ITEM(QUEST_SONG_LULLABY_INTRO)&&play->csCtx.state==CS_STATE_IDLE&&play->msgCtx.msgMode==MSGMODE_NONE&&!(p->stateFlags1&PLAYER_STATE1_400);
  recovered=free?recovered+1:0;done=recovered>=30;
 }
 if(done||frames>3000){std::ofstream("native-elder-lesson.json")<<"{\"passed\":"<<(done?"true":"false")<<",\"sawDrum\":"<<(sawDrum?"true":"false")<<"}";Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}
