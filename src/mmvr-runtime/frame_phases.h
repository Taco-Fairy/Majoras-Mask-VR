#pragma once
#include "settings.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
namespace mmvr {
// Diagnostic-only clocks at frame boundaries, never per triangle. Counters cover
// the interval since the previous XR submission; presentation belongs to that
// previous frame. Depth reads are a subset of game work, not an additive stage.
inline bool FramePhaseEnabled() {
    static const bool enabled = [] {
        const char* test = std::getenv("MMVR_NATIVE_TEST");
        const char* phase = std::getenv("MMVR_FRAME_PHASE_PROFILE");
        return PrivateDebugTools && test && phase && !std::strcmp(test,"1") && !std::strcmp(phase,"1");
    }();
    return enabled;
}
enum class FramePhase { Game, Interpolation, NativePass, GuiBefore, GuiAfter, Present, AudioWait, DepthRead, Count };
inline std::array<double, size_t(FramePhase::Count)> framePhaseMs{};
inline int framePhaseInterpolationIndex = -1;
struct FramePhaseScope {
    FramePhase phase;
    bool enabled;
    std::chrono::steady_clock::time_point begin;
    explicit FramePhaseScope(FramePhase p, bool condition=true):phase(p),enabled(condition && FramePhaseEnabled()) {
        if(enabled)begin=std::chrono::steady_clock::now();
    }
    void Stop() {
        if(enabled){framePhaseMs[size_t(phase)]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();enabled=false;}
    }
    ~FramePhaseScope(){Stop();}
};
inline void RecordFramePhases(int scene,unsigned nativeFrame,bool focused,double beforeXr,double waitMs,
                              const std::array<double,5>& xr,double work,double budget,bool flush) {
    if(!FramePhaseEnabled())return;
    struct Row {int scene,index;unsigned frame;bool focused;double before,wait,work,budget;std::array<double,8> phases;std::array<double,5> xr;};
    static std::vector<Row> rows=[](){std::vector<Row> v;v.reserve(1024);return v;}();
    rows.push_back({scene,framePhaseInterpolationIndex,nativeFrame,focused,beforeXr,waitMs,work,budget,framePhaseMs,xr});
    framePhaseMs={};
    if(!flush && rows.size()<1024)return;
    static std::ofstream out("mmvr-frame-phases.csv");
    static bool header=[](){
        const char* token=std::getenv("MMVR_SESSION_TOKEN");
        std::ofstream("mmvr-frame-phases-session.txt") << (token ? token : "unknown");
        out<<"scene,nativeFrame,interpolationIndex,focused,beforeXrMs,xrWaitMs,workMs,budgetMs,gameMs,interpolationMs,nativePassMs,guiBeforeMs,previousGuiAfterMs,previousPresentMs,audioWaitMs,depthReadSubsetMs,trackingMs,eyesMs,hudMs,uiMs,submitMs\n";return true;}();
    for(const auto& r:rows){out<<r.scene<<','<<r.frame<<','<<r.index<<','<<r.focused<<','<<r.before<<','<<r.wait<<','<<r.work<<','<<r.budget;for(double v:r.phases)out<<','<<v;for(double v:r.xr)out<<','<<v;out<<'\n';}
    out.flush();rows.clear();
}
// Diagnostic-only split of the XR eye stage. Match this CSV to the frame-phase
// session token; a copied timing file from another process is not valid evidence.
inline void RecordEyeSubstages(int scene, unsigned nativeFrame, bool focused, bool paired,
                               double sharedPrepareDrawMs, const std::array<double, 2>& eyeDrawMs,
                               const std::array<double, 2>& eyeComposeCopyMs) {
    if (!FramePhaseEnabled()) return;
    static std::ofstream out("mmvr-eye-substages.csv");
    static unsigned rows = 0;
    if (!rows) {
        const char* token = std::getenv("MMVR_SESSION_TOKEN");
        std::ofstream("mmvr-eye-substages-session.txt") << (token ? token : "unknown");
        out << "scene,nativeFrame,focused,paired,sharedPrepareDrawMs,eye0DrawMs,eye1DrawMs,eye0ComposeCopyMs,eye1ComposeCopyMs\n";
    }
    out << scene << ',' << nativeFrame << ',' << focused << ',' << paired << ',' << sharedPrepareDrawMs
        << ',' << eyeDrawMs[0] << ',' << eyeDrawMs[1] << ',' << eyeComposeCopyMs[0]
        << ',' << eyeComposeCopyMs[1] << '\n';
    if (++rows % 90 == 0) out.flush();
}
// Opt-in motion trace pairs the exact rendered eye pose with XR display timing.
// Buffered rows keep formatting and file writes out of ordinary frame work.
inline void RecordMotionTiming(int scene, unsigned nativeFrame, bool focused, bool stereo,
                               long long displayTime, long long displayPeriod, long long sampledAt,
                               double submittedAt, double intervalMs, float alpha,
                               const std::array<float, 7>& eyePose, const std::array<float, 4>& visualRoot,
                               bool flush) {
    if (!FramePhaseEnabled()) return;
    struct Row {
        int scene; unsigned frame; bool focused, stereo;
        long long display, period, sample;
        double submit, interval; float alpha;
        std::array<float, 7> pose; std::array<float, 4> root;
    };
    static std::vector<Row> rows = [] { std::vector<Row> value; value.reserve(1024); return value; }();
    rows.push_back({scene, nativeFrame, focused, stereo, displayTime, displayPeriod, sampledAt,
                    submittedAt, intervalMs, alpha, eyePose, visualRoot});
    if (!flush && rows.size() < 1024) return;
    static std::ofstream out("mmvr-motion-timing.csv");
    static const bool header = [] {
        const char* token = std::getenv("MMVR_SESSION_TOKEN");
        std::ofstream("mmvr-motion-timing-session.txt") << (token ? token : "unknown");
        out << "scene,nativeFrame,focused,stereo,predictedDisplayTimeNs,displayPeriodNs,poseSampleTimeNs,"
               "submitSteadySeconds,submitIntervalMs,alpha,eyeX,eyeY,eyeZ,eyeQx,eyeQy,eyeQz,eyeQw,"
               "visualX,visualY,visualZ,visualYaw\n";
        out.precision(12);
        return true;
    }();
    for (const auto& row : rows) {
        out << row.scene << ',' << row.frame << ',' << row.focused << ',' << row.stereo << ','
            << row.display << ',' << row.period << ',' << row.sample << ',' << row.submit << ','
            << row.interval << ',' << row.alpha;
        for (float value : row.pose) out << ',' << value;
        for (float value : row.root) out << ',' << value;
        out << '\n';
    }
    out.flush();
    rows.clear();
}
} // namespace mmvr
