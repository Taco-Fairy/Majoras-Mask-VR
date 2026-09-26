#pragma once
extern "C" {
void EnElf_Init(Actor*,PlayState*);void EnElf_Update(Actor*,PlayState*);void EnElf_Destroy(Actor*,PlayState*);
}
static void NativeItemsFeedbackTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto input=*CONTROLLER1(&play->state);auto settings=mmvr::GetSettings();auto collisions=play->colChkCtx;
 auto prepare=[&](){*p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->actor.shape.rot={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->heldActor=p->actor.child=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;*CONTROLLER1(&play->state)={};gSaveContext=save;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();};
 log<<",\"assistedBombs\":[";
 for(int direction=0;direction<4;++direction)for(int drop=0;drop<2;++drop){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::BombArcLift,6.5f);mmvr::GetSettings().Set(mmvr::Setting::BombArcAngle,35);mmvr::GetSettings().Set(mmvr::Setting::ThrowMaxSpeed,10);
  int hand=mmvr::SwordController(mmvr::GetSettings());mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.hands[hand].orientation.w=f.aims[hand].orientation.w=1;
  f.handValid[hand]=f.handTracked[hand]=f.aimValid[hand]=true;f.hands[hand].position={0,-.4f,-.3f};f.epoch=1100+direction*2+drop;f.timeSeconds=800+direction*2+drop;
  auto view=mmvr::YawPose(0,0,2045,0);mmvrgame::RecordTracking(f,view,mmvr::YawPose(0));
  EnBom bomb{};bomb.actor.id=ACTOR_EN_BOM;bomb.actor.params=BOMB_TYPE_BODY;bomb.actor.parent=&p->actor;bomb.actor.world.pos={0,2025,0};bomb.actor.update=EnBom_Update;bomb.actor.terminalVelocity=-20;EnBom_Init(&bomb.actor,play);
  p->heldActor=p->interactRangeActor=p->actor.child=&bomb.actor;p->stateFlags1|=PLAYER_STATE1_CARRYING_ACTOR;
  auto sample=mmvrgame::SampleThrow(play,p);float yaw=direction*1.5707963268f;
  sample.moving=!drop;sample.velocity=drop?std::array<float,3>{}:std::array<float,3>{180*std::sin(yaw),-40,180*std::cos(yaw)};
  bool released=mmvrgame::ReleaseThrowable(play,p,sample);float launch=bomb.actor.velocity.y;auto start=bomb.actor.world.pos;float peak=start.y;bool falling=false;
  for(int tick=0;tick<22;++tick){EnBom_Update(&bomb.actor,play);peak=std::max(peak,bomb.actor.world.pos.y);falling|=bomb.actor.velocity.y<0;}
  float horizontal=std::hypot(bomb.actor.world.pos.x-start.x,bomb.actor.world.pos.z-start.z);
  if(direction||drop)log<<",";log<<"{\"direction\":"<<direction<<",\"drop\":"<<drop<<",\"released\":"<<released<<",\"launchY\":"<<launch<<",\"height\":"<<peak-start.y<<",\"travel\":"<<horizontal<<",\"falling\":"<<falling<<"}";
  EnBom_Destroy(&bomb.actor,play);play->colChkCtx=collisions;
 }
 log<<"],\"nativeFairies\":[";
 for(int form=0;form<PLAYER_FORM_MAX;++form)for(int left=0;left<2;++left)for(int type:{FAIRY_TYPE_2,FAIRY_TYPE_6}){
  prepare();p->transformation=form;gSaveContext.save.playerForm=form;mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::PhysicalBottle,1);
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;p->heldItemButton=EQUIP_SLOT_C_DOWN;MMVR_PlayerEquipEmptyBottle(play,p);
  int hand=1-left;mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=1200+form*100+left*10+type;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
  EnElf fairy{};fairy.actor.id=ACTOR_EN_ELF;fairy.actor.params=type;fairy.actor.update=EnElf_Update;fairy.actor.world.pos={0,2030,0};EnElf_Init(&fairy.actor,play);
  bool caught=false,restSafe=true;
  for(int i=0;i<70&&!caught;++i){
   f.timeSeconds=1000+form*100+left*10+type+i/90.;f.hands[hand].position={i<20?0.f:(i-20)*.006f,-.5f,-.3f};
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateBottle(f,model);
   auto opening=mmvrgame::BottleMouthForForm(p);Vec3f mouth{};for(int k=0;k<3;++k)(&mouth.x)[k]=opening.x*model.m[0][k]+opening.y*model.m[1][k]+opening.z*model.m[2][k]+model.m[3][k];
   if(i%4==0){fairy.actor.world.pos=i<20?Vec3f{0,2030,0}:mouth;if(i>=20)fairy.actor.world.pos.y+=12;fairy.actor.velocity={};fairy.actor.speed=0;
    EnElf_Update(&fairy.actor,play);caught=fairy.actor.parent==&p->actor;
    if(i<20)restSafe&=!caught&&fairy.actor.update!=nullptr&&gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_BOTTLE;
   }
  }
  if(form||left||type!=FAIRY_TYPE_2)log<<",";log<<"{\"form\":"<<form<<",\"left\":"<<left<<",\"type\":"<<type<<",\"caught\":"<<caught<<",\"restSafe\":"<<restSafe<<",\"inventory\":"<<(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_FAIRY)<<"}";
  EnElf_Destroy(&fairy.actor,play);
 }
 // Native glass is in the translucent pool; opaque bottle-hand extras use a different range.
 mmvr::CameraFrame camera{};camera.active=true;camera.handExtraActive[0]=true;camera.handExtras[0]=mmvr::YawPose(0,30,10,-20);
 mmvr::SetNativeTestCamera(camera);Mtx opa[2]{},xlu[2]{};mmvr::SetHandExtraRange(0,&opa[0],&opa[2]);mmvr::SetHandExtraRange(0,&xlu[0],&xlu[2],1);
 bool glass=true;for(const void* address:{(const void*)&opa[0],(const void*)&xlu[0]}){auto native=mmvr::YawPose(0,1,2,3),result=mmvr::YawPose(0,-999,-999,-999);glass&=mmvr::OverrideModelMatrix(address,result.m,native.m)&&std::abs(result.m[3][0]-31)<.001f&&std::abs(result.m[3][1]-12)<.001f&&std::abs(result.m[3][2]+17)<.001f;}
 mmvr::SetHandExtraRange(0,nullptr,nullptr);mmvr::SetHandExtraRange(0,nullptr,nullptr,1);mmvr::SetNativeTestCamera({});
 log<<"],\"trackedBottleGlass\":"<<glass<<",\"hookReticles\":[";
 for(int left=0;left<2;++left)for(int heading=0;heading<4;++heading){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::HookshotReticle,1);MMVR_PlayerEquipHookshot(play,p);
  int hand=1-left;mmvr::TrackingFrame f{};f.origin.orientation.w=f.head.orientation.w=1;f.hands[hand].orientation.w=f.aims[hand].orientation.w=1;f.handTracked[hand]=f.handValid[hand]=f.aimValid[hand]=true;f.timeSeconds=1300+left*4+heading;f.epoch=1400+left*4+heading;
  float yaw=heading*1.5707963268f;auto view=mmvr::YawPose(yaw,0,2045,0);mmvrgame::RecordTracking(f,view,mmvr::YawPose(0));mmvrgame::UpdateHookshotReticle();auto reticle=mmvrgame::ItemReticlePose();
  mmvr::Matrix muzzle;bool valid=mmvrgame::TrackedMuzzle(play,p,muzzle);float error=0;for(int k=0;k<3;++k)error=std::max(error,std::abs(reticle.m[3][k]-(muzzle.m[3][k]-muzzle.m[2][k]*776)));
  mmvr::GetSettings().Set(mmvr::Setting::HookshotReticle,0);mmvrgame::UpdateHookshotReticle();auto off=mmvrgame::ItemReticlePose();bool hidden=off.m[0][0]==0&&off.m[3][3]==0;
  if(left||heading)log<<",";log<<"{\"left\":"<<left<<",\"heading\":"<<heading<<",\"valid\":"<<valid<<",\"error\":"<<error<<",\"hidden\":"<<hidden<<"}";
  MMVR_PlayerEmptyHands(play,p);
 }
 log<<"]";
 *p=saved;gSaveContext=save;*CONTROLLER1(&play->state)=input;mmvr::GetSettings()=settings;play->colChkCtx=collisions;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
}
