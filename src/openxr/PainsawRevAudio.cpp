#include "PainsawRevAudio.h"
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace kharvox {
namespace {
constexpr unsigned kRate=48000;
constexpr float kPi=3.14159265358979323846f;
struct AudioEngine {
    std::mutex gate;
    IXAudio2* mixer{};
    IXAudio2MasteringVoice* master{};
    IXAudio2SourceVoice* source{};
    std::array<std::vector<std::uint8_t>,3> samples{};
    bool attempted{},ready{};

    // Original procedurally synthesized two-stroke engine/buzz. Loud but
    // peak-limited in PCM to avoid clipping and sudden unsafe audio spikes.
    // All samples are produced ONCE per process; never on a render frame.
    static std::vector<std::uint8_t> Build(unsigned stage) {
        const float duration=stage==3?0.90f:stage==2?0.54f:0.49f;
        const auto frames=static_cast<unsigned>(kRate*duration);
        std::vector<std::uint8_t> bytes(frames*sizeof(std::int16_t));
        float phase=0.f,engineFlutter=0.f;
        std::uint32_t rng=0x72E51u+stage*14713u;
        for(unsigned i=0;i<frames;++i){
            const float time=static_cast<float>(i)/kRate;
            const float t=time/duration;
            const float acceleration=std::clamp(t/(stage==3?0.35f:0.34f),0.f,1.f);
            const float deceleration=stage==3?1.f:
                (t<0.65f?1.f:std::max(0.f,1.f-(t-0.65f)/0.35f));
            const float rpm=74.f+(stage==3?145.f:stage==2?119.f:100.f)
                *acceleration*deceleration;
            phase+=2.f*kPi*rpm/kRate;
            if(phase>2.f*kPi)phase-=2.f*kPi;
            rng=rng*1664525u+1013904223u;
            const float noise=(static_cast<float>((rng>>8)&65535u)/32767.5f)-1.f;
            const float flutter=0.85f+0.15f*std::sin(phase*0.25f);
            engineFlutter=engineFlutter*0.84f+noise*0.16f;
            const float buzz=0.57f*std::sin(phase)
                +0.25f*std::sin(2.f*phase+0.29f)
                +0.18f*std::sin(3.f*phase+0.63f)
                +0.15f*std::sin(5.f*phase+0.17f)
                +0.18f*engineFlutter
                +0.10f*noise;
            const float fadeIn=std::clamp(time/0.014f,0.f,1.f);
            const float fadeOut=std::clamp((duration-time)/0.085f,0.f,1.f);
            const float envelope=fadeIn*fadeOut*flutter;
            // Keep audio headroom. User's Windows/headset volume remains
            // authoritative; never modify system volume or other DOOM audio.
            const float sample=std::tanh(buzz*2.2f)*0.88f*envelope;
            const auto pcm=static_cast<std::int16_t>(std::lround(
                std::clamp(sample,-0.95f,0.95f)*32767.f));
            bytes[i*2]=static_cast<std::uint8_t>(pcm&0xff);
            bytes[i*2+1]=static_cast<std::uint8_t>((static_cast<unsigned short>(pcm)>>8)&0xff);
        }
        return bytes;
    }

    void Initialize() noexcept {
        if(attempted)return;
        attempted=true;
        if(FAILED(XAudio2Create(&mixer,0,XAUDIO2_DEFAULT_PROCESSOR)))return;
        if(FAILED(mixer->CreateMasteringVoice(&master)))return;
        WAVEFORMATEX format{};
        format.wFormatTag=WAVE_FORMAT_PCM;
        format.nChannels=1;
        format.nSamplesPerSec=kRate;
        format.nAvgBytesPerSec=kRate*sizeof(std::int16_t);
        format.nBlockAlign=sizeof(std::int16_t);
        format.wBitsPerSample=16;
        if(FAILED(mixer->CreateSourceVoice(&source,&format)))return;
        for(unsigned stage=1;stage<=3;++stage)
            samples[stage-1]=Build(stage);
        ready=true;
    }

    bool Play(unsigned stage) noexcept {
        std::lock_guard<std::mutex> guard(gate);
        Initialize();
        if(!ready)return false;
        stage=std::clamp(stage,1u,3u);
        source->Stop(0);
        source->FlushSourceBuffers(); // new pull replaces last rev; no clipping pile-up
        const auto& pcm=samples[stage-1];
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes=static_cast<UINT32>(pcm.size());
        buffer.pAudioData=pcm.data();
        buffer.Flags=XAUDIO2_END_OF_STREAM;
        if(FAILED(source->SubmitSourceBuffer(&buffer)))return false;
        if(FAILED(source->Start(0)))return false;
        return true;
    }

    void Stop() noexcept {
        std::lock_guard<std::mutex> guard(gate);
        if(!ready)return;
        source->Stop(0);
        source->FlushSourceBuffers();
    }
};
// Deliberately not destroyed inside a DLL loader-lock detach path. The OS
// releases this game-process-owned XAudio2 engine when DOOM exits.
AudioEngine& engine() {
    static AudioEngine* instance=new AudioEngine();
    return *instance;
}
} // namespace
bool PainsawRevAudio::Play(unsigned stage) noexcept {
    return engine().Play(stage);
}
void PainsawRevAudio::Stop() noexcept {
    engine().Stop();
}
} // namespace kharvox
