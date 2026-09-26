#include "ship/resource/Resource.h"
#include <spdlog/spdlog.h>

namespace Ship {
IResource::IResource(std::shared_ptr<ResourceInitData> initData) : mInitData(initData) {
}

// Resources may be retained by process-lifetime model/material caches after
// the global logging registry has been destroyed. Destruction must not touch
// the registry (or dereference optional ResourceInitData).
IResource::~IResource() = default;

bool IResource::IsDirty() {
    return mIsDirty;
}

void IResource::Dirty() {
    mIsDirty = true;
}

std::shared_ptr<ResourceInitData> IResource::GetInitData() {
    return mInitData;
}
} // namespace Ship
