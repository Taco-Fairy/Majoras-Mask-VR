#pragma once
// Isolated tracked gestures with real native transformation updates between them.
// Covers direct form replacement and removal by either hand, including shielding.
static mmvr::Pad NativeMaskRepair(PlayState* play, unsigned tick) {
    struct Case { int item, slot, form, hand; bool remove, shield; };
    static const Case cases[] = {
        {ITEM_MASK_ZORA, SLOT_MASK_ZORA, PLAYER_FORM_ZORA, 1, false, false},
        {ITEM_MASK_DEKU, SLOT_MASK_DEKU, PLAYER_FORM_DEKU, 1, false, true},
        {ITEM_MASK_DEKU, SLOT_MASK_DEKU, PLAYER_FORM_HUMAN, 0, true, true},
        {ITEM_MASK_ZORA, SLOT_MASK_ZORA, PLAYER_FORM_ZORA, 1, false, false},
        {ITEM_MASK_ZORA, SLOT_MASK_ZORA, PLAYER_FORM_HUMAN, 1, true, true},
        {ITEM_MASK_GORON, SLOT_MASK_GORON, PLAYER_FORM_GORON, 1, false, false},
        {ITEM_MASK_GORON, SLOT_MASK_GORON, PLAYER_FORM_HUMAN, 0, true, false},
        {ITEM_MASK_FIERCE_DEITY, SLOT_MASK_FIERCE_DEITY, PLAYER_FORM_FIERCE_DEITY, 1, false, false},
        {ITEM_MASK_FIERCE_DEITY, SLOT_MASK_FIERCE_DEITY, PLAYER_FORM_HUMAN, 1, true, false}
    };
    static unsigned age = 0, index = 0;
    static int stage = 0;
    static bool introOwnershipChecked = false;
    static std::ofstream log("native-mask-repair.log");
    mmvr::Pad pad; pad.active = true;
    mmvr::SetNativeTestTracking(true);
    if (tick < 80) return pad;
    auto* p = GET_PLAYER(play);
    const auto& c = cases[index];
    const bool ready = p->csAction == PLAYER_CSACTION_NONE && play->csCtx.state == CS_STATE_IDLE &&
        play->msgCtx.msgMode == MSGMODE_NONE && !p->actor.init && !MMVR_LocalTransformation(p) &&
        p->itemAction == p->heldItemAction;
    mmvr::TrackingFrame f{};
    f.head.orientation.w = f.origin.orientation.w = 1;
    f.epoch = 94000 + index; f.timeSeconds = 94000 + tick / 30.;
    for (int h=0; h<2; ++h) {
        f.hands[h].orientation.w = f.aims[h].orientation.w = 1;
        f.handValid[h] = f.handTracked[h] = f.aimValid[h] = true;
        f.hands[h].position = {h ? .5f : -.5f, -.3f, -.3f};
    }
    if (stage == 0 && ready) {
        if (!introOwnershipChecked) {
            // The opening curse may set currentMask before the Deku Mask is
            // awarded. A face grab must remain unavailable in that state.
            const auto savedMask = p->currentMask;
            const auto savedItem = gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU];
            p->currentMask = PLAYER_MASK_DEKU;
            gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU] = ITEM_NONE;
            mmvrgame::UpdateMaskContext(play);
            const bool blocked = mmvr::WornMaskItem() < 0;
            p->currentMask = savedMask;
            gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU] = savedItem;
            mmvrgame::UpdateMaskContext(play);
            introOwnershipChecked = true;
            log << "preunlock-deku-mask-blocked=" << blocked << "\n" << std::flush;
            if (!blocked) {
                log << "FAIL preunlock Deku Mask was grabbable\n" << std::flush;
                Ship::Context::GetRawInstance()->GetWindow()->Close();
                return pad;
            }
        }
        gSaveContext.save.saveInfo.inventory.items[c.slot] = c.item;
        if (c.remove) mmvrgame::StowItem(play);
        else mmvrgame::SelectItem(play, c.slot, c.item);
        stage=1; age=0;
    }
    if (stage == 1) {
        if (c.shield) f.grips[0] = 1;
        f.triggers[c.hand] = age >= 6 && age < 22 ? 1 : 0;
        const float t = std::clamp((float(age)-8)/9.f, 0.f, 1.f);
        const float away = c.remove ? t : 1-t;
        f.hands[c.hand].position = {(c.hand ? .5f : -.5f)*away, -.12f-.18f*away, -.14f-.16f*away};
        if (age > 24) { stage=2; age=0; }
    }
    if (stage == 2 && age > 15 && ready && p->transformation == c.form) {
        log << "PASS case=" << index << " form=" << int(p->transformation) << " hand=" << c.hand
            << " remove=" << c.remove << " shield=" << c.shield << "\n" << std::flush;
        if (++index == std::size(cases)) {
            log << "ALL PASS\n" << std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close(); return pad;
        }
        stage=0; age=0;
    }
    auto view=mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+45,p->actor.world.pos.z);
    auto head=mmvr::YawPose(0);
    mmvrgame::RecordFormTracking(f,view,head);
    mmvrgame::RecordTracking(f,view,head);
    mmvrgame::UpdateMaskContext(play);
    mmvr::UpdateMaskTracking(f,true);
    if (age%30 == 0) log << "state case=" << index << " stage=" << stage << " age=" << age
        << " form=" << int(p->transformation) << " item=" << int(p->itemAction) << " held=" << int(p->heldItemAction)
        << " cs=" << int(p->csAction) << " transform=" << MMVR_LocalTransformation(p)
        << " flags=" << p->stateFlags1 << "\n" << std::flush;
    if (++age > 350 || tick > 3500) {
        log << "FAIL timeout case=" << index << "\n" << std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return pad;
}
