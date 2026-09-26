#pragma once
#ifdef MMVR_ENABLE
#include "mods.h"
#include "updater.h"
#include "ship/config/Config.h"
#include <set>
namespace {
std::set<std::string> DisabledVRPacks() {
    std::set<std::string> result;
    try { for (const auto& id : nlohmann::json::parse(CVarGetString("gVR.DisabledPacks", "[]")))
        result.insert(id.get<std::string>()); } catch (...) {}
    return result;
}
void RefreshVRPacks() {
    std::vector<mmvr::ModPack> packs;
    const auto disabled = DisabledVRPacks();
    try {
        for (const char* folder : {"mods", "texturepacks"}) {
            auto root = std::filesystem::path(Ship::Context::LocateFileAcrossAppDirs(folder, appShortName));
            if (!std::filesystem::is_directory(root)) continue;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                if (entry.is_symlink() || !entry.is_regular_file() || !IsValidExtension(entry.path().extension().string())) continue;
                auto id = std::string(folder) + "/" + entry.path().lexically_relative(root).generic_string();
                packs.push_back({id, id, entry.path().string(), !disabled.contains(id)});
            }
        }
#ifdef __ANDROID__
        const auto cache = std::filesystem::current_path() / "shared-pack-cache";
        std::ifstream input(cache / "packs.json");
        if (input) for (const auto& pack : nlohmann::json::parse(input)) {
            auto file = pack.at("file").get<std::string>();
            auto ext = std::filesystem::path(file).extension().string();
            if (file.size() != 64 + ext.size() || !IsValidExtension(ext) ||
                file.substr(0,64).find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::runtime_error("Invalid shared pack index");
            if (!std::filesystem::is_regular_file(cache / file)) continue;
            auto id = "shared/" + file;
            packs.push_back({id, pack.value("name", file), (cache / file).string(), !disabled.contains(id)});
        }
#endif
        std::sort(packs.begin(), packs.end(), [](const auto& a, const auto& b){return a.name < b.name;});
        mmvr::modPacks = std::move(packs);
        mmvr::RebuildModFolders();
        mmvr::updateStatus = std::to_string(mmvr::modPacks.size()) + " packs found. Changes apply after restart. Unpack ZIPs first.";
    } catch (const std::exception& e) { mmvr::updateStatus = std::string("Pack scan failed: ") + e.what(); }
}
void ToggleVRPack(int index) {
    if (index < 0 || size_t(index) >= mmvr::modPacks.size()) return;
    auto disabled = DisabledVRPacks();
    auto& pack = mmvr::modPacks[index];
    const auto old = std::string(CVarGetString("gVR.DisabledPacks", "[]"));
    if (pack.enabled) disabled.insert(pack.id); else disabled.erase(pack.id);
    auto json = nlohmann::json(disabled).dump();
    CVarSetString("gVR.DisabledPacks", json.c_str());
    CVarSave();
    if (!Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded()) {
        CVarSetString("gVR.DisabledPacks", old.c_str());
        mmvr::updateStatus = "Could not save pack selection. Try again.";
        return;
    }
    pack.enabled = !pack.enabled;
    mmvr::updateStatus = "Pack selection saved. Restart the game to apply.";
}
void LoadVRPacks() {
    mmvr::toggleMod = ToggleVRPack;
    RefreshVRPacks();
    for (const auto& pack : mmvr::modPacks)
        if (pack.enabled) {
            auto archive = GetArchiveManager()->AddArchive(pack.path);
            if (!archive) SPDLOG_WARN("Mod pack could not load: {}", pack.name);
        }
    // Alt textures in packs are not selected by the port unless this switch is on.
    if (!CVarGet("gVR.ModAssetsDefaultApplied")) {
        CVarSetInteger("gEnhancements.Mods.AlternateAssets", 1);
        CVarSetInteger("gVR.ModAssetsDefaultApplied", 1);
        CVarSave();
    }
}
}
extern "C" void MMVR_RefreshModCatalog() { RefreshVRPacks(); }
#endif
