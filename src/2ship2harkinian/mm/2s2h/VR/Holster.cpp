#ifdef MMVR_ENABLE
#include "Holster.h"
#include "shoulder_gesture.h"
#include "ItemUse.h"
#include "Interactions.h"
#include "NativeCombat.h"
#include "ui.h"
#include "runtime.h"
#include <fstream>
extern "C" {
#include "global.h"
void MMVR_PlayerEquipSword(PlayState*, Player*, ItemId);
}
namespace {
mmvr::ShoulderHolster holster;
int pending = 0, hand = -1;
bool Allowed(PlayState* play, Player* p) {
    if (!play || !p || !mmvr::PhysicalActionsAllowed() || !mmvrgame::InteractionsEligible(play, p) ||
        mmvr::GetSettings().Get(mmvr::Setting::ShoulderHolster) < .5f || p->heldActor || mmvr::HeldMaskItem() >= 0 ||
        play->msgCtx.msgMode != MSGMODE_NONE)
        return false;
    int selected = mmvrgame::SelectedItem(play);
    bool worn = selected >= ITEM_MASK_DEKU && selected <= ITEM_MASK_GIANT && Player_GetCurMaskItemId(play) == selected;
    return MMVR_IndependentSword(p) || (p->heldItemAction == PLAYER_IA_NONE && (selected == ITEM_NONE || worn));
}
} // namespace
namespace mmvrgame {
void ClearHolster() {
    holster.Reset();
    pending = 0;
}
void UpdateHolster(const mmvr::TrackingFrame& frame) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    int dominant = mmvr::SwordController(mmvr::GetSettings());
    if (hand != dominant) {
        ClearHolster();
        hand = dominant;
    }
    int action = holster.Update(frame, dominant, Allowed(play, p), MMVR_IndependentSword(p),
                                mmvr::GetSettings().Get(mmvr::Setting::HolsterReach));
    if (action)
        pending = action;
}
void ProcessHolster(PlayState* play) {
    if (!pending)
        return;
    int action = pending;
    pending = 0;
    auto* p = GET_PLAYER(play);
    if (!Allowed(play, p))
        return;
    if (action < 0) {
        StowItem(play);
        ClearCombat();
    } else {
        auto sword = Inventory_GetBtnBItem(play); // A worn mask may replace the B action, not the owned sword.
        if ((sword < ITEM_SWORD_KOKIRI || sword > ITEM_SWORD_GILDED) && sword != ITEM_SWORD_DEITY)
            return;
        ClearItemTrigger();
        ClearCombat();
        MMVR_PlayerEquipSword(play, p, static_cast<ItemId>(sword));
    }
    mmvr::HapticPulse(hand, .35f);
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
        std::ofstream("mmvr-combat.log", std::ios::app)
            << "shoulder-holster action=" << action << " hand=" << hand << "\n";
}
} // namespace mmvrgame
#endif
