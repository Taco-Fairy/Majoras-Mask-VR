#pragma once
#include "settings.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
namespace mmvr {
// Per-draw clocks distort normal throughput measurements. Enable them only
// for a deliberately detailed protected profile; ordinary tours use frame totals.
enum class RendererStage { DrawBatch, VertexUpload, DriverDraw, TextureUpload, ShaderCompile, Count };
struct RendererStageTotal {
    double ms = 0;
    unsigned count = 0;
};
inline std::array<RendererStageTotal, size_t(RendererStage::Count)> rendererTotals{};
inline bool RendererMeasurementEnabled() {
    static const bool enabled = []() {
        const char* test = std::getenv("MMVR_NATIVE_TEST");
        const char* detail = std::getenv("MMVR_DETAILED_RENDERER_PROFILE");
        return PrivateDebugTools && test && detail && std::strcmp(test, "1") == 0 && std::strcmp(detail, "1") == 0;
    }();
    return enabled;
}
struct RendererTimingScope {
    RendererStage stage;
    bool enabled;
    std::chrono::steady_clock::time_point start;
    explicit RendererTimingScope(RendererStage s) : stage(s), enabled(RendererMeasurementEnabled()) {
        if (enabled)
            start = std::chrono::steady_clock::now();
    }
    void Stop() {
        if (enabled) {
            auto& value = rendererTotals[size_t(stage)];
            value.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            ++value.count;
            enabled = false;
        }
    }
    ~RendererTimingScope() { Stop(); }
};
} // namespace mmvr
