#pragma once
#include "ScenePresentation.h"
#include "hud_layout.h"
#include "z64eff_blure.h"
extern "C" void EffectBlure_GetComputedValues(EffectBlure*,s32,f32,Vec3s*,Vec3s*,Color_RGBA8*,Color_RGBA8*);
extern "C" void EffectBlure_Draw(void*,GraphicsContext*);
extern "C" void MMVR_VerifySettingsRepair(PlayState* play) {
    int failures=0;
    std::ofstream log("native-settings-repair.log");
    auto check=[&](bool ok,const char* name){log<<(ok?"PASS ":"FAIL ")<<name<<"\n";failures+=!ok;};
    auto* p=GET_PLAYER(play);auto saved=*p;
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    p->transformation=PLAYER_FORM_DEKU;
    auto* effect=static_cast<EffectBlure*>(Effect_GetByIndex(p->meleeWeaponEffectIndex[0]));
    check(effect!=nullptr,"player trail resource exists");
    if(effect){
        auto savedEffect=*effect; effect->calcMode=0;effect->flags=EFFECT_BLURE_FLAG_10;
        Vec3s tip{},base{};Color_RGBA8 a{},b{};
        for(float opacity:{0.f,65.f,100.f,0.f}){
            Change(mmvr::Setting::DekuSpinOpacity,opacity);
            EffectBlure_GetComputedValues(effect,0,0.f,&tip,&base,&a,&b);
            check(a.a==int(255*opacity*.01f)&&b.a==int(255*opacity*.01f),"live native vertex opacity including forced-white trail");
        }
        Gfx* before=play->state.gfxCtx->polyXlu.p;
        EffectBlure_Draw(effect,play->state.gfxCtx);
        check(before==play->state.gfxCtx->polyXlu.p,"zero opacity emits no native trail geometry");
        *effect=savedEffect;
    }
    *p=saved;
    const auto oldMsg=play->msgCtx.msgMode; const auto oldCs=play->csCtx.state;
    const auto oldPause=play->pauseCtx.state; const auto oldTransition=play->transitionTrigger;
    play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
    play->pauseCtx.state=PAUSE_STATE_OFF;play->transitionTrigger=TRANS_TRIGGER_OFF;
    p->csAction=PLAYER_CSACTION_NONE;p->heldActor=nullptr;p->heldItemAction=PLAYER_IA_NONE;
    mmvrgame::ClearFormTracking();mmvrgame::ClearBow();mmvr::CancelHeldMask();
    mmvr::GetSettings().Set(mmvr::Setting::PhysicalFists,1);
    p->transformation=PLAYER_FORM_HUMAN;mmvrgame::ProcessGoronInput(play);
    p->transformation=PLAYER_FORM_GORON;
    CONTROLLER1(&play->state)->press.button=BTN_B;mmvrgame::ProcessGoronInput(play);
    check(mmvrgame::GoronFists(p),"Goron fist equip accepts B without a fresh tracking pose");
    for(int press=0;press<12;++press) {
        auto& controls=*CONTROLLER1(&play->state);
        controls.cur.button=controls.press.button=BTN_B;
        mmvrgame::ProcessGoronInput(play);
        check(mmvrgame::GoronFists(p)==bool(press%2),"repeated Goron B toggles immediately");
        check(!(controls.cur.button&BTN_B)&&!(controls.press.button&BTN_B),"fist input does not leak to native equipment");
        controls.cur.button=controls.press.button=0;mmvrgame::ProcessGoronInput(play);
    }

    CONTROLLER1(&play->state)->press.button=0;
    *p=saved;play->msgCtx.msgMode=oldMsg;play->csCtx.state=oldCs;
    play->pauseCtx.state=oldPause;play->transitionTrigger=oldTransition;
    if (std::getenv("MMVR_MOD_CATALOG_TEST")) {
        int index=-1;
        for(size_t i=0;i<mmvr::modPacks.size();++i) if(mmvr::modPacks[i].name=="mods/preview99-icons.o2r") index=int(i);
        check(index>=0,"nested pack catalog discovers native o2r archive");
        if(index>=0) {
            const bool expectDisabled=std::getenv("MMVR_MOD_EXPECT_DISABLED")!=nullptr;
            check(mmvr::modPacks[index].enabled!=expectDisabled,"pack selection survives process restart");
            mmvr::MenuState catalog;catalog.tab=mmvr::SystemTab;catalog.expanded[34]=true;
            for(const auto& folder:mmvr::modFolders)catalog.expandedModFolders.insert(folder.key);
            int found=0;
            for(int i=0;i<catalog.VisibleRows();++i) found+=catalog.VisibleSetting(i)==mmvr::ModPackRow+index;
            check(found==1,"pack has exactly one navigable checkbox row");
            if(!expectDisabled&&mmvr::toggleMod) mmvr::toggleMod(index);
            check(!mmvr::modPacks[index].enabled,"checkbox disables pack persistently");
        }
    }
    auto a=mmvr::CornerSpread(mmvr::HudGroup::TopLeft,200,100);
    auto b=mmvr::CornerSpread(mmvr::HudGroup::Buttons,200,100);
    auto c=mmvr::CornerSpread(mmvr::HudGroup::BottomLeft,200,100);
    check(a.x==-b.x&&c.x==a.x,"left/right horizontal spread symmetry");
    check(mmvr::CornerSpread(mmvr::HudGroup::Clock,300).x==0,"clock remains centered");
    auto flags=p->stateFlags1;p->stateFlags1|=PLAYER_STATE1_TALKING;
    auto facts=mmvrgame::SceneFacts(play);
    check(facts.localEvent&&!facts.distantAction,"conversation bypasses remote target classification");
    check(mmvr::ResolveSceneView(facts,false)==mmvr::SceneView::Player,"local dialogue stays player view with camera cutscenes off");
    p->stateFlags1=flags;
    mmvr::MenuState retry;retry.open=true;retry.commitSettings=+[](){return false;};retry.Close();
    check(retry.open&&retry.saveFailed,"failed commit keeps menu open");
    auto& menu=mmvr::GetMenu();menu.open=true;menu.commitSettings=CommitSettings;
    Change(mmvr::Setting::DekuSpinOpacity,77);
    Change(mmvr::Setting::HudOpacity,.43f);
    menu.Close();
    check(!menu.open&&!settingsDirty,"close synchronously commits changes");
    auto config=Ship::Context::GetRawInstance()->GetConfig();config->Reload();
    check(config->GetFloat("CVars.gVR.DekuSpinOpacity",-1)==77,"disk config contains committed trail preference");
    check(std::abs(config->GetFloat("CVars.gVR.HudOpacity",-1)-.43f)<.0001f,"disk config contains committed HUD preference");
    log<<"failures="<<failures<<"\n";log.flush();
    std::_Exit(failures?2:0);
}
