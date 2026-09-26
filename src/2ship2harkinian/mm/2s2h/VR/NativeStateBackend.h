#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include <filesystem>
namespace mmvrgame {
// Render-thread only, between native frames, with audio producer quiescent.
// These operations are separate from ordinary game saves and global VR settings.
void InitializeExactStateMenu();
void SaveExactNativeState(int slot,const std::filesystem::path& directory);
void LoadExactNativeState(int slot,const std::filesystem::path& directory);
}
#endif
