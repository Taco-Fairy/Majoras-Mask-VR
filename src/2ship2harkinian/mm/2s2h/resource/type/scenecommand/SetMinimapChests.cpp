#include "SetMinimapChests.h"

namespace SOH {

MinimapChestData* SetMinimapChests::GetPointer() {
    return chests.data();
}

size_t SetMinimapChests::GetPointerSize() {
    return chests.size() * sizeof(MinimapChestData);
}

} // namespace SOH