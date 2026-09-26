#pragma once
#include "Archive.h"
namespace mmvr::states {
enum class PointerType { Data, Function, Transient };
struct PointerField {
    uint64_t at; PointerType type; std::string name;
    // Explicit native sentinel values only (for example NO_LAYER == -1).
    // They are scalar tags, never addresses to guess or relocate.
    std::vector<uintptr_t> literals{};
};
struct LiveBlock {
    std::string id;
    uint32_t schema;
    std::span<const uint8_t> bytes;
    // Only compiler-derived, active-variant pointer fields belong here.
    // Runtime/GPU/OS handles must instead be recreated by their owner.
    std::vector<PointerField> pointers;
};
struct LiveSymbol {
    ReferenceKind kind;
    std::string id;
    const void* address;
    uint64_t bytes;
};
// Copies explicitly owned blocks at a quiescent game boundary and converts
// their typed pointers to symbolic references. Unknown ownership fails the
// capture before any slot file is replaced. This does not discover ownership.
// Owned ranges must be disjoint. Asset ranges may nest; the most-specific
// range wins, with deterministic aliases. Owned memory always takes priority.
Snapshot CaptureGraph(const Identity&, uint64_t tick, const std::vector<LiveBlock>&,
                      const std::vector<LiveSymbol>&);
}
