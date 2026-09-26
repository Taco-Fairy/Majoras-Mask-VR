#pragma once
extern "C" {
#include "z64audio.h"
#include "z64save.h"
SoundFontData* AudioLoad_SyncLoadSeqFonts(s32, u32*);
void Audio_SetFileSelectSettings(s8);
extern u8 sSeqCmdWritePos;
}
namespace mmvrtest {
// Shell-only paired-library test: no audio engine or worker is running here.
inline void VerifyNativeAudioBoundaries() {
    const auto oldCount = gAudioCtx.numSequences;
    auto* oldTable = gAudioCtx.sequenceFontTable;
    struct Restore {
        decltype(gAudioCtx.numSequences) count;
        decltype(gAudioCtx.sequenceFontTable) table;
        ~Restore() { gAudioCtx.numSequences = count; gAudioCtx.sequenceFontTable = table; }
    } restore{oldCount, oldTable};
    alignas(u16) u8 table[4]{};
    reinterpret_cast<u16*>(table)[0] = 2;
    gAudioCtx.numSequences = 1;
    gAudioCtx.sequenceFontTable = table;
    u32 font = 0x12345678;
    RequireResource(AudioLoad_SyncLoadSeqFonts(0, &font) == nullptr && font == 0xFF,
                    "Zero-font native sequence returned an invalid result");
    for (s32 id : {-1, 1, 0x7FFFFFFF}) {
        font = 0x12345678;
        RequireResource(AudioLoad_SyncLoadSeqFonts(id, &font) == nullptr && font == 0x12345678,
                        "Out-of-range native sequence accessed the table");
    }
    unsigned invalidModes = 0;
    for (int mode = -128; mode < 128; ++mode) {
        if (mode == SAVE_AUDIO_STEREO || mode == SAVE_AUDIO_MONO || mode == SAVE_AUDIO_HEADSET ||
            mode == SAVE_AUDIO_SURROUND) continue;
        const auto before = sSeqCmdWritePos;
        Audio_SetFileSelectSettings(static_cast<s8>(mode));
        RequireResource(sSeqCmdWritePos == before, "Invalid audio mode queued a command");
        ++invalidModes;
    }
    std::ofstream("native-audio-boundaries.json") << "{\"passed\":true,\"sequenceCases\":4,\"invalidModes\":"
                                                 << invalidModes << "}";
}
}
