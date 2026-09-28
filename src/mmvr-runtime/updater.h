#pragma once
#include <string>
namespace mmvr {
using UpdateCallback = void (*)(bool install);
inline UpdateCallback updateCallback = nullptr;
inline void (*sharedFilesCallback)() = nullptr;
inline void RequestSharedFiles() {
    if (sharedFilesCallback)
        sharedFilesCallback();
}
inline void (*exportDiagnostics)() = nullptr;
inline bool (*statePreflight)(int) = nullptr;
inline bool (*stateReady)() = nullptr;
inline bool setupGuideVisible = false;
inline bool setupGuideRendered = false;
inline bool setupGuideCompleted = false;
// Programmatic menus need the same headset anchoring/input guard as a button-open.
void OpenSystemSettings();
inline bool systemMenuOpenRequested = false;
inline bool updateAvailable = false;
inline bool confirmUpdateInstall = false;
inline std::string supportStatus;
inline std::string updateStatus = "Select Check for updates.";
inline void SetUpdateCallback(UpdateCallback callback) {
    updateCallback = callback;
}
inline void RequestUpdate(bool install) {
    supportStatus.clear();
    if (updateCallback)
        updateCallback(install);
    else
        updateStatus = "Updater is not available in this host.";
}
} // namespace mmvr
