#include "RestorePlan.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
namespace mmvr::states {
RestorePlan::RestorePlan(const Snapshot& snapshot, const std::vector<RestoreBinding>& bindings,
                         const Resolver& resolve) {
    Validate(snapshot);
    if (snapshot.blocks.size() != bindings.size()) throw Error("Incomplete native restore bindings");
    std::map<std::string, const RestoreBinding*> targets;
    std::vector<const RestoreBinding*> ordered;
    ordered.reserve(bindings.size());
    for (const auto& binding : bindings) {
        const auto address = reinterpret_cast<uintptr_t>(binding.destination.data());
        if (!address || binding.destination.empty() || binding.destination.size() > UINTPTR_MAX-address ||
            !targets.emplace(binding.id, &binding).second)
            throw Error("Invalid native restore destination: " + binding.id);
        ordered.push_back(&binding);
    }
    // Validate once in address order. Each range has already passed overflow
    // checks; any overlap must occur between neighboring sorted intervals.
    std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
        return reinterpret_cast<uintptr_t>(a->destination.data()) <
               reinterpret_cast<uintptr_t>(b->destination.data());
    });
    for (size_t i = 1; i < ordered.size(); ++i) {
        const auto& previous = ordered[i - 1]->destination;
        if (reinterpret_cast<uintptr_t>(ordered[i]->destination.data()) <
            reinterpret_cast<uintptr_t>(previous.data()) + previous.size())
            throw Error("Overlapping native restore destinations: " + ordered[i]->id);
    }
    writes.reserve(snapshot.blocks.size());
    for (const auto& block : snapshot.blocks) {
        const auto found = targets.find(block.id);
        if (found == targets.end() || found->second->schema != block.schema ||
            found->second->destination.size() != block.bytes.size())
            throw Error("Native layout mismatch: " + block.id);
        Write write{found->second->destination, block.bytes};
        for (const auto& reference : block.references) {
            ExternalRange target;
            if (reference.kind == ReferenceKind::Owned) {
                const auto owned = targets.find(reference.target);
                if (owned == targets.end()) throw Error("Missing native restore target: " + reference.target);
                target = {owned->second->destination.data(), owned->second->destination.size()};
            } else {
                if (!resolve) throw Error("Native restore has no external resolver");
                target = resolve(reference.kind, reference.target);
            }
            const auto address = reinterpret_cast<uintptr_t>(target.address);
            if (!address || reference.offset > target.bytes ||
                reference.offset > UINTPTR_MAX - address ||
                (reference.kind == ReferenceKind::Function && (reference.offset || target.bytes)))
                throw Error("Invalid relocated native reference: " + reference.target);
            const uintptr_t pointer = address + reference.offset;
            std::memcpy(write.candidate.data()+reference.at, &pointer, sizeof(pointer));
        }
        writes.push_back(std::move(write));
    }
}
void RestorePlan::Commit() noexcept {
    if (committed) return;
    // No allocation, file access, resolution, or fallible callbacks at commit.
    for (const auto& write : writes)
        std::memcpy(write.destination.data(), write.candidate.data(), write.candidate.size());
    committed = true;
}
}
