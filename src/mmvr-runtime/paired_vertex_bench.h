#pragma once
#include "settings.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace mmvr {
// Private, same-frame benchmark controls. Public builds fold these gates away.
inline thread_local bool pairedVertexPackingReference = false;
inline thread_local bool pairedVertexBenchSampling = false;
inline thread_local uint64_t pairedVertexPackingHits = 0;
inline thread_local uint64_t pairedProjectionHits[2]{};

inline bool PairedVertexBenchEnabled() noexcept {
    if constexpr (!PrivateDebugTools) return false;
    static const bool enabled = [] {
        const char* test = std::getenv("MMVR_NATIVE_TEST");
        const char* bench = std::getenv("MMVR_PAIRED_VERTEX_BENCH");
        return test && bench && std::strcmp(test, "1") == 0 && std::strcmp(bench, "1") == 0;
    }();
    return enabled;
}
} // namespace mmvr
