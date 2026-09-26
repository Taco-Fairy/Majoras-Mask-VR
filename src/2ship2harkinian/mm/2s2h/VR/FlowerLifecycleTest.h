#pragma once
// Real object, player action, particles, skeleton and draw lifecycle. Isolated test saves only.
static mmvr::Pad NativeFlowerLifecycle(PlayState* play,unsigned tick){
 static int phase=0,age=0,cycle=0;static Actor* flower=nullptr;static int stages=0;static bool launched=false;
 static std::ofstream log("native-flower-lifecycle.log"),result("native-flower-lifecycle.json");
 if(tick==1)if(const char* token=std::getenv("MMVR_SESSION_TOKEN"))log<<"session "<<token<<"\n"<<std::flush;
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);mmvr::SetNativeTestTracking(true);
 auto ready=[&](){return p->csAction==PLAYER_CSACTION_NONE&&play->csCtx.state==CS_STATE_IDLE&&play->msgCtx.msgMode==MSGMODE_NONE&&!MMVR_LocalTransformation(p);};
 auto next=[&](int n){phase=n;age=0;};
 if(tick<80)return pad;
 if(phase==0&&ready()){
  gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU]=ITEM_MASK_DEKU;
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_MASK_DEKU;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_MASK_DEKU;
  pad.buttons=BTN_CDOWN;next(1);result<<"[";
 }else if(phase==1&&p->transformation==PLAYER_FORM_DEKU&&ready())next(2);
 else if(phase==2){
  if(age==1){
   if(flower&&mmvrgame::LiveSceneActor(play,flower))Actor_Kill(flower);
   p->actor.world.pos={0,4,350};p->actor.prevPos=p->actor.world.pos;p->actor.velocity={};p->speedXZ=0;
   flower=Actor_Spawn(&play->actorCtx,play,ACTOR_OBJ_ETCETERA,0,0,350,0,0,0,(cycle%2)?0x100:0);
   stages=0;launched=false;mmvr::GetSettings().Set(mmvr::Setting::FlowerCameraSpin,cycle<2?1:0);
  }
  if(age>20&&flower&&flower->update)next(3);
 }else if(phase==3){
  if(age<85)pad.buttons=BTN_A;
  int stage=MMVR_DekuFlowerStage(p);if(stage>0&&stage<=4)stages|=1<<stage;
  launched|=bool(p->stateFlags3&PLAYER_STATE3_200);
  if(age==45||age==90){auto name="native-flower-"+std::to_string(cycle)+"-"+std::to_string(age);mmvr::RequestNativeCapture(name.c_str());}
  if(age>115){if(cycle)result<<",";result<<"{\"cycle\":"<<cycle<<",\"stages\":"<<stages<<",\"launched\":"<<launched<<"}"<<std::flush;
   if(++cycle==4){result<<"]"<<std::flush;log<<"COMPLETE\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}next(4);}
 }else if(phase==4){
  // Remove this case's flower before canceling flight. Repeated A presses after
  // landing could re-enter it, carrying a dive into the following case.
  if(age==1&&flower){if(mmvrgame::LiveSceneActor(play,flower))Actor_Kill(flower);flower=nullptr;}
  if((p->stateFlags3&(PLAYER_STATE3_200|PLAYER_STATE3_2000))&&age%20==1)pad.buttons=BTN_A;
  if(age>100&&(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND)&&MMVR_DekuFlowerStage(p)==0&&ready())next(2);
 }
 mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.timeSeconds=80000+tick/30.;f.epoch=80000;
 for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].position={h?.3f:-.3f,-.3f,-.35f};}
 mmvrgame::TestCameraFrame(f);
 if(age%5==0)log<<tick<<" phase="<<phase<<" age="<<age<<" cycle="<<cycle<<" stage="<<MMVR_DekuFlowerStage(p)<<" form="<<int(p->transformation)<<" floor="<<int(p->actor.floorBgId)<<" grounded="<<bool(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND)<<" launch="<<bool(p->stateFlags3&PLAYER_STATE3_200)<<" glide="<<bool(p->stateFlags3&PLAYER_STATE3_2000)<<" y="<<p->actor.world.pos.y<<" vy="<<p->actor.velocity.y<<"\n"<<std::flush;
 if(++age>350||tick>1500){log<<"ERROR TIMEOUT\n"<<std::flush;result<<",{\"timeout\":true}]"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}
