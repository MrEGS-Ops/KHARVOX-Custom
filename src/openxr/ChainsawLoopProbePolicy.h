#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kharvox {
// OBSERVATION ONLY. No animation pointer, playhead, rewind, clock or damage
// writes. Records what is already published by the validated chainsaw path.
struct ChainsawLoopProbeInput {
    bool chainsawExecution{},trigger{},tracked{},armed{},oneSwipeCommitted{};
    bool cameraSourceValid{};
    float handMetersPerSecond{};
    std::uint64_t nowMs{};
};
struct ChainsawLoopProbeDecision {
    bool started{},ended{},sample{},paused{},resumed{},committed{};
};
struct ChainsawLoopProbeState {
    bool active{},wasPaused{},wasCommitted{};
    bool sawPause{},sawResume{},sawCommitted{},sawCameraSource{};
    unsigned killIndex{},sampleCount{},pauseCount{},resumeCount{},trackingLossCount{};
    std::uint64_t killStartMs{},lastSampleMs{},stationarySinceMs{},lastDurationMs{};
    unsigned lastSampleCount{},lastPauseCount{},lastResumeCount{},lastTrackingLossCount{};
    bool lastSawCameraSource{},lastSawCommitted{};
    std::uint64_t previousMs{};
    void resetKill() noexcept {
        active=false;wasPaused=false;wasCommitted=false;
        sawPause=false;sawResume=false;sawCommitted=false;sawCameraSource=false;
        sampleCount=0;pauseCount=0;resumeCount=0;trackingLossCount=0;
        stationarySinceMs=0;killStartMs=0;lastSampleMs=0;previousMs=0;
    }
};
inline ChainsawLoopProbeDecision updateChainsawLoopProbe(
    ChainsawLoopProbeState& st,const ChainsawLoopProbeInput& in) noexcept {
    ChainsawLoopProbeDecision result{};
    if(!in.chainsawExecution){
        if(st.active){
            st.lastDurationMs=in.nowMs>=st.killStartMs?in.nowMs-st.killStartMs:0;
            st.lastSampleCount=st.sampleCount;
            st.lastPauseCount=st.pauseCount;st.lastResumeCount=st.resumeCount;
            st.lastTrackingLossCount=st.trackingLossCount;
            st.lastSawCameraSource=st.sawCameraSource;
            st.lastSawCommitted=st.sawCommitted;
            result.ended=true;
            st.resetKill();
        }
        return result;
    }
    if(!st.active){
        st.resetKill();st.active=true;++st.killIndex;st.killStartMs=in.nowMs;
        st.lastSampleMs=in.nowMs;
        result.started=true;
    }
    if(!in.tracked&&in.nowMs!=st.previousMs)
        ++st.trackingLossCount;
    st.previousMs=in.nowMs;
    st.sawCameraSource|=in.cameraSourceValid;
    if(in.oneSwipeCommitted&&!st.wasCommitted){
        st.sawCommitted=true;
        result.committed=true;
    }
    st.wasCommitted=in.oneSwipeCommitted;
    // A stationary controller is not a paused animation. It is a
    // repeatable INPUT window to correlate against engine/camera samples.
    const bool stationary=in.armed&&in.tracked&&in.trigger
        &&std::isfinite(in.handMetersPerSecond)
        &&in.handMetersPerSecond<.08f;
    if(stationary){
        if(!st.stationarySinceMs)st.stationarySinceMs=in.nowMs;
        if(!st.wasPaused&&in.nowMs>=st.stationarySinceMs+160){
            st.wasPaused=true;st.sawPause=true;++st.pauseCount;
            result.paused=true;
        }
    } else {
        st.stationarySinceMs=0;
        if(st.wasPaused){
            st.wasPaused=false;st.sawResume=true;++st.resumeCount;
            result.resumed=true;
        }
    }
    if((result.started||in.nowMs>=st.lastSampleMs+50)
       &&st.sampleCount<350){
        result.sample=true;++st.sampleCount;st.lastSampleMs=in.nowMs;
    }
    return result;
}
} // namespace kharvox
