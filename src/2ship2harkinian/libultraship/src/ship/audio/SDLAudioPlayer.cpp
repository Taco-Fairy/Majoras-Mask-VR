#include "ship/audio/SDLAudioPlayer.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace Ship {

SDLAudioPlayer::~SDLAudioPlayer() {
    SPDLOG_TRACE("destruct SDL audio player");
    DoClose();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDLAudioPlayer::DoClose() {
    if (mDevice != 0) {
        // Pause playback first
        SDL_PauseAudioDevice(mDevice, 1);
        // Clear any queued audio to prevent glitches when reopening
        SDL_ClearQueuedAudio(mDevice);
        SDL_CloseAudioDevice(mDevice);
        mDevice = 0;
    }
}

bool SDLAudioPlayer::DoInit() {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        SPDLOG_ERROR("SDL init error: {}", SDL_GetError());
        return false;
    }

    // Always open with the correct number of output channels
    mNumChannels = this->GetNumOutputChannels();

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = this->GetSampleRate();
    want.format = AUDIO_S16SYS;
    want.channels = mNumChannels;
    want.samples = this->GetSampleLength();
    want.callback = NULL;

    mDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (mDevice == 0) {
        SPDLOG_ERROR("SDL_OpenAudio error: {}", SDL_GetError());
        return false;
    }

    SPDLOG_INFO("SDL Audio initialized: {} channels, {} Hz", mNumChannels, this->GetSampleRate());

    mQueuedAudio=false;mQueueStarves=mMaxQueueGap=0;mLastQueueTick=mAudioReportTick=SDL_GetTicks();
    SDL_PauseAudioDevice(mDevice, 0);
    return true;
}

int SDLAudioPlayer::Buffered() {
    return SDL_GetQueuedAudioSize(mDevice) / (sizeof(int16_t) * mNumChannels);
}

void SDLAudioPlayer::DoPlay(const uint8_t* buf, size_t len) {
    const int queued=Buffered();
#ifdef __ANDROID__
    const uint32_t now=SDL_GetTicks();
    if(mQueuedAudio){if(queued==0)++mQueueStarves;mMaxQueueGap=std::max(mMaxQueueGap,now-mLastQueueTick);}
    mLastQueueTick=now;mQueuedAudio=true;
    if(now-mAudioReportTick>=5000){
        SPDLOG_INFO("MMVR audio: queuedFrames={} emptyEvents={} maxProducerGapMs={}",queued,mQueueStarves,mMaxQueueGap);
        mAudioReportTick=now;mQueueStarves=mMaxQueueGap=0;
    }
#endif
    if (queued < 6000) {
        // Don't fill the audio buffer too much in case this happens
        SDL_QueueAudio(mDevice, buf, len);
    }
}
} // namespace Ship
