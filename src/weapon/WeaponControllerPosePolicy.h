#pragma once

namespace kharvox::weaponpose {

// The system controller pointer is an OpenXR AIM ray, NOT its GRIP pose.
// All primary weapon motion uses that AIM frame. If AIM tracking drops out,
// the runtime holds the last valid weapon pose rather than silently tilting
// the gun to a different GRIP frame.
template<class Controller>
inline const Controller& trackedPrimaryWeaponPose(
    const Controller& aim, const Controller&) noexcept {
    return aim;
}

// During normal single-hand gameplay, keep the physical AIM ray. When a
// valid support Grip has actually latched, the existing two-hand solver MUST
// control weapon orientation. Never discard its result after solving.
// Calibration/menu states are excluded from this gameplay-only restoration.
inline constexpr bool twoHandGameplaySteeringActive(
    bool enabled,bool latched,bool sameWeapon,bool supportGripHeld,
    bool supportTracked,bool holstered,bool calibrating) noexcept {
    return enabled&&latched&&sameWeapon&&supportGripHeld&&supportTracked
        &&!holstered&&!calibrating;
}

// Single-hand laser fallback, not a blanket override of solved two-hand aim.
template<class Quaternion>
inline const Quaternion& keepWeaponOnPrimaryLaser(
    const Quaternion& primaryAim, const Quaternion&) noexcept {
    return primaryAim;
}

// Do not translate the barrel's visual aim ray when the owner adjusts the
// glove. Grip calibration moves ONLY the glove, not the gun itself.
template<class Position>
inline const Position& keepGunOnPrimaryLaser(
    const Position& primaryAim, const Position&) noexcept {
    return primaryAim;
}

// Route native aim to the already-computed solver result only while
// normal gameplay has a confirmed active support grip.
template<class Quaternion>
inline const Quaternion& gameplayWeaponAim(
    const Quaternion& primaryAim,const Quaternion& twoHandSolved,
    bool twoHandSteeringActive) noexcept {
    return twoHandSteeringActive?twoHandSolved:primaryAim;
}

// DOOM's animated right-hand weapon-attachment locator includes first-
// person hand animations such as near-wall barrel retreat. Using that
// ANIMATED locator as the origin pivot moves the weapon away from a wall
// even after the native front-pushback animation branch is disabled.
// Tracked custom hands instead reference the already captured neutral
// weapon-attachment locator: the gun stays aligned to the physical controller
// while the DOOM animation may advance independently underneath. Do not
// modify tracking, gun orientation, fine-tune offsets or saved calibrations.
// In guns-only mode or before the settled reference is valid, preserve the
// previous native animated locator for compatibility.
template<class Pivot>
inline const Pivot& nativeWeaponGripPivotForPresentation(
    const Pivot& animatedPivot,const Pivot& neutralPivot,
    bool customTrackedHandMode,bool neutralPivotValid) noexcept {
    return customTrackedHandMode&&neutralPivotValid
        ?neutralPivot:animatedPivot;
}

// Only while a support Grip stays latched in actual gameplay, alternate
// Gauss Siege / Chaingun Turret grip positions should ease between the two
// APPROVED, saved profile endpoints instead of jumping instantly. Position
// and gun-only offset blend together over 320ms, independent of frame rate.
inline constexpr unsigned kHeavyGripTransitionMs=320;
inline float heavyGripTransitionEase(unsigned long long elapsedMs) noexcept {
    const float t=elapsedMs>=kHeavyGripTransitionMs?1.f
        :static_cast<float>(elapsedMs)/kHeavyGripTransitionMs;
    return t*t*(3.f-2.f*t);
}
template<class Vector>
inline Vector heavyGripBlendVector(const Vector& from,const Vector& to,
    float progress) noexcept {
    return {from.x+(to.x-from.x)*progress,
            from.y+(to.y-from.y)*progress,
            from.z+(to.z-from.z)*progress};
}

// The legacy 25-degree gun yaw (and old pitch/roll overrides) must not be
// layered onto the native weapon while laser-locked hands are enabled.
inline constexpr float weaponEulerOffsetDegrees(
    float legacyDegrees, bool controllerAlignedHands) noexcept {
    return controllerAlignedHands ? 0.f : legacyDegrees;
}

// A dedicated PRIMARY HAND ATTACHMENT ANCHOR belongs to the physical
// weapon-hand GRIP controller, not the moving gun root. Return the full
// tracked controller pose so neither position nor orientation can silently
// inherit a translated gun pivot. This is separate from native weapon pose.
template<class Controller>
inline const Controller& controllerFixedPrimaryHandAnchor(
    const Controller& physicalGrip,const Controller&) noexcept {
    return physicalGrip;
}

// Weapon-hand attachment must share ONE PHYSICAL ORIGIN with the
// rendered glove. AIM is the barrel's orientation/ray, but Quest/OpenXR
// exposes a different AIM POSITION and GRIP POSITION. Starting the gun at
// AIM while the glove starts at GRIP is a visible positional mismatch even
// with fine-tune zero. Custom-hands mode uses the physical GRIP position for
// the gun's base attachment, leaving gun-only fine-tune to shift the model.
// Without the tracked custom hand, retain the legacy weapon-origin fallback.
template<class Position>
inline const Position& sharedPhysicalWeaponGripOrigin(
    const Position& controllerGrip,const Position& legacyWeaponOrigin,
    bool customHandGripValid) noexcept {
    return customHandGripValid?controllerGrip:legacyWeaponOrigin;
}

// The gun has its own moving RENDER ORIGIN. Rotate the original offset from
// primary controller GRIP to AIM about the physical grip anchor whenever
// the existing two-hand solver changes gun orientation. One-hand is exactly
// the original AIM position; during two-hand aiming the primary hand/pivot
// stays on the controller and the native gun rotates around it.
template<class Position,class Quaternion,class Subtract,class Add,
    class Rotate,class Conjugate>
inline Position gunOriginAboutControllerHandAnchor(
    const Position& fixedHandAnchor,const Position& originalAimOrigin,
    const Quaternion& originalAimOrientation,
    const Quaternion& solvedGunOrientation,
    Subtract subtract,Add add,Rotate rotate,Conjugate conjugate) {
    const auto gripToAimLocal=rotate(conjugate(originalAimOrientation),
        subtract(originalAimOrigin,fixedHandAnchor));
    return add(fixedHandAnchor,rotate(solvedGunOrientation,gripToAimLocal));
}

// Existing support-hand profiles are relative to the original AIM origin.
// Rebase their local grab point to the physical controller GRIP origin,
// preserving exactly the same world-space grab target before two-hand
// steering begins. The steering solver can then use the controller-fixed
// hand anchor, rather than a gun origin that moves during fine-tuning.
template<class Position,class Quaternion,class Subtract,class Add,
    class Rotate,class Conjugate>
inline Position supportOffsetFromControllerAnchor(
    const Position& savedSupportOffset,const Position& physicalHandAnchor,
    const Position& aimOrigin,const Quaternion& primaryAimOrientation,
    Subtract subtract,Add add,Rotate rotate,Conjugate conjugate) {
    return add(savedSupportOffset,rotate(conjugate(primaryAimOrientation),
        subtract(aimOrigin,physicalHandAnchor)));
}

// In normal single-hand gameplay the primary glove still follows the
// physical primary AIM controller. A truly latched second hand changes
// the weapon's rendered quaternion; use that same frame for the glove to
// avoid the gun appearing to twist inside the gripping hand. This selects
// only a VISUAL reference. Controller tracking / solver input is unchanged.
template<class Quaternion>
inline const Quaternion& primaryGloveVisualFrame(
    const Quaternion& primaryControllerOrientation,
    const Quaternion& renderedWeaponOrientation,
    bool customHandsVisible,bool twoHandGameplayLatched) noexcept {
    return customHandsVisible&&twoHandGameplayLatched
        ?renderedWeaponOrientation
        :customHandsVisible?primaryControllerOrientation
            :renderedWeaponOrientation;
}

// Render the calibrated primary GLOVE quaternion in the same frame as
// the actual gun. The support hand steers the weapon's final orientation
// during normal two-hand gameplay; do not use the primary controller AIM
// quaternion for this visual or the gun spins through a static glove.
// Preserve the saved, weapon-local wrist angle on either physical side.
// This affects ONLY the visible glove; the controller-fixed hand attachment
// position/pivot and native gun orientation remain unchanged.
template<class Quaternion,class Multiply>
inline Quaternion weaponLockedPrimaryGloveOrientation(
    const Quaternion& renderedGunOrientation,
    const Quaternion& savedGloveLocalOrientation,Multiply multiply) {
    return multiply(renderedGunOrientation,savedGloveLocalOrientation);
}

// Both preview and saved support-grip coordinates are measured relative
// to the UNADJUSTED AIM reference (saved in lastWeaponRenderedTrackingAnchor).
// The moving native weapon model receives an additional per-weapon gun-only
// fine-tune translation AFTER that reference is captured. Re-applying that
// gun translation to the support hand made a saved grip visibly jump on
// release/re-equip. Render support using its calibrated reference only.
// When the gun changes offset, the owner may recalibrate its support handle;
// NEVER silently translate a recorded support hand by a second offset.
template<class Position,class Quaternion,class Add,class Rotate>
inline Position supportGloveAtCalibrationReference(
    const Position& unadjustedAimAnchor,
    const Quaternion& renderedWeaponOrientation,
    const Position& savedSupportLocal,Add add,Rotate rotate) {
    return add(unadjustedAimAnchor,
        rotate(renderedWeaponOrientation,savedSupportLocal));
}

// The primary glove's position is the physical weapon-hand controller GRIP
// pose, independent of both per-weapon gun offsets and 1cm joystick nudges.
// Distinct from the gun's AIM pose, so level physical controllers produce
// matching tracked hand origins regardless of weapon size or calibration.
template<class Position>
inline const Position& physicalPrimaryGlovePosition(
    const Position& trackedGrip,const Position&) noexcept {
    return trackedGrip;
}

// Primary hand *visual* fitting can adjust only vertical Y and along-barrel Z.
// Lock lateral X. Does not constrain the visual glove's local quaternion:
// it can be oriented any way the owner chooses and locked to the gun.
template<class Offset>
inline Offset primaryHandVisualPlacement(
    Offset measured, Offset existing) noexcept {
    measured.x=existing.x;
    return measured;
}

// Primary glove is anchored to physical controller GRIP, not the gun.
// Grip-held joystick fine-tune translates ONLY the GUN in X/Y/Z, never
// the primary glove or its controller pose. No gun rotation is introduced.
template<class Offset>
struct PrimaryHandAnchorPlan {
    Offset gunTranslationLocal;   // calibrated Y/Z plus gun-only XYZ fine-tune
    Offset handAttachmentLocal;   // legacy reference, glove uses physical GRIP
};
template<class Offset>
inline PrimaryHandAnchorPlan<Offset> primaryHandAnchorPlan(
    const Offset& requestedHand,const Offset& referenceHand,
    const Offset& fineTune=Offset{}) noexcept {
    // Saved position moves the gun in Y/Z; joystick offsets apply to
    // the gun in all three axes. The real tracked glove is unchanged.
    return {
        Offset{fineTune.x,requestedHand.y-referenceHand.y+fineTune.y,
            requestedHand.z-referenceHand.z+fineTune.z},
        referenceHand
    };
}

} // namespace kharvox::weaponpose
