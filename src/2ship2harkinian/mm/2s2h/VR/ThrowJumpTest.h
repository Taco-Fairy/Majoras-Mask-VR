#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
void EnBom_Init(Actor*,PlayState*);void EnBom_Destroy(Actor*,PlayState*);
int Player_ActionHandler_10(Player*,PlayState*);int func_80839770(Player*,PlayState*);
void Player_Action_25(Player*,PlayState*);void Player_Action_29(Player*,PlayState*);
extern Input* sPlayerControlInput;extern FloorType sPlayerFloorType;extern FloorEffect sPlayerFloorEffect;
extern int sPlayerUseHeldItem;extern float sActorMovementScale;
}
static void NativeThrowJumpTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto savedPlayer=*p;auto savedSave=gSaveContext;auto savedInput=*CONTROLLER1(&play->state);
 auto settings=mmvr::GetSettings();auto collisions=play->colChkCtx;
 auto* control=sPlayerControlInput;auto floor=sPlayerFloorType;auto effect=sPlayerFloorEffect;auto roomType=play->roomCtx.curRoom.type;int use=sPlayerUseHeldItem;
 auto prepare=[&](){*p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->heldActor=p->actor.child=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;*CONTROLLER1(&play->state)={};mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();};
 log<<",\"bombFlight\":[";
 for(int left=0;left<2;++left)for(int direction=0;direction<4;++direction){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::ThrowGain,1);mmvr::GetSettings().Set(mmvr::Setting::ThrowMaxSpeed,10);mmvr::GetSettings().Set(mmvr::Setting::BombArcLift,0);mmvr::GetSettings().Set(mmvr::Setting::BombArcAngle,0);
  auto frame=mmvr::TrackingFrame{};frame.head.orientation.w=1;frame.epoch=900+left*4+direction;float originYaw=.6f,viewYaw=direction*1.5707963268f;
  frame.origin.orientation={0,std::sin(originYaw*.5f),0,std::cos(originYaw*.5f)};
  int hand=1-left;frame.hands[hand].orientation.w=frame.aims[hand].orientation.w=1;frame.hands[hand].position={.2f,-.4f,-.3f};frame.handTracked[hand]=frame.handValid[hand]=frame.aimValid[hand]=true;
  // Poses deliberately stand still: the released velocity must come from OpenXR.
  frame.handVelocityValid[hand]=true;frame.handVelocity[hand]={1.3f,2.1f,-2.8f};auto view=mmvr::YawPose(viewYaw,0,2045,0),head=mmvr::YawPose(0);
  EnBom bomb{};bomb.actor.id=ACTOR_EN_BOM;bomb.actor.params=BOMB_TYPE_BODY;bomb.actor.parent=&p->actor;bomb.actor.world.pos={0,2025,0};bomb.actor.update=EnBom_Update;bomb.actor.terminalVelocity=-20;EnBom_Init(&bomb.actor,play);
  p->heldActor=&bomb.actor;p->interactRangeActor=&bomb.actor;p->actor.child=&bomb.actor;p->stateFlags1|=PLAYER_STATE1_CARRYING_ACTOR;
  for(int sample=0;sample<10;++sample){frame.timeSeconds=600+left*8+direction+sample/90.;mmvrgame::RecordTracking(frame,view,head);}
  auto sample=mmvrgame::SampleThrow(play,p);float angle=viewYaw-originYaw;
  float expected[3]={(1.3f*std::cos(angle)-2.8f*std::sin(angle))*40,2.1f*40,(-1.3f*std::sin(angle)-2.8f*std::cos(angle))*40};
  float velocityError=0;for(int k=0;k<3;++k)velocityError=std::max(velocityError,std::abs(sample.velocity[k]-expected[k]));
  // The native release used to sweep from the player's body and keep obsolete wall/push flags.
  bomb.actor.home.rot={1250,1900,1250};bomb.actor.bgCheckFlags=BGCHECKFLAG_WALL|BGCHECKFLAG_CEILING|BGCHECKFLAG_GROUND;
  bomb.actor.colChkInfo.displacement={10,4,-12};
  bool released=mmvrgame::ReleaseThrowable(play,p,sample);auto release=bomb.actor.world.pos;
  bool clean=bomb.actor.bgCheckFlags==0&&bomb.actor.colChkInfo.displacement.x==0;
  EnBom_Update(&bomb.actor,play);
  float length=std::hypot(expected[0],expected[2]);float speed=std::max(0.f,length/30-.08f);
  float flight[3]={expected[0]/length*speed*sActorMovementScale,(expected[1]/30-1.2f)*sActorMovementScale,expected[2]/length*speed*sActorMovementScale};
  float flightError=0;for(int k=0;k<3;++k)flightError=std::max(flightError,std::abs((&bomb.actor.world.pos.x)[k]-(&release.x)[k]-flight[k]));
  float sweepError=std::abs(bomb.actor.prevPos.x-release.x)+std::abs(bomb.actor.prevPos.y-release.y)+std::abs(bomb.actor.prevPos.z-release.z);
  if(left||direction)log<<",";log<<"{\"left\":"<<left<<",\"direction\":"<<direction<<",\"released\":"<<released<<",\"clean\":"<<clean<<",\"velocityError\":"<<velocityError<<",\"flightError\":"<<flightError<<",\"sweepError\":"<<sweepError<<"}";
  EnBom_Destroy(&bomb.actor,play);play->colChkCtx=collisions;
 }
 log<<"],\"lockOnJumps\":[";
 sPlayerControlInput=CONTROLLER1(&play->state);sPlayerFloorType=FLOOR_TYPE_0;sPlayerFloorEffect=FLOOR_EFFECT_0;play->roomCtx.curRoom.type=0;
 const int directions[]={PLAYER_STICK_DIR_NONE,PLAYER_STICK_DIR_FORWARD,PLAYER_STICK_DIR_LEFT,PLAYER_STICK_DIR_RIGHT,PLAYER_STICK_DIR_BACKWARD};
 for(int form:{PLAYER_FORM_HUMAN,PLAYER_FORM_ZORA})for(int i=0;i<5;++i){
  prepare();p->transformation=form;
  if(form==PLAYER_FORM_HUMAN)MMVR_PlayerEquipSword(play,p,ITEM_SWORD_GILDED);
  else {p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;}
  p->stateFlags1|=PLAYER_STATE1_PARALLEL;p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;
  p->controlStickDirections[p->controlStickDataIndex]=directions[i];sPlayerControlInput->press.button=BTN_A;sPlayerControlInput->cur.button=BTN_A|BTN_Z;
  bool handled=Player_ActionHandler_10(p,play);bool jumping=p->actionFunc==Player_Action_25&&p->actor.velocity.y>0;bool noAttack=p->meleeWeaponState==PLAYER_MELEE_WEAPON_STATE_0&&!(p->stateFlags3&PLAYER_STATE3_2);
  auto velocity=p->actor.velocity.y;sPlayerUseHeldItem=true;bool midairBlocked=!func_80839770(p,play)&&p->actor.velocity.y==velocity;
  sPlayerControlInput->press.button=0;sPlayerUseHeldItem=false;p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;p->actor.velocity.y=-1;p->fallDistance=0;p->fallStartHeight=2000;p->actor.floorHeight=2000;
  Player_Action_25(p,play);bool landed=p->actionFunc!=Player_Action_25&&p->actionFunc!=Player_Action_29;
  if(i||form!=PLAYER_FORM_HUMAN)log<<",";log<<"{\"form\":"<<form<<",\"direction\":"<<directions[i]<<",\"handled\":"<<handled<<",\"jumping\":"<<jumping<<",\"noAttack\":"<<noAttack<<",\"midairBlocked\":"<<midairBlocked<<",\"landed\":"<<landed<<"}";
 }
 prepare();MMVR_PlayerEquipSword(play,p,ITEM_SWORD_GILDED);p->actionFunc=Player_Action_29;p->meleeWeaponAnimation=127;p->actor.velocity.y=-2;p->stateFlags3|=PLAYER_STATE3_2;
 Player_Action_29(p,play);bool recovered=p->actionFunc==Player_Action_25&&p->actor.velocity.y==-2&&!(p->stateFlags3&PLAYER_STATE3_2);
 log<<"],\"jumpAttackRecovered\":"<<recovered;
 log<<",\"airborneSwordDamage\":[";
 for(int sword:{ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY}){
  if(sword!=ITEM_SWORD_KOKIRI&&sword!=ITEM_SWORD_RAZOR&&sword!=ITEM_SWORD_GILDED&&sword!=ITEM_SWORD_GREAT_FAIRY)continue;
  prepare();MMVR_PlayerEquipSword(play,p,(ItemId)sword);p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;MMVR_InitSwordDamage(p);int ground=p->meleeWeaponQuads[0].elem.atDmgInfo.damage;
  p->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;p->actor.floorHeight=p->actor.world.pos.y-50;MMVR_InitSwordDamage(p);int airborne=p->meleeWeaponQuads[0].elem.atDmgInfo.damage;
  if(sword!=ITEM_SWORD_KOKIRI)log<<",";log<<"{\"sword\":"<<sword<<",\"ground\":"<<ground<<",\"air\":"<<airborne<<"}";
 }
 log<<"]";
 *p=savedPlayer;gSaveContext=savedSave;*CONTROLLER1(&play->state)=savedInput;mmvr::GetSettings()=settings;play->colChkCtx=collisions;sPlayerControlInput=control;sPlayerFloorType=floor;sPlayerFloorEffect=effect;play->roomCtx.curRoom.type=roomType;sPlayerUseHeldItem=use;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
}
