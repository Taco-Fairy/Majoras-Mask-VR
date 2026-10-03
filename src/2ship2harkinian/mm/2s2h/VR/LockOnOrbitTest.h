#pragma once
#ifdef MMVR_LOCAL_TEST_TOOLS
// Isolated native camera callback, with the same live target/input/draw gates
// used in gameplay. No player save/profile or normal session is modified.
static void NativeLockOnOrbitTest(PlayState* play) {
    auto* p = GET_PLAYER(play);
    const auto savedPlayer = *p;
    const auto savedSettings = mmvr::GetSettings();
    const auto savedInput = *CONTROLLER1(&play->state);
    const auto savedCs = play->csCtx;
    const auto savedPause = play->pauseCtx.state;
    const auto savedFrames = play->gameplayFrames;
    auto& enemies = play->actorCtx.actorLists[ACTORCAT_ENEMY];
    const auto savedEnemies = enemies;
    Actor enemy{};
    enemy.update = [](Actor*, PlayState*) {};
    enemy.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    enemy.category = ACTORCAT_ENEMY;
    enemy.world.pos = enemy.focus.pos = {0, 2000, 120};
    enemies.first = &enemy;
    enemies.length = 1;
    mmvr::ApplyViewMode(2);
    mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration, 0);
    mmvr::TrackingFrame f{};
    f.head.orientation.w = f.origin.orientation.w = 1;
    f.timeSeconds = 20000;
    unsigned checks = 0;
    auto check = [&](bool ok, const char* name) {
        if (!ok) { std::ofstream("native-lock-on-orbit-failure.txt") << name; throw std::runtime_error(name); }
        ++checks;
    };
    auto prepare = [&](int form, int leftHand, bool enabled) {
        *p = savedPlayer;
        enemy.focus.pos = enemy.world.pos = {0, 2000, 120};
        p->transformation = form;
        p->actor.world.pos = {0, 2000, 0};
        p->actor.shape.rot = p->actor.world.rot = {};
        p->actor.focus.pos = {0, 2048, 0};
        p->actor.init = nullptr;
        p->actor.bgCheckFlags = 0;
        p->heldActor = p->rideActor = p->csActor = nullptr;
        p->currentMask = PLAYER_MASK_NONE;
        p->heldItemAction = p->itemAction = PLAYER_IA_NONE;
        p->heldItemId = ITEM_NONE;
        p->getItemDrawIdPlusOne = 0;
        p->csAction = PLAYER_CSACTION_NONE;
        p->actionFunc = Player_Action_Idle;
        p->stateFlags1 = p->stateFlags2 = 0;
        p->stateFlags3 = PLAYER_STATE3_HOSTILE_LOCK_ON;
        p->focusActor = &enemy;
        p->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_0;
        play->csCtx.state = CS_STATE_IDLE;
        play->csCtx.playerCue = nullptr;
        play->pauseCtx.state = PAUSE_STATE_OFF;
        *CONTROLLER1(&play->state) = {};
        CONTROLLER1(&play->state)->rel.stick_x = 60;
        mmvr::GetSettings().Set(mmvr::Setting::LockOnOrbit, enabled);
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded, leftHand);
        mmvrgame::ResetTestCamera();
        f.head.orientation = {0,0,0,1};
        f.visualValid = true;
        f.visualOffset[0] = f.visualOffset[1] = f.visualOffset[2] = 0;
        ++f.epoch; f.timeSeconds += 1;
        MMVR_PlayerDrawBegin(play, &p->actor);
        MMVR_PlayerDrawEnd(play, &p->actor);
    };
    auto sample = [&] { f.timeSeconds += 1. / 90; return mmvrgame::TestCameraFrame(f); };
    auto yaw = [](const mmvr::CameraFrame& frame) { return mmvr::PoseYaw(mmvr::InversePose(frame.view)); };
    auto difference = [](float a, float b) { return std::remainder(a - b, 6.28318530718f); };
    check(mmvr::Settings{}.Get(mmvr::Setting::LockOnOrbit) == 0, "Default must remain off");
    for (int form = 0; form < PLAYER_FORM_MAX; ++form) for (int hand = 0; hand < 2; ++hand) {
        for (bool enabled : {false, true}) {
            prepare(form, hand, enabled);
            auto first = sample();
            check(first.active, "Native first-person entry inactive");
            // Interpolated movement between native ticks; repeat the exact same
            // sample to ensure dual eye/read callers cannot double-apply yaw.
            for (int i = 1; i <= 12; ++i) {
                f.visualOffset[0] = float(i);
                auto moved = sample();
                const float expected = enabled ? std::atan2(-float(i), 120.f) : 0.f;
                check(std::abs(difference(yaw(moved), yaw(first)) - expected) < .0001f,
                      "Strafe orbit/default camera mismatch");
                auto repeat = mmvrgame::TestCameraFrame(f);
                check(std::abs(difference(yaw(repeat), yaw(moved))) < .0001f, "Repeated frame rotated twice");
                const auto pose = mmvr::InversePose(moved.view);
                check(std::abs(pose.m[3][0] - i) < .001f && std::abs(pose.m[3][2]) < .001f,
                      "Orbit must not move the native player/camera anchor");
            }
            // Release causes no inverse turn; the new heading is retained.
            auto before = sample(); p->focusActor = nullptr; f.visualOffset[0] += 2;
            auto released = sample();
            check(std::abs(difference(yaw(released), yaw(before))) < .0001f, "Unlock snapped the camera");
        }
    }
    // Actual native eligibility boundaries, independently of geometry tests.
    for (int guard = 0; guard < 8; ++guard) {
        prepare(PLAYER_FORM_HUMAN, 0, true);
        auto first = sample();
        switch (guard) {
            case 0: p->stateFlags3 = 0; break; // Parallel mode without a native locked actor.
            case 1: enemy.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED; break;
            case 2: enemy.update = nullptr; break;
            case 3: enemies.first = nullptr; break; // Not a live scene actor.
            case 4: enemy.flags |= ACTOR_FLAG_LOCK_ON_DISABLED; break;
            case 5: play->pauseCtx.state = PAUSE_STATE_MAIN; break;
            case 6: p->csAction = PLAYER_CSACTION_WAIT; break;
            case 7: p->rideActor = &enemy; break;
        }
        f.visualOffset[0] = 5;
        auto held = sample();
        check(std::abs(difference(yaw(held), yaw(first))) < .0001f, "Ineligible context rotated camera");
        enemy.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
        enemy.update = [](Actor*, PlayState*) {};
        enemies.first = &enemy;
    }
    // Friendly targets use the same native focus path, without a hostile flag or stick input.
    for (int hz : {72, 80, 90, 120, 200}) for (int nativeHz : {20, 30}) {
        prepare(PLAYER_FORM_HUMAN, 0, true);
        p->stateFlags3 = 0;
        p->stateFlags1 = PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS;
        enemy.flags = ACTOR_FLAG_ATTENTION_ENABLED;
        CONTROLLER1(&play->state)->rel.stick_x = 0;
        f.visualAlpha = 1;
        const float baselineYaw = yaw(sample());
        const auto nativeStart = play->gameplayFrames;
        const auto renderStart = f.timeSeconds;
        for (int i = 1; i <= hz; ++i) {
            const double tick = double(i) * nativeHz / hz;
            const auto nativeTick = uint32_t(std::floor(tick)) + 1;
            play->gameplayFrames = nativeStart + nativeTick;
            enemy.focus.pos.x = float(nativeTick);
            f.visualAlpha = float(tick - std::floor(tick));
            f.timeSeconds = renderStart + double(i) / hz;
            const auto frame = mmvrgame::TestCameraFrame(f);
            if (std::abs(difference(yaw(frame), baselineYaw) - std::atan2(float(tick), 120.f)) >= .00015f) {
                const auto facts = mmvrgame::SceneFacts(play);
                std::ofstream("native-orbit-target-detail.json") << "{\"hz\":" << hz << ",\"nativeHz\":" << nativeHz
                    << ",\"i\":" << i << ",\"tick\":" << tick << ",\"yaw\":" << yaw(frame)
                    << ",\"active\":" << frame.active << ",\"transition\":" << facts.transition
                    << ",\"locked\":" << facts.playerLocked << ",\"cinematic\":" << facts.cinematic
                    << ",\"physical\":" << mmvr::PhysicalActionsAllowed() << "}";
            }
            check(std::abs(difference(yaw(frame), baselineYaw) - std::atan2(float(tick), 120.f)) < .00015f,
                  "Moving friendly target did not track at interpolated XR cadence");
        }
        enemy.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    }
    // Acquire an off-center target without a snap, then settle on its native focus.
    prepare(PLAYER_FORM_HUMAN, 0, true);
    enemy.focus.pos = {120, 2000, 0};
    auto acquire = sample();
    check(std::abs(difference(yaw(acquire), 3.14159265359f)) < .0001f, "Off-axis acquisition snapped");
    auto settled = acquire;
    for (int i = 0; i < 90; ++i) {
        auto next = sample();
        check(std::abs(difference(yaw(next), yaw(settled))) <= 6.28318530718f/90 + .0001f,
              "Target acquisition exceeded per-frame turn bound");
        settled = next;
    }
    check(std::abs(difference(yaw(settled), yaw(acquire)) - 1.57079632679f) < .0002f,
          "Off-axis target failed to center");
    prepare(PLAYER_FORM_HUMAN, 0, true);
    auto attackStart = sample(); p->meleeWeaponState = PLAYER_MELEE_WEAPON_STATE_1;
    f.visualOffset[0] = 5;
    auto attacking = sample();
    check(std::abs(difference(yaw(attacking), yaw(attackStart)) - std::atan2(-5.f, 120.f)) < .0001f,
          "Ordinary melee must not interrupt strafe orbit");
    // Head yaw still controls gaze; orbit changes the basis, never the raw pose.
    prepare(PLAYER_FORM_HUMAN, 0, true);
    sample(); f.visualOffset[0] = 4;
    auto facing = sample(); const auto beforeHead = MMVR_InputYaw(0);
    f.head.orientation = {0, std::sin(.2f), 0, std::cos(.2f)};
    auto looked = sample();
    check(std::abs(difference(yaw(looked), yaw(facing))) < .0001f, "Head movement rotated orbit basis");
    check(std::abs(static_cast<s16>(MMVR_InputYaw(0) - beforeHead) * (3.14159265359f / 32768) - .4f) < .0002f,
          "Head tracking stopped owning gaze");
    *p = savedPlayer; enemies = savedEnemies; play->csCtx = savedCs;
    play->gameplayFrames = savedFrames;
    play->pauseCtx.state = savedPause; *CONTROLLER1(&play->state) = savedInput;
    mmvr::GetSettings() = savedSettings;
    mmvrgame::ResetTestCamera(); mmvr::SetNativeTestTracking(false);
    std::ofstream("native-lock-on-orbit.json") << "{\"passed\":true,\"checks\":" << checks << "}";
}
#endif
