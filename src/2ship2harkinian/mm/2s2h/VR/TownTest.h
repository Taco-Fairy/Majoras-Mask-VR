#pragma once
#include "ScenePresentation.h"
#include "CrossPosts.h"
extern "C" { unsigned long long MMVR_DebugAudioNonzero(); extern u8 sStartSeqDisabled; }
static mmvr::Pad NativeTownTest(PlayState* play,unsigned tick){
 static int visit=-1,frames=0,titleFrames=0,badRoute=0,localTransitionFrames=0,badLocalRoute=0,locationFrames=0,badLocationRoute=0;static bool leaving=false;
 static unsigned long long audioStart=0;static std::ofstream log("native-town.json");
 static std::ofstream trace("native-town.log");
 const int scenes[]={SCENE_CLOCKTOWER,SCENE_TOWN,SCENE_ICHIBA,SCENE_BACKTOWN};
 const int entrances[]={ENTRANCE(SOUTH_CLOCK_TOWN,0),ENTRANCE(EAST_CLOCK_TOWN,0),ENTRANCE(WEST_CLOCK_TOWN,0),ENTRANCE(NORTH_CLOCK_TOWN,0)};
 const int days[]={1,2,3,1};
 mmvr::Pad pad;pad.active=true;
 auto next=[&](int index){
  visit=index;frames=titleFrames=badRoute=localTransitionFrames=badLocalRoute=locationFrames=badLocationRoute=0;leaving=true;audioStart=MMVR_DebugAudioNonzero();
  gSaveContext.save.day=days[index%4];gSaveContext.save.eventDayCount=days[index%4];gSaveContext.save.time=index<4?CLOCK_TIME(6,0):CLOCK_TIME(12,0);gSaveContext.save.isNight=false;
  gSaveContext.nextCutsceneIndex=0;gSaveContext.respawnFlag=0;play->nextEntrance=entrances[index%4];play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
 };
 if(visit<0){if(tick==20){log<<"[";next(0);}return pad;}
 if(leaving){if(play->sceneId!=scenes[visit%4])return pad;leaving=false;}
 ++frames;
 // Explicit native day/night messages; area banners alone cannot count as dawn coverage.
 if(visit<5&&frames==80){const u16 cards[]={0x1BB2,0x1BB3,0x1BB4,0x1BB5,0x1BB6};Message_DisplaySceneTitleCard(play,cards[visit]);}
 auto facts=mmvrgame::SceneFacts(play);
 if(play->msgCtx.msgMode>=MSGMODE_SCENE_TITLE_CARD_FADE_IN_BACKGROUND&&play->msgCtx.msgMode<=MSGMODE_SCENE_TITLE_CARD_FADE_OUT_BACKGROUND&&!facts.titleSequence&&facts.playerPresent&&!facts.worldUnavailable){
  ++locationFrames;
  if(mmvr::ResolveSceneView(facts,false)!=mmvr::SceneView::Player||mmvr::ResolveSceneView(facts,true)!=mmvr::SceneView::Player)++badLocationRoute;
 }

 if(facts.transition&&!facts.titleSequence&&facts.playerPresent&&!facts.worldUnavailable&&!facts.remote&&!facts.scripted&&!facts.distantAction){
  ++localTransitionFrames;
  if(mmvr::ResolveSceneView(facts,false)!=mmvr::SceneView::Player||mmvr::ResolveSceneView(facts,true)!=mmvr::SceneView::Player)++badLocalRoute;
 }

 // Dawn cards keep their authored theater framing; Night cards are UI overlays
 // and must leave the live player view visible with either camera preference.
 if(facts.titleSequence){
  ++titleFrames;
  const auto expected=facts.nightTitleCard?mmvr::SceneView::Player:mmvr::SceneView::Theater;
  if(mmvr::ImmersiveScene(facts)||mmvr::ResolveSceneView(facts,false)!=expected||
     mmvr::ResolveSceneView(facts,true)!=expected)++badRoute;
 } if(frames%20==0)trace<<visit<<" frame="<<frames<<" title="<<facts.titleSequence<<" mode="<<int(play->msgCtx.msgMode)<<" cs="<<CutsceneManager_GetCurrentCsId()<<" transition="<<facts.transition<<" bgm="<<AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN)<<" startDisabled="<<int(sStartSeqDisabled)<<" reset="<<int(gAudioCtx.resetStatus)<<" spec="<<int(gAudioSpecId)<<"\n"<<std::flush;
 if(frames==25)mmvr::RequestNativeCapture((std::string("native-town-title-")+std::to_string(visit)).c_str());
 if(frames==160)mmvr::RequestNativeCapture((std::string("native-town-ready-")+std::to_string(visit)).c_str());
 if(frames==220){
  const int areaFade=MMVR_AreaFadeType(play,TRANS_TYPE_FADE_BLACK);
  const int customFade=MMVR_AreaFadeType(play,TRANS_TYPE_CIRCLE);
  if(visit)log<<",";
  log<<"{\"scene\":"<<play->sceneId<<",\"day\":"<<int(gSaveContext.save.day)<<",\"titleFrames\":"<<titleFrames<<",\"badRoute\":"<<badRoute
     <<",\"locationFrames\":"<<locationFrames<<",\"badLocationRoute\":"<<badLocationRoute
     <<",\"dayTitleExpected\":"<<(visit<5)<<",\"localTransitionFrames\":"<<localTransitionFrames<<",\"badLocalRoute\":"<<badLocalRoute
     <<",\"ready\":"<<mmvr::ImmersiveScene(facts)<<",\"bgm\":"<<AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN)
     <<",\"areaFadeWhite\":"<<(areaFade==TRANS_TYPE_FADE_WHITE)<<",\"specialFadePreserved\":"<<(customFade==TRANS_TYPE_CIRCLE)<<",\"expectedBgm\":"<<NA_BGM_CLOCK_TOWN_MAIN_SEQUENCE<<",\"audioNonzero\":"<<(MMVR_DebugAudioNonzero()-audioStart)<<"}"<<std::flush;
  if(visit<7)next(visit+1);else{log<<"]";log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
 }
 return pad;
}
