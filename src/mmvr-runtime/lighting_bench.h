#pragma once
#include <cstddef>
#include <cstdint>
namespace mmvr {
enum class LightingBenchVariant : uint8_t { Off, PriorOneWay, RetainedThreeWay };
struct LightingBenchStats {
    uint64_t vertexCalls=0,calls=0, hits=0, misses=0;
    size_t peakCacheBytes=0;
};
LightingBenchVariant GetLightingBenchVariant() noexcept;
void SetLightingBenchVariant(LightingBenchVariant variant, int replayIndex) noexcept;
int LightingBenchReplayIndex() noexcept;
void ResetLightingBenchStats() noexcept;
void RecordLightingBenchVertexCall() noexcept;
void RecordLightingBenchCall(bool hit, size_t cacheBytes) noexcept;
LightingBenchStats GetLightingBenchStats() noexcept;
}
