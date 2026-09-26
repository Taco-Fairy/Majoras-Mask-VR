#pragma once

namespace Rando {

// Draw the native randomizer option pages inside the dedicated in-headset VR menu.
// This is a presentation wrapper; option ownership and persistence remain native.
void DrawVrRandomizerMenu();

// Shared seed editor: keeps native callbacks and input limits in one place.
void DrawVrSeedInput();

// Reset only the selected page when the VR menu is closed or its tab changes.
void ResetVrRandomizerMenu();

} // namespace Rando
