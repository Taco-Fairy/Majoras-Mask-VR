// Exercise the real G_READFB dispatcher with a CPU-only renderer. This keeps
// the native pictograph result stable across the later eye/HUD replay while
// preserving legacy effect readbacks and two-word command consumption.
void Interpreter::VerifyNativeReadbackCapture() {
    const char* enabled = std::getenv("MMVR_NATIVE_TEST");
    if (!enabled || std::strcmp(enabled, "1") != 0) return;
    if (mmvr::IsExtraPass())
        throw std::runtime_error("Native readback fixture must start outside an eye replay");

    struct Restore {
        GfxExecStack stack = std::move(g_exec_stack);
        decltype(ucode_handler_index) ucode = ucode_handler_index;
        bool tracking = mmvr::nativeTestTracking;
        ~Restore() {
            g_exec_stack = std::move(stack);
            ucode_handler_index = ucode;
            mmvr::SetNativeTestTracking(false);
            if (tracking) mmvr::SetNativeTestTracking(true);
        }
    } restore;

    TriangleProbeRenderer backend;
    backend.readbackEnabled = true;
    Interpreter probe;
    probe.mRapi = &backend;
    CommandInterpreterScope commandScope(&probe);
    ucode_handler_index = ucode_f3dex2;

    unsigned checks = 0;
    auto check = [&](bool condition) {
        ++checks;
        if (!condition) throw std::runtime_error("G_READFB native capture regression");
    };
    auto stepReadback = [&](std::array<uint16_t, 4>& pixels, bool nativeOnly, bool byteswap) {
        F3DGfx commands[2]{};
        constexpr unsigned framebuffer = 7;
        commands[0].words.w0 = (uintptr_t(OTR_G_READFB) << 24) | framebuffer |
                               (byteswap ? uintptr_t(1u << 8) : 0) |
                               (nativeOnly ? uintptr_t(1u << 9) : 0);
        commands[0].words.w1 = reinterpret_cast<uintptr_t>(pixels.data());
        commands[1].words.w0 = (uint32_t(9) << 16) | uint32_t(5);
        commands[1].words.w1 = (uint32_t(2) << 16) | uint32_t(2);
        g_exec_stack.start(commands);
        gfx_step();
        return g_exec_stack.currCmd() == commands + 2;
    };
    auto expected = [](uint16_t color, bool byteswap) {
#if !defined(IS_BIGENDIAN)
        if (byteswap) return uint16_t((color << 8) | (color >> 8));
#endif
        return color;
    };

    mmvr::SetNativeTestTracking(true);
    std::array<uint16_t, 4> photo{};
    backend.readbackColor = 0x1234;
    const bool nativeConsumed = stepReadback(photo, true, true);
    check(backend.readbackCalls == 1 && backend.lastReadbackFramebuffer == 7 &&
          backend.lastReadbackWidth == 2 && backend.lastReadbackHeight == 2 &&
          std::all_of(photo.begin(), photo.end(), [&](uint16_t value) { return value == expected(0x1234, true); }));
    check(nativeConsumed);

    const auto capturedPhoto = photo;
    const auto beforeSkipped = backend.readbackCalls;
    backend.readbackColor = 0x0000;
    mmvr::SetNativeTestEye(0.0f);
    const bool skippedConsumed = stepReadback(photo, true, true);
    check(skippedConsumed && backend.readbackCalls == beforeSkipped && photo == capturedPhoto);

    // The native-only bit must not suppress ordinary native effects such as
    // the Deku-bubble path, even when its extra eye readback is byte-swapped.
    backend.readbackColor = 0x5A2B;
    const bool bubbleConsumed = stepReadback(photo, false, true);
    check(bubbleConsumed && backend.readbackCalls == beforeSkipped + 1 &&
          std::all_of(photo.begin(), photo.end(), [&](uint16_t value) { return value == expected(0x5A2B, true); }));

    mmvr::SetNativeTestTracking(false);
    mmvr::SetNativeTestTracking(true);
    backend.readbackColor = 0x6C91;
    const bool nextNativeConsumed = stepReadback(photo, true, true);
    check(nextNativeConsumed && backend.readbackCalls == beforeSkipped + 2 &&
          std::all_of(photo.begin(), photo.end(), [&](uint16_t value) { return value == expected(0x6C91, true); }));

    const char* session = std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream("native-readback-capture.json") << "{\"session\":\"" << (session ? session : "")
        << "\",\"passed\":true,\"checks\":" << checks
        << ",\"nativeColorRetainedAcrossEyeReplay\":true,\"legacyReadbackStillRuns\":true"
        << ",\"newNativeCaptureRefreshesBuffer\":true,\"twoWordCommandsConsumed\":true}";
}
