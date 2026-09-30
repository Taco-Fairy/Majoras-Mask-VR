// Test encounters keep their native scene/controller dependencies. Progress is sandboxed.
namespace {
struct Trial { const char* name; int entrance,scene,actor; };
constexpr Trial trials[]={
 {"ODOLWA",ENTRANCE(ODOLWAS_LAIR,0),SCENE_MITURIN_BS,ACTOR_BOSS_01},
 {"GOHT",ENTRANCE(GOHTS_LAIR,0),SCENE_HAKUGIN_BS,ACTOR_BOSS_HAKUGIN},
 {"GYORG",ENTRANCE(GYORGS_LAIR,0),SCENE_SEA_BS,ACTOR_BOSS_03},
 {"TWINMOLD",ENTRANCE(TWINMOLDS_LAIR,0),SCENE_INISIE_BS,ACTOR_BOSS_02},
 {"MAJORA",ENTRANCE(MAJORAS_LAIR,0),SCENE_LAST_BS,ACTOR_BOSS_07},
 {"GIANT PRACTICE",ENTRANCE(TWINMOLDS_LAIR,0),SCENE_INISIE_BS,ACTOR_BOSS_02}};
int trial=-1;bool trialSnapshot=false,trialReturning=false;
int townPanoramaPending=-1;
int koumeRoomPending=0;
int woodfallWebPending=0,woodfallWebSettle=0;
int interactionRoom=3;
Save trialSave{};u8 trialEvents[8]{};CycleSceneFlags trialCycles[120]{};
u16 trialSkyboxTime=0;s16 trialTimeSpeed=0;
bool IsTownArrival(DebugNativeTriggerKind kind){
 return kind==DebugNativeTriggerKind::NorthClockTownArrival||
        kind==DebugNativeTriggerKind::WestClockTownArrival||
        kind==DebugNativeTriggerKind::EastClockTownArrival;
}
struct TownPanoramaScript {s16 csId,scriptIndex;u8 spawnFlag;const char* logName;};
TownPanoramaScript TownPanoramaFor(DebugNativeTriggerKind kind){
 switch(kind){
 case DebugNativeTriggerKind::NorthClockTownArrival:return {20,0,0x15,"north-panorama"};
 case DebugNativeTriggerKind::WestClockTownArrival:return {13,2,0x14,"west-panorama"};
 case DebugNativeTriggerKind::EastClockTownArrival:return {13,0,0x13,"east-panorama"};
 default:return {CS_ID_NONE,CS_SCRIPT_ID_NONE,CS_SPAWN_FLAG_NONE,"invalid-panorama"};
 }
}
void RestoreTrial(){if(!trialSnapshot)return;gSaveContext.save=trialSave;gSaveContext.skyboxTime=trialSkyboxTime;R_TIME_SPEED=trialTimeSpeed;std::memcpy(gSaveContext.eventInf,trialEvents,sizeof(trialEvents));std::memcpy(gSaveContext.cycleSceneFlags,trialCycles,sizeof(trialCycles));}
void PrepareKafeiCheckpoint(int preset){
 if(preset<0)return;
 auto& save=gSaveContext.save;
 save.day=preset<2?u8(preset+1):3;
 save.eventDayCount=save.day;
 const u16 time=preset==0?CLOCK_TIME(6,50):preset==1?CLOCK_TIME(15,5):preset==2?CLOCK_TIME(18,0):CLOCK_TIME(4,30);
 save.time=time;gSaveContext.skyboxTime=time;R_TIME_SPEED=1;
 // Start each checkpoint at the authored schedule branch being tested. The
 // debug portal snapshot restores every changed flag when returning to the lab.
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_51_02);
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_51_04);
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_PENDANT_OF_MEMORIES);
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_DELIVERED_PENDANT_OF_MEMORIES);
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_ESCAPED_SAKONS_HIDEOUT);
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECOVERED_STOLEN_BOMB_BAG);
 CLEAR_WEEKEVENTREG(WEEKEVENTREG_SAKON_DEAD);
 switch(preset){
 case 1: SET_WEEKEVENTREG(WEEKEVENTREG_51_02); break;
 case 3:
  SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_PENDANT_OF_MEMORIES);
  SET_WEEKEVENTREG(WEEKEVENTREG_DELIVERED_PENDANT_OF_MEMORIES);
  SET_WEEKEVENTREG(WEEKEVENTREG_ESCAPED_SAKONS_HIDEOUT);
  break;
 default: break;
 }
}
void TrialTransition(PlayState* play,int entranceId,bool human=false){
 mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvr::CancelHeldMask();mmvr::GetMenu().open=false;
 play->nextEntrance=entranceId;gSaveContext.nextCutsceneIndex=0;gSaveContext.cutsceneTrigger=0;gSaveContext.respawnFlag=0;
 gSaveContext.save.cutsceneIndex=0;
 if(human){gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;gSaveContext.save.equippedMask=PLAYER_MASK_NONE;}
 // The return action also works when opened over the ordinary pause menu.
 play->pauseCtx.state=PAUSE_STATE_OFF;R_PAUSE_BG_PRERENDER_STATE=0;
 play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
}
void PrepareTrial(PlayState* play){
 if(trialReturning&&play->sceneId==SCENE_SPOT00){
  RestoreTrial();trialSnapshot=false;trialReturning=false;trial=-1;
  gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;gSaveContext.save.equippedMask=PLAYER_MASK_NONE;
  // The return entrance can reload the debug scene without changing its scene
  // identity. Drop cached camera height/pose state so Link's calibrated head
  // height is reacquired before the first frame in the hall.
  MMVR_CameraCoordinateBoundary(play);
  mmvr::Recenter();
  return;
 }
 if(!trialSnapshot||trial<0||play->sceneId!=trials[trial].scene)return;
 const int scene=play->sceneId;
 gSaveContext.save.saveInfo.permanentSceneFlags[scene]={};gSaveContext.cycleSceneFlags[scene]={};
 play->actorCtx.sceneFlags.clearedRoom=0;play->actorCtx.sceneFlags.clearedRoomTemp=0;
 switch(trial){
 case 0:CLEAR_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE);SET_EVENTINF(EVENTINF_INTRO_CS_WATCHED_ODOLWA);break;
 case 1:CLEAR_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE);SET_EVENTINF(EVENTINF_INTRO_CS_WATCHED_GOHT);break;
 case 2:CLEAR_WEEKEVENTREG(WEEKEVENTREG_CLEARED_GREAT_BAY_TEMPLE);SET_EVENTINF(EVENTINF_INTRO_CS_WATCHED_GYORG);break;
 case 3:case 5:CLEAR_WEEKEVENTREG(WEEKEVENTREG_CLEARED_STONE_TOWER_TEMPLE);SET_EVENTINF(EVENTINF_INTRO_CS_WATCHED_TWINMOLD);break;
 case 4:SET_EVENTINF(EVENTINF_INTRO_CS_WATCHED_MAJORA);break;
 }
 Refill();Log()<<"trial-scene name="<<trials[trial].name<<" scene="<<scene<<"\n"<<std::flush;mmvr::Recenter();
}
}
extern "C" int MMVR_DebugGiantPractice(PlayState* play){return requested&&trialSnapshot&&trial==5&&play&&play->sceneId==SCENE_INISIE_BS;}
extern "C" int MMVR_DebugTrialBegin(PlayState* play,int index){
 if(!MMVR_DebugRoomActive(play)||index<0||index>=ARRAY_COUNT(trials)||play->transitionTrigger!=TRANS_TRIGGER_OFF)return false;
 trialSave=gSaveContext.save;trialSkyboxTime=gSaveContext.skyboxTime;trialTimeSpeed=R_TIME_SPEED;std::memcpy(trialEvents,gSaveContext.eventInf,sizeof(trialEvents));std::memcpy(trialCycles,gSaveContext.cycleSceneFlags,sizeof(trialCycles));trialSnapshot=true;trialReturning=false;trial=index;
 TrialTransition(play,trials[index].entrance,index==5);return true;
}
extern "C" int MMVR_DebugLocationBegin(PlayState* play,int index){
 if(!MMVR_DebugRoomActive(play)||index<0||index>=ARRAY_COUNT(debugLocations)||play->transitionTrigger!=TRANS_TRIGGER_OFF)return false;
 trialSave=gSaveContext.save;trialSkyboxTime=gSaveContext.skyboxTime;trialTimeSpeed=R_TIME_SPEED;std::memcpy(trialEvents,gSaveContext.eventInf,sizeof(trialEvents));std::memcpy(trialCycles,gSaveContext.cycleSceneFlags,sizeof(trialCycles));
 trialSnapshot=true;trialReturning=false;trial=-1;
 PrepareKafeiCheckpoint(debugLocations[index].kafeiPreset);
 const int interaction=debugLocations[index].interactionPreset;
 interactionRoom=3;
 koumeRoomPending=0;woodfallWebPending=0;woodfallWebSettle=0;
 if(interaction>=0) {
  // This is inside the existing reversible trial snapshot, not the player's save.
  gSaveContext.save.day=1;gSaveContext.save.eventDayCount=1;
  gSaveContext.save.time=CLOCK_TIME(10,0);gSaveContext.skyboxTime=gSaveContext.save.time;
  gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;gSaveContext.save.equippedMask=PLAYER_MASK_NONE;
  if(interaction>=8 && interaction<=15) {
   // Use authored entrances rather than unchecked room coordinates. All quest
   // and inventory changes are restored by the existing visitor snapshot.
   auto& info=gSaveContext.save.saveInfo;
   info.inventory.items[SLOT_BOW]=ITEM_BOW;
   info.inventory.items[SLOT_HOOKSHOT]=ITEM_HOOKSHOT;
   info.inventory.items[SLOT_BOMB]=ITEM_BOMB;
   info.inventory.items[SLOT_ARROW_FIRE]=ITEM_ARROW_FIRE;
   info.inventory.items[SLOT_ARROW_ICE]=ITEM_ARROW_ICE;
   info.inventory.items[SLOT_ARROW_LIGHT]=ITEM_ARROW_LIGHT;
   Inventory_ChangeUpgrade(UPG_QUIVER, 1);
   Inventory_ChangeUpgrade(UPG_BOMB_BAG, 1);
   info.playerData.isMagicAcquired=true;
   info.playerData.rupees=200;
   Refill();
   if(interaction==14) CLEAR_WEEKEVENTREG(WEEKEVENTREG_08_80);
   if(interaction==10) {
    gSaveContext.save.time=CLOCK_TIME(15,30);
    gSaveContext.skyboxTime=gSaveContext.save.time;
   }
  } else if(interaction==0) {
   SET_WEEKEVENTREG(WEEKEVENTREG_TALKED_KOUME_INJURED);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_SAVED_KOUME);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_RED_POTION_FOR_KOUME);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_KOTAKE_BOTTLE);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_FAILED_RECEIVED_RED_POTION_FOR_KOUME_SHOP);
   gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_NONE;
  } else if(interaction==3) {
   SET_WEEKEVENTREG(WEEKEVENTREG_TALKED_KOUME_INJURED);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_SAVED_KOUME);
   gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_POTION_RED;
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_POTION_RED;
   C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
   gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
   koumeRoomPending=1;
  } else if(interaction==4) {
   // Recreate the authored room's unburned web, within the trial snapshot.
   gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_MITURIN]={};
   gSaveContext.cycleSceneFlags[SCENE_MITURIN]={};
   gSaveContext.save.saveInfo.inventory.items[SLOT_DEKU_STICK]=ITEM_DEKU_STICK;
   AMMO(ITEM_DEKU_STICK)=30;
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_DEKU_STICK;
   C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_DEKU_STICK;
   gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
   woodfallWebPending=1;
  } else if(interaction==7) {
   // Native Great Bay room 7; leave Wart dormant for the headset-look test.
   gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_SEA]={};
   gSaveContext.cycleSceneFlags[SCENE_SEA]={};
   gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]=ITEM_BOW;
   AMMO(ITEM_BOW)=30;
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOW;
   C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOW;
   gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
   interactionRoom=7;woodfallWebPending=1;
  } else if(interaction==5) {
   SET_WEEKEVENTREG(WEEKEVENTREG_20_20); // Open native gravestone/water before arrival.
   gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;
   C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
   gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
   gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
   gSaveContext.save.equippedMask=PLAYER_MASK_NONE;
  } else if(interaction==6) {
   // Use the actual milk-run entrance and its native intro, cart and attackers.
   // The existing trial snapshot restores these prerequisites on returning.
   gSaveContext.save.day=2;gSaveContext.save.eventDayCount=2;
   gSaveContext.save.time=CLOCK_TIME(19,0);gSaveContext.skyboxTime=gSaveContext.save.time;
   SET_WEEKEVENTREG(WEEKEVENTREG_DEFENDED_AGAINST_ALIENS);
   SET_WEEKEVENTREG(WEEKEVENTREG_31_40);
   SET_WEEKEVENTREG(WEEKEVENTREG_31_80);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_34_80);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_ESCORTED_CREMIA);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_52_02);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_59_02);
   gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]=ITEM_BOW;
   AMMO(ITEM_BOW)=30;
   BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOW;
   C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOW;
   gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
  } else if(interaction==1) {
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_09_80);
   CLEAR_WEEKEVENTREG(WEEKEVENTREG_23_20);
   SET_WEEKEVENTREG(WEEKEVENTREG_88_01);
  }
 }
 Log()<<"location-trial index="<<index<<" name="<<debugLocations[index].name<<"\n"<<std::flush;
 TrialTransition(play,debugLocations[index].entrance);
 if(interaction==7) {
  // Initialize the native scene in Wart's room, not in the entrance room.
  // The room floor also extends outside its walls, so a floor hit alone is
  // insufficient to validate the old post-load landing point.
  auto& spawn=gSaveContext.respawn[RESPAWN_MODE_DOWN];spawn={};
  spawn.entrance=debugLocations[index].entrance;spawn.roomIndex=7;
  spawn.pos={-1830.f,-570.f,-2390.f};spawn.yaw=static_cast<s16>(0x8000);
  spawn.playerParams=PLAYER_PARAMS(0xFF,PLAYER_START_MODE_B);
  gSaveContext.respawnFlag=1;
 }
 return true;
}
extern "C" int MMVR_DebugCutsceneBegin(PlayState* play,int index){
 if(!MMVR_DebugRoomActive(play)||index<0||index>=ARRAY_COUNT(debugCutscenes)||play->transitionTrigger!=TRANS_TRIGGER_OFF)return false;
 const auto& entry=debugCutscenes[index];
 if(Entrance_GetSceneIdAbsolute(entry.entrance)!=entry.scene)return false;
 trialSave=gSaveContext.save;trialSkyboxTime=gSaveContext.skyboxTime;trialTimeSpeed=R_TIME_SPEED;std::memcpy(trialEvents,gSaveContext.eventInf,sizeof(trialEvents));std::memcpy(trialCycles,gSaveContext.cycleSceneFlags,sizeof(trialCycles));
 trialSnapshot=true;trialReturning=false;trial=-1;
 Log()<<"cutscene-trial index="<<index<<" name="<<entry.name<<" scene="<<entry.scene<<" layer="<<entry.cutsceneIndex<<"\n"<<std::flush;
 // SPOT00 story scripts use human Link in the native source; restore the
 // visitor's original form from trialSave when returning to the hall.
 TrialTransition(play,entry.entrance,entry.scene==SCENE_SPOT00);
 // This mirrors the native transition callsites. In particular, the Song of
 // Time lesson is in SPOT00, which must not be replaced by the synthetic hall.
 gSaveContext.nextCutsceneIndex=entry.cutsceneIndex;
 if(entry.scene==SCENE_SPOT00)requested=false;
 return true;
}
extern "C" int MMVR_DebugNativeTriggerBegin(PlayState* play,int index){
 if(!MMVR_DebugRoomActive(play)||index<0||index>=ARRAY_COUNT(debugNativeTriggers)||play->transitionTrigger!=TRANS_TRIGGER_OFF)return false;
 const auto& entry=debugNativeTriggers[index];
 if(Entrance_GetSceneIdAbsolute(entry.entrance)!=entry.scene)return false;
 trialSave=gSaveContext.save;trialSkyboxTime=gSaveContext.skyboxTime;trialTimeSpeed=R_TIME_SPEED;std::memcpy(trialEvents,gSaveContext.eventInf,sizeof(trialEvents));std::memcpy(trialCycles,gSaveContext.cycleSceneFlags,sizeof(trialCycles));
 trialSnapshot=true;trialReturning=false;trial=-1;
 // Leave the real actor/song in charge of starting the sequence. The snapshot
 // protects prerequisites and inventory when the visitor returns to the hall.
 switch(entry.kind){
 case DebugNativeTriggerKind::BombersBalloon:
  CLEAR_WEEKEVENTREG(WEEKEVENTREG_75_40);CLEAR_WEEKEVENTREG(WEEKEVENTREG_73_10);CLEAR_WEEKEVENTREG(WEEKEVENTREG_85_02);
  gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]=ITEM_BOW;AMMO(ITEM_BOW)=30;break;
 case DebugNativeTriggerKind::InvertedTime:
  gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA]=ITEM_OCARINA_OF_TIME;SET_QUEST_ITEM(QUEST_SONG_TIME);break;
 case DebugNativeTriggerKind::NorthClockTownArrival:
 case DebugNativeTriggerKind::WestClockTownArrival:
 case DebugNativeTriggerKind::EastClockTownArrival:
  // The selected town's spawn 0 has no one-time entrance flag. Start its
  // authored spawn-1 panorama after scene initialization without setting it.
  break;
 case DebugNativeTriggerKind::TingleBalloon:
  // The native En_Bal actor owns the balloon hit and cutscene. Give the
  // visitor its prerequisites, then let them shoot the real balloon.
  gSaveContext.save.saveInfo.playerData.isMagicAcquired=true;
  gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]=ITEM_BOW;AMMO(ITEM_BOW)=30;break;
 case DebugNativeTriggerKind::WoodfallCrystal:
  gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]=ITEM_BOW;AMMO(ITEM_BOW)=30;
  // The authored room-0 crystal is temporary switch 0x60 with its own csId 1;
  // temporary switches are cleared by a normal scene load, not in CycleSceneFlags.
  break;
 }
 Log()<<"native-trigger index="<<index<<" name="<<entry.name<<" scene="<<entry.scene<<"\n"<<std::flush;
 TrialTransition(play,entry.entrance,true);
 townPanoramaPending=IsTownArrival(entry.kind)?index:-1;
 return true;
}
void MaybePlaceInteractionRoomCheckpoint(PlayState* play){
 if(!woodfallWebPending||!play||!trialSnapshot||trialReturning)return;
 const bool wart=interactionRoom==7;
 if(play->sceneId!=(wart?SCENE_SEA:SCENE_MITURIN)||play->transitionTrigger!=TRANS_TRIGGER_OFF||
    play->transitionMode!=TRANS_MODE_OFF||!GET_PLAYER(play)||play->roomCtx.status)return;
 if(woodfallWebPending==1){
  if(++woodfallWebSettle<45)return;
  if(play->roomCtx.curRoom.num!=interactionRoom){
   if(Room_RequestNewRoom(play,&play->roomCtx,interactionRoom))woodfallWebPending=2;
   return;
  }
 }
 if(woodfallWebPending==2)Room_FinishRoomChange(play,&play->roomCtx);
 auto* player=GET_PLAYER(play);
 // Resolve the actual upper walkway rather than starting below its surface.
 Vec3f landing=wart?Vec3f{-1830.f,-500.f,-2390.f}:Vec3f{-1110.f,-1100.f,-750.f};
 CollisionPoly* floor=nullptr;int floorBg=BGCHECK_SCENE;
 const float floorY=BgCheck_EntityRaycastFloor5(&play->colCtx,&floor,&floorBg,&player->actor,&landing);
 Vec3f clearance{landing.x,floorY+30.f,landing.z};
 if(!floor||floorY < (wart?-590.f:-1200.f)||floorY > (wart?-550.f:-1170.f)||
    BgCheck_SphVsFirstWall(&play->colCtx,&clearance,20.f)){
  woodfallWebPending=0;
  Log()<<"interaction-room-checkpoint rejected unsafe floor\n"<<std::flush;
  MMVR_DebugTrialReturn(play);return;
 }
 landing.y=floorY;
 player->actor.world.pos=landing;
 // Player_UpdateCommon reconstructs prevPos from home.pos. Both must move,
 // otherwise its next wall sweep starts back at the old entrance.
 player->actor.prevPos=player->actor.home.pos=landing;
 player->actor.floorHeight=floorY;player->actor.floorPoly=floor;player->actor.floorBgId=floorBg;
 player->actor.shape.rot.y=player->actor.world.rot.y=player->yaw=wart?static_cast<s16>(0x8000):0;
 player->actor.velocity={};player->actor.speed=player->speedXZ=0;
 player->fallStartHeight=static_cast<s16>(floorY);player->fallDistance=0;
 MMVR_CameraCoordinateBoundary(play);mmvr::Recenter();
 woodfallWebPending=0;
 Log()<<"interaction-room-checkpoint room="<<interactionRoom<<" position="<<landing.x<<","<<landing.y<<","<<landing.z<<" ready\n"<<std::flush;
}
void MaybePlaceKoumeCheckpoint(PlayState* play){
 if(!koumeRoomPending||!play||!trialSnapshot||trialReturning)return;
 if(play->sceneId!=SCENE_26SARUNOMORI||play->transitionTrigger!=TRANS_TRIGGER_OFF||
    play->transitionMode!=TRANS_MODE_OFF||!GET_PLAYER(play))return;
 if(play->roomCtx.status)return;
 if(koumeRoomPending==1&&play->roomCtx.curRoom.num!=7){
  if(Room_RequestNewRoom(play,&play->roomCtx,7))koumeRoomPending=2;
  return;
 }
 if(koumeRoomPending==2)Room_FinishRoomChange(play,&play->roomCtx);
 auto* player=GET_PLAYER(play);
 // Day-one Koume's authored room and approach side; leave her native AI intact.
 player->actor.world.pos={-1698.f,0.f,-50.f};
 player->actor.prevPos=player->actor.world.pos;
 player->actor.shape.rot.y=player->actor.world.rot.y=static_cast<s16>(-9701);
 player->actor.velocity={};player->actor.speed=0;
 MMVR_CameraCoordinateBoundary(play);mmvr::Recenter();
 koumeRoomPending=0;
 Log()<<"koume-handoff-checkpoint room=7 ready\n"<<std::flush;
}
void MaybeStartTownPanorama(PlayState* play){
 if(townPanoramaPending<0||!play||!trialSnapshot||trialReturning)return;
 const auto& target=debugNativeTriggers[townPanoramaPending];
 const auto expected=TownPanoramaFor(target.kind);
 if(play->sceneId!=target.scene)return; // Still transitioning from the hall.
 if(play->curSpawn!=0){townPanoramaPending=-1;Log()<<expected.logName<<" invalid-spawn\n";return;}
 if(!GET_PLAYER(play)||play->transitionTrigger!=TRANS_TRIGGER_OFF||
    play->pauseCtx.state!=PAUSE_STATE_OFF||play->csCtx.state!=CS_STATE_IDLE||
    CutsceneManager_GetCurrentCsId()!=CS_ID_NONE)return;
 // Ask the initialized native cutscene manager for this town's authored
 // spawn-1 entry. Restore the actual spawn before starting; the entrance
 // handler never runs here, so its one-time week-event flag stays unchanged.
 const u8 spawn=play->curSpawn;
 play->curSpawn=1;
 const s16 csId=CutsceneManager_FindEntranceCsId();
 play->curSpawn=spawn;
 const ActorCutscene* entry=csId!=CS_ID_NONE?CutsceneManager_GetCutsceneEntry(csId):nullptr;
 const int scriptIndex=entry?entry->scriptIndex:CS_SCRIPT_ID_NONE;
 const bool authored=csId==expected.csId&&scriptIndex==expected.scriptIndex&&
     scriptIndex>=0&&scriptIndex<play->csCtx.scriptListCount&&play->csCtx.scriptList&&
     play->csCtx.scriptList[scriptIndex].spawn==1&&
     play->csCtx.scriptList[scriptIndex].spawnFlags==expected.spawnFlag&&
     SOH::Cutscene::IsAreaIntroduction(play->csCtx.scriptList[scriptIndex].script);
 townPanoramaPending=-1; // A failed validation must never start an unrelated scene.
 if(!authored){Log()<<expected.logName<<" rejected csId="<<csId<<" script="<<scriptIndex<<"\n"<<std::flush;return;}
 u8 weekBefore[sizeof(gSaveContext.save.saveInfo.weekEventReg)];
 std::memcpy(weekBefore,gSaveContext.save.saveInfo.weekEventReg,sizeof(weekBefore));
 gSaveContext.showTitleCard=false; // The native script displays the title card.
 const s16 started=CutsceneManager_Start(csId,nullptr);
 const bool weekUnchanged=std::memcmp(weekBefore,gSaveContext.save.saveInfo.weekEventReg,sizeof(weekBefore))==0;
 Log()<<expected.logName<<" csId="<<csId<<" script="<<scriptIndex<<" titleCard="<<authored
      <<" started="<<(started==csId&&CutsceneManager_GetCurrentCsId()==csId)
      <<" weekUnchanged="<<weekUnchanged<<"\n"<<std::flush;
}
extern "C" int MMVR_DebugTrialReturn(PlayState* play){
 if(!mmvr::PrivateDebugTools||!play||gSaveContext.fileNum!=2||play->transitionTrigger!=TRANS_TRIGGER_OFF)return false;
 // Check the existing room identity before enabling a new hall request.
 if(MMVR_DebugRoomActive(play))return false;
 requested=true; audioProbe=true;
 townPanoramaPending=-1;koumeRoomPending=0;woodfallWebPending=0;woodfallWebSettle=0;
 RestoreTrial();trialReturning=true;TrialTransition(play,ENTRANCE(CUTSCENE,0),true);return true;
}

// Isolated harness: validate every native entrance/dispatch and the same save
// normalization/return helpers used by actual portal trips. This does not load
// all rooms or pretend to complete their quests.
static bool NativeLocationCatalogTest(PlayState* play,std::ostream& log){
 auto before=gSaveContext;auto previousSave=trialSave;auto previousSkybox=trialSkyboxTime;auto previousTimeSpeed=trialTimeSpeed;auto oldTrial=trial;auto oldSnapshot=trialSnapshot;auto oldReturning=trialReturning;auto oldPanoramaPending=townPanoramaPending;
 const s16 originalRate=R_TIME_SPEED;
 u8 oldEvents[8];CycleSceneFlags oldCycles[120];std::memcpy(oldEvents,trialEvents,sizeof(oldEvents));std::memcpy(oldCycles,trialCycles,sizeof(oldCycles));
 auto oldNext=play->nextEntrance;auto oldTrigger=play->transitionTrigger;auto oldType=play->transitionType;auto oldScene=play->sceneId;auto oldPause=play->pauseCtx.state;auto oldPrerender=R_PAUSE_BG_PRERENDER_STATE;
 bool ok=true;int verified=0;
 for(int i=0;i<ARRAY_COUNT(debugLocations);++i){
  gSaveContext=before;play->sceneId=oldScene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  bool entry=Entrance_GetSceneIdAbsolute(debugLocations[i].entrance)==debugLocations[i].scene;
  bool started=MMVR_DebugLocationBegin(play,i)&&play->nextEntrance==debugLocations[i].entrance&&trialSnapshot;
  bool kafeiReady=true;
  if(debugLocations[i].kafeiPreset>=0){
   const int preset=debugLocations[i].kafeiPreset;
   const u8 expectedDay=preset<2?u8(preset+1):3;
   const u16 expectedTime=preset==0?CLOCK_TIME(6,50):preset==1?CLOCK_TIME(15,5):
                          preset==2?CLOCK_TIME(18,0):CLOCK_TIME(4,30);
   kafeiReady=gSaveContext.save.day==expectedDay&&gSaveContext.save.eventDayCount==expectedDay&&
              gSaveContext.save.time==expectedTime&&gSaveContext.skyboxTime==expectedTime;
   if(preset==1)kafeiReady=kafeiReady&&CHECK_WEEKEVENTREG(WEEKEVENTREG_51_02)!=0;
   if(preset==2)kafeiReady&=!CHECK_WEEKEVENTREG(WEEKEVENTREG_RECOVERED_STOLEN_BOMB_BAG)&&
                            !CHECK_WEEKEVENTREG(WEEKEVENTREG_SAKON_DEAD)&&
                            !CHECK_WEEKEVENTREG(WEEKEVENTREG_ESCAPED_SAKONS_HIDEOUT);
   if(preset==3)kafeiReady=kafeiReady&&
       CHECK_WEEKEVENTREG(WEEKEVENTREG_DELIVERED_PENDANT_OF_MEMORIES)!=0&&
       CHECK_WEEKEVENTREG(WEEKEVENTREG_ESCAPED_SAKONS_HIDEOUT)!=0;
   if(!kafeiReady)log<<"kafei-checkpoint-preset="<<preset<<" day="<<int(gSaveContext.save.day)
       <<" eventDay="<<int(gSaveContext.save.eventDayCount)<<" time="<<gSaveContext.save.time
       <<" expectedTime="<<expectedTime<<" skyboxTime="<<gSaveContext.skyboxTime
       <<" week51_02="<<CHECK_WEEKEVENTREG(WEEKEVENTREG_51_02)
       <<" delivered="<<CHECK_WEEKEVENTREG(WEEKEVENTREG_DELIVERED_PENDANT_OF_MEMORIES)
       <<" escaped="<<CHECK_WEEKEVENTREG(WEEKEVENTREG_ESCAPED_SAKONS_HIDEOUT)<<"\n";
  }
  gSaveContext.save.saveInfo.playerData.rupees=0;gSaveContext.save.saveInfo.weekEventReg[0]^=0x80;gSaveContext.eventInf[0]^=0x80;reinterpret_cast<u8*>(gSaveContext.cycleSceneFlags)[0]^=1;
  Save normalized=gSaveContext.save;MMVR_DebugNormalizeSave(&normalized);
  auto expectedInfo=before.save.saveInfo;
  // Normal saves deliberately resume in Clock Town, never the synthetic hall.
  expectedInfo.playerData.savedSceneId=SCENE_CLOCKTOWER;
  bool protectedSave=std::memcmp(&normalized.saveInfo,&expectedInfo,sizeof(normalized.saveInfo))==0&&normalized.entrance==ENTRANCE(SOUTH_CLOCK_TOWN,0);
  play->sceneId=debugLocations[i].scene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  bool returned=MMVR_DebugTrialReturn(play)&&play->nextEntrance==ENTRANCE(CUTSCENE,0);
  bool restored=std::memcmp(&gSaveContext.save.saveInfo,&before.save.saveInfo,sizeof(before.save.saveInfo))==0&&
   std::memcmp(gSaveContext.eventInf,before.eventInf,sizeof(trialEvents))==0&&std::memcmp(gSaveContext.cycleSceneFlags,before.cycleSceneFlags,sizeof(trialCycles))==0;
  const bool kafeiTimeRestored=gSaveContext.save.day==before.save.day&&
      gSaveContext.save.eventDayCount==before.save.eventDayCount&&gSaveContext.save.time==before.save.time&&
      gSaveContext.skyboxTime==before.skyboxTime;
  bool passed=entry&&started&&protectedSave&&kafeiReady&&returned&&restored&&kafeiTimeRestored;ok&=passed;verified+=passed;
  if(!passed)log<<"catalog-failure index="<<i<<" entry="<<entry<<" started="<<started<<" protected="<<protectedSave<<" kafei="<<kafeiReady<<" returned="<<returned<<" restored="<<restored<<" timeRestored="<<kafeiTimeRestored<<"\n";
 }
 int cutscenesVerified=0;const bool beforeRequested=requested;
 for(int i=0;i<ARRAY_COUNT(debugCutscenes);++i){
  gSaveContext=before;requested=true;play->sceneId=oldScene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  const auto& entry=debugCutscenes[i];
  bool sceneMatches=Entrance_GetSceneIdAbsolute(entry.entrance)==entry.scene;
  bool started=MMVR_DebugCutsceneBegin(play,i)&&play->nextEntrance==entry.entrance&&
      gSaveContext.nextCutsceneIndex==entry.cutsceneIndex&&trialSnapshot;
  bool hallOverrideDisabled=entry.scene!=SCENE_SPOT00||!requested;
  play->sceneId=entry.scene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  bool returned=MMVR_DebugTrialReturn(play)&&play->nextEntrance==ENTRANCE(CUTSCENE,0);
  bool restored=std::memcmp(&gSaveContext.save.saveInfo,&before.save.saveInfo,sizeof(before.save.saveInfo))==0;
  bool passed=sceneMatches&&started&&hallOverrideDisabled&&returned&&restored;
  ok&=passed;cutscenesVerified+=passed;
  if(!passed)log<<"cutscene-catalog-failure index="<<i<<" scene="<<sceneMatches<<" started="<<started
               <<" override="<<hallOverrideDisabled<<" returned="<<returned<<" restored="<<restored<<"\n";
 }
 int triggersVerified=0;
 for(int i=0;i<visibleNativeTriggerCount;++i){
  gSaveContext=before;requested=true;play->sceneId=oldScene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  const auto& entry=debugNativeTriggers[i];
  bool sceneMatches=Entrance_GetSceneIdAbsolute(entry.entrance)==entry.scene;
  bool started=MMVR_DebugNativeTriggerBegin(play,i)&&play->nextEntrance==entry.entrance&&
      gSaveContext.nextCutsceneIndex==0&&trialSnapshot;
  const bool weekUnchanged=!IsTownArrival(entry.kind)||
      std::memcmp(gSaveContext.save.saveInfo.weekEventReg,before.save.saveInfo.weekEventReg,
                  sizeof(before.save.saveInfo.weekEventReg))==0;
  bool prerequisites=false;
  switch(entry.kind){
  case DebugNativeTriggerKind::BombersBalloon:
   prerequisites=entry.entrance==ENTRANCE(NORTH_CLOCK_TOWN,0)&&entry.scene==SCENE_BACKTOWN&&
                 !CHECK_WEEKEVENTREG(WEEKEVENTREG_75_40)&&!CHECK_WEEKEVENTREG(WEEKEVENTREG_73_10)&&
                 !CHECK_WEEKEVENTREG(WEEKEVENTREG_85_02)&&AMMO(ITEM_BOW)==30;break;
  case DebugNativeTriggerKind::InvertedTime:
   prerequisites=gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA]==ITEM_OCARINA_OF_TIME&&CHECK_QUEST_ITEM(QUEST_SONG_TIME);break;
  case DebugNativeTriggerKind::NorthClockTownArrival:
   prerequisites=entry.entrance==ENTRANCE(NORTH_CLOCK_TOWN,0)&&entry.scene==SCENE_BACKTOWN&&townPanoramaPending==i;break;
  case DebugNativeTriggerKind::WestClockTownArrival:
   prerequisites=entry.entrance==ENTRANCE(WEST_CLOCK_TOWN,0)&&entry.scene==SCENE_ICHIBA&&townPanoramaPending==i;break;
  case DebugNativeTriggerKind::EastClockTownArrival:
   prerequisites=entry.entrance==ENTRANCE(EAST_CLOCK_TOWN,0)&&entry.scene==SCENE_TOWN&&townPanoramaPending==i;break;
  case DebugNativeTriggerKind::TingleBalloon:
   prerequisites=entry.entrance==ENTRANCE(NORTH_CLOCK_TOWN,0)&&entry.scene==SCENE_BACKTOWN&&
                 gSaveContext.save.saveInfo.playerData.isMagicAcquired&&
                 gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]==ITEM_BOW&&AMMO(ITEM_BOW)==30&&
                 townPanoramaPending==-1;break;
  case DebugNativeTriggerKind::WoodfallCrystal:
   prerequisites=gSaveContext.save.saveInfo.inventory.items[SLOT_BOW]==ITEM_BOW&&AMMO(ITEM_BOW)==30;break;
  }
  play->sceneId=entry.scene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  bool returned=MMVR_DebugTrialReturn(play)&&play->nextEntrance==ENTRANCE(CUTSCENE,0);
  bool restored=std::memcmp(&gSaveContext.save.saveInfo,&before.save.saveInfo,sizeof(before.save.saveInfo))==0&&
   std::memcmp(gSaveContext.eventInf,before.eventInf,sizeof(trialEvents))==0&&
   std::memcmp(gSaveContext.cycleSceneFlags,before.cycleSceneFlags,sizeof(trialCycles))==0;
  bool passed=sceneMatches&&started&&prerequisites&&weekUnchanged&&returned&&restored;
  ok&=passed;triggersVerified+=passed;
  if(!passed)log<<"native-trigger-failure index="<<i<<" scene="<<sceneMatches<<" started="<<started
                <<" prerequisites="<<prerequisites<<" weekUnchanged="<<weekUnchanged<<" returned="<<returned<<" restored="<<restored<<"\n";
 }
 requested=beforeRequested;
 // Ordinary Clock Town exit clears the hall request; return must re-enable it.
 const bool oldRequested=requested,oldAudio=audioProbe;
 bool ordinaryReturn=true;
 for(int scene:{SCENE_CLOCKTOWER,SCENE_SPOT00}){
  requested=false;trialSnapshot=false;play->sceneId=scene;play->transitionTrigger=TRANS_TRIGGER_OFF;
  ordinaryReturn&=MMVR_DebugTrialReturn(play)&&requested&&play->nextEntrance==ENTRANCE(CUTSCENE,0);
 }
 log<<"ordinary-exit-return="<<ordinaryReturn<<"\n";ok&=ordinaryReturn;
 requested=oldRequested;audioProbe=oldAudio;
 gSaveContext=before;trialSave=previousSave;trialSkyboxTime=previousSkybox;trialTimeSpeed=previousTimeSpeed;R_TIME_SPEED=originalRate;trial=oldTrial;trialSnapshot=oldSnapshot;trialReturning=oldReturning;townPanoramaPending=oldPanoramaPending;
 std::memcpy(trialEvents,oldEvents,sizeof(oldEvents));std::memcpy(trialCycles,oldCycles,sizeof(oldCycles));
 play->nextEntrance=oldNext;play->transitionTrigger=oldTrigger;play->transitionType=oldType;play->sceneId=oldScene;play->pauseCtx.state=oldPause;R_PAUSE_BG_PRERENDER_STATE=oldPrerender;
 int page=catalogPage;for(int i=0;i<catalogPages;++i){catalogPage=i;BuildCatalogPage();ok&=!CatalogMesh().commands.empty()&&!CatalogMesh().vertices.empty();}catalogPage=page;BuildCatalogPage();
 int group=cutsceneGroup,cutscenePageBefore=cutscenePage;
 for(int g=0;g<2;++g){cutsceneGroup=g;for(int i=0;i<CutscenePageCount();++i){cutscenePage=i;BuildCutsceneCatalogPage();ok&=!CutsceneCatalogMesh().commands.empty()&&!CutsceneCatalogMesh().vertices.empty();}}
 cutsceneGroup=group;cutscenePage=cutscenePageBefore;BuildCutsceneCatalogPage();
 log<<"catalog-verified="<<verified<<" total="<<ARRAY_COUNT(debugLocations)<<" pages="<<catalogPages<<" nativeRoomLoads=0\n"<<std::flush;
 log<<"cutscene-catalog-verified="<<cutscenesVerified<<" total="<<ARRAY_COUNT(debugCutscenes)<<" scriptedPages="<<scriptedCutscenePages<<" interactivePages="<<interactiveCutscenePages<<"\n"<<std::flush;
 log<<"native-trigger-catalog-verified="<<triggersVerified<<" total="<<visibleNativeTriggerCount<<"\n"<<std::flush;
 return ok;
}

// Exercise the same helpers called by the VR menu while its open state keeps
// normal Play_Update paused. A native dusk refresh must remain pending through
// a second click, and consecutive day clicks must not close the menu.
static bool NativeDebugTimeSkipTest(PlayState* play,std::ostream& log){
 auto before=gSaveContext;
 auto& menu=mmvr::GetMenu();
 const bool wasOpen=menu.open;
 const int oldSetupActors=play->numSetupActors;
 const auto oldWeather=gWeatherMode;
 const auto oldLightning=play->envCtx.lightningState;
 gSaveContext.save.day=1;
 gSaveContext.save.eventDayCount=1;
 gSaveContext.save.time=CLOCK_TIME(17,0);
 gSaveContext.skyboxTime=gSaveContext.save.time;
 play->numSetupActors=4;
 menu.open=true;
 const bool hoursAvailable=mmvrgame::CanSkipDebugHours(play);
 mmvrgame::SkipDebugHours(play);
 const bool firstHours=menu.open&&CURRENT_DAY==1&&CURRENT_TIME==CLOCK_TIME(19,0)&&play->numSetupActors>0;
 mmvrgame::SkipDebugHours(play);
 const u16 secondTime=CURRENT_TIME;
 // CLOCK_TIME truncates each hour conversion, so two incremental +2h steps
 // can differ by one native time unit from one absolute 21:00 conversion.
 const bool secondHours=menu.open&&CURRENT_DAY==1&&
   std::abs(int(secondTime)-int(CLOCK_TIME(21,0)))<=1&&play->numSetupActors>0;
 gSaveContext.save.time=CLOCK_TIME(10,0);
 gSaveContext.skyboxTime=gSaveContext.save.time;
 const bool daysAvailable=mmvrgame::CanSkipDebugDay(play);
 mmvrgame::SkipDebugDay(play);
 const bool firstDay=menu.open&&CURRENT_DAY==2&&CURRENT_TIME==CLOCK_TIME(10,0)&&play->numSetupActors>0;
 mmvrgame::SkipDebugDay(play);
 const bool secondDay=menu.open&&CURRENT_DAY==3&&CURRENT_TIME==CLOCK_TIME(10,0)&&play->numSetupActors>0;
 gSaveContext=before;
 play->numSetupActors=oldSetupActors;
 gWeatherMode=oldWeather;
 play->envCtx.lightningState=oldLightning;
 menu.open=wasOpen;
 const bool restored=std::memcmp(&gSaveContext.save,&before.save,sizeof(before.save))==0&&
   gSaveContext.skyboxTime==before.skyboxTime&&menu.open==wasOpen&&play->numSetupActors==oldSetupActors;
 const bool ok=hoursAvailable&&firstHours&&secondHours&&daysAvailable&&firstDay&&secondDay&&restored;
 log<<"debug-time-hours-available="<<hoursAvailable<<" first="<<firstHours<<" second="<<secondHours
    <<" secondRaw="<<secondTime<<" expectedRaw="<<CLOCK_TIME(21,0)
    <<" days-available="<<daysAvailable<<" first="<<firstDay<<" second="<<secondDay
    <<" restored="<<restored<<" PASS="<<ok<<"\n"<<std::flush;
 return ok;
}
