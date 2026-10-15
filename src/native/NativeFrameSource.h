#pragma once
#include <cstdint>
namespace kharvox::native {
enum class SceneDomain : unsigned { Gameplay, Cinematic, Scripted, Inactive };
struct FrameSourceIdentity { uint64_t poseId{},level{}; SceneDomain domain{SceneDomain::Inactive}; };
constexpr SceneDomain sceneDomain(bool quad,bool cinematic,bool scripted,bool gameplay){
    return quad?SceneDomain::Inactive:cinematic?SceneDomain::Cinematic:
        scripted?SceneDomain::Scripted:gameplay?SceneDomain::Gameplay:SceneDomain::Inactive;
}
// The observer's cinematic-participant guard has a deliberate grace period.
// When DOOM's camera has ALREADY returned to gameplay, do not insert a
// spurious Scripted stereo scene between Cinematic and Gameplay. That extra
// domain change forces SFS to discard its finished left/right pair and can
// submit a few no-layer frames (visible as a blink in the headset).
constexpr bool useScriptedParticipantScene(
    bool nativeParticipantGuardActive,
    bool actualGameplayCameraReturned) noexcept {
    return nativeParticipantGuardActive && !actualGameplayCameraReturned;
}

// The prepared tracking sample must be the one used throughout both native roots.
constexpr bool sourcePoseStable(FrameSourceIdentity source,uint64_t firstPose,
                               uint64_t lastPose,uint64_t level){
    return source.poseId&&source.domain!=SceneDomain::Inactive&&source.level==level
        &&source.poseId==firstPose&&firstPose==lastPose;
}
// A completed frame may use an older prediction, but never another scene domain
// or level. Compare pose identity to its producer, not the next XR prediction.
constexpr bool sameSceneContext(FrameSourceIdentity source,FrameSourceIdentity desired,
                                bool sourceViewSpace,bool desiredViewSpace){
    return source.poseId&&source.domain!=SceneDomain::Inactive
        &&source.domain==desired.domain&&source.level==desired.level
        &&sourceViewSpace==desiredViewSpace;
}
}
