#pragma once
extern "C" {extern u8 sIsOcarinaInputEnabled;extern s8 sOcarinaInstrumentId;extern u32 sOcarinaFlags;}
// Shared teaching UI lifecycle only. NPC handoff/quest continuation has separate recipes.
static mmvr::Pad NativeSongStaffLifecycle(PlayState* play,unsigned tick){
 static const int song=std::atoi(std::getenv("MMVR_SONG_STAFF"));
 static int phase=0,noteTick=0;static bool success=false;static std::ofstream log("native-song-staff.log");
 mmvr::Pad pad;pad.active=true;mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
 auto finish=[&](const char* status,const char* reason){log<<status<<" song="<<song<<" success="<<success<<" reason="<<reason<<'\n'<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
 if(song<0||song>OCARINA_SONG_GORON_LULLABY_INTRO){finish("FAIL","invalid-song");return pad;}
 if(!phase&&tick>=60){
 // This supplemental staff fixture supplies the native teacher ownership that
 // stops Player_Action_63 from replacing a lesson with free play.
 static Actor teacher{};teacher.id=ACTOR_EN_TEST;teacher.update=[](Actor*,PlayState*){};
 auto* player=GET_PLAYER(play);player->ocarinaInteractionActor=&teacher;
 player->ocarinaInteractionDistance=-1;player->actor.flags|=ACTOR_FLAG_OCARINA_INTERACTION;
 gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA]=ITEM_OCARINA_OF_TIME;
 BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_OCARINA_OF_TIME;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_OCARINA;
 gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;pad.buttons=BTN_CDOWN;phase=-1;}
 if(phase==-1){
  if(GET_PLAYER(play)->stateFlags2&PLAYER_STATE2_USING_OCARINA){Message_DisplayOcarinaStaff(play,OCARINA_ACTION_DEMONSTRATE_SONATA+song);phase=1;}
  else if(tick%12==0)pad.buttons=BTN_CDOWN;
 }
 if(phase==1&&play->msgCtx.msgMode==MSGMODE_SONG_DEMONSTRATION_DONE&&AudioOcarina_GetPlaybackStaff()->state==0){
  log<<"demonstration-completed\n"<<std::flush;Message_DisplayOcarinaStaff(play,OCARINA_ACTION_PROMPT_SONATA+song);phase=2;
 }
 if(phase==2&&play->msgCtx.msgMode==MSGMODE_SONG_PROMPT){
  static const u16 keys[]={BTN_A,BTN_CDOWN,BTN_CRIGHT,BTN_CLEFT,BTN_CUP};
  const auto& notes=gOcarinaSongButtons[song];unsigned n=noteTick<10?999u:unsigned(noteTick-10)/20;
  if(n<notes.numButtons&&notes.buttonIndex[n]<5&&(noteTick-10)%20<8)pad.buttons=keys[notes.buttonIndex[n]];
  ++noteTick;
 }else if(play->msgCtx.msgMode==MSGMODE_SONG_PROMPT_FAIL)noteTick=0;
 if(phase==2&&play->msgCtx.msgMode==MSGMODE_OCARINA_AWAIT_INPUT&&tick%12==0)pad.buttons=BTN_A;
 if(phase==2&&noteTick%20==0&&play->msgCtx.msgMode==MSGMODE_SONG_PROMPT){auto* staff=AudioOcarina_GetPlayingStaff();log<<"note="<<noteTick<<" staff="<<int(staff->pos)<<","<<int(staff->state)<<" action="<<play->msgCtx.ocarinaAction<<" key="<<pad.buttons<<'\n'<<std::flush;}
 success|=phase==2&&play->msgCtx.msgMode==MSGMODE_SONG_PROMPT_SUCCESS&&play->msgCtx.songPlayed==song;
 if(success&&(play->msgCtx.ocarinaMode==OCARINA_MODE_END||play->msgCtx.ocarinaMode==OCARINA_MODE_EVENT)){finish("PASS","demonstration-prompt-native-success");return pad;}
 if(tick%100==0)log<<"tick="<<tick<<" phase="<<phase<<" msg="<<int(play->msgCtx.msgMode)<<" mode="<<int(play->msgCtx.ocarinaMode)<<" enabled="<<int(sIsOcarinaInputEnabled)<<" instrument="<<int(sOcarinaInstrumentId)<<" audioFlags="<<sOcarinaFlags<<" playback="<<int(AudioOcarina_GetPlaybackStaff()->state)<<" notes="<<noteTick<<'\n'<<std::flush;
 if(tick>1800)finish("BLOCKED","teaching-lifecycle-prerequisite");
 return pad;
}
