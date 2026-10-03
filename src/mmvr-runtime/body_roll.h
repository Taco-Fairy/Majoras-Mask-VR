#pragma once
#include "body_ik.h"
#include <array>

namespace mmvr::body {
// Only an eye-render pose is held. Native joints, callbacks and roll collision
// continue updating. Fixed storage; no per-frame allocation or resource lookup.
struct RollPose {
    static constexpr unsigned LimbCount = 24;
    std::array<Matrix, LimbCount> local{}, posed{};
    std::array<const void*, LimbCount> addresses{};
    Matrix root{}, inverse{};
    const void* owner = nullptr;
    uint64_t generation = 0;
    uint32_t valid = 0;
    int form = -1;
    bool enabled = false, rolling = false, ready = false;

    void Begin(bool use, bool roll, const void* actor, uint64_t epoch, int shape,
               const Matrix& actorRoot, uint32_t required) {
        if (!use || owner != actor || generation != epoch || form != shape) *this = {};
        addresses.fill(nullptr);
        enabled = use && Finite(actorRoot);
        rolling = enabled && roll;
        owner = actor; generation = epoch; form = shape;
        root = actorRoot; inverse = InversePose(root);
        ready = (valid & required) == required;
    }
    void Record(unsigned limb, const void* address, const Matrix& native) {
        if (!enabled || limb >= LimbCount || !address || !Finite(native)) return;
        if (!rolling) {
            local[limb] = Multiply(native, inverse);
            valid |= uint32_t(1) << limb;
        } else if (ready && (valid & (uint32_t(1) << limb))) {
            posed[limb] = Multiply(local[limb], root);
            addresses[limb] = address;
        }
    }
    const Matrix* Find(const void* address) const {
        if (!rolling || !ready || !address) return nullptr;
        for (unsigned i = 0; i < LimbCount; ++i)
            if (addresses[i] == address) return &posed[i];
        return nullptr;
    }
    bool Waiting() const { return rolling && !ready; }
};
} // namespace mmvr::body
