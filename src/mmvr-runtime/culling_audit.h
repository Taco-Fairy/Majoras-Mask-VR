#pragma once
#include "settings.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace mmvr {
inline bool CullingAuditEnabled() {
    static const bool enabled = [] {
        const char* test = std::getenv("MMVR_NATIVE_TEST");
        const char* audit = std::getenv("MMVR_CULL_AUDIT");
        return PrivateDebugTools && test && audit && std::strcmp(test, "1") == 0 && std::strcmp(audit, "1") == 0;
    }();
    return enabled;
}
inline thread_local bool cullingReplayReference = false;
// Same-pose diagnostic switch for material batching only; no gameplay setting.
inline thread_local bool materialBatchReference = false;
inline thread_local bool textureBatchReference = false;
inline thread_local uint64_t replayDrawSubmissions = 0, replayDrawTriangles = 0;
inline bool CullingReferenceEnabled() {
    static const bool enabled = [] {
        const char* test = std::getenv("MMVR_NATIVE_TEST");
        const char* reference = std::getenv("MMVR_CULL_REFERENCE");
        return PrivateDebugTools && test && reference && std::strcmp(test, "1") == 0 && std::strcmp(reference, "1") == 0;
    }();
    return cullingReplayReference || enabled;
}
inline bool CullingPixelsEnabled() {
    static const bool enabled = [] {
        const char* test = std::getenv("MMVR_NATIVE_TEST");
        const char* pixels = std::getenv("MMVR_CULL_PIXELS");
        return PrivateDebugTools && test && pixels && std::strcmp(test, "1") == 0 && std::strcmp(pixels, "1") == 0;
    }();
    return enabled;
}
inline std::string cullingPixelRequest;
inline void RequestCullingPixelCheck(const std::string& name) {
    if (CullingPixelsEnabled())
        cullingPixelRequest = name;
}
// Only the same homogeneous clip planes already used by triangle rejection.
// Near-plane rejection remains disabled, matching the existing renderer.
template <class Vertex>
inline bool BoundsOutsideClip(const Vertex* vertices, std::size_t capacity, std::size_t first, std::size_t last) {
    if (!vertices || first > last || last >= capacity)
        return false;
    unsigned common = 1 | 2 | 4 | 8 | 32;
    for (std::size_t i = first; i <= last; ++i)
        common &= vertices[i].clip_rej;
    return common != 0;
}
struct CullingAudit {
    std::size_t activeDepth = 0;
    uint64_t commands = 0, candidateCommands = 0, bounds = 0, rejectedBounds = 0;
    uint64_t candidates = 0, acceptedTrianglesInCandidates = 0, skippedLists = 0;
    uint64_t packetTests = 0, skippedPackets = 0, skippedPacketCommands = 0, skippedPacketVertices = 0;
    void BeginCommand(std::size_t depth) {
        if (activeDepth && depth < activeDepth)
            activeDepth = 0;
        ++commands;
        if (activeDepth)
            ++candidateCommands;
    }
    void Candidate(std::size_t depth) {
        if (!activeDepth) {
            activeDepth = depth;
            ++candidates;
        }
    }
    void AcceptedTriangle() {
        if (activeDepth)
            ++acceptedTrianglesInCandidates;
    }
    void NewReplay() {
        activeDepth = 0;
    }
};
inline CullingAudit cullingAudit;
inline void WriteCullingAuditVisit(const char* name) {
    const char* test = std::getenv("MMVR_NATIVE_TEST");
    if (!PrivateDebugTools || !test || std::strcmp(test, "1") != 0)
        return;
    static std::ofstream log = [] {
        std::ofstream output("native-culling-audit.log");
        const char* token = std::getenv("MMVR_SESSION_TOKEN");
        output << "session " << (token ? token : "") << "\n";
        output << "mode "
               << (CullingAuditEnabled()       ? "observe-only"
                   : CullingReferenceEnabled() ? "reference"
                                               : "native-bounds")
               << "; cumulative counters include transitions\n";
        return output;
    }();
    const auto& a = cullingAudit;
    log << name << " commands=" << a.commands << " candidateCommands=" << a.candidateCommands << " bounds=" << a.bounds
        << " rejectedBounds=" << a.rejectedBounds << " candidates=" << a.candidates << " packetTests=" << a.packetTests
        << " skippedPackets=" << a.skippedPackets << " skippedPacketCommands=" << a.skippedPacketCommands
        << " skippedPacketVertices=" << a.skippedPacketVertices
        << " acceptedTrianglesInCandidates=" << a.acceptedTrianglesInCandidates << " skippedLists=" << a.skippedLists
        << "\n"
        << std::flush;
}
} // namespace mmvr
