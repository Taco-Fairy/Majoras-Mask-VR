#include "Cutscene.h"
#include <mutex>
#include <unordered_set>
#include <libultraship/libultra/gbi.h>

namespace SOH {
namespace {
struct IntroScripts { std::mutex mutex; std::unordered_set<const void*> entries, maskFalls, playerScripts; };
// Resource teardown can run during process shutdown; retain the tiny registry
// until process exit rather than depend on static destruction order.
IntroScripts& Registry() { static auto* value = new IntroScripts; return *value; }
}
Cutscene::~Cutscene() { auto& r=Registry(); std::lock_guard lock(r.mutex); r.entries.erase(commands.data()); r.maskFalls.erase(commands.data()); r.playerScripts.erase(commands.data()); }
bool Cutscene::IsAreaIntroduction(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.entries.contains(script);
}
bool Cutscene::HasOriginalMaskFall(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.maskFalls.contains(script);
}
bool Cutscene::HasPlayerParticipation(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.playerScripts.contains(script);
}
uint32_t* Cutscene::GetPointer() {
    if (hasPlayerCue) { auto& r=Registry(); std::lock_guard lock(r.mutex); r.playerScripts.insert(commands.data()); }
    if (hasOriginalMaskFall) { auto& r=Registry(); std::lock_guard lock(r.mutex); r.maskFalls.insert(commands.data()); }
    if (showsSceneTitleCard) { auto& r=Registry(); std::lock_guard lock(r.mutex); r.entries.insert(commands.data()); }
    return commands.data();
}

size_t Cutscene::GetPointerSize() {
    return commands.size() * sizeof(uint32_t);
}
} // namespace SOH