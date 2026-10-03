#include "menu_search.h"
#include <cstdio>
#include <stdexcept>

namespace mmvr { Settings& GetSettings() noexcept { static Settings settings; return settings; } }
int main() {
    static_assert(!mmvr::PrivateDebugTools);
    mmvr::MenuState menu;
    unsigned checks = 0;
    const auto require = [&](bool ok) { if (!ok) throw std::runtime_error("Public VR search regression"); ++checks; };
    for (const auto& entry : mmvr::VrMenuSearchEntries(menu)) {
        require(entry.section != 33);
        require(entry.row != int(mmvr::Setting::DebugRoomSpawn));
        require(entry.row != int(mmvr::Setting::DebugSkipCutscenes));
        require(entry.row != int(mmvr::Setting::DebugHitboxes));
        require(entry.row != int(mmvr::Setting::SwordDiagnostics));
        require(entry.row != int(mmvr::Setting::PhysicalSword));
        require(entry.row != int(mmvr::Setting::PhysicalShield));
        require(entry.row != int(mmvr::Setting::PhysicalBow));
        require(entry.row != int(mmvr::Setting::PhysicalBottle));
        require(entry.row != int(mmvr::Setting::PhysicalCarry));
        require(entry.row != int(mmvr::Setting::PhysicalMasks));
        require(entry.row != int(mmvr::Setting::PhysicalFists));
        require(entry.row != int(mmvr::Setting::PhysicalFins));
        require(entry.row != int(mmvr::Setting::TrackedAim));
        require(menu.FocusSearchRow(entry.row));
    }
    require(!menu.FocusSearchRow(mmvr::MenuRows + 33));
    require(!menu.FocusSearchRow(int(mmvr::Setting::DebugRoomSpawn)));
    require(!menu.FocusSearchRow(int(mmvr::Setting::PhysicalSword)));
    std::printf("Public VR menu search: %u checks passed\n", checks);
}
