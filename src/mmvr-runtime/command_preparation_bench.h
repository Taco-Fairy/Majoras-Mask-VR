#pragma once
#include <cstdint>

namespace mmvr {
// Private same-scene A/B instrumentation; never a player setting. The harness
// resets counters at each replay and keeps all other renderer options equal.
struct CommandPreparationCounters {
    uint64_t preparations = 0;
    uint64_t packedReuses = 0;
    uint64_t projectionReuses = 0;
    uint64_t preservedCommands = 0;
    uint64_t draws = 0;
    uint64_t triangles = 0;
    uint64_t loadBlockCalls = 0, loadBlockDescriptorSame = 0, loadBlockConservative = 0;
    uint64_t loadTileCalls = 0, loadTileDescriptorSame = 0, loadTileConservative = 0;
    uint64_t setTileCalls = 0, setTileSame = 0;
    uint64_t setTileSizeCalls = 0, setTileSizeSame = 0;
    uint64_t importTextureCalls = 0;
};
inline thread_local bool commandPreparationReference = false;
inline thread_local bool commandPreparationSampling = false;
inline thread_local bool commandPreparationExperiment = false;
inline thread_local CommandPreparationCounters commandPreparationCounters{};
}
