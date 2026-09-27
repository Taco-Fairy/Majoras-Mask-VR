#pragma once
// Native item-button transformations, without tracked-hand gesture shortcuts.
static mmvr::Pad NativeThirdPersonLifecycle(PlayState* play, unsigned tick) {
    static int stage=0, age=0, index=0;
    static bool sawAnimation=false, theater=true;
    static std::ofstream log("native-third-person.log");
    const int forms[]={PLAYER_FORM_DEKU,PLAYER_FORM_GORON,PLAYER_FORM_ZORA};
    const int items[]={ITEM_MASK_DEKU,ITEM_MASK_GORON,ITEM_MASK_ZORA};
    const int slots[]={SLOT_MASK_DEKU,SLOT_MASK_GORON,SLOT_MASK_ZORA};
    mmvr::Pad pad; pad.active=true;
    auto* p=GET_PLAYER(play);
    if(tick<80) return pad;
    if(stage==0) {
        CVarSetFloat("gVR.ViewMode",1); mmvr::GetSettings().Set(mmvr::Setting::ViewMode,1);
        mmvr::ApplyViewMode(1); mmvr::SetNativeTestTracking(true);
        gSaveContext.save.saveInfo.inventory.items[slots[index]]=items[index];
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=items[index];
        C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slots[index];
        stage=1; age=0; sawAnimation=false; theater=true;
    }
    const bool transforming=MMVR_LocalTransformation(p)||MMVR_FormReloadActive(play);
    if(transforming) { sawAnimation=true; theater &= mmvrgame::SceneView(play)==mmvr::SceneView::Theater; }
    const bool ready=!transforming && !p->actor.init && p->csAction==PLAYER_CSACTION_NONE &&
        play->csCtx.state==CS_STATE_IDLE && play->msgCtx.msgMode==MSGMODE_NONE;
    if(stage==1 && age==3) pad.buttons=BTN_CDOWN;
    if(stage==1 && age>10 && ready && p->transformation==forms[index]) {
        log<<(sawAnimation&&theater?"PASS":"FAIL")<<" wear="<<index<<" theater="<<theater<<" animation="<<sawAnimation<<"\n";
        stage=2; age=0; sawAnimation=false; theater=true;
    } else if(stage==2 && age==3) pad.buttons=BTN_CDOWN;
    else if(stage==2 && age>10 && ready && p->transformation==PLAYER_FORM_HUMAN) {
        log<<(sawAnimation&&theater?"PASS":"FAIL")<<" remove="<<index<<" theater="<<theater<<" animation="<<sawAnimation<<"\n";
        if(++index==3) {log<<"COMPLETE masks=3\n";log.close();Ship::Context::GetRawInstance()->GetWindow()->Close();}
        else stage=0;
        age=0;
    }
    if(++age>1000) {log<<"FAIL timeout stage="<<stage<<" form="<<int(p->transformation)<<"\n";log.close();Ship::Context::GetRawInstance()->GetWindow()->Close();}
    return pad;
}
