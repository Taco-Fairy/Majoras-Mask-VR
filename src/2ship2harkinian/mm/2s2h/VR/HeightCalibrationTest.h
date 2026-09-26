#pragma once
static void NativeHeightCalibrationCheck(PlayState* play) {
    auto* p=GET_PLAYER(play);auto settings=mmvr::GetSettings();auto* profile=mmvr::ProfileForForm(p->transformation);
    auto id=profile->eyeHeight;const auto& definition=mmvr::SettingDefinitions[size_t(id)];
    mmvr::GetSettings().Set(mmvr::Setting::ModelFormHeight,1);
    mmvr::GetSettings().Set(id,definition.initial);
    Player uncalibrated=*p;
    mmvr::GetSettings().Set(mmvr::Setting::ExperimentalFirstPersonMotion,0);
    uncalibrated.actor.focus.pos.y=uncalibrated.actor.world.pos.y+20.f;
    const float stableSpawnLow=mmvrgame::FormEyeHeight(&uncalibrated);
    uncalibrated.actor.focus.pos.y=uncalibrated.actor.world.pos.y+110.f;
    const float stableSpawnHigh=mmvrgame::FormEyeHeight(&uncalibrated);
    const bool stableFallback=std::abs(stableSpawnHigh-stableSpawnLow)<.001f;
    mmvr::GetSettings().Set(mmvr::Setting::ExperimentalFirstPersonMotion,1);
    uncalibrated.actor.focus.pos.y=uncalibrated.actor.world.pos.y+20.f;
    const float experimentalLow=mmvrgame::FormEyeHeight(&uncalibrated);
    uncalibrated.actor.focus.pos.y=uncalibrated.actor.world.pos.y+110.f;
    const float experimentalHigh=mmvrgame::FormEyeHeight(&uncalibrated);
    const bool experimentalOptIn=std::abs(experimentalHigh-experimentalLow)>50.f;
    mmvr::GetSettings().Set(mmvr::Setting::ExperimentalFirstPersonMotion,0);
    float model=p->actor.focus.pos.y-p->actor.world.pos.y;
    float automatic=mmvrgame::FormEyeHeight(p);
    mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.timeSeconds=90000+p->transformation*10;f.epoch=90000+p->transformation;
    mmvrgame::ResetTestCamera();
    float cameraHeight=0;
    for(int i=0;i<120;++i){f.timeSeconds+=1.0/120;auto frame=mmvrgame::TestCameraFrame(f);cameraHeight=mmvr::InversePose(frame.view).m[3][1]-p->actor.world.pos.y;}
    float baseline=cameraHeight;
    mmvr::GetSettings().Set(id,std::min(definition.maximum,definition.initial+10));
    float requested=automatic+mmvr::GetSettings().Get(id)-definition.initial;
    for(int i=0;i<120;++i){f.timeSeconds+=1.0/120;auto frame=mmvrgame::TestCameraFrame(f);cameraHeight=mmvr::InversePose(frame.view).m[3][1]-p->actor.world.pos.y;}
    float raised=cameraHeight;
    mmvr::GetSettings().Set(id,definition.initial-1);
    for(int i=0;i<120;++i){f.timeSeconds+=1.0/120;auto frame=mmvrgame::TestCameraFrame(f);cameraHeight=mmvr::InversePose(frame.view).m[3][1]-p->actor.world.pos.y;}
    bool lowered=std::abs(cameraHeight-(automatic-1))<.05f;
    cameraHeight=raised;
    mmvr::MenuState menu;menu.playerForm=p->transformation;
    menu.tab=p->transformation==PLAYER_FORM_HUMAN?mmvr::ViewTab:mmvr::FormsTab;
    for(int section=0;section<mmvr::MenuSectionCount;++section)
        if(mmvr::MenuSections[section].tab==menu.tab)menu.expanded[section]=true;
    bool menuMaps=false;
    for(int row=0;row<menu.VisibleRows();++row){menu.row=row;menuMaps|=menu.Selected()==int(id);}
    const bool spawnCalibrationContinuous=std::abs(automatic-stableSpawnHigh)<6.f;
    const auto oldFlags1=p->stateFlags1,oldFlags3=p->stateFlags3;
    mmvr::GetSettings().Set(id,definition.initial);
    p->stateFlags1|=PLAYER_STATE1_400000;
    const float shieldHeight=mmvrgame::FormEyeHeight(p);
    p->stateFlags1=oldFlags1;p->stateFlags3|=PLAYER_STATE3_1000;
    const float curlHeight=mmvrgame::FormEyeHeight(p);
    p->stateFlags3=oldFlags3|PLAYER_STATE3_8000000;
    const bool regularRollStable=std::abs(mmvrgame::FormEyeHeight(p)-automatic)<.001f;
    p->stateFlags3=oldFlags3;
    const bool postureRestored=std::abs(mmvrgame::FormEyeHeight(p)-automatic)<.001f;
    const bool compact=p->transformation==PLAYER_FORM_GORON;
    const bool shieldWorks=compact ? shieldHeight<automatic-8 : std::abs(shieldHeight-automatic)<.001f;
    const bool curlWorks=p->transformation==PLAYER_FORM_GORON ? curlHeight<automatic-25 : std::abs(curlHeight-automatic)<.001f;
    bool passed=shieldWorks&&curlWorks&&postureRestored&&regularRollStable&&stableFallback&&experimentalOptIn&&spawnCalibrationContinuous&&lowered&&menuMaps&&std::abs(automatic-model)<6&&std::abs(baseline-automatic)<.05f&&std::abs(cameraHeight-requested)<.05f;
    std::ofstream log("native-height-calibration.jsonl",std::ios::app);
    log<<"{\"shieldHeight\":"<<shieldHeight<<",\"curlHeight\":"<<curlHeight<<",\"postureRestored\":"<<postureRestored<<",\"regularRollStable\":"<<regularRollStable<<",\"form\":"<<int(p->transformation)<<",\"model\":"<<model<<",\"automatic\":"<<automatic<<",\"stableSpawnLow\":"<<stableSpawnLow<<",\"stableSpawnHigh\":"<<stableSpawnHigh<<",\"spawnCalibrationContinuous\":"<<spawnCalibrationContinuous<<",\"experimentalLow\":"<<experimentalLow<<",\"experimentalHigh\":"<<experimentalHigh<<",\"cameraBaseline\":"<<baseline<<",\"requested\":"<<requested<<",\"cameraAdjusted\":"<<cameraHeight<<",\"menuMaps\":"<<menuMaps<<",\"stableFallback\":"<<stableFallback<<",\"experimentalOptIn\":"<<experimentalOptIn<<",\"loweredOneUnit\":"<<lowered<<",\"passed\":"<<passed<<"}\n";
    mmvr::GetSettings()=settings;mmvrgame::ResetTestCamera();
}
