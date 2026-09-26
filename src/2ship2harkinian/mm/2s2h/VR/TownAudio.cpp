#ifdef MMVR_ENABLE
#include "TownAudio.h"
#include "ScenePresentation.h"
#include <fstream>
#include <algorithm>
extern "C" {
#include "global.h"
extern u8 sStartSeqDisabled;
void Audio_StartSceneSequence(u16 seqId);
}
extern "C" void MMVR_UpdateTownAudio(PlayState* play) {
    static PlayState* owner = nullptr;
    static int scene = -1;
    static unsigned lastFrame = 0;
    static bool recovered = false;
    if (!play)
        return;
    if (owner != play || scene != play->sceneId || play->gameplayFrames < lastFrame) {
        owner = play;
        scene = play->sceneId;
        recovered = false;
    }
    lastFrame = play->gameplayFrames;
    const bool town =
        scene == SCENE_CLOCKTOWER || scene == SCENE_TOWN || scene == SCENE_ICHIBA || scene == SCENE_BACKTOWN;
    if (!town || recovered || lastFrame < 30 || lastFrame > 240)
        return;
    // Follow the native daytime/ambience policy; a silent night is intentional.
    if (play->sceneSequences.ambienceId != AMBIENCE_ID_13 &&
        (CURRENT_TIME < CLOCK_TIME(6, 0) || CURRENT_TIME > CLOCK_TIME(17, 10)))
        return;
    const auto facts = mmvrgame::SceneFacts(play);
    if (facts.transition || facts.titleSequence || facts.cinematic || !facts.alive ||
        play->pauseCtx.state != PAUSE_STATE_OFF || play->msgCtx.msgMode != MSGMODE_NONE || sStartSeqDisabled ||
        gAudioCtx.resetStatus != 0 || Environment_IsFinalHours(play) ||
        play->sceneSequences.seqId != NA_BGM_CLOCK_TOWN_MAIN_SEQUENCE ||
        AudioSeq_GetActiveSeqId(SEQ_PLAYER_SFX) == NA_BGM_DISABLED)
        return;
    if (AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN) != NA_BGM_DISABLED) {
        recovered = true;
        return;
    }
    // A dawn/heap transition can stop BGM after Play_Init caches it as started.
    // Reissue once after the native audio and title sequence are ready. Never override a live sequence.
    const int day = std::clamp(int(gSaveContext.save.day) - 1, 0, 2);
    Audio_StartSceneSequence(play->sceneSequences.seqId);
    SEQCMD_SET_SEQPLAYER_IO(SEQ_PLAYER_BGM_MAIN, 4, day);
    recovered = true;
    std::ofstream("mmvr-town-audio.log", std::ios::app) << "recover scene=" << scene << " frame=" << lastFrame
                                                        << " day=" << day + 1 << " spec=" << int(gAudioSpecId) << "\n";
}
#endif
