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
inline std::string updateStatus = "Select Check for updates.";
inline void SetUpdateCallback(UpdateCallback callback) {
    updateCallback = callback;
}
inline void RequestUpdate(bool install) {
    if (updateCallback)
        updateCallback(install);
    else
        updateStatus = "Updater is not available in this host.";
}
} // namespace mmvr
