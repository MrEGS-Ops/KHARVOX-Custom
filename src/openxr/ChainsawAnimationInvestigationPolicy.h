#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace kharvox {
// EXPERIMENTAL READ-ONLY EVIDENCE TRIAGE. No guessed native offsets, function
// trampolines, animation seeks, memory scans, pointers or game-state writes.
// Every candidate is an EXISTING, already-published engine/renderer observation.
// A candidate changing with the kill is NOT proof it is the animation playhead.
enum class ChainsawEvidenceChannel : unsigned {
    RenderTime, WeaponPresentedOrigin, NativePropOrigin, AnimatedGripPivot,
    CinematicSource, Count
};
inline const char* chainsawEvidenceName(unsigned c) noexcept {
    switch(static_cast<ChainsawEvidenceChannel>(c)){
    case ChainsawEvidenceChannel::RenderTime:return "render-time-unknown-units";
    case ChainsawEvidenceChannel::WeaponPresentedOrigin:return "weapon-presented-transform";
    case ChainsawEvidenceChannel::NativePropOrigin:return "native-prop-transform";
    case ChainsawEvidenceChannel::AnimatedGripPivot:return "animated-grip-joint";
    case ChainsawEvidenceChannel::CinematicSource:return "cinematic-camera-source";
    default:return "unknown";
    }
}
constexpr unsigned chainsawEvidenceChannelCount=5;
constexpr unsigned chainsawInvestigationMaxKills=12;
constexpr unsigned chainsawInvestigationMaxSamplesPerKill=1200; // ~30sec at 25ms
constexpr std::uint64_t chainsawInvestigationSamplePeriodMs=25;
struct ChainsawAnimationEvidenceInput {
    std::uint64_t nowMs{};
    bool nativeExecution{},nativeSync{},trigger{},tracked{};
    bool chordLocked{},chordFinished{},doubleTrigger{};
    bool renderValid{},propValid{},gripValid{},cameraValid{};
    int requestedHz{},renderTime{},renderFrame{};
    float handSpeed{},chordProgress{},ovalRadius{},triggerPressure{};
    std::array<float,3> renderOrigin{},propOrigin{},gripOrigin{},cameraOrigin{};
};
struct ChainsawEvidenceStats {
    unsigned pairs{},changed{},changedHandStill{},changedHandMoving{};
    unsigned changedSlowClock{},changedFastClock{},backward{},largeJumps{};
    double totalDelta{},slowClockDelta{},fastClockDelta{};
    std::uint64_t slowClockMs{},fastClockMs{};
};
struct ChainsawInvestigationResult {
    bool started{},ended{},sample{},chordStart{},chordEnd{},doublePressed{};
    bool triggerReleased{},triggerPressed{},clockSwitched{};
    bool capped{},baselineSample{};
};
struct ChainsawAnimationInvestigationState {
    bool active{},lastChordLocked{},lastChordEnd{},lastDouble{},lastTrigger{};
    unsigned killIndex{},sampleCount{},lastSampleCount{},baselineSamples{};
    std::uint64_t startedMs{},lastSampleMs{},lastBaselineMs{},previousMs{};
    int lastHz{},lastRenderFrame{};
    std::array<bool,chainsawEvidenceChannelCount> priorValid{};
    std::array<std::array<float,3>,chainsawEvidenceChannelCount> prior{};
    std::array<ChainsawEvidenceStats,chainsawEvidenceChannelCount> stats{},lastStats{};
    std::array<ChainsawEvidenceStats,chainsawEvidenceChannelCount> baselineStats{};
    std::array<bool,chainsawEvidenceChannelCount> baselineValid{};
    std::array<std::array<float,3>,chainsawEvidenceChannelCount> baselinePrior{};
    std::uint64_t lastKillDurationMs{};
    void resetKill() noexcept {
        active=false;lastChordLocked=false;lastChordEnd=false;
        lastDouble=false;lastTrigger=false;sampleCount=0;
        startedMs=0;lastSampleMs=0;previousMs=0;lastHz=0;
        priorValid={};prior={};stats={};
    }
};
inline bool chainsawEvidenceValid(const ChainsawAnimationEvidenceInput& in,
                                  unsigned channel) noexcept {
    switch(static_cast<ChainsawEvidenceChannel>(channel)){
    case ChainsawEvidenceChannel::RenderTime:return in.renderValid;
    case ChainsawEvidenceChannel::WeaponPresentedOrigin:return in.renderValid;
    case ChainsawEvidenceChannel::NativePropOrigin:return in.propValid;
    case ChainsawEvidenceChannel::AnimatedGripPivot:return in.gripValid;
    case ChainsawEvidenceChannel::CinematicSource:return in.cameraValid;
    default:return false;
    }
}
inline std::array<float,3> chainsawEvidenceValue(
    const ChainsawAnimationEvidenceInput& in,unsigned channel) noexcept {
    switch(static_cast<ChainsawEvidenceChannel>(channel)){
    case ChainsawEvidenceChannel::RenderTime:
        return {static_cast<float>(in.renderTime),0.f,0.f};
    case ChainsawEvidenceChannel::WeaponPresentedOrigin:return in.renderOrigin;
    case ChainsawEvidenceChannel::NativePropOrigin:return in.propOrigin;
    case ChainsawEvidenceChannel::AnimatedGripPivot:return in.gripOrigin;
    case ChainsawEvidenceChannel::CinematicSource:return in.cameraOrigin;
    default:return {};
    }
}
inline void chainsawEvidenceObserve(
    ChainsawEvidenceStats& stats,const std::array<float,3>& before,
    const std::array<float,3>& now,unsigned channel,
    bool still,int requestedHz,std::uint64_t deltaMs) noexcept {
    float d=0.f;
    if(channel==0) d=now[0]-before[0];
    else {
        const float x=now[0]-before[0],y=now[1]-before[1],z=now[2]-before[2];
        d=std::sqrt(x*x+y*y+z*z);
    }
    if(!std::isfinite(d))return;
    ++stats.pairs;
    const bool changed=channel==0 ? std::fabs(d)>=1.f : std::fabs(d)>=.002f;
    if(changed){
        ++stats.changed;
        if(still)++stats.changedHandStill;else ++stats.changedHandMoving;
        if(requestedHz>=250)++stats.changedSlowClock;
        if(requestedHz>0&&requestedHz<=45)++stats.changedFastClock;
    }
    if(channel==0 && d<0)++stats.backward;
    if(std::fabs(d)>(channel==0?20000.f:100.f))++stats.largeJumps;
    stats.totalDelta+=std::fabs(d);
    if(requestedHz>=250){stats.slowClockDelta+=std::fabs(d);stats.slowClockMs+=deltaMs;}
    if(requestedHz>0&&requestedHz<=45){stats.fastClockDelta+=std::fabs(d);stats.fastClockMs+=deltaMs;}
}
// These scores rank observed *proxy quality* and explicitly do NOT verify
// a native animation timeline. Source freshness and still-hand observations
// are more meaningful than simple correlation to controller travel.
inline int chainsawEvidenceProxyScore(const ChainsawEvidenceStats& s) noexcept {
    if(s.pairs<3)return 0;
    int score=static_cast<int>(std::min(25u,s.pairs/4))
        +static_cast<int>(std::min(20u,s.changed))
        +static_cast<int>(std::min(25u,s.changedHandStill*3))
        +static_cast<int>(std::min(15u,s.changedSlowClock*2))
        +static_cast<int>(std::min(15u,s.changedFastClock*2));
    score-=static_cast<int>(std::min(30u,s.backward*5+s.largeJumps*10));
    return std::clamp(score,0,100);
}
inline ChainsawInvestigationResult updateChainsawAnimationInvestigation(
    ChainsawAnimationInvestigationState& st,
    const ChainsawAnimationEvidenceInput& in) noexcept {
    ChainsawInvestigationResult out{};
    if(!in.nativeExecution){
        if(st.active){
            out.ended=true;st.lastSampleCount=st.sampleCount;
            st.lastStats=st.stats;
            st.lastKillDurationMs=in.nowMs>=st.startedMs
                ?in.nowMs-st.startedMs:0;
            st.resetKill();
        }
        // Bounded comparison against chainsaw-equipped non-executing state.
        if(in.nowMs>=st.lastBaselineMs+200 && st.baselineSamples<120){
            st.lastBaselineMs=in.nowMs;++st.baselineSamples;
            out.baselineSample=true;
            for(unsigned c=0;c<chainsawEvidenceChannelCount;++c){
                const bool valid=chainsawEvidenceValid(in,c);
                const auto value=chainsawEvidenceValue(in,c);
                if(valid&&st.baselineValid[c])
                    chainsawEvidenceObserve(st.baselineStats[c],st.baselinePrior[c],
                        value,c,true,0,200);
                st.baselineValid[c]=valid;st.baselinePrior[c]=value;
            }
        }
        return out;
    }
    if(st.killIndex>=chainsawInvestigationMaxKills&&!st.active){
        out.capped=true;return out;
    }
    if(!st.active){
        st.resetKill();st.active=true;++st.killIndex;st.startedMs=in.nowMs;
        st.lastSampleMs=0;st.previousMs=in.nowMs;
        st.lastTrigger=in.trigger;st.lastHz=in.requestedHz;
        out.started=true;
    }else{
        out.chordStart=in.chordLocked&&!st.lastChordLocked;
        out.chordEnd=in.chordFinished&&!st.lastChordEnd;
        out.doublePressed=in.doubleTrigger&&!st.lastDouble;
        out.triggerReleased=!in.trigger&&st.lastTrigger;
        out.triggerPressed=in.trigger&&!st.lastTrigger;
        out.clockSwitched=(in.requestedHz==30&&st.lastHz!=30)
            ||(in.requestedHz>=250&&st.lastHz<250&&st.lastHz>0);
    }
    const bool event=out.started||out.chordStart||out.chordEnd||
        out.doublePressed||out.triggerReleased||out.triggerPressed||out.clockSwitched;
    if(st.sampleCount<chainsawInvestigationMaxSamplesPerKill
       &&(event||in.nowMs>=st.lastSampleMs+chainsawInvestigationSamplePeriodMs)){
        out.sample=true;++st.sampleCount;
        const bool still=in.tracked&&std::isfinite(in.handSpeed)&&in.handSpeed<.08f;
        const auto deltaMs=in.nowMs>=st.previousMs?in.nowMs-st.previousMs:0;
        for(unsigned c=0;c<chainsawEvidenceChannelCount;++c){
            const bool valid=chainsawEvidenceValid(in,c);
            const auto value=chainsawEvidenceValue(in,c);
            if(valid&&st.priorValid[c]&&deltaMs)
                chainsawEvidenceObserve(st.stats[c],st.prior[c],value,
                    c,still,in.requestedHz,deltaMs);
            st.priorValid[c]=valid;st.prior[c]=value;
        }
        st.lastSampleMs=in.nowMs;st.previousMs=in.nowMs;
    }
    st.lastChordLocked=in.chordLocked;st.lastChordEnd=in.chordFinished;
    st.lastDouble=in.doubleTrigger;st.lastTrigger=in.trigger;
    st.lastHz=in.requestedHz;st.lastRenderFrame=in.renderFrame;
    return out;
}
} // namespace kharvox
