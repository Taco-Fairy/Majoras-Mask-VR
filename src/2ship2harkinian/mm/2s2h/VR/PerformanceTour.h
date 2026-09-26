#pragma once
#include "renderer_metrics.h"
#include "culling_audit.h"
#include "FormAim.h"
// Opt-in headset tour: only scene transitions are scripted. The pad and head
// pose remain live, and the native clock is never frozen.
static mmvr::Pad NativePerformanceInteractiveTour(PlayState* play,unsigned tick){
 using Clock=std::chrono::steady_clock;
 struct Stop{const char* name;int entrance;};
 static const Stop stops[]={
  {"south-clock-town",ENTRANCE(SOUTH_CLOCK_TOWN,0)},
  {"east-clock-town",ENTRANCE(EAST_CLOCK_TOWN,0)},
  {"great-bay",ENTRANCE(GREAT_BAY_COAST,0)},
  {"telescope",ENTRANCE(TERMINA_FIELD,10)},
  {"termina-field",ENTRANCE(TERMINA_FIELD,0)},
  {"south-clock-town-repeat",ENTRANCE(SOUTH_CLOCK_TOWN,0)}
 };
 static std::ofstream log=[](){
  std::ofstream out("native-performance-interactive.log");
  if(const char* token=std::getenv("MMVR_SESSION_TOKEN"))out<<"session "<<token<<"\n";
  out<<"mode interactive timePolicy native saveProtected "
     <<(std::getenv("MMVR_PROTECT_SAVES")&&std::strcmp(std::getenv("MMVR_PROTECT_SAVES"),"1")==0)
     <<"\n"<<std::flush;
  return out;
 }();
 static int visit=-1;
 static bool entering=false,returnRequested=false,complete=false,focusLost=false;
 static Clock::time_point enteredAt{},readyAt{},lastSample{},focusLostAt{};
 auto pad=mmvr::ConsumePad();
 auto now=Clock::now();
 auto steadyMs=[](Clock::time_point t){return std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();};
 if(complete)return pad;
 if(gSaveContext.fileNum!=2){log<<"ERROR expected protected slot 2, got "<<gSaveContext.fileNum<<"\n"<<std::flush;complete=true;return pad;}
 if(!std::getenv("MMVR_PROTECT_SAVES")||std::strcmp(std::getenv("MMVR_PROTECT_SAVES"),"1")!=0){
  log<<"ERROR saves not protected\n"<<std::flush;complete=true;return pad;
 }
 if(!mmvr::InputFocused()){
  if(!focusLost){focusLost=true;focusLostAt=now;log<<"focus-lost steadyMs="<<steadyMs(now)<<"\n"<<std::flush;}
  return pad;
 }
 if(focusLost){
  const auto pause=now-focusLostAt;
  if(readyAt!=Clock::time_point{})readyAt+=pause;
  if(enteredAt!=Clock::time_point{})enteredAt+=pause;
  focusLost=false;log<<"focus-resumed steadyMs="<<steadyMs(now)<<"\n"<<std::flush;
 }
 if(returnRequested){
  if(MMVR_DebugRoomActive(play)){
   log<<"COMPLETE hall-ready scene="<<play->sceneId<<" steadyMs="<<steadyMs(now)<<"\n"<<std::flush;
   complete=true;
  }
  return pad;
 }
 auto start=[&](){
  ++visit;entering=true;enteredAt=now;readyAt={};lastSample={};
  gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;
  gSaveContext.save.cutsceneIndex=0;
  play->nextEntrance=stops[visit].entrance;
  play->transitionTrigger=TRANS_TRIGGER_START;
  play->transitionType=TRANS_TYPE_FADE_WHITE;
  log<<"enter "<<stops[visit].name<<" index="<<visit<<" steadyMs="<<steadyMs(now)<<"\n"<<std::flush;
 };
 if(visit<0){if(tick>60&&play->transitionTrigger==TRANS_TRIGGER_OFF)start();return pad;}
 if(entering){
  if(play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF||
     gSaveContext.save.entrance!=stops[visit].entrance){
   if(now-enteredAt>std::chrono::seconds(45)){
    log<<"ERROR transition timeout "<<stops[visit].name<<" scene="<<play->sceneId
       <<" entrance="<<gSaveContext.save.entrance<<" steadyMs="<<steadyMs(now)<<"\n"<<std::flush;
    complete=true;
   }
   return pad;
  }
  entering=false;readyAt=lastSample=now;
  log<<"ready "<<stops[visit].name<<" scene="<<play->sceneId<<" room="<<int(play->roomCtx.curRoom.num)
     <<" steadyMs="<<steadyMs(now)<<" nativeFrame="<<play->gameplayFrames<<"\n"<<std::flush;
 }
 if(now-lastSample>=std::chrono::seconds(1)){
  lastSample=now;
  auto* p=GET_PLAYER(play);
  if(!p){log<<"ERROR player unavailable scene="<<play->sceneId<<" steadyMs="<<steadyMs(now)<<"\n"<<std::flush;complete=true;return pad;}
  const auto& pos=p->actor.world.pos;
  const auto head=mmvrgame::FormHeadPose();
  log<<"sample "<<stops[visit].name<<" second="
     <<std::chrono::duration_cast<std::chrono::seconds>(now-readyAt).count()
     <<" steadyMs="<<steadyMs(now)<<" scene="<<play->sceneId<<" nativeFrame="<<play->gameplayFrames
     <<" nativeTime="<<CURRENT_TIME<<" pos="<<pos.x<<","<<pos.y<<","<<pos.z
     <<" yaw="<<p->actor.shape.rot.y<<" headYaw=";
  if(head.m[3][3])log<<mmvr::PoseYaw(head);else log<<"unavailable";
  log<<" stick="<<int(pad.x)<<","<<int(pad.y)<<" buttons="<<pad.buttons<<"\n"<<std::flush;
 }
 if(now-readyAt>=std::chrono::seconds(60)&&play->transitionTrigger==TRANS_TRIGGER_OFF){
  log<<"stop-complete "<<stops[visit].name<<" scene="<<play->sceneId
     <<" steadyMs="<<steadyMs(now)<<"\n"<<std::flush;
  if(visit+1<int(ARRAY_COUNT(stops))){
   start();
  }else{
   returnRequested=MMVR_DebugTrialReturn(play)!=0;
   log<<"return-requested success="<<returnRequested<<" steadyMs="<<steadyMs(now)<<"\n"<<std::flush;
  }
 }
 return pad;
}
// Local diagnostic only. Normal save writes are blocked before this mode boots.
static mmvr::Pad NativePerformanceTour(PlayState* play,unsigned tick){
 struct Stop{const char* name;int entrance;};
 static const Stop stops[]={
  {"south-clock-town",ENTRANCE(SOUTH_CLOCK_TOWN,0)},{"east-clock-town",ENTRANCE(EAST_CLOCK_TOWN,0)},
  {"west-clock-town",ENTRANCE(WEST_CLOCK_TOWN,0)},{"north-clock-town",ENTRANCE(NORTH_CLOCK_TOWN,0)},
  {"termina-field",ENTRANCE(TERMINA_FIELD,0)},{"telescope",ENTRANCE(TERMINA_FIELD,10)},
  {"swamp",ENTRANCE(SOUTHERN_SWAMP_POISONED,0)},{"woodfall",ENTRANCE(WOODFALL_TEMPLE,0)},
  {"goron-village",ENTRANCE(GORON_VILLAGE_WINTER,0)},{"great-bay",ENTRANCE(GREAT_BAY_COAST,0)},
  {"ikana",ENTRANCE(IKANA_CANYON,0)},{"south-clock-town-repeat",ENTRANCE(SOUTH_CLOCK_TOWN,0)}};
 // Catalog visits are entrance-only coverage, not room/story verification.
 static const Stop catalog[]={
#define DEFINE_SCENE(segment,scene,title,draw,restriction,flags,entrance,mapIndex,label) {#scene,ENTRANCE(entrance,0)},
#define DEFINE_SCENE_UNSET(scene)
#include "tables/scene_table.h"
#undef DEFINE_SCENE
#undef DEFINE_SCENE_UNSET
 };
 static const bool catalogMode=std::getenv("MMVR_PERFORMANCE_CATALOG_START")!=nullptr;
 static std::vector<Stop> selected;
 static bool catalogValid=true;
 static const bool freezeTime=[](){const char* value=std::getenv("MMVR_PERFORMANCE_FREEZE_TIME");return value&&std::string(value)=="1";}();
 static const bool hotspots=std::getenv("MMVR_PERFORMANCE_HOTSPOTS")!=nullptr;
 static constexpr int hotspotRoute[]={5,9,1};
 static int route=-1;
 static int visit=-1,frames=0,wait=0;static bool entering=false;static std::ofstream log=[](){
  std::ofstream result("native-performance-tour.log");
  if(const char* token=std::getenv("MMVR_SESSION_TOKEN"))result<<"session "<<token<<"\n"<<std::flush;
  result<<"timePolicy "<<(freezeTime?"frozen":"native")<<"\n"<<std::flush;
  result<<"detailedRendererProfile "<<mmvr::RendererMeasurementEnabled()<<"\n"<<std::flush;
  return result;
 }();
 mmvr::Pad pad;pad.active=true;
 // Run the exact-matrix comparison in the actual ARM64 app before any timed
 // scene begins. It restores the live recordings and native matrix stack.
 if(tick==1){
  if(catalogMode){
   const char* protect=std::getenv("MMVR_PROTECT_SAVES");
   const char* startText=std::getenv("MMVR_PERFORMANCE_CATALOG_START");
   const char* countText=std::getenv("MMVR_PERFORMANCE_CATALOG_COUNT");
   char* end=nullptr;long start=std::strtol(startText,&end,10);
   catalogValid=protect&&std::string(protect)=="1"&&*startText&&!*end&&start>=0&&!hotspots;
   long count=countText?std::strtol(countText,&end,10):8;
   catalogValid=catalogValid&&(!countText||(*countText&&!*end))&&count>=1&&count<=8;
   std::vector<Stop> eligible;
   const char* priority[]={"SCENE_BOMYA","SCENE_AYASHIISHOP","SCENE_YADOYA","SCENE_POSTHOUSE","SCENE_FISHERMAN","SCENE_F01","SCENE_22DEKUCITY","SCENE_HAKUGIN"};
   for(const auto* wanted:priority)for(const auto& stop:catalog)if(std::string(stop.name)==wanted)eligible.push_back(stop);
   for(const auto& stop:catalog){
    const std::string name=stop.name;bool already=false;for(const auto& entry:eligible)if(name==entry.name)already=true;
    if(already)continue;
    // Boss/moon/intro script scenes require a purpose-built story fixture.
    if(name.find("_BS")!=std::string::npos||name.find("LAST_")!=std::string::npos||name=="SCENE_SOUGEN"||name=="SCENE_SPOT00"||name=="SCENE_OPENINGDAN"||name=="SCENE_KONPEKI_ENT"){
     log<<"excluded "<<name<<" requires scripted boss/moon/intro fixture\n";continue;
    }
    eligible.push_back(stop);
   }
   catalogValid=catalogValid&&start<long(eligible.size())&&count<=long(eligible.size())-start;
   if(!catalogValid){log<<"ERROR invalid/unprotected catalog request\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
   selected.assign(eligible.begin()+start,eligible.begin()+start+count);
   log<<"catalog start="<<start<<" count="<<count<<" eligible="<<eligible.size()<<" total="<<ARRAY_COUNT(catalog)<<"\n";
   log<<"requirements entrance-zero day-two-noon existing-story-flags; interior rooms and time variants NOT covered\n"<<std::flush;
  }
  FrameInterpolation_VerifyScratch();
  auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
  window->GetInterpreterWeak().lock()->VerifyNativeBoundsCulling();
  window->GetInterpreterWeak().lock()->VerifyTextureOwnershipMoves();
  log<<"cullingPixels "<<mmvr::CullingPixelsEnabled()<<"\n"<<std::flush;
  log<<"nativeBounds "<<(mmvr::CullingAuditEnabled()?"observe-only":mmvr::CullingReferenceEnabled()?"reference":"enabled")<<"\n"<<std::flush;
  const char* reference=std::getenv("MMVR_INTERPOLATION_REFERENCE");
  log<<"interpolationPath "<<(reference&&std::string(reference)=="1"?"reference":"compiled")<<"\n"<<std::flush;
 }
 // The diagnostic input hook still runs while focus-loss pauses gameplay.
 // Do not advance visits/timeouts or label that paused scene as measured.
 static bool waitingForFocus=false;
 static std::chrono::steady_clock::time_point focusLostAt;
 const bool flat=std::getenv("MMVR_FLAT_PROFILE")!=nullptr||std::getenv("MMVR_PERFORMANCE_NO_XR")!=nullptr;
 if(!flat&&!mmvr::InputFocused()){
  auto now=std::chrono::steady_clock::now();
  if(!waitingForFocus){waitingForFocus=true;focusLostAt=now;log<<"waiting XR focus\n"<<std::flush;}
  if(now-focusLostAt>std::chrono::seconds(120)){log<<"ERROR XR focus unavailable; performance not measured\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
  return pad;
 }
 if(waitingForFocus){waitingForFocus=false;log<<"resumed XR focus\n"<<std::flush;}
 if(catalogMode&&(!catalogValid||selected.empty()))return pad;
 const Stop* activeStops=catalogMode?selected.data():stops;
 auto next=[&](){++route;visit=hotspots?hotspotRoute[route]:route;frames=wait=0;entering=true;
  gSaveContext.save.day=gSaveContext.save.eventDayCount=2;gSaveContext.save.time=CLOCK_TIME(12,0);gSaveContext.save.isNight=false;
  gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;gSaveContext.save.cutsceneIndex=0;
  play->nextEntrance=activeStops[visit].entrance;play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_WHITE;
  log<<"enter "<<activeStops[visit].name<<"\n"<<std::flush;
 };
 if(visit<0){if(tick>60)next();return pad;}
 if(entering){
  if(play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF||gSaveContext.save.entrance!=activeStops[visit].entrance){
   if(++wait>900){log<<"ERROR transition timeout entrance="<<gSaveContext.save.entrance<<" expected="<<activeStops[visit].entrance<<" mode="<<int(play->transitionMode)<<" trigger="<<int(play->transitionTrigger)<<" nativeFrame="<<play->gameplayFrames<<"\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;
  }entering=false;log<<"ready "<<activeStops[visit].name<<" scene="<<play->sceneId<<" room="<<int(play->roomCtx.curRoom.num)<<"\n"<<std::flush;
 }
 ++frames;auto* p=GET_PLAYER(play);gSaveContext.save.saveInfo.playerData.health=gSaveContext.save.saveInfo.playerData.healthCapacity;
 if(freezeTime){play->envCtx.sceneTimeSpeed=0;R_TIME_SPEED=0;}
 // Hold and release the native zoom button during the actual telescope player action.
 if(!catalogMode&&visit==5&&frames>=180&&frames<330)pad.buttons=BTN_A;
 if(frames==150||frames==310||frames==440){
  log<<"sample "<<activeStops[visit].name<<" frame="<<frames<<" fovy="<<play->view.fovy<<" nativeTime="<<CURRENT_TIME<<" timeSpeed="<<R_TIME_SPEED<<" telescope="<<bool(play->actorCtx.flags&ACTORCTX_FLAG_TELESCOPE_ON)<<"\n"<<std::flush;
  auto name=std::string("native-tour-")+activeStops[visit].name+"-"+std::to_string(frames);mmvr::RequestNativeCapture(name.c_str());mmvr::RequestCullingPixelCheck(name);
 }
 if(frames==450)mmvr::WriteCullingAuditVisit(activeStops[visit].name);
 if(frames>=450){if(route+1<(catalogMode?int(selected.size()):hotspots?int(ARRAY_COUNT(hotspotRoute)):int(ARRAY_COUNT(stops))))next();else{auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());window->GetInterpreterWeak().lock()->WriteTextureCacheDiagnostics("native-texture-cache-stats.json");window->GetInterpreterWeak().lock()->WriteColorCombinerCacheDiagnostics("native-color-combiner-cache-stats.json");window->GetInterpreterWeak().lock()->WriteTrianglePreparationDiagnostics("native-triangle-preparation-stats.json");log<<"COMPLETE\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}}
 return pad;
}
