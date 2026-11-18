#pragma once
#include <array>
#include <cstdint>
#include "AerWorldViewHistory.h"
#include "../common/AerSourceTracking.h"

enum class VREye : int {
    Mono = 0,
    Left = 1,
    Right = 2
};

bool KharvoxCameraInstallDiagnosticHook();
void KharvoxCameraFinishNativeStereoBootstrap();
void KharvoxCameraCompleteNativeStereoBootstrapAfterRender();
void KharvoxCameraPollDiagnostic();
void KharvoxCameraSetHeadPose(
    float yawDegrees, float pitchDegrees, float rollDegrees,
    float forwardUnits, float lateralUnits, float upUnits,
    bool valid, float referenceBodyYaw = 0.0f, bool referenceBodyValid = false);
void KharvoxCameraSetArtificialTurnYaw(float yawDegrees);
unsigned long long KharvoxCameraDiagnosticPoseId();
// Copies only recognized, observed gameplay cameras; caller owns engine writes.
kharvox::AerWorldViewResult KharvoxCameraAlignAerWorldView(const float pose[12],const float fov[2],
    kharvox::AerWorldView& source,kharvox::AerWorldView& target);
kharvox::AerWorldViewResult KharvoxCameraRecognizeAerWorldView(const float pose[12],const float fov[2],kharvox::AerWorldView& source);
void KharvoxCameraRecordAerWorldSource(const kharvox::AerWorldView& source);
bool KharvoxCameraUsesAerGameplaySource();
kharvox::AerSourceObservation KharvoxCameraTakeAerWorldSource();
void KharvoxCameraSetCrouchState(bool active);
// Optional controller-driven interaction focus ray in DOOM world coordinates.
void KharvoxCameraSetFocusHandPose(const float origin[3], const float direction[3], bool valid);
 
// Read-only observation of DOOM's existing FocusTracker selection state.
// These are *opaque* native fields, NOT a validated physics entity hit.
// Does not enable dev CVars, issue extra traces, or write entity memory.
struct KharvoxWorldGrabNativeFocus {
    unsigned int indexA{},indexB{};
    unsigned long long nativeReference{},levelGeneration{},sampleSequence{};
};
void KharvoxCameraEnableWorldGrabNativeFocusObservation(bool enabled);
// DEV-only native physics inspection. The game thread may edit a proven
// rigid body's original position buffer. No live entity pointer crosses
// the DOOM/OpenXR boundary; disabled by default and in Native release.
struct KharvoxWorldGrabPhysicsSample {
    unsigned int handle{},path{};
    unsigned long long generation{},sequence{};
    int mode{},status{}; // 1=no body, 2=origin readable, 3=writable,
                         // 4=write committed, 5=readback position changed
    bool writable{},changed{};
    // 0=none, 1=physics-owned GetOrigin pointer, 2=external GetOrigin
    // pointer, 3=unique origin-vector match inside physics object.
    int storageRoute{},originMatches{};
    float distanceFromHand{-1.f};
    float origin[3]{};
    std::array<char,64> physicsClass{};
};
void KharvoxCameraConfigureWorldGrabPhysics(int mode,bool enabled);
bool KharvoxCameraReadWorldGrabPhysics(KharvoxWorldGrabPhysicsSample& sample);
bool KharvoxCameraReadWorldGrabNativeFocus(KharvoxWorldGrabNativeFocus& sample);
// Candidate handles intercepted immediately before DOOM resolves two native
// trace hits into entities. May include static/invalid/non-interactable hits.
// Deliberately NOT a movable physics result or a physics object pointer.
struct KharvoxWorldGrabTraceCandidate {
    unsigned int nativeHandle{};
    unsigned int sourcePath{}; // 0 = primary, 1 = secondary FocusTracker trace
    unsigned long long levelGeneration{},sampleSequence{};
};
bool KharvoxCameraReadWorldGrabTraceCandidate(
    unsigned int sourcePath,KharvoxWorldGrabTraceCandidate& sample);
// Observation at the next native FocusTracker boundary: DOOM's own entity
// manager has resolved a non-null candidate, before interactable filtering.
// No entity pointer leaves the game thread. The vtable RVA is an opaque
// fingerprint, NOT evidence of a movable physics body.
struct KharvoxWorldGrabResolvedEntity {
    unsigned int sourcePath{},nativeHandle{};
    std::uint64_t vtableRva{},levelGeneration{},sampleSequence{};
    bool vtableInExecutable{};
    // Best-effort MSVC RTTI type, only when a complete-object locator and
    // TypeDescriptor are validated inside the DOOM image. Empty otherwise.
    // Class name != map asset name or proof of dynamic/physics eligibility.
    std::array<char,80> nativeClassName{};
};
bool KharvoxCameraReadWorldGrabResolvedEntity(
    unsigned int sourcePath,KharvoxWorldGrabResolvedEntity& sample);


void KharvoxCameraSetImmersiveCinematicFov(
    float fovXDegrees, float fovYDegrees,
    bool requested, bool active);
void KharvoxCameraSetImmersiveCinematicFreelook(bool requested);
// Experimental: apply authored camera yaw in non-execution story cinematics
// while retaining HMD residual rotation. Defaults to OFF.
void KharvoxCameraSetStoryCameraFocus(bool requested);
// True while the preceding execution/ledge's native cutscene tail remains.
void KharvoxCameraSetStoryFocusActionTail(bool blocked);
bool KharvoxCameraWorldActive();
bool KharvoxCameraGameplayActive();
bool KharvoxCameraNativeStereoActive();
bool KharvoxCameraNativeTwoViewActive();
bool KharvoxCameraCutsceneActive();
// Read-only, observed native cinematic camera translation; DOOM world units.
// This is NOT an animation playhead or proof of an animator state.
bool KharvoxCameraReadCinematicSourceOrigin(float origin[3]);
bool KharvoxCameraBossSequenceActive();
bool KharvoxCameraSyncAttackActive();
// Temporary Glory Kill playback override. A value <= 0 clears it.
// The camera hook restores DOOM's baseline timescale when the sync attack ends.
void KharvoxCameraSetGloryKillTimescaleOverride(float timescale);
// Returns false if DOOM's native timescale CVar cannot be safely resolved.
// Safe to call on every story-skip toggle; OFF restores the captured baseline.
bool KharvoxCameraSetStoryCinematicFastForward(bool enabled);
// A chainsaw takedown can enter cinematic mode before DOOM reports the same
// sync-attack flag as an ordinary Glory Kill. Accept only a latched chainsaw
// attack followed by a native camera cinematic; reset on focus loss.
void KharvoxCameraSetChainsawTakedownCinematic(bool active);
// Experimental: request DOOM's OWN native SSG reload animator on re-draw.
// Only writes positively resolved, reversible native reload CVars. Does
// nothing if safe lookup fails; does not replace the model with a fake.
void KharvoxCameraServiceSuperShotgunNativeReloadReplay(
    bool requestNewReplay, bool superShotgunActiveAndSafe);
// Reapply native Glory Kill Highlight selection after DOOM reloads campaign settings.
// Does not affect Glory Kill availability, animation or timing.
// Native DOOM tutorial setting only. Never force-close tutorial or automap screens.
void KharvoxCameraEnforceTutorialsDisabled();
void KharvoxCameraSetBoxingGuardSlowmo(bool active);
void KharvoxCameraResetCombatTimeDilation();
bool KharvoxCameraLedgeTransitionActive();
// True only when DOOM's native player command tracker currently permits both
// view control and weapon buttons. This distinguishes playable first-person
// scripted intervals from passive scenes which merely render a weapon model.
bool KharvoxCameraPlayerWeaponControlActive();
// Opt-in RS experiment: conditionally clears only validated movement-direction
// inhibit bits. Never touches view, weapons, physics position or mission flags.
bool KharvoxCameraApplyStoryFreeWalk(bool requested, bool eligible);
// Independent, opt-in automatic narrative diagnostic and conditional motion
// release. Script/audio/mission events are never advanced by this API.
void KharvoxCameraServiceUninterruptedStory(bool active, bool stickMovement);
// While Gauss Siege is charging, learn and clear only charge-added movement
// inhibit bits. Calling with false updates the normal gameplay baseline.
void KharvoxCameraSetGaussChargeMovementOverride(bool active);
// Returns the live gameplay body pose for VR rendering without mutating physics.
bool KharvoxCameraGetBodyPose(float origin[3], float axis[9]);
bool KharvoxCameraGetHeadRenderPose(float origin[3], float axis[9]);
bool KharvoxCameraGetHudCenterRenderPose(float origin[3], float axis[9]);
unsigned long long KharvoxCameraCurrentPresentSerial();
unsigned long long KharvoxCameraLevelTransitionGeneration();
bool KharvoxCameraGetPlayerPhysicsOrigin(float origin[3]);
void KharvoxCameraSetStereoEye(float localXUnits, float fovXDegrees, float fovYDegrees, float opticalCenterYawDegrees, float opticalCenterPitchDegrees, bool enabled);
// Separates an authored animated-camera interval from ordinary gameplay. The
// gameplay body is gravity-level at all other times; physical HMD pitch/roll is
// still applied independently after that body pose is published.
void KharvoxCameraSetAnimatedSequenceActive(bool active);
// Programs the eye which DOOM will render next and enables native camera-pose
// pairing for animated AER sequences. The base cinematic/gameplay camera pose
// is shared by the two eyes; IPD and the current HMD pose remain per-eye.
void KharvoxCameraSetAerRenderPair(int renderEye, bool enabled);
void KharvoxCameraSetNativeStereoParameters(float halfEyeSeparationUnits, float screenSeparation);
void KharvoxCameraConfigureEye(VREye eye, float localXUnits, float fovXDegrees, float fovYDegrees,
    float opticalCenterYawDegrees, float opticalCenterPitchDegrees);
bool KharvoxSameFrameStereoInstalled();
VREye KharvoxCameraCurrentEye();
