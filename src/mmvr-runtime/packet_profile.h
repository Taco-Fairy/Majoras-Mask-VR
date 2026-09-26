#pragma once
#include "settings.h"
#include "culling_audit.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <fstream>
namespace mmvr {
// Paired same-pose diagnostic only. Both paths retain all previous renderer
// optimizations. Readback completes GPU work; reported times are NOT game FPS.
template <class Prepare, class Draw, class Read>
void ProfilePacketCulling(const std::string& label, int eye, Prepare prepare, Draw draw, Read read) {
    const char* enabled = std::getenv("MMVR_PACKET_PROFILE");
    if (!CullingPixelsEnabled() || !enabled || std::strcmp(enabled, "1"))
        return;
    const bool batch = std::getenv("MMVR_BATCH_PROFILE") != nullptr;
    struct Restore {
        bool reference = textureBatchReference;
        float value = GetSettings().Get(Setting::HeadsetCulling);
        ~Restore() {
            GetSettings().Set(Setting::HeadsetCulling, value);
            textureBatchReference = reference;
        }
    } restore;
    std::vector<double> before, after, cpuBefore, cpuAfter;
    uint64_t beforeDraws=0, afterDraws=0, beforeTriangles=0, afterTriangles=0;
    for (int round = 0; round < 5; ++round)
        for (int side = 0; side < 2; ++side) {
            bool candidate = (round + side) % 2;
            prepare();
            cullingReplayReference = false;
            GetSettings().Set(Setting::HeadsetCulling, !batch && candidate ? 1.f : 0.f);
            textureBatchReference = batch && !candidate;
            const auto drawsStart = replayDrawSubmissions, trianglesStart = replayDrawTriangles;
            auto start = std::chrono::steady_clock::now();
            draw();
            const auto cpuElapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            (candidate ? afterDraws : beforeDraws) = replayDrawSubmissions - drawsStart;
            (candidate ? afterTriangles : beforeTriangles) = replayDrawTriangles - trianglesStart;
            auto pixels = read();
            auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (round)
                { (candidate ? after : before).push_back(elapsed);
                  (candidate ? cpuAfter : cpuBefore).push_back(cpuElapsed); }
        }
    std::sort(cpuBefore.begin(), cpuBefore.end());
    std::sort(cpuAfter.begin(), cpuAfter.end());
    std::sort(before.begin(), before.end());
    std::sort(after.begin(), after.end());
    static std::ofstream report("native-packet-profile.jsonl");
    report << "{\"label\":\"" << label << "\",\"eye\":" << eye << ",\"referenceMs\":" << (before[1] + before[2]) * .5
           << ",\"candidateMs\":" << (after[1] + after[2]) * .5 << ",\"referenceCpuSubmitMs\":" << (cpuBefore[1]+cpuBefore[2])*.5
           << ",\"candidateCpuSubmitMs\":" << (cpuAfter[1]+cpuAfter[2])*.5
           << ",\"referenceDraws\":" << beforeDraws << ",\"candidateDraws\":" << afterDraws
           << ",\"referenceTriangles\":" << beforeTriangles << ",\"candidateTriangles\":" << afterTriangles
           << ",\"samples\":4,\"wholeGameFps\":false}\n"
           << std::flush;
}
} // namespace mmvr
