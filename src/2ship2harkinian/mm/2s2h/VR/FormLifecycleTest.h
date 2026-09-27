#pragma once
#include "PlayerBody.h"
#include "HeightCalibrationTest.h"
// This fixture runs the real player update/object reload between samples. It does
// not replace action functions or clear gameplay flags to force completion.
static mmvr::Pad NativeFormLifecycle(PlayState* play,unsigned tick){
 static int phase=0,age=0,index=0;static unsigned entered=0;static bool wrote=false;static Vec3f start{};
 static std::ofstream trace("native-form-lifecycle.log");static std::ofstream result("native-form-lifecycle.json");
 static Actor* carryTarget=nullptr;static bool carryGrab=false,carryHeld=true,carryVerified=false;
 static float measuredEye=0,measuredFocus=0;
 static bool goronSpikes=false,goronStreaks=false,goronStreaksStopped=false;
 static Vec3f goronReturn{};
 static int zoraFirstThrow=-1,zoraMaxFlying=0;static bool zoraAim=false,zoraLeft=false,zoraRight=false,zoraReturned=false;
 static bool bodyReloadHidden=true;
 static bool wore=false,instrument=false,moved=false,removed=false,selected=false;static int notes=0,notesPlayed=0;static Actor* bottleTarget=nullptr;static bool bottleCaught=false,bottleReturned=false,bottleGuard=true;
 const int forms[]={PLAYER_FORM_DEKU,PLAYER_FORM_GORON,PLAYER_FORM_ZORA,PLAYER_FORM_FIERCE_DEITY};const int items[]={ITEM_MASK_DEKU,ITEM_MASK_GORON,ITEM_MASK_ZORA,ITEM_MASK_FIERCE_DEITY};const int slots[]={SLOT_MASK_DEKU,SLOT_MASK_GORON,SLOT_MASK_ZORA,SLOT_MASK_FIERCE_DEITY};
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);mmvr::SetNativeTestTracking(true);
 if(tick==70 && std::getenv("MMVR_HEIGHT_TEST")) NativeHeightCalibrationCheck(play);
 if(tick<80)return pad;
 if(!wrote){result<<"[";wrote=true;}
 auto next=[&](int value){phase=value;age=0;entered=tick;};
 auto ready=[&](){return p->csAction==PLAYER_CSACTION_NONE&&play->csCtx.state==CS_STATE_IDLE&&play->msgCtx.msgMode==MSGMODE_NONE&&!MMVR_LocalTransformation(p)&&!MMVR_ItemPresentationActive(p)&&!(p->stateFlags2&PLAYER_STATE2_USING_OCARINA);};
 auto assign=[&](int slot,int item){gSaveContext.save.saveInfo.inventory.items[slot]=item;BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;mmvrgame::SelectItem(play,slot,item);};
 mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=60000+index;f.timeSeconds=60000+tick/30.;int hand=mmvr::SwordController(mmvr::GetSettings());
 for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;f.hands[h].position={h?.5f:-.5f,-.3f,-.3f};}
 auto view=mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+45,p->actor.world.pos.z),head=mmvr::YawPose(0);
 if(phase==0&&ready()){assign(slots[index],items[index]);wore=instrument=moved=removed=false;selected=mmvrgame::SelectedItem(play)==items[index];notes=notesPlayed=0;bottleCaught=bottleReturned=false;bottleGuard=true;bottleTarget=nullptr;carryTarget=nullptr;carryGrab=false;carryHeld=true;carryVerified=false;next(1);}
 else if(phase==1){f.triggers[hand]=age>=3&&age<9?1:0;if(age>=3){float t=std::clamp((age-3)/5.f,0.f,1.f);f.hands[hand].position={(hand?.5f:-.5f)*(1-t),-.3f+.18f*t,-.3f+.16f*t};}if(age>10)next(2);}
 else if(phase==2&&p->transformation==forms[index]&&ready()){wore=true;bodyReloadHidden&=mmvrgame::TestBodyRenderWithoutPose();if(forms[index]==PLAYER_FORM_FIERCE_DEITY || std::getenv("MMVR_HEIGHT_TEST")){mmvrgame::StowItem(play);next(20);}else{assign(SLOT_OCARINA,ITEM_OCARINA_OF_TIME);selected&=mmvrgame::SelectedItem(play)==ITEM_OCARINA_OF_TIME;next(3);}}
 else if(phase==20){if(age>=30){if(std::getenv("MMVR_HEIGHT_TEST")) NativeHeightCalibrationCheck(play);start=p->actor.world.pos;next(6);}}
 else if(phase==3){f.triggers[hand]=age>=3&&age<7?1:0;if(p->stateFlags2&PLAYER_STATE2_USING_OCARINA){instrument=true;next(4);}}
 else if(phase==4){if((age>=20&&age<=25)||(age>=35&&age<=40)||(age>=50&&age<=55)){pad.buttons=age<35?BTN_A:age<50?BTN_CDOWN:BTN_CRIGHT;if(age==20||age==35||age==50)++notes;}if(play->msgCtx.ocarinaStaff)notesPlayed=std::max(notesPlayed,int(play->msgCtx.ocarinaStaff->pos));if(age==70){auto name="native-form-instrument-"+std::to_string(index);mmvr::RequestNativeCapture(name.c_str());}if(age>=80){pad.buttons=BTN_B;next(5);}}
 else if(phase==5&&ready()){mmvrgame::StowItem(play);assign(SLOT_BOTTLE_1,ITEM_BOTTLE);next(9);}
 else if(phase==9){
  f.hands[hand].position={std::min(.6f,-.4f+age*.018f),-.35f,-.45f};
  auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());auto opening=mmvrgame::BottleMouthForForm(p);Vec3f mouth{};
  for(int k=0;k<3;++k)(&mouth.x)[k]=opening.x*model.m[0][k]+opening.y*model.m[1][k]+opening.z*model.m[2][k]+model.m[3][k];
  if(age==4)bottleTarget=Actor_Spawn(&play->actorCtx,play,ACTOR_EN_ELF,mouth.x,mouth.y,mouth.z,0,0,0,6);
  if(bottleTarget&&bottleTarget->update){bottleTarget->world.pos=mouth;bottleTarget->velocity={};bottleTarget->speed=0;}
  if(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_FAIRY){bottleCaught=true;bottleTarget=nullptr;next(10);}
 }
 else if(phase==10){if(age==3){auto held=p->heldItemAction;mmvrgame::StowItem(play);mmvrgame::SelectItem(play,SLOT_MASK_GORON,ITEM_MASK_GORON);bottleGuard&=p->heldItemAction==held&&mmvrgame::SelectedItem(play)==ITEM_FAIRY;}if(age%6<2)pad.buttons=BTN_A;if(age>15&&ready()){bottleReturned=true;mmvrgame::StowItem(play);next(11);}}
 else if(phase==11){
  f.hands[hand].position={hand?.5f:-.5f,-.5f+(age>8&&age<15?.15f:0.f),-.3f};f.triggers[hand]=age>=6&&age<15?1:0;
  if(age==2)carryTarget=Actor_Spawn(&play->actorCtx,play,ACTOR_OBJ_TSUBO,p->actor.world.pos.x+(hand?20:-20),p->actor.world.pos.y,p->actor.world.pos.z-12,0,0,0,0x11f);
  if(p->heldActor&&p->heldActor==carryTarget)carryGrab=true;
  if(age>=8&&age<14&&forms[index]!=PLAYER_FORM_DEKU)carryHeld&=p->heldActor==carryTarget&&carryTarget!=nullptr;
  if(age>22&&!p->heldActor&&ready()){
   carryVerified=(forms[index]==PLAYER_FORM_DEKU?!carryGrab:carryGrab&&carryHeld)&&!(p->stateFlags1&PLAYER_STATE1_CARRYING_ACTOR);
   if(mmvrgame::LiveSceneActor(play,carryTarget))Actor_Kill(carryTarget);carryTarget=nullptr;start=p->actor.world.pos;if(forms[index]==PLAYER_FORM_ZORA){mmvrgame::StowItem(play);next(12);}else if(forms[index]==PLAYER_FORM_GORON){
    goronReturn=p->actor.world.pos;
    mmvrgame::MovePlayerBody(play,p,{-1700,0,-900});p->actor.home.pos=p->actor.world.pos;p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=(s16)0x8000;
    mmvr::GetSettings().Set(mmvr::Setting::GoronSpeedStreaks,.1f);CVarSetFloat("gVR.GoronSpeedStreaks",.1f);next(21);
   }else next(6);
  }
 }
 else if(phase==21){
  // Let native pickup offers settle after fixture placement before A.
  pad.buttons=age>=8?BTN_A:0;pad.y=age>=8?80:0;
  goronSpikes|=p->unk_B86[1]>=7&&std::abs(p->speedXZ)>10;
  goronStreaks|=mmvr::SpeedStreaks()>.09f;
  if(age%10==0)trace<<"goron-roll age="<<age<<" spikes="<<p->unk_B86[1]<<" speed="<<p->speedXZ<<" streaks="<<mmvr::SpeedStreaks()<<" held="<<(p->heldActor?p->heldActor->id:-1)<<" x="<<p->actor.world.pos.x<<" z="<<p->actor.world.pos.z<<"\n"<<std::flush;
  if(age>=100){pad={};pad.active=true;next(22);}
 }
 else if(phase==22){
  if(age>=25&&ready()&&!(p->stateFlags3&PLAYER_STATE3_1000)){goronStreaksStopped=mmvr::SpeedStreaks()==0;mmvrgame::MovePlayerBody(play,p,{goronReturn.x,goronReturn.y,goronReturn.z});p->actor.home.pos=p->actor.world.pos;start=p->actor.world.pos;next(6);}
 }
 else if(phase==12){
  if(age>=5&&age<40)pad.buttons=BTN_B;
  zoraAim|=mmvrgame::FormReticleVisible(p);
  int flying=0;for(Actor* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)if(a->id==ACTOR_EN_BOOM&&a->update){++flying;zoraLeft|=a->params==0;zoraRight|=a->params==1;}
  if(flying&&zoraFirstThrow<0)zoraFirstThrow=age;zoraMaxFlying=std::max(zoraMaxFlying,flying);
  if(age>=43&&age<58&&(age%2))pad.buttons=BTN_B; // Mash while fins are away.
  if(age==64)trace<<"zora-release first="<<zoraFirstThrow<<" maxFlying="<<zoraMaxFlying<<"\n"<<std::flush;
  if(age>65&&zoraLeft&&zoraRight&&!(p->stateFlags1&PLAYER_STATE1_ZORA_BOOMERANG_THROWN)&&!MMVR_FormAimStage(p)){zoraReturned=true;next(6);}
 }
 else if(phase==6){if(age<=1){measuredEye=mmvrgame::FormEyeHeight(p);measuredFocus=p->actor.focus.pos.y-p->actor.world.pos.y;}pad.y=60;if(age>=20){moved=std::hypot(p->actor.world.pos.x-start.x,p->actor.world.pos.z-start.z)>1;pad.y=0;next(7);}}
 // Exercise removal while Zora is in the fin-aim upper-body action, not only idle.
 else if(phase==7){if(age==0&&std::getenv("MMVR_TRANSFORM_STALE_FADE")){R_PLAY_FILL_SCREEN_ON=-20;R_PLAY_FILL_SCREEN_ALPHA=100;}if(forms[index]==PLAYER_FORM_ZORA&&age<12)pad.buttons=BTN_B;float t=std::clamp((age-7)/7.f,0.f,1.f);f.hands[hand].position={(hand?.55f:-.55f)*t,-.12f-.18f*t,-.14f-.16f*t};f.triggers[hand]=age>=3&&age<20?1:0;if(age>22)next(8);}
 else if(phase==8&&p->transformation==PLAYER_FORM_HUMAN&&ready()){removed=true;bodyReloadHidden&=mmvrgame::TestBodyRenderWithoutPose();if(index)result<<",";result<<"{\"form\":"<<forms[index]<<",\"bodyReloadHidden\":"<<bodyReloadHidden<<",\"selected\":"<<selected<<",\"wore\":"<<wore<<",\"instrument\":"<<instrument<<",\"notesSent\":"<<notes<<",\"notesPlayed\":"<<notesPlayed<<",\"moved\":"<<moved<<",\"removed\":"<<removed<<",\"bottleCaught\":"<<bottleCaught<<",\"bottleReturned\":"<<bottleReturned<<",\"bottleGuard\":"<<bottleGuard<<",\"zoraAim\":"<<zoraAim<<",\"zoraLeft\":"<<zoraLeft<<",\"zoraRight\":"<<zoraRight<<",\"zoraReturned\":"<<zoraReturned<<",\"modelEye\":"<<measuredEye<<",\"modelFocus\":"<<measuredFocus<<",\"carryVerified\":"<<carryVerified<<",\"goronSpikes\":"<<goronSpikes<<",\"goronStreaks\":"<<goronStreaks<<",\"goronStreaksStopped\":"<<goronStreaksStopped<<"}";result.flush();if(++index==4){result<<"]";result.flush();trace.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}next(0);}
 if((phase==2||phase==8)&&age==25){mmvrgame::TestCameraFrame(f);auto name="native-transform-"+std::to_string(index)+(phase==2?"-on":"-off");mmvr::RequestNativeCapture(name.c_str());}
 mmvrgame::RecordFormTracking(f,view,head);mmvrgame::RecordTracking(f,view,head);if(phase==9)mmvrgame::UpdateBottle(f,mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings()));mmvrgame::UpdateMaskContext(play);mmvr::UpdateMaskTracking(f,true);
 if(age%15==0)trace<<"head-height form="<<int(p->transformation)<<" phase="<<phase<<" y="<<p->bodyPartsPos[PLAYER_BODYPART_HEAD].y-p->actor.world.pos.y<<" focusHeight="<<p->actor.focus.pos.y-p->actor.world.pos.y<<" vrHeight="<<mmvrgame::FormEyeHeight(p)<<" nativeHeight="<<Player_GetHeight(p)<<"\n";
 if(age%15==0)trace<<tick<<" index="<<index<<" phase="<<phase<<" age="<<age<<" form="<<int(p->transformation)<<" mask="<<Player_GetCurMaskItemId(play)<<" selected="<<mmvrgame::SelectedItem(play)<<" action="<<(const void*)p->actionFunc<<" item="<<int(p->itemAction)<<" held="<<int(p->heldItemAction)<<" cs="<<int(p->csAction)<<","<<int(play->csCtx.state)<<" msg="<<int(play->msgCtx.msgMode)<<" flags="<<p->stateFlags1<<","<<p->stateFlags2<<"\n"<<std::flush;
 if(++age>350||tick>2400){trace<<"TIMEOUT\n";trace.flush();if(index)result<<",";result<<"{\"failedPhase\":"<<phase<<",\"form\":"<<forms[index]<<"}]";result.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}
