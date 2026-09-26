#pragma once
#include "LiveGraph.h"
#include "RestorePlan.h"
namespace mmvr::states {
// Allocator regions own bytes. Typed views may overlap a region (an actor in an
// arena, for example), but cannot create another owner or silently add pointers.
class Ownership {
    std::vector<LiveBlock> blocks;
    std::vector<LiveSymbol> symbols;
    std::vector<RestoreBinding> destinations;
public:
    void Own(std::string id, uint32_t schema, std::span<uint8_t> memory);
    void Symbol(LiveSymbol);
    void Pointer(const void* fieldAddress, PointerType, std::string name, std::vector<uintptr_t> literals = {});
    Snapshot Capture(const Identity&, uint64_t tick) const;
    RestorePlan Prepare(const Snapshot&, const Resolver&) const;
    const std::vector<LiveBlock>& Blocks() const { return blocks; }
};
}
