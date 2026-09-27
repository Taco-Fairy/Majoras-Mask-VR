#ifdef __ANDROID__
#include <SDL.h>
#include <libultraship/libultraship.h>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept>
#include <cstdio>
#include <chrono>
#include <string>
#include <fstream>
// The native game contains transitions advanced by draw callbacks. Suspending
// only GLES while continuing simulation can strand those transitions and waste
// audio work. Pump Android/OpenXR lifecycle events before each native tick.
extern "C" bool MMVR_AndroidCanRunFrame() {
    auto window = Ship::Context::GetRawInstance()->GetWindow();
    window->HandleEvents();
    const bool ready = window->IsFrameReady();
    static bool suspended = false;
    if (suspended == ready) {
        suspended = !ready;
        SDL_Log("MMVR native simulation %s with Android surface", ready ? "resumed" : "suspended");
    }
    return ready;
}
extern "C" void MMVR_AndroidPrepare() {
    const char* path = SDL_AndroidGetExternalStoragePath();
    if (!path || chdir(path) != 0)
        throw std::runtime_error("Cannot open MMVR app storage");
    // Native diagnostics must survive a failure before the normal logger starts.
    std::freopen("native-startup.log", "a", stdout);
    std::freopen("native-startup.log", "a", stderr);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IOLBF, 0);
    auto stamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    auto token = std::to_string(getpid()) + "-" + std::to_string(stamp);
    setenv("MMVR_SESSION_TOKEN", token.c_str(), 1);
    SDL_Log("MMVR app storage ready session=%s", token.c_str());
    setenv("MMVR_ENABLE", "1", 1);
#ifdef MMVR_LOCAL_TEST_TOOLS
    std::ifstream request("mmvr-test-request.txt");
    std::string test;
    std::getline(request, test);
    request.close();
    if (!test.empty() && test.back() == '\r')
        test.pop_back();
    if (test == "koume-potion" || test == "koume-manual") {
        if (test == "koume-manual") setenv("MMVR_KOUME_MANUAL", "1", 1);
        std::remove("mmvr-test-request.txt");
        setenv("MMVR_NATIVE_TEST", "1", 1);
        setenv("MMVR_PROTECT_SAVES", "1", 1);
        setenv("MMVR_CREATE_COMPLETE_SLOT3", "0", 1);
        setenv("MMVR_POTION_SHOP_TEST", "1", 1);
        setenv("MMVR_KOUME_POTION_TEST", "1", 1);
        setenv("MMVR_KOUME_PHYSICAL", "1", 1);
        SDL_Log("MMVR protected Koume handoff diagnostic");
    }
    if (test == "state-capture-flat" || test == "state-reload-flat" || test == "state-import-flat") {
        // Explicit private fixture, never a player slot or immersive session.
        std::remove("mmvr-test-request.txt");
        setenv("MMVR_ENABLE", "0", 1);
        setenv("MMVR_FLAT_PROFILE", "1", 1);
        setenv("MMVR_NATIVE_TEST", "1", 1);
        setenv("MMVR_PROTECT_SAVES", "1", 1);
        setenv("MMVR_NATIVE_STATE_TEST", "1", 1);
        setenv("MMVR_NATIVE_STATE_ACTION", "Hookshot", 1);
        setenv("MMVR_NATIVE_STATE_BACKEND_PROBE", "1", 1);
        setenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY", "state-compat-fixture", 1);
        if (test != "state-capture-flat") {
            setenv("MMVR_NATIVE_STATE_RELOAD", "1", 1);
            setenv("MMVR_NATIVE_STATE_LIVE_PROBE", "1", 1);
        } else {
            unsetenv("MMVR_NATIVE_STATE_RELOAD");
            unsetenv("MMVR_NATIVE_STATE_LIVE_PROBE");
        }
        if (test == "state-import-flat") {
            setenv("MMVR_NATIVE_STATE_IMPORTED", "1", 1);
            unsetenv("MMVR_NATIVE_STATE_TRACE"); // Imported baseline is unknown.
        } else {
            unsetenv("MMVR_NATIVE_STATE_IMPORTED");
            setenv("MMVR_NATIVE_STATE_TRACE", "1", 1);
        }
        SDL_Log("MMVR protected flat state fixture: %s", test.c_str());
    }
    if(test == "performance-hotspots-detailed") {
        setenv("MMVR_DETAILED_RENDERER_PROFILE","1",1);
        test="performance-frame-phases";
    }
    if(test == "performance-ordering-pixels"){setenv("MMVR_ORDERING_PIXELS","1",1);setenv("MMVR_CULL_PIXELS","1",1);test="performance-post-submit";}
    if(test == "performance-post-submit"){setenv("MMVR_POST_SUBMIT_NATIVE","1",1);test="performance-frame-phases";}
    if(test == "performance-frame-phases"){
        setenv("MMVR_FRAME_PHASE_PROFILE","1",1);
        setenv("MMVR_INTERPOLATION_TIME_TRACE","1",1);
        test="performance-hotspots";
    }
    if (test == "paired-vertex-bench") {
        std::remove("mmvr-test-request.txt");
        setenv("MMVR_NATIVE_TEST", "1", 1);
        setenv("MMVR_PROTECT_SAVES", "1", 1);
        setenv("MMVR_PAIRED_VERTEX_BENCH", "1", 1);
        // Use the existing protected three-scene route; no player walking or save edits.
        setenv("MMVR_PERFORMANCE_TEST", "1", 1);
        setenv("MMVR_PERFORMANCE_HOTSPOTS", "1", 1);
        SDL_Log("MMVR protected paired-vertex benchmark boot");
    }
    if(test == "performance-prewait-compare") {setenv("MMVR_PREWAIT_COMPARE","1",1);test="performance-interactive";}
    if(test == "performance-depth-verify") {setenv("MMVR_ASYNC_DEPTH_VERIFY","1",1);test="performance-interactive";}
    if(test == "performance-depth-reference") {setenv("MMVR_ASYNC_DEPTH_REFERENCE","1",1);test="performance-interactive";}
    if(test == "performance-interactive") {
        std::remove("mmvr-test-request.txt");
        setenv("MMVR_NATIVE_TEST", "1", 1);
        setenv("MMVR_PROTECT_SAVES", "1", 1);
        setenv("MMVR_PERFORMANCE_INTERACTIVE", "1", 1);
        setenv("MMVR_FRAME_PHASE_PROFILE", "1", 1);
        setenv("MMVR_INTERPOLATION_TIME_TRACE", "1", 1);
        SDL_Log("MMVR protected interactive performance tour");
    }
    if (test == "flower" || test == "room-flat" || test == "arena-flat" || test == "performance" ||
        test == "performance-detailed" || test == "performance-flat-detailed" || test == "performance-hotspots" || (test == "performance-packet-profile" || test == "performance-batch-profile") || test == "performance-90" ||
        test == "performance-cull-pixels" || test == "performance-cull-reference" || test == "performance-cull-audit" || test == "performance-90-reference" || test == "performance-stream" || test == "performance-flat" || test == "performance-flat-legacy" ||
        test == "shader-flat") {
        std::remove("mmvr-test-request.txt");
        setenv("MMVR_NATIVE_TEST", "1", 1);
        setenv("MMVR_PROTECT_SAVES", "1", 1);
        setenv(test == "flower"        ? "MMVR_FLOWER_TEST"
               : test == "shader-flat" ? "MMVR_SHADER_TEST"
               : test == "arena-flat"  ? "MMVR_ARENA_EXPANSION_TEST"
               : test == "room-flat"   ? "MMVR_DEBUG_TEST"
                                       : "MMVR_PERFORMANCE_TEST",
               "1", 1);
        if (test == "performance-detailed" || test == "performance-flat-detailed")
            setenv("MMVR_DETAILED_RENDERER_PROFILE", "1", 1);
        if (test == "performance-cull-pixels")
            setenv("MMVR_CULL_PIXELS", "1", 1);
        if (test == "performance-cull-reference")
            setenv("MMVR_CULL_REFERENCE", "1", 1);
        if (test == "performance-cull-audit")
            setenv("MMVR_CULL_AUDIT", "1", 1);
        if (test == "performance-90-reference")
            setenv("MMVR_INTERPOLATION_REFERENCE", "1", 1);
        if ((test == "performance-packet-profile" || test == "performance-batch-profile")) {
            setenv("MMVR_CULL_PIXELS", "1", 1);
            setenv("MMVR_PACKET_PROFILE", "1", 1);
            if(test == "performance-batch-profile") setenv("MMVR_BATCH_PROFILE", "1", 1);
        }
        if (test == "performance-hotspots" || (test == "performance-packet-profile" || test == "performance-batch-profile"))
            setenv("MMVR_PERFORMANCE_HOTSPOTS", "1", 1);
        if ((test == "performance-packet-profile" || test == "performance-batch-profile") || test == "performance-hotspots" || test == "performance-90" || test == "performance-90-reference" || test == "performance-cull-audit" || test == "performance-cull-reference" || test == "performance-cull-pixels")
            setenv("MMVR_TEST_DISPLAY_HZ", "90", 1);
        if (test == "performance-stream")
            setenv("MMVR_STREAM_VERTEX_UPLOAD", "1", 1);
        if (test == "performance-flat" || test == "performance-flat-detailed" || test == "performance-flat-legacy" ||
            test == "shader-flat" || test == "arena-flat" || test == "room-flat") {
            setenv("MMVR_ENABLE", "0", 1);
            setenv("MMVR_FLAT_PROFILE", "1", 1);
        }
        if (test == "performance-flat-legacy")
            setenv("MMVR_LEGACY_VERTEX_UPLOAD", "1", 1);
        SDL_Log("MMVR protected diagnostic boot: %s", test.c_str());
    }
#endif
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0"); // Keep pumping xrEndSession during Android pause.
}
#endif
