#pragma once
// Actual bow draw/release with all arrow variants, plus tracked hookshot socket.
static void NativeHeadAimTest(PlayState* play) {
 auto* p=GET_PLAYER(play); const Player saved=*p; const auto save=gSaveContext;
 auto settings=mmvr::GetSettings(); auto input=*CONTROLLER1(&play->state); auto* oldInput=sPlayerControlInput;
 // Explicit native-scale synthetic hand positions; calibrated-world poses use a separate fixture.
 mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
 sPlayerControlInput=CONTROLLER1(&play->state); mmvr::ApplyViewMode(2); mmvr::SetNativeTestTracking(true);
 p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
 p->csAction=PLAYER_CSACTION_NONE;p->transformation=PLAYER_FORM_HUMAN;p->heldActor=nullptr;
 mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,1);
 auto frame=mmvr::TrackingFrame{};frame.origin.orientation.w=1;frame.head.orientation={0,.70710678118f,0,.70710678118f};frame.epoch=100;
 for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;}
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(1.57079632679f);
 std::ofstream log("native-head-aim.json");log<<"{\"bow\":[";
 auto savedSave=gSaveContext;auto savedInput=*CONTROLLER1(&play->state);mmvr::GetSettings().Set(mmvr::Setting::PhysicalBow,1);
 const int bowItems[]={ITEM_BOW,ITEM_ARROW_FIRE,ITEM_ARROW_ICE,ITEM_ARROW_LIGHT};
 for(int variant=0;variant<4;++variant){
  mmvrgame::ClearTracking();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,variant%2);int dominant=mmvr::SwordController(mmvr::GetSettings());
  MMVR_PlayerEquipBow(play,p,bowItems[variant]);BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=bowItems[variant];AMMO(ITEM_BOW)=20;
  gSaveContext.magicState=MAGIC_STATE_IDLE;gSaveContext.save.saveInfo.playerData.magic=48;
  frame.grips[0]=frame.grips[1]=0;
  auto model=mmvr::YawPose(0,.35f,2028.95f,0);for(int k=0;k<3;++k)model.m[k][k]=.01f;
  bool stringTracks=true;bool prematureAmmo=false;
  for(int sample=0;sample<95;++sample){
   frame.timeSeconds=50+variant*2+sample/90.0;frame.epoch=200+variant;
   float pull=sample<18?0.f:std::min(.45f,(sample-18)/90.f);float trigger=sample>=10&&sample<75?1.f:0.f;
   frame.triggers[dominant]=trigger;frame.triggers[1-dominant]=0;
   frame.hands[dominant].position={0,-.5f,pull};frame.hands[1-dominant].position={0,-.5f,0};
   frame.aims[1-dominant]=frame.hands[1-dominant];
   auto& input=*CONTROLLER1(&play->state);input.cur.button=trigger>0?(dominant?BTN_CDOWN:BTN_Z):0;input.press.button=sample==12?input.cur.button:0;
   mmvrgame::RecordTracking(frame,view,head);mmvrgame::UpdateBow(frame,view,head,model);mmvrgame::UpdateBow(frame,view,head,model);
   if(sample>=20&&sample<75){auto string=mmvrgame::BowStringPose();stringTracks&=std::abs(-800*string.m[1][2]+string.m[3][2]-pull*40)<.01f;}
   if(sample%3==0)mmvrgame::ProcessBowInput(play);
   if(sample<75&&AMMO(ITEM_BOW)!=20)prematureAmmo=true;
  }
  Actor* arrow=nullptr;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)if(a->id==ACTOR_EN_ARROW&&a->update&&((EnArrow*)a)->vrReleasePower>0){arrow=a;break;}
  bool launched=false;int type=-1;float speed=0;int yaw=0;
  if(arrow){if(arrow->init){arrow->init(arrow,play);arrow->init=nullptr;}func_8088A594((EnArrow*)arrow,play);launched=arrow->parent==nullptr&&arrow->speed>0;type=arrow->params;speed=arrow->speed;yaw=arrow->world.rot.y;Actor_Kill(arrow);}
  if(variant)log<<",";log<<"{\"variant\":"<<variant<<",\"hand\":"<<dominant<<",\"ammo\":"<<int(AMMO(ITEM_BOW))<<",\"earlyAmmo\":"<<prematureAmmo<<",\"launched\":"<<launched<<",\"type\":"<<type<<",\"speed\":"<<speed<<",\"yaw\":"<<yaw<<",\"stringTracks\":"<<stringTracks<<"}";
 }

 log<<"]";
 mmvrgame::ClearTracking();p->heldActor=nullptr;
 mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);
 MMVR_PlayerEquipHookshot(play,p);
 frame.epoch=900;frame.timeSeconds=900;frame.triggers[0]=frame.triggers[1]=0;
 mmvr::Matrix a{},b{};
 mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,0);mmvrgame::RecordTracking(frame,view,head);
 bool before=mmvrgame::TrackedMuzzle(play,p,a);
 mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,1);frame.timeSeconds+=.02;mmvrgame::RecordTracking(frame,view,head);
 bool after=mmvrgame::TrackedMuzzle(play,p,b);
 bool origin=before&&after;
 for(int c=0;c<3;++c)origin &= std::abs(a.m[3][c]-b.m[3][c])<.001f;
 log<<",\"hookHeadDirection\":"<<(after&&std::abs(b.m[2][0]-1)<.001f&&std::abs(b.m[2][2])<.001f)
    <<",\"hookOriginPreserved\":"<<origin<<"}";log.close();
 mmvrgame::ClearTracking();*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;
 *CONTROLLER1(&play->state)=input;sPlayerControlInput=oldInput;
 Ship::Context::GetRawInstance()->GetWindow()->Close();
}
