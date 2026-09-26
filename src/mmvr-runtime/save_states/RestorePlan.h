#pragma once
#include "Archive.h"
namespace mmvr::states {
// Addresses belong to the current process. The plan stages copies with pointers
// already relocated to their final destinations; live memory is untouched until
// every component and external reference has passed validation.
struct RestoreBinding {
    std::string id;
    uint32_t schema;
    std::span<uint8_t> destination;
};
class RestorePlan {
    struct Write { std::span<uint8_t> destination; Bytes candidate; };
    std::vector<Write> writes;
    bool committed = false;
public:
    RestorePlan(const Snapshot&, const std::vector<RestoreBinding>&, const Resolver&);
    void Commit() noexcept;
};
}
