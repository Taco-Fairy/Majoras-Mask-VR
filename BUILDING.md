# Building Majora's Mask VR

This repository contains the native port under `src/2ship2harkinian`, shared VR code under `src/mmvr-runtime`, and Android/updater code under `platform`. The three modified native dependencies are vendored at their tested revisions; do not replace them with unmodified upstream submodules. `SOURCE_VERSION.json` records the tested source and original dependency revisions. Keep file bytes unchanged: generated save-state adapters validate their source hashes.

The release's Nintendo ROM and extracted `mm.o2r` are not included. Supply your own supported legal dump to play. Private test tools are off in beta builds. The optional Fipps font is omitted from distribution; the public VR font fallback is retained.

## Tools

Use CMake 3.26 or newer, Python 3, Git and PowerShell 7. Windows uses Visual Studio 2022 C++ tools and the Windows SDK, x64 static vcpkg dependencies and OpenXR. Quest uses JDK 17, Android SDK platform 35/build tools 35.0.0, NDK 27.0.12077973 and Ninja. Set `JAVA_HOME` and `ANDROID_HOME` to your own installations. The Gradle wrapper pins its distribution checksum.

## Windows

1. Install vcpkg into `toolchains/vcpkg`, bootstrap it and install `openxr:x64-windows-static`. Native CMake installs its other declared dependencies.
2. Run `./tools/build-vr-first-person.ps1 -Step Configure`, then `-Step Build`, then `-Step Install` from PowerShell. The support archive is generated into the build/install output; no ROM is needed to compile the port.
3. The installed output is under `run/vr-first-person`. Copy the updater/launcher files from `platform/updater` and `platform/version.json` alongside the executable for a distributable layout. Build the runtime probe separately from `src/mmvr-runtime` with `MMVR_BUILD_TESTS=ON` and the same OpenXR_DIR; copy `mmvr-runtime-probe.exe` alongside the launcher. Keep dependency license notices and extraction metadata with your build.

## Quest

1. Set the tool environment above and run `./tools/build-quest.ps1 -Step Configure`, then `-Step Native`.
2. Generate the support archive using the Windows build above, then run `./tools/package-quest.ps1`. It stages native libraries, extraction metadata, support resources, version/feed and license notices before Gradle packaging.
3. A first local package creates your own key under `toolchains/quest-signing`. That directory must stay private. Your own signer cannot update an official Full Dive Games installation; use a separate development installation/device. Keep official saves backed up. Official signing keys are never included here.

The original build-machine paths have been replaced with tool environment variables in the helper scripts. These are corresponding source/build instructions, not a claim of bit-for-bit binary reproducibility on every toolchain. The release binaries were built from the recorded source inputs; the live development checkout is kept separate from this publication checkout.

## Native-state adapter maintenance

Generated adapters and their metadata are included. If changing the native player/actor layouts or static state, regenerate and validate them before shipping. The `state-*` Python tools use the matching `libclang` Python package and library under `tools/state-reflection` (install with `python -m pip install libclang==18.1.1 --target tools/state-reflection` on Windows) and the Android compilation database. Save states remain exact-build/content-specific.
