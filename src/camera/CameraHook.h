#pragma once
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
bool KharvoxCameraBossSequenceActive();
// Writes a narrow death-time idPlayer->managed-entity reference snapshot for
// the external Supervisor. No continuous diagnostics are performed in-game.
bool KharvoxCameraWriteRevengeDiscoverySnapshot();
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
// Applies one horizontal dash through idPhysics_Player's validated velocity
// interface. localRight/localForward come from the movement stick.
bool KharvoxCameraApplyDirectionalDash(float localRight, float localForward);
// Applies one horizontal dash through the validated idPhysics_Player object.
// forward/lateral are normalized body-relative intent values.
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
