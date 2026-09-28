#pragma once
#include "ChestReceiptCatalog.inc"
extern "C" {
#include "overlays/actors/ovl_En_Box/z_en_box.h"
void EnBox_WaitOpen(EnBox*,PlayState*);
void EnBox_Open(EnBox*,PlayState*);
}
static mmvr::Pad NativeChestReceipt(PlayState* play,unsigned tick) {
 static const int index=std::atoi(std::getenv("MMVR_CHEST_RECEIPT"));
 static int phase=0,age=0,settled=0;static int initialFairies=0;static unsigned commits=0;static bool opened=false,previousA=false;
 static std::ofstream log("native-chest-receipt.log");mmvr::Pad pad;pad.active=true;
 auto finish=[&](const char* status,const char* why){log<<status<<" chest="<<index<<" opened="<<opened<<" commits="<<commits<<" reason="<<why<<'\n'<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
 if(index<0||index>=ARRAY_COUNT(nativeChestRecipes)){finish("FAIL","invalid-index");return pad;}
 static int recipeIndex=index;static bool needsZora=false;
 const auto& r=nativeChestRecipes[recipeIndex];
 if(tick>1500){log<<"final phase="<<phase<<" scene="<<play->sceneId<<" room="<<int(play->roomCtx.curRoom.num)<<" cs="<<int(GET_PLAYER(play)->csAction)<<" mask="<<int(Player_GetMask(play))<<" transition="<<play->transitionMode<<" roomStatus="<<int(play->roomCtx.status)<<'\n';finish("BLOCKED","scene-chest-or-receipt-prerequisite");return pad;}
 if(phase==0){
  if(tick<40)return pad;
  // The frozen-lake chest is intentionally inaccessible until spring.
  if(r.scene==SCENE_17SETUGEN){
   for(int i=0;i<ARRAY_COUNT(nativeChestRecipes);++i){
    const auto& other=nativeChestRecipes[i];
    if(other.scene==SCENE_17SETUGEN2&&other.params==r.params){
     log<<"frozen-lake checkpoint-to-spring recipe="<<i<<" thaw-quest-not-certified\n"<<std::flush;
     recipeIndex=i;return pad;
    }
   }
  }
  gSaveContext.save.saveInfo.permanentSceneFlags[r.scene]={};gSaveContext.cycleSceneFlags[r.scene]={};
  // Receipt-only checkpoint: load the authored room AFTER its combat prerequisite.
  // Setting this after actor initialization leaves enemies alive and invalidates
  // the approach. Never set the chest/result flags that this check must earn.
  gSaveContext.cycleSceneFlags[r.scene].clearedRoom=1u<<r.room;
  gSaveContext.save.saveInfo.permanentSceneFlags[r.scene].clearedRoom=1u<<r.room;
  // Grotto 5's third chest is inside bombable rock switch 4 (params 0x104).
  // Use the native destroyed-rock checkpoint before its actor initializes.
  if(r.scene==SCENE_KAKUSIANA&&r.room==5&&r.params==20646){
   gSaveContext.cycleSceneFlags[r.scene].switch0|=1u<<4;
   gSaveContext.save.saveInfo.permanentSceneFlags[r.scene].switch0|=1u<<4;
  }
  // Native post-obstacle checkpoints; do not set treasure or reward flags.
  if(r.scene==SCENE_KAKUSIANA&&r.room==14&&r.params==20610){
   gSaveContext.cycleSceneFlags[r.scene].switch0|=1u<<24;
   gSaveContext.save.saveInfo.permanentSceneFlags[r.scene].switch0|=1u<<24;
  }
  if(r.scene==SCENE_BOTI&&r.room==1&&r.params==3968){
   gSaveContext.cycleSceneFlags[r.scene].switch0|=1u<<11;
   gSaveContext.save.saveInfo.permanentSceneFlags[r.scene].switch0|=1u<<11;
  }
  // These authored chests are on the lake/seabed. Human Link floats away
  // before native opening can begin; load the legitimate underwater form.
  const bool underwater=(r.scene==SCENE_00KEIKOKU&&r.params==20608)||
    r.scene==SCENE_17SETUGEN2||(r.scene==SCENE_31MISAKI&&r.params==20640);
  if(r.scene==SCENE_F41){
   gSaveContext.cycleSceneFlags[SCENE_F40].switch0|=1u<<20;
   gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_F40].switch0|=1u<<20;
   gSaveContext.cycleSceneFlags[SCENE_F41].switch0|=1u<<20;
   gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_F41].switch0|=1u<<20;
  }
  // Receipt checkpoint after the native beehive escape. Aveil explicitly
  // detects Stone Mask, so possession alone is not this room's prerequisite.
  if(r.scene==SCENE_PIRATE){SET_WEEKEVENTREG(WEEKEVENTREG_83_02);CLEAR_WEEKEVENTREG(WEEKEVENTREG_80_08);}
  gSaveContext.save.playerForm=(underwater||needsZora)?PLAYER_FORM_ZORA:PLAYER_FORM_HUMAN;
  // Do not wear the Great Fairy Mask: it attracts unrelated room fairies
  // and would contaminate the exactly-one receipt assertion.
  gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_STONE]=ITEM_MASK_STONE;
  gSaveContext.save.equippedMask=r.scene==SCENE_PIRATE?PLAYER_MASK_STONE:PLAYER_MASK_NONE;
  // Native third-person mask ownership requires a C-button assignment.
  // Without it Player removes the mask automatically during the checkpoint.
  if(gSaveContext.save.equippedMask!=PLAYER_MASK_NONE){
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_LEFT)=ITEM_MASK_STONE;
   C_SLOT_EQUIP(0,EQUIP_SLOT_C_LEFT)=SLOT_MASK_STONE;
   gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT]=BTN_ENABLED;
  }
  GET_PLAYER(play)->currentMask=gSaveContext.save.equippedMask;
  initialFairies=0;for(auto count:gSaveContext.save.saveInfo.inventory.strayFairies)initialFairies+=count;
  gSaveContext.save.cutsceneIndex=gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;
  gSaveContext.save.day=gSaveContext.save.eventDayCount=1;gSaveContext.save.time=CLOCK_TIME(12,0);
  if(r.scene==SCENE_CLOCKTOWER&&r.params==20641)gSaveContext.save.day=gSaveContext.save.eventDayCount=3;
  if(r.scene==SCENE_F41||r.scene==SCENE_PIRATE||r.scene==SCENE_OPENINGDAN){
   // Receipt-only native respawn checkpoint. These default entrances start
   // unrelated scripted captures/intro actions instead of free gameplay.
   auto& respawn=gSaveContext.respawn[RESPAWN_MODE_DOWN];respawn={};
   respawn.entrance=r.entrance;respawn.roomIndex=r.room;
   respawn.pos={r.x,r.y+50.f,r.z};respawn.playerParams=PLAYER_PARAMS(0xFF,PLAYER_START_MODE_B);
   gSaveContext.respawnFlag=1;
   log<<"native receipt respawn checkpoint; default entrance cutscene not certified\n"<<std::flush;
  }
  play->nextEntrance=r.entrance;play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
  phase=1;return pad;
 }
 if(play->sceneId!=r.scene||play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF||play->roomCtx.status)return pad;
 if(phase==1){
  // Finish the native entrance in its original room before changing rooms.
  // Moving rooms during a door/arrival action strands its animation state.
  if(++age<60||Player_InCsMode(play)){
   if(tick%100==0){auto* player=GET_PLAYER(play);log<<"arrival tick="<<tick<<" scene="<<play->sceneId<<" room="<<int(play->roomCtx.curRoom.num)<<" csAction="<<int(player->csAction)<<" flags="<<player->stateFlags1<<" msg="<<int(play->msgCtx.msgMode)<<" position="<<player->actor.world.pos.x<<","<<player->actor.world.pos.y<<","<<player->actor.world.pos.z<<" spawnParams="<<(play->linkActorEntry?play->linkActorEntry->params:0)<<'\n'<<std::flush;}
   if(play->msgCtx.msgLength&&tick%8==0)pad.buttons=BTN_A;
   return pad;
  }
  age=0;
  if(play->roomCtx.curRoom.num!=r.room){if(Room_RequestNewRoom(play,&play->roomCtx,r.room))phase=2;return pad;}
  phase=3;
 }
 if(phase==2){Room_FinishRoomChange(play,&play->roomCtx);phase=3;}
 ++age;
 if(tick%100==0&&phase<5)log<<"setup tick="<<tick<<" phase="<<phase<<" scene="<<play->sceneId<<" room="<<play->roomCtx.curRoom.num<<" cs="<<Player_InCsMode(play)<<" playerFlags="<<GET_PLAYER(play)->stateFlags1<<'\n'<<std::flush;
 EnBox* chest=nullptr;
 for(Actor* a=play->actorCtx.actorLists[ACTORCAT_CHEST].first;a;a=a->next)
  if(a->id==ACTOR_EN_BOX&&a->update&&a->room==r.room&&static_cast<u16>(a->params)==static_cast<u16>(r.params)&&std::fabs(a->home.pos.x-r.x)<1&&std::fabs(a->home.pos.y-r.y)<1&&std::fabs(a->home.pos.z-r.z)<1){chest=reinterpret_cast<EnBox*>(a);break;}
 if(!chest){if(age>120)finish("BLOCKED","authored-chest-absent");return pad;}
 // The actor can exist while its object is still loading; Init rotates the
 // chest 180 degrees. Never establish an approach from its pre-init transform.
 if(chest->dyna.actor.init)return pad;
 // A ceiling chest is the same treasure in the opposite temple orientation.
 // This fixture certifies receipt after inversion, not the inversion puzzle.
 if(phase==3&&(chest->dyna.actor.shape.rot.x==0x7FFF)&&
    (r.scene==SCENE_INISIE_N||r.scene==SCENE_INISIE_R)){
  const int destination=r.scene==SCENE_INISIE_N?SCENE_INISIE_R:SCENE_INISIE_N;
  for(int i=0;i<ARRAY_COUNT(nativeChestRecipes);++i){
   const auto& other=nativeChestRecipes[i];
   if(other.scene==destination&&other.room==r.room&&other.params==r.params){
    log<<"ceiling-chest checkpoint-to-upright recipe="<<i<<" inversion-puzzle-not-certified\n"<<std::flush;
    recipeIndex=i;phase=0;age=0;return pad;
   }
  }
  finish("BLOCKED","ceiling-chest-upright-counterpart-missing");return pad;
 }
 auto* p=GET_PLAYER(play);
 if(phase==3&&!needsZora&&p->transformation!=PLAYER_FORM_ZORA){
  float surface=0;WaterBox* water=nullptr;
  const auto& pos=chest->dyna.actor.world.pos;
  if(WaterBox_GetSurface1(play,&play->colCtx,pos.x,pos.z,&surface,&water)&&surface>pos.y+20){
   log<<"underwater checkpoint-to-Zora surface="<<surface<<" chestY="<<pos.y<<'\n'<<std::flush;
   needsZora=true;phase=0;age=0;return pad;
  }
 }
 static bool hookRegistered=false;
 if(!hookRegistered){GameInteractor::Instance->RegisterGameHook<GameInteractor::OnItemGive>([](u8){++commits;});hookRegistered=true;}
 if(chest->getItemId==GI_NONE){
  // Authored empty decorative chests have no receipt; Init opens them natively.
  if(chest->actionFunc==EnBox_Open&&chest->alpha==255&&commits==0&&age>=100)
   finish("PASS","native-empty-chest-stable-no-reward");
  return pad;
 }
 if(phase==3){
  // Only unlock prerequisites, never the treasure/receipt result under test.
  Flags_SetClear(play,r.room);Flags_SetClearTemp(play,r.room);
  // The receipt checkpoint includes extinguishing the authored fire ring.
  // Native switch handling lowers it; never remove collision or grant damage immunity.
  for(int category=0;category<ACTORCAT_MAX;++category)
   for(Actor* actor=play->actorCtx.actorLists[category].first;actor;actor=actor->next)
    if(actor->id==ACTOR_OBJ_FIRESHIELD&&actor->room==r.room&&
       std::fabs(actor->world.pos.x-chest->dyna.actor.world.pos.x)<150&&
       std::fabs(actor->world.pos.z-chest->dyna.actor.world.pos.z)<150&&
       std::fabs(actor->world.pos.y-chest->dyna.actor.world.pos.y)<100){
     Flags_SetSwitch(play,actor->params&0x7F);
     log<<"native fire-ring prerequisite switch="<<(actor->params&0x7F)<<'\n'<<std::flush;
    }
  if(chest->switchFlag<128)Flags_SetSwitch(play,chest->switchFlag);
  phase=4;
 }
 if(phase==4){
  if(tick%100==0)log<<"chest-ready wait="<<(chest->actionFunc==EnBox_WaitOpen)<<" open="<<(chest->actionFunc==EnBox_Open)<<" alpha="<<int(chest->alpha)<<" signal="<<chest->unk_1EC<<" switch="<<int(chest->switchFlag)<<" item="<<chest->getItemId<<'\n'<<std::flush;
  // Falling/appearing chests must finish their native prerequisite animation
  // before the approach checkpoint is established.
  if(chest->actionFunc!=EnBox_WaitOpen||age<100||Player_InCsMode(play))return pad;
  auto pos=chest->dyna.actor.world.pos;const auto yaw=chest->dyna.actor.shape.rot.y;
  pos.x-=Math_SinS(yaw)*30;pos.z-=Math_CosS(yaw)*30;
  p->actor.world.pos=p->actor.prevPos=p->actor.home.pos=pos;
  p->actor.velocity={};p->actor.speed=p->speedXZ=0;
  p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=yaw;
  MMVR_CameraCoordinateBoundary(play);phase=5;
 }
 if(phase<5)return pad;
 mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
 // Match the synthetic headset to the authored chest, for the native visibility gate.
 static float headYaw=0;
 const s16 toward=static_cast<s16>(chest->dyna.actor.yawTowardsPlayer+0x8000);
 headYaw+=static_cast<s16>(toward-MMVR_InputYaw(0))*(3.14159265358979323846f/32768.f);
 mmvr::TrackingFrame tracking{};tracking.head.orientation.y=std::sin(headYaw*.5f);tracking.head.orientation.w=std::cos(headYaw*.5f);
 tracking.origin.orientation.w=1;tracking.epoch=71001;tracking.timeSeconds=tick/20.;
 mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(tracking));
 Vec3f local{};Actor_WorldToActorCoords(&chest->dyna.actor,&local,&p->actor.world.pos);
 const bool nativeOffer=p->interactRangeActor==&chest->dyna.actor&&p->getItemId<GI_NONE;
 const bool atOpenPosition=local.z>-50&&local.z<0&&std::fabs(local.y)<10&&std::fabs(local.x)<20&&Player_IsFacingActor(&chest->dyna.actor,0x3000,play);
 if(!opened&&p->transformation==PLAYER_FORM_ZORA&&
    (p->stateFlags1&PLAYER_STATE1_8000000)&&p->currentBoots!=PLAYER_BOOTS_ZORA_UNDERWATER){
  if(tick%8==0)pad.buttons=BTN_B; // Native sink/walk input, not forced physics.
 }else if(!previousA&&((nativeOffer&&!opened&&
    (!(p->stateFlags1&PLAYER_STATE1_8000000)||(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND)))||play->msgCtx.msgLength))pad.buttons=BTN_A;
 previousA=(pad.buttons&BTN_A)!=0;
 opened|=Flags_GetTreasure(play,ENBOX_GET_CHEST_FLAG(&chest->dyna.actor))!=0;
 const bool fairy=chest->getItemId==GI_STRAY_FAIRY;
 int fairyDelta=-initialFairies;for(auto count:gSaveContext.save.saveInfo.inventory.strayFairies)fairyDelta+=count;
 const bool received=fairy?(fairyDelta==1&&commits==0):commits==1;
 if(opened&&received&&play->msgCtx.msgMode==MSGMODE_NONE&&!(p->stateFlags1&(PLAYER_STATE1_400|PLAYER_STATE1_CARRYING_ACTOR|PLAYER_STATE1_20000000)))++settled;
 else settled=0;
 if(tick%100==0)log<<"tick="<<tick<<" distance="<<chest->dyna.actor.xzDistToPlayer<<" pos="<<p->actor.world.pos.x<<","<<p->actor.world.pos.y<<","<<p->actor.world.pos.z<<" chest="<<chest->dyna.actor.world.pos.x<<","<<chest->dyna.actor.world.pos.y<<","<<chest->dyna.actor.world.pos.z<<" yaw="<<p->actor.shape.rot.y<<" targetYaw="<<toward<<" visible="<<MMVR_ButtonInteractionVisible(play,&chest->dyna.actor)<<" local="<<local.x<<","<<local.y<<","<<local.z<<" offer="<<nativeOffer<<" mask="<<int(Player_GetMask(play))<<" health="<<gSaveContext.save.saveInfo.playerData.health<<" boots="<<int(p->currentBoots)<<" openPos="<<atOpenPosition<<" alpha="<<int(chest->alpha)<<" getItem="<<p->getItemId<<" csAction="<<int(p->csAction)<<" manager="<<CutsceneManager_GetCurrentCsId()<<" chestSignal="<<chest->unk_1EC<<" chestOpen="<<(chest->actionFunc==EnBox_Open)<<" anim="<<p->skelAnime.curFrame<<" flags="<<p->stateFlags1<<" item="<<chest->getItemId<<" opened="<<opened<<" commits="<<commits<<'\n'<<std::flush;
 if(settled>=20){log<<"fairy-count-delta="<<fairyDelta<<'\n';finish("PASS",fairy?"native-fairy-chest-completed":"native-chest-completed");}
 else if(commits>1||fairyDelta>1)finish("FAIL","unexpected-item-commits");
 return pad;
}
