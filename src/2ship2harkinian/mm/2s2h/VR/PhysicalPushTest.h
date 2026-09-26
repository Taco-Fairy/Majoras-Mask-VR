#pragma once
#include "Interactions.h"
extern "C" {
#include "overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"
void Player_Action_45(Player*, PlayState*);
void Player_Action_Idle(Player*, PlayState*);
void Player_Action_WaitForPutAway(Player*, PlayState*);
void func_80837BF8(PlayState*, Player*);
s32 func_8083E14C(PlayState*, Player*);
}
// Exercises a real dynamic push-block surface in the isolated native harness.
static void NativePhysicalPushTest(PlayState* play, std::ostream& out) {
    auto* p = GET_PLAYER(play);
    const Player baseline = *p;
    const auto settings = mmvr::GetSettings();
    const auto input = *CONTROLLER1(&play->state);
    mmvr::GetSettings().Set(mmvr::Setting::PhysicalCarry, 1);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson, 1);
    ObjOshihiki* block = nullptr;
    for (auto& list : play->actorCtx.actorLists)
        for (auto* a = list.first; a; a = a->next)
            if (a->id == ACTOR_OBJ_OSHIHIKI && a->update && !a->init)
                block = reinterpret_cast<ObjOshihiki*>(a);
    int passed = 0, cases = 0;
    out << "{\"checks\":[";
    auto check = [&](const char* name, bool ok) {
        if (cases++) out << ',';
        passed += ok;
        out << "{\"name\":\"" << name << "\",\"passed\":" << (ok ? "true" : "false") << '}';
    };
    auto nativeDrawPose = [&](const Actor* actor) {
        MtxF native;
        Matrix_Push();
        auto rotation = actor->shape.rot;
        if (actor->flags & ACTOR_FLAG_IGNORE_QUAKE) {
            Matrix_SetTranslateRotateYXZ(actor->world.pos.x + play->mainCamera.quakeOffset.x,
                                         actor->world.pos.y + (actor->shape.yOffset * actor->scale.y) +
                                             play->mainCamera.quakeOffset.y,
                                         actor->world.pos.z + play->mainCamera.quakeOffset.z, &rotation);
        } else {
            Matrix_SetTranslateRotateYXZ(actor->world.pos.x,
                                         actor->world.pos.y + (actor->shape.yOffset * actor->scale.y),
                                         actor->world.pos.z, &rotation);
        }
        Matrix_Scale(actor->scale.x, actor->scale.y, actor->scale.z, MTXMODE_APPLY);
        Matrix_Get(&native);
        Matrix_Pop();
        mmvr::Matrix result;
        std::memcpy(&result, &native, sizeof(result));
        return result;
    };
    auto matricesClose = [](const mmvr::Matrix& a, const mmvr::Matrix& b) {
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                if (std::abs(a.m[row][column] - b.m[row][column]) > .001f) return false;
        return true;
    };
    auto blendPose = [](const mmvr::Matrix& oldPose, const mmvr::Matrix& newPose, float alpha) {
        mmvr::Matrix result{};
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                result.m[row][column] = oldPose.m[row][column] * (1.f - alpha) + newPose.m[row][column] * alpha;
        return result;
    };
    check("nativeBlockLoaded", block != nullptr);
    if (block) {
        const auto originalPosition = block->dyna.actor.world.pos;
        const auto originalRotation = block->dyna.actor.shape.rot;
        const auto originalScale = block->dyna.actor.scale;
        const auto originalYOffset = block->dyna.actor.shape.yOffset;
        *p = baseline;
        p->transformation = PLAYER_FORM_HUMAN;
        p->csAction = PLAYER_CSACTION_NONE;
        p->stateFlags1 = p->stateFlags2 = p->stateFlags3 = 0;
        p->heldActor = p->rideActor = nullptr;
        p->actor.shape.rot.y = 0;
        p->speedXZ = p->actor.speed = 0;
        p->actor.world.pos = {originalPosition.x, originalPosition.y, originalPosition.z - 100};
        p->actionFunc = Player_Action_Idle;
        mmvrgame::ClearTracking();
        mmvr::TrackingFrame f{};
        f.epoch = 180001; f.timeSeconds = 8000;
        f.head.orientation.w = f.origin.orientation.w = 1;
        mmvr::Matrix hands[2];
        auto view = mmvr::YawPose(0, p->actor.world.pos.x, p->actor.world.pos.y + 45, p->actor.world.pos.z);
        auto relative = mmvr::YawPose(0);
        bool contacts = true;
        for (int h = 0; h < 2; ++h) {
            f.hands[h].orientation.w = f.aims[h].orientation.w = 1;
            f.handValid[h] = f.handTracked[h] = f.aimValid[h] = true;
            f.triggers[h] = 1;
            Vec3f from{originalPosition.x + (h ? 8.f : -8.f), originalPosition.y + 25, originalPosition.z - 180};
            Vec3f to{from.x, from.y, originalPosition.z + 20}, hit{};
            CollisionPoly* poly = nullptr;
            int bg = BGCHECK_SCENE;
            const bool found = BgCheck_EntityLineTest2(&play->colCtx, &from, &to, &hit, &poly,
                                                      true, false, false, true, &bg, &p->actor);
            contacts &= found && bg == block->dyna.bgId;
            if (!found) continue;
            p->actor.wallPoly = poly; p->actor.wallBgId = bg; p->actor.wallYaw = -0x8000;
            hands[h] = mmvr::YawPose(0, hit.x, hit.y - 2.75f, hit.z - 1.f);
            for (int i = 0; i < 3; ++i) hands[h].m[i][i] *= .01f;
            f.hands[h].position = {(hit.x - view.m[3][0]) / 40.f,
                                  (hit.y - view.m[3][1]) / 40.f, (hit.z - view.m[3][2]) / 40.f};
        }
        p->actor.bgCheckFlags = BGCHECKFLAG_GROUND | BGCHECKFLAG_PLAYER_WALL_INTERACT;
        p->yDistToLedge = 60;
        f.physicalPushRenderPose = nativeDrawPose(&block->dyna.actor);
        f.physicalPushRenderOwner = &block->dyna.actor;
        f.physicalPushRenderPoseValid = true;
        auto record = [&] {
            mmvrgame::RecordPhysicalPushTracking(f, view, relative, hands);
        };
        check("nativeSurfaceContacts", contacts);
        record(); check("bothHandsReady", MMVR_PhysicalPushReady(play, p));
        f.triggers[1] = 0; record(); check("oneTriggerRejected", !MMVR_PhysicalPushReady(play, p));
        f.triggers[1] = 1;
        const auto frontHand = hands[1];
        Vec3f topFrom{originalPosition.x, originalPosition.y + 200, originalPosition.z};
        Vec3f topTo{originalPosition.x, originalPosition.y - 10, originalPosition.z}, topHit{};
        CollisionPoly* topPoly = nullptr; int topBg = BGCHECK_SCENE;
        const bool topFound = BgCheck_EntityLineTest2(&play->colCtx, &topFrom, &topTo, &topHit, &topPoly,
                                                     true, true, true, true, &topBg, &p->actor);
        if (topFound) {
            hands[1].m[3][0] = topHit.x;
            hands[1].m[3][1] = topHit.y + 1 - 2.75f;
            hands[1].m[3][2] = topHit.z;
        }
        record(); check("topSurfaceContact", topFound && topBg == block->dyna.bgId && MMVR_PhysicalPushReady(play, p));
        hands[1] = frontHand;
        hands[1].m[3][0] += 200; record(); check("secondHandMustTouchSameBlock", !MMVR_PhysicalPushReady(play, p));
        hands[1].m[3][0] -= 200; record();
        p->rightHandActor = &block->dyna.actor;
        MMVR_BeginPhysicalPush(play, p);
        p->actionFunc = Player_Action_45;
        check("gripStarted", MMVR_PhysicalPushActive(p) && MMVR_PhysicalPushHeld(play, p));
        check("activeTargetDrawIsTagged",
              MMVR_PhysicalPushTargetDraw(play, p, &block->dyna.actor) &&
                  !MMVR_PhysicalPushTargetDraw(play, p, &p->actor));
        float speed = 0; short yaw = 0;
        MMVR_PhysicalPushMovement(play, p, &speed, &yaw);
        check("noInitialMotion", speed == 0);
        for (auto& hand : f.hands) hand.position.z += .075f;
        record(); speed = 0; MMVR_PhysicalPushMovement(play, p, &speed, &yaw);
        check("armPushIntent", speed > 0 && yaw == 0);
        for (auto& hand : f.hands) hand.position.z -= .15f;
        record(); speed = 0; MMVR_PhysicalPushMovement(play, p, &speed, &yaw);
        check("armPullIntent", speed > 0 && yaw == short(-0x8000));
        speed = 3; yaw = 1234; MMVR_PhysicalPushMovement(play, p, &speed, &yaw);
        check("nativeStickPreserved", speed == 3 && yaw == 1234);
        // A render callback can run between simulation updates. Its root is
        // the interpolated matrix replacement, not the actor's latest pose.
        const auto gripPose = f.physicalPushRenderPose;
        mmvr::Matrix inverseGripPose{};
        const bool invertibleGripPose = mmvr::InverseAffine(gripPose, inverseGripPose);
        const mmvr::Matrix localHands[2] = {mmvr::Multiply(hands[0], inverseGripPose),
                                            mmvr::Multiply(hands[1], inverseGripPose)};
        block->dyna.actor.world.pos.x += 7;
        block->dyna.actor.world.pos.z -= 3;
        block->dyna.actor.shape.rot.y += 0x1800;
        block->dyna.actor.scale.x = originalScale.x * 1.2f;
        block->dyna.actor.scale.y = originalScale.y * .8f;
        block->dyna.actor.scale.z = originalScale.z * 1.1f;
        block->dyna.actor.shape.yOffset = originalYOffset + 5.f;
        const auto currentSimulationPose = nativeDrawPose(&block->dyna.actor);
        f.physicalPushRenderPose = blendPose(gripPose, currentSimulationPose, .5f);
        f.physicalPushRenderPoseValid = true;
        record();
        mmvr::Matrix locked[2]{};
        mmvrgame::ApplyPhysicalPushHandLock(play, p, locked);
        const auto expectedInterpolated0 = mmvr::Multiply(localHands[0], f.physicalPushRenderPose);
        const auto expectedInterpolated1 = mmvr::Multiply(localHands[1], f.physicalPushRenderPose);
        check("handsFollowInterpolatedRenderRoot", invertibleGripPose &&
              matricesClose(locked[0], expectedInterpolated0) &&
              matricesClose(locked[1], expectedInterpolated1) &&
              !matricesClose(locked[0], mmvr::Multiply(localHands[0], currentSimulationPose)));
        f.physicalPushRenderPoseValid = false;
        record();
        mmvrgame::ApplyPhysicalPushHandLock(play, p, locked);
        check("missingRenderAnchorUsesNativeDrawPose",
              matricesClose(locked[0], mmvr::Multiply(localHands[0], currentSimulationPose)) &&
              matricesClose(locked[1], mmvr::Multiply(localHands[1], currentSimulationPose)));
        f.physicalPushRenderPoseValid = true;
        f.physicalPushRenderOwner = &p->actor;
        record();
        mmvrgame::ApplyPhysicalPushHandLock(play, p, locked);
        check("otherActorsRenderRootIsRejected",
              matricesClose(locked[0], mmvr::Multiply(localHands[0], currentSimulationPose)) &&
              matricesClose(locked[1], mmvr::Multiply(localHands[1], currentSimulationPose)));
        block->dyna.actor.world.pos = originalPosition;
        block->dyna.actor.shape.rot = originalRotation;
        block->dyna.actor.scale = originalScale;
        block->dyna.actor.shape.yOffset = originalYOffset;
        f.physicalPushRenderPose = nativeDrawPose(&block->dyna.actor);
        f.physicalPushRenderOwner = &block->dyna.actor;
        f.physicalPushRenderPoseValid = true;
        record();
        f.triggers[0] = 0; record();
        p->stateFlags2 |= PLAYER_STATE2_10;
        CONTROLLER1(&play->state)->cur.button = BTN_A;
        const bool released = func_8083E14C(play, p);
        check("eitherTriggerReleasesCommittedStep", released && !MMVR_PhysicalPushActive(p) &&
                                                        !(p->stateFlags2 & PLAYER_STATE2_10));
        p->rightHandActor = &block->dyna.actor;
        p->actor.bgCheckFlags |= BGCHECKFLAG_PLAYER_WALL_INTERACT;
        check("originalAStillHolds", !func_8083E14C(play, p));
        p->actionFunc = Player_Action_Idle;
        f.triggers[0] = f.triggers[1] = 1; record();
        check("restartAfterRelease", MMVR_BeginPhysicalPush(play, p));
        p->actionFunc = Player_Action_WaitForPutAway;
        p->afterPutAwayFunc = func_80837BF8;
        check("putAwayKeepsOwnership", MMVR_PhysicalPushActive(p) && MMVR_PhysicalPushHeld(play, p));
        p->actionFunc = Player_Action_Idle;
        check("otherNativeActionClearsOwnership", !MMVR_PhysicalPushActive(p));
        check("restartAfterInterruption", MMVR_BeginPhysicalPush(play, p));
        p->actionFunc = Player_Action_45;
        int staleAnchor = 0;
        mmvr::SetPhysicalPushAnchor(&staleAnchor);
        MMVR_EndPhysicalPush(p);
        check("releaseDropsNativeMatrixAddress", mmvr::PhysicalPushAnchor() == nullptr &&
                                                   !MMVR_PhysicalPushActive(p));
        p->actionFunc = Player_Action_Idle;
        check("restartAfterExplicitRelease", MMVR_BeginPhysicalPush(play, p));
        p->actionFunc = Player_Action_45;
        mmvrgame::ClearPhysicalPushTracking();
        p->stateFlags2 |= PLAYER_STATE2_10;
        check("trackingLossReleases", !MMVR_PhysicalPushHeld(play, p) && func_8083E14C(play, p) &&
                                       !MMVR_PhysicalPushActive(p) && !(p->stateFlags2 & PLAYER_STATE2_10));
        mmvr::SetPhysicalPushAnchor(&staleAnchor);
        mmvrgame::ClearPhysicalPushTracking();
        check("trackingClearDropsNativeMatrixAddress", mmvr::PhysicalPushAnchor() == nullptr);
        MMVR_EndPhysicalPush(p);
    }
    *p = baseline;
    mmvr::GetSettings() = settings;
    *CONTROLLER1(&play->state) = input;
    mmvrgame::ClearTracking();
    out << "],\"passed\":" << passed << ",\"total\":" << cases << '}';
}
