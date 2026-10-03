#pragma once
// Disposable native fixture only. No public settings, slots or speed changes.
extern "C" void KaleidoScope_Update(PlayState*);
static void NativeSaveContinueTest(PlayState* play) {
    std::ofstream log("native-save-continue.log");
    unsigned checks=0, failures=0;
    auto check=[&](bool ok,const char* name){++checks;log<<name<<"="<<ok<<"\n";failures+=!ok;};
    auto cwd=std::filesystem::current_path();
    const bool isolated=cwd.parent_path().filename().string().rfind("game-audit-",0)==0 &&
        cwd.parent_path().parent_path().filename()=="artifacts" && std::getenv("MMVR_SESSION_TOKEN") &&
        std::getenv("MMVR_PROTECT_SAVES");
    check(isolated,"isolated-directory");
    if(!isolated){log<<"FAIL ordinary-save checks="<<checks<<"\n";log.close();std::_Exit(2);}
    for(const char* key:{"gEnhancements.Saving.PersistentOwlSaves","gEnhancements.Saving.PauseSave",
                         "gEnhancements.Saving.RememberSaveLocation"}) {
        CVarSetInteger(key,1);ShipInit::Init(key);
    }
    auto* p=GET_PLAYER(play);
    p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
    play->sceneId=SCENE_BACKTOWN;play->transitionTrigger=TRANS_TRIGGER_OFF;play->transitionMode=TRANS_MODE_OFF;
    play->sramCtx.status=0;play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
    gSaveContext.fileNum=2;gSaveContext.flashSaveAvailable=1;
    auto loadAndCheck=[&](int rupees) {
        auto file=std::make_unique<FileSelectState>();file->isOwlSave[2+FILE_NUM_OWL_SAVE_OFFSET]=true;
        Sram_OpenSave(file.get(),&play->sramCtx);
        check(gSaveContext.save.entrance==ENTRANCE(NORTH_CLOCK_TOWN,0),"disk-entrance");
        check(gSaveContext.save.day==2 && gSaveContext.save.time==CLOCK_TIME(15,30),"disk-day-time");
        check(gSaveContext.save.saveInfo.playerData.rupees==rupees,"disk-rupees");
        check(!GameInteractor_Should(VB_DELETE_OWL_SAVE,true),"owl-retained-on-continue");
    };
    const bool reader=std::string(std::getenv("MMVR_SAVE_CONTINUE_TEST"))=="read";
    if(reader) {
        loadAndCheck(23);loadAndCheck(23);
    } else {
        gSaveContext.save.shipSaveInfo.pauseSaveEntrance=ENTRANCE(NORTH_CLOCK_TOWN,0);
        gSaveContext.save.isOwlSave=true;
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveLoad>(2);
        gSaveContext.save.day=2;gSaveContext.save.time=CLOCK_TIME(15,30);
        gSaveContext.save.saveInfo.playerData.rupees=17;
        auto* input=CONTROLLER1(&play->state);*input={};input->press.button=BTN_B;
        check(GameInteractor_Should(VB_SAVE_ON_B_BUTTON_IN_PAUSE_MENU,false),"pause-B-offers-save");
#ifdef _WIN32
        _putenv_s("MMVR_PROTECT_SAVES","0");
#else
        setenv("MMVR_PROTECT_SAVES","0",1);
#endif
        play->pauseCtx.state=PAUSE_STATE_SAVEPROMPT;
        play->pauseCtx.savePromptState=PAUSE_SAVEPROMPT_STATE_1;
        play->pauseCtx.promptChoice=0; // Native PAUSE_PROMPT_YES (private overlay constant).
        input->press.button=BTN_A;
        KaleidoScope_Update(play);
        check(play->pauseCtx.savePromptState==PAUSE_SAVEPROMPT_STATE_4,"pause-confirmation-wrote-save");
        loadAndCheck(17);
        play->pauseCtx.state=PAUSE_STATE_OFF;play->sramCtx.status=0;*input={};
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveLoad>(2);
        gSaveContext.save.saveInfo.playerData.rupees=23;
        check(SavingEnhancements_SaveGame(),"VR-save-accepted");
        loadAndCheck(23);
#ifdef _WIN32
        _putenv_s("MMVR_PROTECT_SAVES","1");
#else
        setenv("MMVR_PROTECT_SAVES","1",1);
#endif
    }
    // Test real registered suppression hooks across cycle, statue and remembered saves.
    // Cursed Deku has the recovered ocarina but not the wearable mask/healing song.
    gSaveContext.save.playerForm=PLAYER_FORM_DEKU;
    INV_CONTENT(ITEM_OCARINA_OF_TIME)=ITEM_OCARINA_OF_TIME;
    INV_CONTENT(ITEM_MASK_DEKU)=ITEM_NONE;
    gSaveContext.save.saveInfo.inventory.questItems&=~(1u<<QUEST_SONG_HEALING);
    s16 cs=1;
    for(int owl=0;owl<2;++owl) for(int remembered=0;remembered<2;++remembered) {
        gSaveContext.save.isOwlSave=owl;
        gSaveContext.save.shipSaveInfo.pauseSaveEntrance=remembered?ENTRANCE(CLOCK_TOWER_INTERIOR,2):-1;
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveLoad>(2);
        play->sceneId=Entrance_GetSceneIdAbsolute(ENTRANCE(CLOCK_TOWER_INTERIOR,2));
        check(play->sceneId==SCENE_INSIDETOWER,"authored-clock-tower-interior");
        check(GameInteractor_Should(VB_START_CUTSCENE,true,&cs,(Actor*)nullptr),"salesman-story-not-suppressed");
        play->sceneId=SCENE_BACKTOWN;
        check(bool(GameInteractor_Should(VB_START_CUTSCENE,true,&cs,(Actor*)nullptr))==!(owl&&remembered),"only-remembered-arrivals-skipped");
        Input input{};GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPassPlayerInputs>(&input);
        check(GameInteractor_Should(VB_START_CUTSCENE,true,&cs,(Actor*)nullptr),"normal-cutscenes-restored");
    }
    log<<(failures?"FAIL":"PASS")<<" ordinary-save checks="<<checks<<"\n";log.close();std::_Exit(failures?2:0);
}
