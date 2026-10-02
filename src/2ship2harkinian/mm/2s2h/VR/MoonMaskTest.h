#pragma once
#include "ItemUse.h"
#include "Masks.h"
extern "C" {
#include "overlays/actors/ovl_En_Js/z_en_js.h"
PlayerItemAction Player_ItemToItemAction(Player*, ItemId);
PlayerItemAction func_8085B854(PlayState*, Player*, ItemId);
void func_80969748(EnJs*, PlayState*);
void func_80969C54(EnJs*, PlayState*);
void func_80969400(s32);
extern u16 sMasksGivenOnMoonBits[];
}

// Private copied-save test: real Moon-child offer handlers, not inventory deletion.
static void NativeMoonMaskTest(PlayState* play) {
    auto* p = GET_PLAYER(play);
    const auto player = *p;
    const auto save = gSaveContext;
    const auto msg = play->msgCtx;
    const auto cs = play->csCtx;
    const auto savedInterface = play->interfaceCtx;
    const auto pause = play->pauseCtx.state;
    const auto transition = play->transitionTrigger;
    const auto input = *CONTROLLER1(&play->state);
    const auto settings = mmvr::GetSettings();
    auto* oldInput = sPlayerControlInput;
    const auto offerReader = play->unk_18794;
    auto& in = *CONTROLLER1(&play->state);
    sPlayerControlInput = &in;
    play->unk_18794 = func_8085B854;
    mmvr::SetNativeTestTracking(true);
    mmvr::ApplyViewMode(2);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson, 1);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration, 0);
    EnJs child{};
    child.actor.id = ACTOR_EN_JS;
    SkelAnime_InitFlex(play, &child.skelAnime, (FlexSkeletonHeader*)gMoonChildSkel,
                      (AnimationHeader*)gMoonChildStandingAnim, child.jointTable,
                      child.morphTable, MOONCHILD_LIMB_MAX);
    mmvr::TrackingFrame frame{};
    frame.head.orientation.w = frame.origin.orientation.w = 1;
    for (int h = 0; h < 2; ++h) {
        frame.hands[h].orientation.w = frame.aims[h].orientation.w = 1;
        frame.hands[h].position = {0, -.4f, -.4f};
        frame.handTracked[h] = frame.handValid[h] = frame.aimValid[h] = true;
    }
    unsigned assertions = 0, offers = 0;
    bool passed = true;
    double clock = 15000;
    std::ofstream log("native-moon-masks.log");
    auto check = [&](bool ok, const char* label, int item, int group) {
        ++assertions;
        passed &= ok;
        if (!ok) log << "FAIL " << label << " item=" << item << " child=" << group
                     << " selected=" << mmvrgame::SelectedItem(play) << " worn=" << int(p->currentMask)
                     << " message=" << int(play->msgCtx.msgMode) << "\n";
    };
    auto prepare = [&](int item, int group, int left, int quick) {
        mmvrgame::ClearItemSelection();
        mmvrgame::ClearTracking();
        *p = player; gSaveContext = save; play->msgCtx = msg; play->csCtx = cs;
        std::memset(gSaveContext.masksGivenOnMoon, 0, sizeof(gSaveContext.masksGivenOnMoon));
        for (int mask = ITEM_MASK_DEKU; mask <= ITEM_MASK_GIANT; ++mask)
            gSaveContext.save.saveInfo.inventory.items[SLOT(mask)] = mask;
        p->transformation = PLAYER_FORM_HUMAN;
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        p->actor.init = nullptr; p->actionFunc = Player_Action_Idle;
        p->csAction = PLAYER_CSACTION_NONE;
        p->stateFlags1 = p->stateFlags2 = p->stateFlags3 = 0;
        p->heldActor = p->actor.child = nullptr;
        p->talkActor = &child.actor;
        p->exchangeItemAction = PLAYER_IA_NONE;
        p->currentMask = PLAYER_MASK_NONE;
        p->heldItemId = ITEM_NONE; p->itemAction = p->heldItemAction = PLAYER_IA_NONE;
        play->msgCtx.msgMode = MSGMODE_NONE; play->csCtx.state = CS_STATE_IDLE;
        play->pauseCtx.state = PAUSE_STATE_OFF; play->transitionTrigger = TRANS_TRIGGER_OFF;
        gSaveContext.save.unk_06 = 0; gSaveContext.save.saveInfo.playerData.health = 0x30;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN] = BTN_ENABLED;
        in = {};
        mmvr::SetInputContext(true, false);
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded, left);
        mmvr::GetSettings().Set(mmvr::Setting::QuickWheelItems, quick);
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN) = item;
        C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) = SLOT(item);
        child.actor.params = group;
        frame.epoch++; frame.timeSeconds = clock += 1;
        frame.triggers[0] = frame.triggers[1] = 0;
        mmvrgame::RecordTracking(frame, mmvr::YawPose(0), mmvr::YawPose(0));
    };
    auto prompt = [&] {
        p->stateFlags1 |= PLAYER_STATE1_TALKING;
        play->msgCtx.msgLength = 1;
        play->msgCtx.msgMode = MSGMODE_TEXT_DONE;
        play->msgCtx.nextTextId = 0xFFFF;
        play->msgCtx.textboxEndType = TEXTBOX_ENDTYPE_PAUSE_MENU;
    };
    auto edge = [&](int hand, float trigger) {
        in = {}; frame.timeSeconds = clock += .02;
        frame.triggers[0] = frame.triggers[1] = 0; frame.triggers[hand] = trigger;
        mmvrgame::UpdateItemTrigger(frame);
        mmvrgame::ProcessItemTrigger(play);
        return bool(in.press.button & BTN_CDOWN);
    };

    for (int group = 1; group <= 8; ++group)
        for (int item = ITEM_MASK_TRUTH; item <= ITEM_MASK_GIANT; ++item)
            for (int left = 0; left < 2; ++left) for (int quick = 0; quick < 2; ++quick) {
                prepare(item, group, left, quick);
                check(mmvrgame::MaskAvailable(item), "owned-before-give", item, group);
                // A selection made before talking must also become invalid after the offer.
                check(mmvrgame::SelectItem(play, SLOT(item), item), "select-before-dialogue", item, group);
                mmvrgame::UpdateMaskContext(play); mmvrgame::ProcessItemTrigger(play);
                check(!quick || mmvr::HeldMaskItem() == item, "quick-held-before-dialogue", item, group);
                prompt();
                check(mmvrgame::ExchangePromptActive(play), "native-request-active", item, group);
                check(!edge(1-left, 0) && edge(1-left, 1), "tracked-offer", item, group);
                if (group <= 4) func_80969748(&child, play);
                else func_80969C54(&child, play);
                ++offers;
                const u16 nativeBit = sMasksGivenOnMoonBits[SLOT(item)-ITEM_NUM_SLOTS];
                check(bool(gSaveContext.masksGivenOnMoon[nativeBit >> 8] & (u8)nativeBit),
                      "native-child-accepted", item, group);
                check(gSaveContext.save.saveInfo.inventory.items[SLOT(item)] == item,
                      "native-inventory-retained", item, group);
                check(GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_DOWN) == ITEM_NONE,
                      "native-c-button-unequipped", item, group);
                check(mmvrgame::InventorySlotItem(SLOT(item)) == ITEM_NONE &&
                      mmvrgame::WheelSlotItem(play, SLOT(item)) == ITEM_NONE &&
                      !mmvrgame::ItemAllowed(p, item) && !mmvrgame::MaskAvailable(item),
                      "all-availability-paths-empty", item, group);
                mmvrgame::UpdateMaskContext(play);
                check(mmvrgame::SelectedItem(play) == ITEM_NONE && mmvr::HeldMaskItem() < 0 &&
                      mmvr::TakeMaskUse() < 0, "stale-held-and-selected-cleared", item, group);
                mmvrgame::SelectItem(play, SLOT(item), item);
                check(mmvrgame::SelectedItem(play) == ITEM_NONE && !edge(1-left, 0) && !edge(1-left, 1),
                      "cannot-reselect-or-offer-again", item, group);
                play->msgCtx.msgMode = MSGMODE_NONE; p->stateFlags1 = 0;
                check(!edge(1-left, 0) && !edge(1-left, 1), "cannot-equip-after-dialogue", item, group);
                // The original trial exit returns this child's masks. Do not destroy
                // ownership or saved assignments to hide the donated slot permanently.
                func_80969400(group);
                check(mmvrgame::MaskAvailable(item) && mmvrgame::WheelSlotItem(play, SLOT(item)) == item,
                      "native-return-restores-availability", item, group);
                // Returning from a trial creates a new idle player. The copied
                // exchange action must not masquerade as an unfinished handoff.
                *p = player;
                p->transformation = PLAYER_FORM_HUMAN;
                p->actor.init = nullptr; p->actionFunc = Player_Action_Idle;
                p->csAction = PLAYER_CSACTION_NONE;
                p->stateFlags1 = p->stateFlags2 = p->stateFlags3 = 0;
                p->itemAction = p->heldItemAction = PLAYER_IA_NONE;
                p->heldActor = p->actor.child = nullptr;
                p->talkActor = nullptr; p->exchangeItemAction = PLAYER_IA_NONE;
                check(mmvrgame::SelectItem(play, SLOT(item), item) && mmvrgame::SelectedItem(play) == item,
                      "returned-mask-reselectable", item, group);
            }
    // Native Moon children refuse the four transformation masks.
    for (int item = ITEM_MASK_DEKU; item <= ITEM_MASK_FIERCE_DEITY; ++item) {
        prepare(item, 1, 0, 0); prompt();
        mmvrgame::SelectItem(play, SLOT(item), item); edge(1, 0); edge(1, 1);
        func_80969748(&child, play);
        check(!mmvrgame::MaskGivenOnMoon(item) && mmvrgame::MaskAvailable(item),
              "transformation-refusal-preserved", item, 1);
    }
    // Native removal of a surrendered wearable is deferred until dialogue is over.
    for (int item = ITEM_MASK_TRUTH; item < ITEM_MASK_GIANT; ++item) {
        prepare(item, 1, 0, 1);
        p->currentMask = GET_MASK_FROM_IA(Player_ItemToItemAction(p, static_cast<ItemId>(item)));
        gSaveContext.save.equippedMask = p->currentMask;
        const u16 bit = sMasksGivenOnMoonBits[SLOT(item)-ITEM_NUM_SLOTS];
        gSaveContext.masksGivenOnMoon[bit >> 8] |= (u8)bit;
        prompt();
        const auto worn = p->currentMask;
        mmvrgame::ProcessMasks(play);
        check(p->currentMask == worn, "dialogue-owns-removal", item, 1);
        play->msgCtx.msgMode = MSGMODE_NONE; p->stateFlags1 = 0;
        p->actor.flags &= ~ACTOR_FLAG_TALK;
        mmvrgame::ProcessMasks(play);
        check(p->currentMask == PLAYER_MASK_NONE && gSaveContext.save.equippedMask == PLAYER_MASK_NONE,
              "worn-donated-mask-removed", item, 1);
    }
    for (int slot = -1; slot <= 49; ++slot) {
        if (slot >= ITEM_NUM_SLOTS && slot < ITEM_NUM_SLOTS+MASK_NUM_SLOTS) continue;
        const int expected = slot >= 0 && slot < ITEM_NUM_SLOTS
            ? gSaveContext.save.saveInfo.inventory.items[slot] : ITEM_NONE;
        check(mmvrgame::InventorySlotItem(slot) == expected, "non-mask-slot-unchanged", slot, 0);
    }
    mmvrgame::ClearItemSelection(); mmvrgame::ClearTracking();
    *p = player; gSaveContext = save; play->msgCtx = msg; play->csCtx = cs;
    play->interfaceCtx = savedInterface; play->pauseCtx.state = pause; play->transitionTrigger = transition;
    in = input; mmvr::GetSettings() = settings; sPlayerControlInput = oldInput;
    play->unk_18794 = offerReader;
    log << (passed ? "PASS" : "FAIL") << " moon-masks offers=" << offers
        << " assertions=" << assertions << "\n";
    log.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
