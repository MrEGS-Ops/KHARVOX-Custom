#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace kharvox {
// Single tall elliptical capture region, aligned with the real OpenXR quad.
// A tracked hand may be physically closer than the quad yet appear INSIDE it.
// Test the ray from the HMD through the hand against the quad plane; a 3D
// sphere centred at the quad incorrectly rejected nearly every real hand.
// While holding Trigger, leave the projected oval, then return INTO it.
// Once latched, tracked hand motion controls speed ANYWHERE, capped at 60 Hz.
// Trigger release holds 300 Hz; the second press latches 30 Hz until native exit.
// A re-press in the SAME kill does not require another oval entry gesture.
// 3 is the existing independent Pointer Focus experiment.
enum class ChainsawExperimentalMode:int {GripPull=1,PointerFocus=3};
inline const char* chainsawModeName(ChainsawExperimentalMode m) noexcept {
    return m==ChainsawExperimentalMode::PointerFocus?"POINTER_FOCUS":"SINGLE_PULL_ZONE";
}
struct ChainsawVec3 {float x{},y{},z{};};
inline ChainsawVec3 minus3(ChainsawVec3 a,ChainsawVec3 b) noexcept {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
inline float dot3(ChainsawVec3 a,ChainsawVec3 b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
inline float length3(ChainsawVec3 a) noexcept {
    const float d=dot3(a,a);return d>=0.f&&std::isfinite(d)?std::sqrt(d):0.f;
}
inline ChainsawVec3 zonePosition(ChainsawVec3 head,float yaw,int /*legacyZone*/) noexcept {
    // Fallback where forward XR orientation is absent; NOT a target lock.
    const float s=std::sin(yaw),c=std::cos(yaw);
    return {head.x-s*1.10f,head.y-.17f,head.z-c*1.10f};
}
inline ChainsawVec3 singleZoneCenter(ChainsawVec3 head,ChainsawVec3 gaze,
                                    float yaw=0.f) noexcept {
    // Aim-aligned, ~1.10m forward, ~17cm below viewing direction to visually
    // frame an enemy's upper torso. Frozen at native execution entry. We do
    // NOT pretend to know the true demon position without an enemy-target hook.
    const float mag=length3(gaze);
    if(mag<.8f||mag>1.2f)return zonePosition(head,yaw,0);
    return {head.x+1.10f*gaze.x/mag,
            head.y+1.10f*gaze.y/mag-.17f,
            head.z+1.10f*gaze.z/mag};
}
// The procedural outline is centred at texture-space radius .69.
// Match the quad's REAL ellipse, rather than an unreachable 3D sphere.
constexpr float chainsawZoneQuadWidthMeters=1.30f;
constexpr float chainsawZoneQuadHeightMeters=1.72f;
constexpr float chainsawZoneEnterRadius=.69f; // normalized ellipse radius
constexpr float chainsawZoneExitRadius=.81f;  // hysteresis on leaving
// Continuous guide on the weapon hand while a native chainsaw kill runs.
// Uses the SAME perspective-correct quad projection as the rendered oval.
// Zero at centre, increases smoothly to 86% at the EXIT radius and remains
// at that maximum outside the circle. Invalid tracking means NO misleading cue.
constexpr float chainsawZoneGuideMaxAmplitude=.86f;
constexpr float chainsawZoneGuideFrequencyHz=125.f;
inline float chainsawZoneGuidanceAmplitude(float projectedRadius) noexcept {
    if(!std::isfinite(projectedRadius)||projectedRadius<0.f)return 0.f;
    const float fraction=std::clamp(projectedRadius/chainsawZoneExitRadius,0.f,1.f);
    return chainsawZoneGuideMaxAmplitude*std::pow(fraction,1.5f);
}
// A deliberate 25 cm tracked stroke starts AFTER oval entry, never on entry.
// Reaching it is diagnostic, not a forced fast-finish; live hand speed rules.
constexpr float chainsawSingleSwipeDistanceMeters=.25f;
constexpr int chainsawPhysicalMotionFastHz=45;
constexpr int chainsawDoubleTriggerFastFinishHz=30;
constexpr int chainsawTriggerReleaseIdleHz=300;
struct ChainsawZoneProjection {
    bool valid{};
    float normalizedRadius{-1.f};
    float handDepthMeters{};
};
inline ChainsawZoneProjection projectChainsawHandToOval(
    ChainsawVec3 head,ChainsawVec3 hand,ChainsawVec3 center,
    ChainsawVec3 forward,ChainsawVec3 right,ChainsawVec3 up) noexcept {
    const float forwardLen=length3(forward);
    if(forwardLen<.8f||forwardLen>1.2f||
       length3(right)<.8f||length3(up)<.8f)return {};
    const ChainsawVec3 n{forward.x/forwardLen,forward.y/forwardLen,
                         forward.z/forwardLen};
    const auto delta=minus3(hand,head);
    const float handDepth=dot3(delta,n);
    const float planeDepth=dot3(minus3(center,head),n);
    // A controller behind/against the player's face is not a cut gesture.
    if(!std::isfinite(handDepth)||handDepth<.15f||handDepth>2.f||
       !std::isfinite(planeDepth)||planeDepth<.20f)return {};
    const float t=planeDepth/handDepth;
    const ChainsawVec3 projected{head.x+delta.x*t,
                                 head.y+delta.y*t,head.z+delta.z*t};
    const auto fromCenter=minus3(projected,center);
    const float nx=2.f*dot3(fromCenter,right)/chainsawZoneQuadWidthMeters;
    const float ny=2.f*dot3(fromCenter,up)/chainsawZoneQuadHeightMeters;
    const float radius=std::sqrt(nx*nx+ny*ny);
    if(!std::isfinite(radius))return {};
    return {true,radius,handDepth};
}

struct ChainsawExperimentState {
    bool active{},releasedGrip{},latchedGrip{},finished{},pointerStarted{},wasGripHeld{};
    bool seenOutside{},inside{},lastTracked{},oneSwipeCommitted{};
    bool resumeEligible{}; // only after a successfully armed kill is interrupted
    bool fastFinishLatched{}; // second Trigger press: 30 Hz until native kill end
    unsigned triggerRestarts{}; // same native execution, never a new kill
    float lastZoneRadius{-1.f},lastHandDepth{};
    int hovered{-1},lastHover{-1},startZone{-1},targetZone{-1};
    std::array<ChainsawVec3,4> zones{}; // only zones[0] rendered/used
    ChainsawExperimentalMode mode{ChainsawExperimentalMode::GripPull};
    ChainsawVec3 previous{},rayStart{};
    float progress{},peak{},filteredSpeed{},strokeTravelMeters{};
    unsigned impactIndex{};
    std::uint64_t lastUpdateMs{},entryMs{};
    void reset() noexcept {*this={};}
};
struct ChainsawExperimentResult {
    int targetHz{300};
    float progress{};
    bool armed{},completed{},started{},impact{};
    const char* detail{"WAIT"};
    int hoveredZone{-1},startZone{-1},targetZone{-1};
    bool enteredZone{};
};

// Only an ACTUAL DOOM chainsaw execution owns this state. The control never
// starts a kill, steals Trigger input, invents collision, or edits other actions.
// The initial Trigger press normally starts the kill before the first execution
// frame; therefore we must NOT require a second/released Trigger press.
// 'progress' here is LIVE hand speed for the current frame, not total animation
// progress. Normal DOOM decides when the execution finishes.
inline ChainsawExperimentResult updateChainsawExperiment(
    ChainsawExperimentState& st,ChainsawExperimentalMode mode,
    bool nativeExecution,bool triggerHeld,bool handTracked,
    float x,float y,float z,bool rayValid,float rayX,float rayY,float rayZ,
    std::uint64_t nowMs,int idleHz=300,int strokeDistanceCm=32,
    ChainsawVec3 worldHead={},float worldYaw=0.f,bool headTracked=true,
    ChainsawVec3 headForward={},ChainsawVec3 headRight={1.f,0.f,0.f},
    ChainsawVec3 headUp={0.f,1.f,0.f}) noexcept {
    idleHz=std::clamp(idleHz,30,480);
    (void)strokeDistanceCm; // Retained for older saved calibration schemas.
    if(!nativeExecution){st.reset();return {idleHz};}
    if(mode!=ChainsawExperimentalMode::PointerFocus)
        mode=ChainsawExperimentalMode::GripPull;
    const bool valid=handTracked&&std::isfinite(x)&&std::isfinite(y)
        &&std::isfinite(z)&&std::abs(x)<20.f&&std::abs(y)<20.f&&std::abs(z)<20.f;
    const ChainsawVec3 hand{x,y,z};
    const bool validHead=headTracked&&std::isfinite(worldYaw)
        &&std::isfinite(worldHead.x)&&std::isfinite(worldHead.y)&&std::isfinite(worldHead.z);
    if(!st.active){
        if(!validHead)return {idleHz};
        st.active=true;st.mode=mode;st.lastUpdateMs=nowMs;
        st.zones[0]=singleZoneCenter(worldHead,headForward,worldYaw);
        st.previous=hand;st.lastTracked=valid;
        st.seenOutside=false; // Must observe the hand OUTSIDE with Trigger held.
    }
    if(st.mode!=mode){st.reset();return {idleHz};}
    const bool timely=nowMs>st.lastUpdateMs
        &&nowMs-st.lastUpdateMs<=175;
    const auto elapsed=nowMs>=st.lastUpdateMs
        ?nowMs-st.lastUpdateMs:0;
    st.lastUpdateMs=nowMs;
    bool started=false,entered=false;
    if(mode==ChainsawExperimentalMode::GripPull){
        const auto projection=valid&&validHead
            ?projectChainsawHandToOval(worldHead,hand,st.zones[0],
                headForward,headRight,headUp)
            :ChainsawZoneProjection{};
        st.lastZoneRadius=projection.normalizedRadius;
        st.lastHandDepth=projection.handDepthMeters;
        // Release AFTER arming holds an explicit 300 Hz idle (near-freeze).
        // Do not undo fast-finish once its second-press latch is set.
        if(!triggerHeld&&!st.fastFinishLatched){
            if(st.latchedGrip)st.resumeEligible=true;
            st.latchedGrip=false;
            st.seenOutside=false;
            st.filteredSpeed=0.f;
            st.progress=0.f;
            st.oneSwipeCommitted=false;
            st.strokeTravelMeters=0.f;
        }
        if(projection.valid&&projection.normalizedRadius>=chainsawZoneExitRadius){
            if(triggerHeld)st.seenOutside=true;
            st.inside=false;
        }else if(projection.valid
                 &&projection.normalizedRadius<=chainsawZoneEnterRadius){
            st.inside=true;
        }else if(!projection.valid){
            st.inside=false;
        }
        st.hovered=projection.valid&&st.inside?0:-1;
        entered=st.hovered==0&&st.lastHover!=0;
        st.lastHover=st.hovered;
        // The initial cut requires entering the oval with Trigger held.
        // ONLY after that cut has armed can release -> second press latch
        // 30 Hz for the rest of the SAME native execution, even outside the oval.
        const bool canInitialArm=st.seenOutside&&st.inside&&projection.valid;
        if(!st.latchedGrip&&triggerHeld
           &&(st.resumeEligible||(valid&&canInitialArm))){
            const bool restarting=st.resumeEligible;
            st.resumeEligible=false;
            st.latchedGrip=true;st.startZone=0;st.entryMs=nowMs;
            st.filteredSpeed=0.f;
            st.progress=0.f;
            st.strokeTravelMeters=0.f;
            st.oneSwipeCommitted=false;
            if(restarting){
                ++st.triggerRestarts;
                st.fastFinishLatched=true;
            }
            // Never count the oval crossing or motion while unheld toward
            // the deliberate 25 cm CUT. Never carry stale velocity forward.
            if(valid)st.previous=hand;
            st.lastTracked=false;
            started=true;
        }
        // The oval is the INITIAL activation gate only. Hand speed drives
        // the engine tick ANYWHERE, including beyond the oval. No entry boost.
        if(st.latchedGrip){
            if(st.fastFinishLatched){
                // Second Trigger press: deliberately finish the ENTIRE
                // remaining native execution at 30 Hz. No animation rewind.
                st.filteredSpeed=0.f;st.progress=1.f;
            }else if(!triggerHeld||!valid){
                st.filteredSpeed=0.f;st.progress=0.f;
            }else{
                const float delta=st.lastTracked&&timely
                    ?length3(minus3(hand,st.previous)):0.f;
                // Ignore tracking jitter and discontinuous teleports.
                // Oval ENTRY was excluded when first arming the stroke.
                const bool genuineStroke=timely&&elapsed>0
                    &&delta>=.008f&&delta<.28f;
                if(!st.oneSwipeCommitted&&genuineStroke){
                    st.strokeTravelMeters+=delta;
                    st.oneSwipeCommitted=
                        st.strokeTravelMeters>=chainsawSingleSwipeDistanceMeters;
                }
                const float instant=timely&&delta<.28f&&elapsed>0
                    ?std::clamp(delta*1000.f/float(elapsed),0.f,1.5f):0.f;
                st.filteredSpeed=std::clamp(
                    .44f*instant+.56f*st.filteredSpeed,0.f,1.5f);
                // Reaching 25 cm does not force full speed. Stopping the hand
                // must STILL slow the cut; second Trigger press is fast finish.
                st.progress=std::clamp(st.filteredSpeed/.55f,0.f,1.f);
            }
        }
        if(valid)st.previous=hand;
        st.lastTracked=valid;
    }else{
        const ChainsawVec3 ray{rayX,rayY,rayZ};
        const bool rayOk=rayValid&&std::isfinite(rayX)&&std::isfinite(rayY)
            &&std::isfinite(rayZ)&&length3(ray)>.8f&&length3(ray)<1.2f;
        if(!st.pointerStarted&&rayOk){
            st.pointerStarted=true;st.rayStart=ray;started=true;
        }else if(rayOk){
            st.progress=std::max(st.progress,std::clamp(
                (std::acos(std::clamp(dot3(st.rayStart,ray),-1.f,1.f))
                 -.03491f)/.45379f,0.f,1.f));
        }
    }
    st.finished=st.progress>=.999f;
    const int physicalFloorHz=std::min(idleHz,chainsawPhysicalMotionFastHz);
    int hz=std::clamp(static_cast<int>(std::lround(
        idleHz-(idleHz-(mode==ChainsawExperimentalMode::GripPull
            ?physicalFloorHz:30))*st.progress)),
        mode==ChainsawExperimentalMode::GripPull?physicalFloorHz:30,idleHz);
    if(mode==ChainsawExperimentalMode::GripPull){
        if(st.fastFinishLatched)hz=chainsawDoubleTriggerFastFinishHz;
        else if(st.resumeEligible&&!triggerHeld)hz=chainsawTriggerReleaseIdleHz;
    }
    const unsigned quarters=std::min(4u,static_cast<unsigned>(st.progress*4.f));
    const bool impact=quarters>st.impactIndex;
    st.impactIndex=std::max(st.impactIndex,quarters);
    // Timing begins ONLY after a deliberate first oval entry. Afterward,
    // Trigger release owns 300 Hz, the second press latches 30 Hz, and
    // native execution exit ALWAYS drops the override. Tracking loss before
    // fast-finish relinquishes active motion speed, as before.
    const bool owningTiming=mode==ChainsawExperimentalMode::PointerFocus
        ?st.pointerStarted
        :(st.fastFinishLatched||(st.resumeEligible&&!triggerHeld)
          ||(st.latchedGrip&&triggerHeld&&valid));
    return {hz,st.progress,owningTiming,
        st.finished,started,impact,
        st.fastFinishLatched?"DOUBLE_TRIGGER_FAST_FINISH":chainsawModeName(mode),
        st.hovered,st.startZone,st.targetZone,entered};
}
} // namespace kharvox
