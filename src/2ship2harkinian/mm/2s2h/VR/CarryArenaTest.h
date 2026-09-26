#pragma once
#include "Carry.h"
#include "Bombchu.h"
#include "FormAim.h"
#include "ItemUse.h"
extern "C" {
void MMVR_PlayerEquipSword(PlayState*,Player*,ItemId);
void Player_DestroyHookshot(Player*);
#include "overlays/actors/ovl_Obj_Tsubo/z_obj_tsubo.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "overlays/actors/ovl_En_Niw/z_en_niw.h"
s32 Player_UpperAction_CarryActor(Player*,PlayState*);
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
void func_809289B4(ObjTsubo*);
void EnIshi_SetupIdle(EnIshi*);
void EnBomChu_WaitForRelease(EnBomChu*,PlayState*);
void Player_Action_Idle(Player*,PlayState*);
Actor* Actor_Delete(ActorContext*,Actor*,PlayState*);
}
extern "C" { char* ResourceMgr_LoadVtxArrayByName(const char*); size_t ResourceMgr_GetVtxArraySizeByName(const char*); }
// Independent support vertex: an outward axis offset has exactly that distance
// from the mesh convex envelope; no production closest-point code is reused.
static bool CarryFixtureSupport(Actor* a,const char* path,int axis,Vec3f& point){
 auto* v=reinterpret_cast<Vtx*>(ResourceMgr_LoadVtxArrayByName(path));auto n=ResourceMgr_GetVtxArraySizeByName(path);
 if(!v||n<4||n>512)return false;
 size_t best=0;for(size_t i=1;i<n;++i)if(v[i].v.ob[axis]>v[best].v.ob[axis])best=i;
 Vec3f local{v[best].v.ob[0]*a->scale.x,v[best].v.ob[1]*a->scale.y,v[best].v.ob[2]*a->scale.z};
 // Fixture actors are explicitly unrotated before these measurements.
 point={a->world.pos.x+local.x,a->world.pos.y+local.y+a->shape.yOffset*a->scale.y,a->world.pos.z+local.z};return true;
}
struct CarryArenaResult {bool rocks=true,pots=true,cuccos=true,tolerance=true,restrictions=true,bombchus=true,reticle=true,obstruction=true;};
static CarryArenaResult NativeCarryArenaTest(PlayState* play,std::ostream& log){
 CarryArenaResult result;auto* p=GET_PLAYER(play);auto player=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto tasks=play->animTaskQueue;auto col=play->colChkCtx;auto ticks=play->gameplayFrames;auto sceneFlags=play->actorCtx.sceneFlags;
 mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::PhysicalCarry,1);
 ObjTsubo* pot=nullptr;for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)if(a->id==ACTOR_OBJ_TSUBO&&a->update&&!a->init){pot=reinterpret_cast<ObjTsubo*>(a);break;}
 auto prepare=[&](){*p=player;gSaveContext=save;p->actor.world.pos={0,0,230};p->actor.prevPos=p->actor.world.pos;p->actor.velocity={};p->actor.speed=p->speedXZ=0;p->actor.shape.rot.y=p->actor.world.rot.y=(s16)0x8000;p->transformation=PLAYER_FORM_HUMAN;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->heldActor=p->actor.child=nullptr;p->actionFunc=Player_Action_Idle;p->currentMask=PLAYER_MASK_NONE;*CONTROLLER1(&play->state)={};mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvrgame::SelectItem(play,-1,ITEM_NONE);};
 mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=24000;
 for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=f.handVelocityValid[h]=true;}
 auto view=mmvr::YawPose(0,0,45,230),head=mmvr::YawPose(0);
 // Raw tracked grip, not SampleHandThrow's already-transformed carried root.
 auto rawGrip=[&](int h){auto m=mmvr::PoseMatrix(f.hands[h]);m.m[3][0]=(m.m[3][0]-head.m[3][0])*40;m.m[3][1]*=40;m.m[3][2]=(m.m[3][2]-head.m[3][2])*40;return mmvr::Multiply(m,view);};
 auto sample=[&](double t){f.timeSeconds=t;mmvrgame::RecordFormTracking(f,view,head);mmvrgame::RecordTracking(f,view,head);mmvrgame::ProcessItemTrigger(play);MMVR_UpdateHeldItem(play,p);};
 if(!pot)result.pots=false;
 else {auto original=*pot;
  const char* potPaths[]={"__OTR__objects/gameplay_dangeon_keep/gameplay_dangeon_keepVtx_017AE0","__OTR__objects/object_racetsubo/object_racetsuboVtx_000000","__OTR__objects/object_tsubo/object_tsuboVtx_001400","__OTR__objects/gameplay_dangeon_keep/gameplay_dangeon_keepVtx_017AE0"};
  pot->actor.shape.rot={};Vec3f support{};
  result.tolerance=CarryFixtureSupport(&pot->actor,potPaths[OBJ_TSUBO_GET_TYPE(&pot->actor)],1,support);
  if(result.tolerance){auto insidePoint=support,outsidePoint=support;insidePoint.y+=9;outsidePoint.y+=11;
   result.tolerance=std::abs(mmvrgame::CarryGrabSeparation(&pot->actor,insidePoint)-9)<.02f&&mmvrgame::CarryGrabSeparation(&pot->actor,outsidePoint)>10;}
  *pot=original;
  mmvr::GetSettings().Set(mmvr::Setting::CarryGrabDistance,.25f);
  for(int form:{PLAYER_FORM_HUMAN,PLAYER_FORM_GORON,PLAYER_FORM_ZORA,PLAYER_FORM_FIERCE_DEITY})for(int left=0;left<2;++left)for(int thrown=0;thrown<2;++thrown)for(int grabHand=0;grabHand<2;++grabHand){
   prepare();p->transformation=form;gSaveContext.save.playerForm=form;mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);*pot=original;pot->actor.world.pos={0,0,200};pot->actor.prevPos=pot->actor.world.pos;pot->actor.parent=nullptr;pot->actor.xzDistToPlayer=30;pot->actor.playerHeightRel=0;pot->actor.yawTowardsPlayer=0;pot->actor.bgCheckFlags=BGCHECKFLAG_GROUND;pot->cylinderCollider.base.acFlags&=~AC_HIT;func_809289B4(pot);
   mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int hand=grabHand;
   // Approach from the exterior side for both drop and throw. An interior
   // starting grip would ask collision resolution to lift the pot from below.
   for(int h=0;h<2;++h){f.triggers[h]=0;f.hands[h].position={(pot->cylinderCollider.dim.radius+9.f)/40,(pot->cylinderCollider.dim.height*.5f-45)/40,-.75f};f.hands[h].orientation={0,0,0,1};f.handVelocity[h]={};}
   head.m[3][1]=f.head.position.y=-.35f;f.epoch++;double t=2000+left*10+thrown*2;
   pot->actor.shape.rot={};sample(t);sample(t+.01);
   // The actor itself offers GI_NONE, preserving its idle/held state restrictions.
   pot->actor.update(&pot->actor,play);f.triggers[hand]=1;sample(t+.02);
   auto grabSample=mmvrgame::SampleThrow(play,p);
   bool grabbed=p->heldActor==&pot->actor&&mmvrgame::CarriedObject(p);
   log<<"carry-reach valid="<<grabSample.valid<<" radius="<<pot->cylinderCollider.dim.radius<<" height="<<pot->cylinderCollider.dim.height<<"\n";
   bool deferred=!mmvrgame::CarryReady(play,p);
   pot->actor.update(&pot->actor,play);++play->gameplayFrames;
   // The exterior contact does not require lifting beyond the player's
   // physical reach. Settle the attachment and check floor protection there.
   sample(t+.025);bool noPenetration=pot->actor.world.pos.y>=-.02f;
   mmvr::Matrix firstPose,secondPose;
   bool anchored=grabbed&&noPenetration&&mmvrgame::CarryPose(play,p,rawGrip(hand),firstPose);
   float before=pot->actor.world.pos.y;f.hands[hand].position.y+=.5f;sample(t+.03);
   anchored&=mmvrgame::CarryPose(play,p,rawGrip(hand),secondPose);
   bool follows=grabbed&&std::abs(pot->actor.world.pos.y-before-20)<.02f;
   for(int r=0;r<3;++r)for(int c=0;c<3;++c)anchored&=std::abs(firstPose.m[r][c]-secondPose.m[r][c])<.001f;
   log<<"pot-contact floorSafe="<<noPenetration<<" anchored="<<anchored<<" before="<<before<<" after="<<pot->actor.world.pos.y<<"\n";
   result.pots&=anchored;
   f.handVelocity[hand]=thrown?XrVector3f{2,3,-1}:XrVector3f{};
   f.timeSeconds=t+.04;mmvrgame::RecordTracking(f,view,head);auto expected=mmvrgame::SampleThrow(play,p);
   f.triggers[hand]=0;sample(t+.05);
   bool released=grabbed&&!p->heldActor&&!pot->actor.parent;
   bool velocity=(thrown?(pot->actor.velocity.y>0&&pot->actor.velocity.y>=expected.velocity[1]/30-.01f):std::abs(pot->actor.velocity.y)<.01f)&&std::abs(pot->actor.speed-std::hypot(expected.velocity[0],expected.velocity[2])/30)<.01f;
   bool broke=false;
   if(released)for(int i=0;i<160&&pot->actor.update;++i){play->colChkCtx.colATCount=play->colChkCtx.colACCount=play->colChkCtx.colOCCount=0;pot->actor.update(&pot->actor,play);broke=pot->actor.update==nullptr;}
   result.pots&=grabbed&&deferred&&follows&&released&&velocity&&broke;
   log<<"carry-pot form="<<form<<" left="<<left<<" hand="<<grabHand<<" throw="<<thrown<<" grab="<<grabbed<<" deferred="<<deferred<<" follows="<<follows<<" released="<<released<<" velocity="<<velocity<<" broke="<<broke<<"\n";
  }
  for(int left=0;left<2;++left)for(int bow=0;bow<3;++bow)for(int stow=0;stow<2;++stow){
   prepare();*pot=original;pot->actor.world.pos={0,0,200};pot->actor.prevPos=pot->actor.world.pos;pot->actor.parent=nullptr;pot->actor.xzDistToPlayer=30;pot->actor.playerHeightRel=0;pot->actor.bgCheckFlags=BGCHECKFLAG_GROUND;func_809289B4(pot);
   mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int freeHand=bow==1?1-left:left;
   if(bow==2){gSaveContext.save.saveInfo.inventory.items[SLOT_HOOKSHOT]=ITEM_HOOKSHOT;mmvrgame::SelectItem(play,SLOT_HOOKSHOT,ITEM_HOOKSHOT);if(p->heldActor&&p->heldActor->init){p->heldActor->init(p->heldActor,play);p->heldActor->init=nullptr;}}
   else if(bow==1){gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]=ITEM_BOW;mmvrgame::SelectItem(play,SLOT_BOW,ITEM_BOW);}
   else MMVR_PlayerEquipSword(play,p,ITEM_SWORD_KOKIRI);
   f.epoch++;head.m[3][1]=f.head.position.y=0;double t=2300+left*10+bow*2+stow;
   for(int h=0;h<2;++h){f.triggers[h]=0;f.hands[h].position={0,(pot->cylinderCollider.dim.height*.5f-45)/40,-.75f};f.hands[h].orientation={0,0,0,1};f.handVelocity[h]={};}
   sample(t);sample(t+.01);pot->actor.update(&pot->actor,play);f.triggers[freeHand]=1;sample(t+.02);
   bool grab=p->heldActor==&pot->actor&&mmvrgame::CarryHand(p)==freeHand;
   pot->actor.update(&pot->actor,play);++play->gameplayFrames;
   if(stow)mmvrgame::StowItem(play);else{f.triggers[freeHand]=0;sample(t+.03);}
   bool released=!mmvrgame::HeldThrowable(p)&&!pot->actor.parent;
   bool equipment=stow?p->heldItemAction==PLAYER_IA_NONE:bow==2?MMVR_IndependentHookshot(p)&&p->heldActor&&p->heldActor->id==ACTOR_ARMS_HOOK:bow==1?MMVR_IndependentBow(p)!=0:MMVR_IndependentSword(p)!=0;
   result.pots&=grab&&released&&equipment;
   log<<"armed-pot left="<<left<<" bow="<<bow<<" stow="<<stow<<" grabbed="<<grab<<" released="<<released<<" equipment="<<equipment<<"\n";
   if(bow==2)Player_DestroyHookshot(p);
  }
  *pot=original;
 }
 EnNiw* bird=nullptr;for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)if(a->id==ACTOR_EN_NIW&&a->update&&!a->init){bird=reinterpret_cast<EnNiw*>(a);break;}
 if(!bird)result.cuccos=false;else{auto original=*bird;
  for(int hand=0;hand<2;++hand){
   prepare();*bird=original;bird->actor.world.pos={0,0,200};bird->actor.prevPos=bird->actor.home.pos=bird->actor.world.pos;bird->actor.parent=nullptr;bird->actor.xzDistToPlayer=30;bird->actor.playerHeightRel=0;bird->actor.yawTowardsPlayer=0;bird->actor.bgCheckFlags=BGCHECKFLAG_GROUND;
   head.m[3][1]=f.head.position.y=0;f.epoch++;double t=2150+hand;
   for(int h=0;h<2;++h){f.triggers[h]=0;f.hands[h].position={0,(bird->collider.dim.yShift+bird->collider.dim.height*.5f-45)/40,-.75f};f.hands[h].orientation={0,0,0,1};f.handVelocity[h]={};}
   sample(t);sample(t+.01);bird->actor.update(&bird->actor,play);bird->actor.shape.rot=bird->actor.world.rot={};f.triggers[hand]=1;sample(t+.02);
   bool grabbed=p->heldActor==&bird->actor&&mmvrgame::CarriedObject(p)&&mmvrgame::CarryHand(p)==hand;
   bird->actor.update(&bird->actor,play);++play->gameplayFrames;
   p->actor.velocity.y=-1;p->actor.terminalVelocity=-20;p->actor.gravity=-1.2f;
   if(grabbed)Player_UpperAction_CarryActor(p,play);
   bool glide=grabbed&&p->actor.terminalVelocity==-2&&p->actor.gravity==-.5f;
   // Neutral pickup faces away (-Z); thereafter preserve the pickup
   // orientation relative to the hand, including physical roll/pitch.
   mmvr::Matrix neutralPose,turnedPose;
   bool orientation=mmvrgame::CarryPose(play,p,rawGrip(hand),neutralPose);
   orientation&=std::abs(neutralPose.m[2][0])<.001f&&neutralPose.m[2][2]>.999f;
   f.hands[hand].orientation={1,0,0,0};f.handVelocity[hand]={1,3,-2};sample(t+.03);
   orientation&=mmvrgame::CarryPose(play,p,rawGrip(hand),turnedPose);
   for(int r=0;r<3;++r)for(int c=0;c<3;++c){
    // 180 degrees about X leaves X unchanged and reverses Y/Z.
    float expected=neutralPose.m[r][c]*(c==0?1.f:-1.f);
    orientation&=std::abs(turnedPose.m[r][c]-expected)<.001f;
   }
   f.triggers[hand]=0;sample(t+.04);
   result.cuccos&=orientation;
   log<<"cucco-orientation hand="<<hand<<" contactRotation="<<orientation<<"\n";
   bool released=!p->heldActor&&!bird->actor.parent&&bird->actor.velocity.y>0;
   result.cuccos&=grabbed&&glide&&released;
   log<<"cucco hand="<<hand<<" grab="<<grabbed<<" glide="<<glide<<" release="<<released<<"\n";
  }*bird=original;
 }
 head.m[3][1]=f.head.position.y=0;prepare();Actor heavy{};heavy.id=ACTOR_EN_ISHI;heavy.params=1;heavy.update=[](Actor*,PlayState*){};result.restrictions=!MMVR_AttachCarryActor(play,p,&heavy);
 for(int form=0;form<PLAYER_FORM_MAX;++form)for(int kind=0;kind<2;++kind){prepare();p->transformation=form;heavy.params=kind;heavy.parent=nullptr;bool attached=MMVR_AttachCarryActor(play,p,&heavy)!=0;bool allowed=form!=PLAYER_FORM_DEKU&&(!kind||form==PLAYER_FORM_GORON);result.restrictions&=attached==allowed;if(attached)MMVR_NativeThrow(play,p);heavy.parent=nullptr;}
 prepare();p->heldItemAction=PLAYER_IA_BOTTLE_EMPTY;p->itemAction=PLAYER_IA_BOTTLE_EMPTY;result.restrictions&=!mmvrgame::TryGrabCarry(play,p);
 EnIshi* rock=nullptr;for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)if(a->id==ACTOR_EN_ISHI&&(a->params&1)&&a->update&&!a->init){rock=reinterpret_cast<EnIshi*>(a);break;}
 if(!rock)result.rocks=false;else{auto original=*rock;
  for(int left=0;left<2;++left){prepare();p->transformation=PLAYER_FORM_GORON;gSaveContext.save.playerForm=PLAYER_FORM_GORON;*rock=original;rock->actor.world.pos={0,0,155};rock->actor.prevPos=rock->actor.home.pos=rock->actor.world.pos;rock->actor.parent=nullptr;rock->actor.xzDistToPlayer=75;rock->actor.playerHeightRel=0;rock->actor.bgCheckFlags=BGCHECKFLAG_GROUND;rock->collider.base.acFlags&=~AC_HIT;Collider_UpdateCylinder(&rock->actor,&rock->collider);EnIshi_SetupIdle(rock);Flags_UnsetSwitch(play,46);
   mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int hand=1-left;f.epoch++;double t=2200+left;
   for(int h=0;h<2;++h){f.triggers[h]=0;f.hands[h].position={0,0,-.5f};f.hands[h].orientation={0,0,0,1};f.handVelocity[h]={};}
   rock->actor.shape.rot={};sample(t);Vec3f rockContact{};
   bool contactKnown=CarryFixtureSupport(&rock->actor,"__OTR__objects/gameplay_field_keep/gameplay_field_keepVtx_006028",2,rockContact);
   auto gripSample=mmvrgame::SampleHandThrow(play,p,hand);auto calibrated=mmvrgame::CarryPalmPose(gripSample.pose,hand);
   if(contactKnown)for(int c=0;c<3;++c)(&f.hands[hand].position.x)[c]+=((&rockContact.x)[c]-calibrated.m[3][c])/40.f;
   sample(t+.01);log<<"rock-contact known="<<contactKnown<<" separation="<<mmvrgame::CarryGrabSeparation(&rock->actor,rockContact)<<"\n";
   rock->actor.update(&rock->actor,play);f.triggers[hand]=1;sample(t+.02);bool grabbed=p->heldActor==&rock->actor;
   rock->actor.update(&rock->actor,play);++play->gameplayFrames;sample(t+.025);float y=rock->actor.world.pos.y;f.hands[hand].position.y+=.3f;sample(t+.03);bool follows=grabbed&&std::abs(rock->actor.world.pos.y-y-12)<.02f;
   auto palm=mmvr::PoseMatrix({{.38268343f,0,0,.92387953f},{0,180,155}});mmvr::Matrix rotated;
   bool rotates=mmvrgame::CarryPose(play,p,palm,rotated);
   for(int row=0;row<3;++row)for(int col=0;col<3;++col)rotates&=std::abs(rotated.m[row][col]-palm.m[row][col])<.0001f;
   // Contact point, not the old fixed center offset, must follow the palm.
   // Verify a translated grip moves the complete attachment equally, without
   // forcing a cylinder-center anchor or prescribing a new pickup orientation.
   auto shiftedPalm=palm;shiftedPalm.m[3][1]+=10;mmvr::Matrix shifted;
   rotates&=mmvrgame::CarryPose(play,p,shiftedPalm,shifted);
   rotates&=std::abs(shifted.m[3][1]-rotated.m[3][1]-10)<.02f&&contactKnown;
   result.rocks&=rotates;
   f.handVelocity[hand]={2,3,-1};f.triggers[hand]=0;sample(t+.04);bool released=grabbed&&!p->heldActor&&!rock->actor.parent;
   auto releasePos=rock->actor.world.pos;bool rises=false;
   bool broke=false;if(released)for(int tick=0;tick<150&&rock->actor.update;++tick){CollisionCheck_ClearContext(play,&play->colChkCtx);rock->actor.update(&rock->actor,play);if(tick==0)rises=rock->actor.world.pos.y>releasePos.y&&rock->actor.world.pos.x>releasePos.x;broke=!rock->actor.update;}
   bool once=Flags_GetSwitch(play,46);result.rocks&=grabbed&&follows&&released&&rises&&broke&&once;
   log<<"carry-rock left="<<left<<" grabbed="<<grabbed<<" follows="<<follows<<" released="<<released<<" rises="<<rises<<" broke="<<broke<<" switch="<<once<<"\n";
  }*rock=original;
 }
 for(int direction=0;direction<4;++direction){
  prepare();int left=direction%2;mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);int hand=1-left;
  gSaveContext.save.saveInfo.inventory.items[SLOT_BOMBCHU]=ITEM_BOMBCHU;AMMO(ITEM_BOMBCHU)=10;BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOMBCHU;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOMBCHU;
  mmvrgame::SelectItem(play,SLOT_BOMBCHU,ITEM_BOMBCHU);float yaw=direction*1.57079632679f;f.head.orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};f.triggers[0]=f.triggers[1]=0;f.epoch++;
  double t=2100+direction;sample(t);sample(t+.01);mmvr::CameraFrame preview;mmvrgame::UpdateBombchuReticle(preview);
  bool visible=preview.itemReticle.m[3][3]!=0;auto oldPause=play->pauseCtx.state;play->pauseCtx.state=PAUSE_STATE_MAIN;mmvrgame::UpdateBombchuReticle(preview);bool pauseHidden=preview.itemReticle.m[3][3]==0;play->pauseCtx.state=oldPause;
  mmvr::Matrix placement;bool valid=mmvrgame::BombchuPlacement(play,p,mmvrgame::FormHeadPose(),placement);
  bool idle=AMMO(ITEM_BOMBCHU)==10&&!p->heldActor;
  f.triggers[hand]=1;sample(t+.02);Actor* actor=nullptr;
  for(auto* a=play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first;a;a=a->next)if(a->id==ACTOR_EN_BOM_CHU&&a->update){actor=a;break;}
  bool launched=false,held=actor&&p->heldActor==actor&&actor->parent==&p->actor,stock=AMMO(ITEM_BOMBCHU)==9;
  if(actor){if(actor->init){actor->init(actor,play);actor->init=nullptr;}auto* chu=reinterpret_cast<EnBomChu*>(actor);EnBomChu_WaitForRelease(chu,play);
   held&=!chu->isMoving&&actor->parent==&p->actor;sample(t+.03);stock&=AMMO(ITEM_BOMBCHU)==9;
   f.triggers[hand]=0;sample(t+.04);EnBomChu_WaitForRelease(chu,play);
   launched=chu->isMoving&&!actor->parent&&!p->heldActor&&std::abs(chu->axisForwards.x-placement.m[2][0])<.01f&&std::abs(chu->axisForwards.z-placement.m[2][2])<.01f&&std::hypot(actor->world.pos.x-placement.m[3][0],actor->world.pos.z-placement.m[3][2])<.01f;
   auto start=actor->world.pos;actor->update(actor,play);
   launched&=(actor->world.pos.x-start.x)*placement.m[2][0]+(actor->world.pos.z-start.z)*placement.m[2][2]>0;
   Actor_Delete(&play->actorCtx,actor,play);
  }
  // Holding the trigger must not repeat placements.
  sample(t+.05);stock&=AMMO(ITEM_BOMBCHU)==9;
  result.bombchus&=idle&&valid&&held&&launched&&stock;result.reticle&=visible&&pauseHidden;
  log<<"bombchu-case heading="<<direction<<" left="<<left<<" idle="<<idle<<" valid="<<valid<<" launched="<<launched<<" stock="<<stock<<" reticle="<<visible<<" pause="<<pauseHidden<<"\n";
 }
 prepare();p->actor.world.pos={630,0,-630};auto wallHead=mmvr::YawPose(0,630,45,-630);mmvr::Matrix invalid;
 result.obstruction=!mmvrgame::BombchuPlacement(play,p,wallHead,invalid);
 mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();mmvrgame::ClearItemSelection();*p=player;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;play->animTaskQueue=tasks;play->colChkCtx=col;play->gameplayFrames=ticks;play->actorCtx.sceneFlags=sceneFlags;
 return result;
}
