#ifdef MMVR_ENABLE
#include "updater.h"
#include "runtime.h"
#include "mods.h"
#include "ui.h"
#include "device_info.h"
#include <libultraship/bridge/consolevariablebridge.h>
extern "C" void MMVR_RefreshModCatalog();
#include "ship/Context.h"
#include "ship/window/Window.h"
#include <chrono>
#include <cctype>
extern "C" {
#include "global.h"
}
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#elif defined(__ANDROID__)
#include <SDL.h>
#include <jni.h>
#endif
namespace {
#ifdef _WIN32
HANDLE worker = nullptr;
bool installing = false, closeRequested = false;
std::filesystem::file_time_type lastStatus{};
void Request(bool install) {
    if (worker) {
        DWORD code = 0;
        if (GetExitCodeProcess(worker, &code) && code == STILL_ACTIVE) {
            mmvr::updateStatus = "An update operation is already running.";
            return;
        }
        CloseHandle(worker);
        worker = nullptr;
    }
    auto root = std::filesystem::current_path();
    auto script = root / "update-mmvr.ps1";
    if (!std::filesystem::is_regular_file(script)) {
        mmvr::updateStatus = "Updater files are missing from this installation.";
        return;
    }
    std::error_code error;
    lastStatus = std::filesystem::last_write_time(root / "updates/status.json", error);
    wchar_t system[MAX_PATH];
    if (!GetSystemDirectoryW(system, MAX_PATH)) {
        mmvr::updateStatus = "Cannot locate Windows PowerShell.";
        return;
    }
    std::wstring shell = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + shell + L"\" -NoLogo -NoProfile -ExecutionPolicy Bypass -File \"" +
                           script.wstring() + L"\" -Root \"" + root.wstring() + L"\"";
    command += install ? L" -WaitForPid " + std::to_wstring(GetCurrentProcessId()) : L" -CheckOnly";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(shell.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, root.c_str(),
                        &startup, &process)) {
        mmvr::updateStatus = "Cannot start updater: " + std::to_string(GetLastError());
        return;
    }
    CloseHandle(process.hThread);
    worker = process.hProcess;
    installing = install;
    closeRequested = false;
    mmvr::updateStatus = "Checking for updates...";
}
void Poll() {
    auto path = std::filesystem::current_path() / "updates/status.json";
    std::error_code error;
    auto stamp = std::filesystem::last_write_time(path, error);
    if (error || stamp == lastStatus)
        return;
    try {
        std::ifstream input(path);
        auto status = nlohmann::json::parse(input);
        mmvr::updateStatus = status.value("message", std::string("Update status unavailable."));
        if (installing && !closeRequested && status.value("state", std::string()) == "waiting") {
            closeRequested = true;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        // A transient read/parse failure must remain eligible for the next poll.
        lastStatus = stamp;
    } catch (...) { mmvr::updateStatus = "Waiting for updater status..."; }
}
#elif defined(__ANDROID__)
void Request(bool install) {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity)
        return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type, "requestMMVRUpdate", "(Z)V");
    if (method)
        env->CallVoidMethod(activity, method, jboolean(install));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mmvr::updateStatus = "Android update request failed.";
    }
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}
void Poll() {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity)
        return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type, "getMMVRUpdateStatus", "()Ljava/lang/String;");
    jstring value = method ? static_cast<jstring>(env->CallObjectMethod(activity, method)) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mmvr::updateStatus = "Android updater status unavailable.";
    } else if (value) {
        const char* text = env->GetStringUTFChars(value, nullptr);
        if (text) {
            mmvr::updateStatus = text;
            env->ReleaseStringUTFChars(value, text);
        }
        env->DeleteLocalRef(value);
    }
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}
#else
void Request(bool) {
    mmvr::updateStatus = "Updater unavailable for this platform.";
}
void Poll() {
}
#endif
} // namespace
static void SharedFiles() {
#ifdef __ANDROID__
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity)
        return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type, "requestMMVRSharedFolder", "()V");
    if (method)
        env->CallVoidMethod(activity, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mmvr::updateStatus = "Cannot open the MMVR folder picker.";
    }
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
#else
    mmvr::updateStatus = "Place .o2r / .otr packs in mods next to 2ship.exe. Restart after changes.";
#endif
}
static void RefreshMods() {
#ifdef __ANDROID__
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return;
    auto type = env->GetObjectClass(activity);
    auto method = env->GetMethodID(type, "refreshMMVRPacks", "()V");
    if (method) env->CallVoidMethod(activity, method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); mmvr::updateStatus = "Pack refresh failed."; }
    env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
#else
    MMVR_RefreshModCatalog();
#endif
}
static void ExportDiagnostics() {
    try {
        nlohmann::json report;
        report["schema"] = 1;
        report["runtime"] = mmvr::deviceInfo.runtime;
        report["headset"] = mmvr::deviceInfo.headset;
        report["displayHz"] = mmvr::deviceInfo.displayHz;
        report["cadenceHz"] = mmvr::deviceInfo.cadenceHz;
        report["eyeResolution"] = {mmvr::deviceInfo.eyeWidth, mmvr::deviceInfo.eyeHeight};
        // Only allowlisted numeric VR preferences, never paths, saves or raw logs.
        for (size_t i=0;i<size_t(mmvr::Setting::Count);++i)
            report["vrSettings"][mmvr::SettingDefinitions[i].key] = mmvr::GetSettings().Get(mmvr::Setting(i));
        report["modCount"] = mmvr::modPacks.size();
        report["mods"]=nlohmann::json::array();
        for(const auto& pack:mmvr::modPacks) {
            // Stable opaque IDs distinguish configurations without leaking folder names.
            uint64_t id=14695981039346656037ull;
            for(unsigned char c:pack.id) {id^=c;id*=1099511628211ull;}
            report["mods"].push_back({{"opaqueId",std::to_string(id)},{"enabled",pack.enabled}});
        }
        for(const char* name:{"mmvr.log","logs/2 Ship 2 Harkinian.log"}) {
            std::ifstream log(name,std::ios::binary|std::ios::ate);
            if(!log)continue;
            const auto size=log.tellg();
            if(size<0)continue;
            log.seekg(std::max<std::streamoff>(0,std::streamoff(size)-65536));
            std::string line; unsigned errors=0,warnings=0,crashes=0;
            while(std::getline(log,line)) {
                std::transform(line.begin(),line.end(),line.begin(),[](unsigned char c){return char(std::tolower(c));});
                errors+=line.find("error")!=std::string::npos;
                warnings+=line.find("warning")!=std::string::npos;
                crashes+=line.find("crash")!=std::string::npos || line.find("fatal")!=std::string::npos;
            }
            report["recentLogSignals"][name]={{"errors",errors},{"warnings",warnings},{"crashOrFatal",crashes}};
        }
        report["enabledModCount"] = std::count_if(mmvr::modPacks.begin(),mmvr::modPacks.end(),[](const auto& p){return p.enabled;});
        std::ifstream version("version.json");
        if(version) {
            auto value=nlohmann::json::parse(version);
            report["version"]=value.value("version",std::string("unknown"));
            report["build"]=value.value("build",0);
        }
        report["privacy"]="No save files, mod names, personal paths, credentials or raw logs included.";
        std::filesystem::create_directories("diagnostics");
        std::ofstream out("diagnostics/mmvr-report.json",std::ios::trunc);
        out << report.dump(2); out.flush();
        if(!out)throw std::runtime_error("Cannot write report");
        mmvr::supportStatus="Report saved: diagnostics/mmvr-report.json (review before sharing).";
    } catch(...) { mmvr::supportStatus="Diagnostic export failed. Check available storage."; }
}
extern "C" void MMVR_PollUpdater() {
    mmvr::refreshMods = RefreshMods;
    mmvr::exportDiagnostics = ExportDiagnostics;

    mmvr::sharedFilesCallback = SharedFiles;
    mmvr::SetUpdateCallback(Request);
    static auto next = std::chrono::steady_clock::time_point{};
    auto now = std::chrono::steady_clock::now();
    if (now < next)
        return;
    next = now + std::chrono::milliseconds(500);
#ifdef __ANDROID__
    static std::filesystem::file_time_type packStamp{};
    std::error_code packError;
    auto stamp = std::filesystem::last_write_time("shared-pack-cache/packs.json", packError);
    if (!packError && stamp != packStamp) { packStamp = stamp; MMVR_RefreshModCatalog(); }
#endif
    static bool started = false, notified = false;
    if (!started) {
        started = true;
        if (mmvr::GetSettings().Get(mmvr::Setting::CheckUpdatesOnLaunch) > .5f &&
            !(mmvr::PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST"))) Request(false);
    }
    Poll();
    mmvr::updateAvailable = mmvr::updateStatus.rfind("Update ",0)==0 &&
        mmvr::updateStatus.find("is available")!=std::string::npos;
    if (mmvr::updateAvailable && !notified && mmvr::InputFocused() && (!gPlayState || gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN || mmvr::GetMenu().open)) {
        notified=true;
        // Show the notice without installing or closing the game. Input remains
        // in the ordinary menu, which the player can dismiss normally.
        auto& menu=mmvr::GetMenu();
        if(!menu.open) mmvr::OpenSystemSettings();
    }
}
#endif
