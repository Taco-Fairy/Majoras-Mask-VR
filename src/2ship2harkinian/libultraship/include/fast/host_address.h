#pragma once
#include <cstdint>
namespace Fast {
// Range-check the address bits only. Android/AArch64 heap pointers may carry
// an allocator tag in the top byte; retain that tag on every dereference/free.
constexpr bool PlausibleHostAddress(uintptr_t address, bool topByteTagged) {
#if UINTPTR_MAX > 0xFFFFFFFFu
    if (topByteTagged) address &= UINT64_C(0x00FFFFFFFFFFFFFF);
    return address >= 0x10000 && address <= UINT64_C(0x0000FFFFFFFFFFFF);
#else
    return address >= 0x10000;
#endif
}
constexpr bool PlausibleHostAddress(uintptr_t address) {
#if defined(__ANDROID__) && defined(__aarch64__)
    return PlausibleHostAddress(address, true);
#else
    return PlausibleHostAddress(address, false);
#endif
}
static_assert(PlausibleHostAddress(0x10000, false));
static_assert(!PlausibleHostAddress(0, true));
#if UINTPTR_MAX > 0xFFFFFFFFu
static_assert(PlausibleHostAddress(UINT64_C(0xB400007234567890), true));
static_assert(!PlausibleHostAddress(UINT64_C(0xB400007234567890), false));
static_assert(!PlausibleHostAddress(UINT64_C(0xFFFFFFFFFFFFFFFF), true));
static_assert(!PlausibleHostAddress(UINT64_C(0xB400000000000001), true));
#endif
}
