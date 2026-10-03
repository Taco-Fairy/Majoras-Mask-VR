#pragma once
#include "2s2h/resource/type/Cutscene.h"
#include <ship/resource/File.h>
extern "C" {
#include "z64bombers_notebook.h"
float MMVR_DialogueScale(int);
int MMVR_TextAlpha(int);
int MMVR_TextBoxAlpha(int);
s32* ResourceMgr_LoadCSByName(const char* path);
extern BombersNotebook sBombersNotebook;
extern u8 sBombersNotebookOpen;
}
// Private full native draw lifecycle. Keeps the player's real saves untouched.
static void NativeNotebookTest(PlayState* play, unsigned tick) {
    static SaveContext save;
    static PauseContext pause;
    static MessageContext msg;
    static BombersNotebook notebook;
    static u8 open;
    static mmvr::Settings settings;
    static std::ofstream log;
    static bool passed = true;
    auto check = [&](bool ok, const char* name) { if (!ok) { passed=false; log<<"FAIL "<<name<<"\n"; } };
    if (tick == 60) {
        log.open("native-notebook-book.log");
        save=gSaveContext; pause=play->pauseCtx; msg=play->msgCtx;
        notebook=sBombersNotebook; open=sBombersNotebookOpen; settings=mmvr::GetSettings();
        const char* names[] = {"gTerminaFieldSkullKidFlashbackRainCs",
            "gTerminaFieldSkullKidFlashbackDrawingWithFairiesCs",
            "gTerminaFieldSkullKidFlashbackPlayingWithFairiesCs"};
        for (const char* name : names) {
            const auto asset = std::string("__OTR__scenes/nonmq/Z2_00KEIKOKU/") + name;
            const auto* loaded = ResourceMgr_LoadCSByName(asset.c_str());
            check(loaded && SOH::Cutscene::IsHistoricalFlashback(loaded), "archive-historical-script");
            const void* ptr;
            {
                auto init=std::make_shared<Ship::ResourceInitData>();
                init->Path=std::string("scenes/nonmq/Z2_00KEIKOKU/")+name;
                SOH::Cutscene resource(init);resource.commands={0,0};resource.hasPlayerCue=true;
                ptr=resource.GetPointer();
                check(SOH::Cutscene::IsHistoricalFlashback(ptr),"historical-script-registered");
                check(SOH::Cutscene::HasPlayerParticipation(ptr),"player-metadata-preserved");
            }
            check(!SOH::Cutscene::IsHistoricalFlashback(ptr),"historical-resource-teardown");
        }
        auto init=std::make_shared<Ship::ResourceInitData>();
        init->Path="scenes/nonmq/Z2_00KEIKOKU/gTerminaFieldSkullKidDrawingStartCs";
        SOH::Cutscene lead(init);lead.commands={0,0};
        check(!SOH::Cutscene::IsHistoricalFlashback(lead.GetPointer()),"local-graffiti-lead-in-preserved");
        mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
        play->pauseCtx.state=PAUSE_STATE_MAIN;play->pauseCtx.bombersNotebookOpen=true;
        sBombersNotebookOpen=true;
        BombersNotebook_Init(&sBombersNotebook);
        Input input{};BombersNotebook_Update(play,&sBombersNotebook,&input);
        check(sBombersNotebook.loadState==BOMBERS_NOTEBOOK_LOAD_STATE_DONE,"native-notebook-loaded");
        check(MMVR_NotebookBook(),"physical-book-route");
        for(float size:{0.f,60.f,100.f,200.f}) {
            mmvr::GetSettings().Set(mmvr::Setting::TextBoxSize,size);
            mmvr::GetSettings().Set(mmvr::Setting::TextSize,size);
            mmvr::GetSettings().Set(mmvr::Setting::TextOpacity,0);
            mmvr::GetSettings().Set(mmvr::Setting::TextBoxOpacity,.35f);
            check(MMVR_DialogueScale(0)==1 && MMVR_DialogueScale(1)==1,"notebook-native-text-layout");
            check(MMVR_TextAlpha(200)==200 && MMVR_TextBoxAlpha(180)==180,"notebook-native-text-alpha");
        }
        mmvr::GetSettings().Set(mmvr::Setting::TextBoxSize,60);
        mmvr::GetSettings().Set(mmvr::Setting::TextSize,100);
        mmvr::SetNotebook(true);check(mmvr::NotebookActive(),"physical-book-composition");
        // Native event selection shows its description immediately; there is no
        // separate A-button action. Exercise both touch and original stick routes.
        const XrPosef hand={{0,0,0,1},{0,-.2f,-.55f}};
        auto tap=[&](float x,float y) { mmvr::SetNativeTestNotebook(hand,x,y); Input neutral{};
            BombersNotebook_Update(play,&sBombersNotebook,&neutral); };
        CLEAR_WEEKEVENTREG(gBombersNotebookWeekEventFlags[BOMBERS_NOTEBOOK_EVENT_LEARNED_SECRET_CODE]);
        SET_WEEKEVENTREG(gBombersNotebookWeekEventFlags[BOMBERS_NOTEBOOK_PERSON_BOMBERS]);
        sBombersNotebook.cursorPage=sBombersNotebook.cursorPageRow=0;sBombersNotebook.scrollAmount=0;
        tap(150,120);check(sBombersNotebook.cursorEntry==0,"unknown-event-not-selectable");
        SET_WEEKEVENTREG(gBombersNotebookWeekEventFlags[BOMBERS_NOTEBOOK_EVENT_LEARNED_SECRET_CODE]);
        tap(150,120);check(sBombersNotebook.cursorEntry==3,"touch-known-event");
        check(play->msgCtx.currentTextId!=0,"event-description-visible");
        Input left{};left.rel.stick_x=-60;BombersNotebook_Update(play,&sBombersNotebook,&left);
        check(sBombersNotebook.cursorEntry==0,"stick-left-returns-to-person-without-negative-index");
        tap(450,120);check(sBombersNotebook.cursorEntry==15,"touch-event-on-right-page");
        tap(65,170);check(sBombersNotebook.cursorPageRow==1&&sBombersNotebook.cursorEntry==0,"touch-person");
        tap(60,330);check(sBombersNotebook.cursorPage==4,"touch-next-page");
        tap(60,90);check(sBombersNotebook.cursorPage==0,"touch-previous-page");
        gSaveContext=save;
        sBombersNotebook.cursorPageRow=0; sBombersNotebook.cursorEntry=0;
        mmvr::SetNativeTestNotebook(hand,NAN,NAN);
    }
    if (tick == 63) mmvr::RequestNativeCapture("native-notebook-book");
    if (tick == 66) {
        check(sBombersNotebookOpen && MMVR_NotebookBook(),"book-survived-native-draws");
        check(std::memcmp(&save.save,&gSaveContext.save,sizeof(Save))==0,"ordinary-save-unchanged");
        gSaveContext=save;play->pauseCtx=pause;play->msgCtx=msg;
        sBombersNotebook=notebook;sBombersNotebookOpen=open;mmvr::GetSettings()=settings;
        mmvr::SetNotebook(false);mmvr::SetNativeTestTracking(false);
        log<<(passed?"PASS":"FAIL")<<" notebook-book nativeDrawFrames=6\n";log.close();
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
}

