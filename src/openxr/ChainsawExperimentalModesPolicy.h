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
// A cut is one directed chord: enter the oval on one edge with Trigger
// held, traverse the same line toward the OPPOSITE edge, then fast-finish.
// Motion toward the opposite boundary drives native engine tick pacing;
// sideways flailing/backtracking cannot commit a cut.  This does not
// seek the DOOM animator playhead (not verified); DOOM owns native exit.
// Any second Trigger press during the SAME confirmed execution immediately
// latches 30 Hz, even if no oval was ever entered.
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
// Enlarge the physical and drawn oval together by ~15%; capture projection
// and OpenXR quad sizing both use these SAME dimensions.
constexpr float chainsawZoneQuadWidthMeters=1.50f;
constexpr float chainsawZoneQuadHeightMeters=1.98f;
constexpr float chainsawZoneEnterRadius=.69f; // VISIBLE ring/inside threshold
constexpr float chainsawZoneExitRadius=.81f;  // initial outside-arm hysteresis
// Execution-only weapon-hand OUTSIDE cue: unmistakable "bzzz ... bzzz ...".
// Strong 150 Hz vibration for 160ms, 200ms silence; never a steady buzz.
// Pulse phase starts immediately upon moving outside the visible ring.
// Inside, missing projection, and lost tracking remain absolutely silent.
constexpr float chainsawZoneGuidePulseAmplitude=.92f;
constexpr float chainsawZoneGuidePulseFrequencyHz=150.f;
constexpr std::uint64_t chainsawZoneGuidePulsePeriodMs=360;
constexpr std::uint64_t chainsawZoneGuidePulseOnMs=160;
inline bool chainsawZoneOutsidePulseOn(
    float projectedRadius,std::uint64_t millisecondsOutside) noexcept {
    return std::isfinite(projectedRadius)
        && projectedRadius>=chainsawZoneEnterRadius
        && millisecondsOutside%chainsawZoneGuidePulsePeriodMs
            <chainsawZoneGuidePulseOnMs;
}
// One opposite-edge chord replaces the old arbitrary 25cm cumulative travel.
// The strict lateral corridor discourages swinging in circles or sideways.
constexpr float chainsawCutChordLateralTolerance=.26f; // normalized quad
constexpr float chainsawCutFinishOppositeDot=.57f; // far side, not nearby exit
constexpr float chainsawCutRateFullSpeedPerSecond=.65f;
// Kept only for historical trace interpretation, not a completion target.
constexpr float chainsawSingleSwipeDistanceMeters=.25f;
constexpr int chainsawPhysicalMotionFastHz=45;
constexpr int chainsawDoubleTriggerFastFinishHz=30;
constexpr int chainsawTriggerReleaseIdleHz=300;
struct ChainsawZoneProjection {
    bool valid{};
    float normalizedRadius{-1.f};
    float handDepthMeters{};
    float nx{},ny{}; // projected normalized quad position, not just radius
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
    return {true,radius,handDepth,nx,ny};
}

struct ChainsawExperimentState {
    bool active{},releasedGrip{},latchedGrip{},finished{},pointerStarted{},wasGripHeld{};
    bool seenOutside{},inside{},lastTracked{},oneSwipeCommitted{};
    bool resumeEligible{}; // after an armed kill's first trigger release
    bool fastFinishLatched{}; // second Trigger press: 30 Hz until native kill end
    bool cutPathLocked{}; // one straight chord, locked at oval entry
    bool zoneGuideWasOutside{}; // reset pulse phase at actual visible-ring exit
    std::uint64_t zoneGuideOutsideSinceMs{};
    unsigned triggerRestarts{}; // same native execution, never a new kill
    float lastZoneRadius{-1.f},lastHandDepth{};
    int hovered{-1},lastHover{-1},startZone{-1},targetZone{-1};
    std::array<ChainsawVec3,4> zones{}; // only zones[0] rendered/used
    ChainsawExperimentalMode mode{ChainsawExperimentalMode::GripPull};
    ChainsawVec3 previous{},rayStart{};
    float progress{},peak{},filteredSpeed{},strokeTravelMeters{};
    float lastOutsideX{},lastOutsideY{};
    float cutStartX{},cutStartY{},cutDirX{},cutDirY{},cutLength{};
    float cutLastPositionProgress{};
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
// 'progress' is distance along the requested input chord, NEVER native
// animation frame/playhead progress. DOOM decides when the kill finishes.
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
        st.wasGripHeld=triggerHeld; // initial DOOM trigger press preceded native exec
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
        // Count the physical second press independently of oval entry or
        // tracked-hand status. Initial press started DOOM's native execution
        // BEFORE its first sample; subsequent released -> held is fast finish.
        const bool secondPress=triggerHeld&&!st.wasGripHeld;
        st.wasGripHeld=triggerHeld;
        if(secondPress&&!st.fastFinishLatched){
            st.fastFinishLatched=true;
            ++st.triggerRestarts; // diagnostic: fast-finish press count
        }
        const auto projection=valid&&validHead
            ?projectChainsawHandToOval(worldHead,hand,st.zones[0],
                headForward,headRight,headUp)
            :ChainsawZoneProjection{};
        st.lastZoneRadius=projection.normalizedRadius;
        st.lastHandDepth=projection.handDepthMeters;
        const bool guideOutside=projection.valid
            &&projection.normalizedRadius>=chainsawZoneEnterRadius;
        if(guideOutside&&!st.zoneGuideWasOutside)
            st.zoneGuideOutsideSinceMs=nowMs;
        st.zoneGuideWasOutside=guideOutside;
        // Release AFTER arming holds an explicit 300 Hz idle (near-freeze).
        // Do not undo fast-finish once its second-press latch is set.
        if(!triggerHeld&&!st.fastFinishLatched&&!st.oneSwipeCommitted){
            if(st.latchedGrip)st.resumeEligible=true;
            st.latchedGrip=false;
            st.seenOutside=false;
            st.filteredSpeed=0.f;
            st.progress=0.f;
            st.oneSwipeCommitted=false;
            st.strokeTravelMeters=0.f;
            st.cutPathLocked=false;
        }
        if(projection.valid&&projection.normalizedRadius>=chainsawZoneExitRadius){
            if(triggerHeld){
                st.seenOutside=true;
                st.lastOutsideX=projection.nx;
                st.lastOutsideY=projection.ny;
            }
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
        // Initial arm locks a radial chord from the observed OUTSIDE edge
        // toward the OPPOSITE visible-ring edge. No new chord after trigger
        // release: the next press fast-finishes at 30Hz at any position.
        const bool canInitialArm=st.seenOutside&&st.inside&&projection.valid;
        if(!st.latchedGrip&&!st.fastFinishLatched&&triggerHeld&&valid
           &&canInitialArm){
            st.resumeEligible=false;
            st.latchedGrip=true;st.startZone=0;st.entryMs=nowMs;
            st.filteredSpeed=0.f;st.progress=0.f;
            st.strokeTravelMeters=0.f;st.oneSwipeCommitted=false;
            const float outsideLength=std::sqrt(
                st.lastOutsideX*st.lastOutsideX+
                st.lastOutsideY*st.lastOutsideY);
            // A fully valid outside observation is required to lock direction.
            if(outsideLength>=chainsawZoneExitRadius){
                st.cutDirX=-st.lastOutsideX/outsideLength;
                st.cutDirY=-st.lastOutsideY/outsideLength;
                st.cutStartX=projection.nx;st.cutStartY=projection.ny;
                const float entryAlong=-(projection.nx*st.cutDirX+
                                         projection.ny*st.cutDirY);
                st.cutLength=std::max(.40f,chainsawZoneEnterRadius+entryAlong);
                st.cutLastPositionProgress=0.f;
                st.cutPathLocked=true;
                st.previous=hand;st.lastTracked=false; // entry is NOT travel
                started=true;
            }else st.latchedGrip=false;
        }
        // The physical gesture is one chosen chord. Only forward movement
        // along it accelerates the animation clock. Stationary/reverse/
        // lateral motion returns to slow idle; no arbitrary 25cm accumulator
        // can complete the kill, and tracked-controller teleports are rejected.
        if(st.latchedGrip&&st.cutPathLocked
           &&!st.fastFinishLatched&&!st.oneSwipeCommitted){
            const float dx=projection.nx-st.cutStartX;
            const float dy=projection.ny-st.cutStartY;
            const float lateral=std::fabs(-dx*st.cutDirY+dy*st.cutDirX);
            const float along=dx*st.cutDirX+dy*st.cutDirY;
            const bool validCorridor=triggerHeld&&valid&&projection.valid
                &&lateral<=chainsawCutChordLateralTolerance;
            const float path=validCorridor
                ?std::clamp(along/st.cutLength,0.f,1.f)
                :st.cutLastPositionProgress;
            const float realDelta=st.lastTracked&&timely
                ?length3(minus3(hand,st.previous)):0.f;
            const bool genuineStep=st.lastTracked&&timely&&elapsed>0
                &&realDelta>=.008f&&realDelta<.28f;
            const float advanced=genuineStep&&validCorridor
                ?std::max(0.f,path-st.cutLastPositionProgress):0.f;
            if(genuineStep&&validCorridor){
                st.strokeTravelMeters+=realDelta;
                st.progress=std::max(st.progress,path);
            }
            const float rate=genuineStep
                ?std::clamp(advanced*1000.f/float(elapsed),0.f,2.f):0.f;
            st.filteredSpeed=std::clamp(
                .68f*rate+.32f*st.filteredSpeed,0.f,2.f);
            const float opposite=projection.nx*st.cutDirX+
                                 projection.ny*st.cutDirY;
            // Finishing is crossing the far VISIBLE ring, on the opposite
            // side of the start point, inside the same narrow cutting line.
            if(validCorridor&&genuineStep
               &&path>=.98f
               &&projection.normalizedRadius>=chainsawZoneEnterRadius
               &&opposite>=chainsawCutFinishOppositeDot){
                st.oneSwipeCommitted=true;
                st.progress=1.f;
                st.filteredSpeed=0.f;
            }
            if(genuineStep&&validCorridor)st.cutLastPositionProgress=path;
        }else if(!st.fastFinishLatched){
            st.filteredSpeed=0.f;
        }
        if(st.fastFinishLatched||st.oneSwipeCommitted)
            st.progress=1.f;
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
    st.finished=mode==ChainsawExperimentalMode::GripPull
        ?(st.fastFinishLatched||st.oneSwipeCommitted)
        :st.progress>=.999f;
    const int physicalFloorHz=std::min(idleHz,chainsawPhysicalMotionFastHz);
    const float clockMotion=mode==ChainsawExperimentalMode::GripPull
        ?std::clamp(st.filteredSpeed/chainsawCutRateFullSpeedPerSecond,0.f,1.f)
        :st.progress;
    int hz=std::clamp(static_cast<int>(std::lround(
        idleHz-(idleHz-(mode==ChainsawExperimentalMode::GripPull
            ?physicalFloorHz:30))*clockMotion)),
        mode==ChainsawExperimentalMode::GripPull?physicalFloorHz:30,idleHz);
    if(mode==ChainsawExperimentalMode::GripPull){
        if(st.fastFinishLatched||st.oneSwipeCommitted)
            hz=chainsawDoubleTriggerFastFinishHz;
        else if(st.resumeEligible&&!triggerHeld)
            hz=chainsawTriggerReleaseIdleHz;
    }
    const unsigned quarters=std::min(4u,static_cast<unsigned>(st.progress*4.f));
    const bool impact=quarters>st.impactIndex;
    st.impactIndex=std::max(st.impactIndex,quarters);
    // Native execution exit ALWAYS drops the override. A second press
    // bypasses even the initial oval arm and missing tracking. Edge-to-edge
    // completion also latches 30Hz to let DOOM finish its native animation.
    // Progress here is INPUT stroke fraction, NOT a native animation playhead.
    const bool owningTiming=mode==ChainsawExperimentalMode::PointerFocus
        ?st.pointerStarted
        :(st.fastFinishLatched||st.oneSwipeCommitted
          ||(st.resumeEligible&&!triggerHeld)
          ||(st.latchedGrip&&triggerHeld&&valid));
    return {hz,st.progress,owningTiming,
        st.finished,started,impact,
        st.fastFinishLatched?"DOUBLE_TRIGGER_FAST_FINISH":
        st.oneSwipeCommitted?"OPPOSITE_EDGE_FINISH":chainsawModeName(mode),
        st.hovered,st.startZone,st.targetZone,entered};
}
} // namespace kharvox
