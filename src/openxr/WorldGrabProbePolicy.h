#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

// Stage 1: input-only world-grab probe. Never accesses arbitrary DOOM entities,
// never writes native physics and never clamps locomotion.
namespace kharvox {
// This is a visual ray only: valid tracking does NOT establish a barrel hit.
// Session-only DEV controls must never take ownership from weapon handling.
struct WorldGrabDebugLaserInput {
  bool enabled{},gameplay{},tracked{},controllersReady{},
      wheelBusy{},supportBusy{},menuOpen{};
};
inline constexpr bool worldGrabDebugLaserVisible(
    const WorldGrabDebugLaserInput& in) noexcept {
  return in.enabled&&in.gameplay&&in.tracked&&in.controllersReady
      &&!in.wheelBusy&&!in.supportBusy&&!in.menuOpen;
}
// During a real, free-hand DEV Grip, let DOOM's existing FocusTracker cast
// from the same physical controller as the debug laser. Weapon-hand focus
// and native player interaction are restored as soon as Grip releases.
// This only changes the EXISTING interaction trace source; it is not a
// separate generic rigid-body raycast and cannot itself make props grabbable.
inline constexpr bool worldGrabBorrowNativeFocus(
    bool devEnabled,bool gameplay,bool worldFocusContext,
    bool freeHandGripHeld,bool nativeActionBusy) noexcept {
    return devEnabled&&gameplay&&worldFocusContext
        &&freeHandGripHeld&&!nativeActionBusy;
}
struct WorldGrabProbeInput {
  bool enabled{},gameplay{},controllersReady{},supportBusy{},wheelBusy{};
  std::array<bool,2> tracked{},freeHand{},grip{}; // LEFT, RIGHT
  std::array<float,2> distanceFromHead{};
};
struct WorldGrabProbeEvents {
  std::array<bool,2> started{},released{},pastReach{};
};
// Experimental in-headset visual inspector. It displays DOOM's OWN opaque
// focus-path observations as clues, not a fabricated physics/body hit.
struct WorldGrabInspectorPath {
  std::uint32_t nativeHandle{};
  std::uint64_t vtableRva{},levelGeneration{},lastCandidateMs{},lastResolvedMs{};
  bool hasCandidate{},hasResolved{},vtableInExecutable{};
  std::string nativeClassName;
};
struct WorldGrabInspectorSnapshot {
  std::array<WorldGrabInspectorPath,2> paths{};
  std::uint32_t focusIndexA{},focusIndexB{};
  bool nativeFocusSeen{},focusHasReference{};
  std::uint64_t focusLastMs{};
  bool enabled{},gripHeld{},freeHandRight{};
  int nativeDragMode{},nativeDragStatus{};
  bool nativeDragHolding{};
  std::string lastEvent{"Grip to inspect DOOM focus candidates"};
};
inline bool worldGrabInspectorFresh(std::uint64_t nowMs,
    std::uint64_t lastSeenMs,std::uint64_t ttlMs=1750) noexcept {
  return lastSeenMs!=0&&nowMs>=lastSeenMs&&nowMs-lastSeenMs<=ttlMs;
}
struct WorldGrabProbeState {
  static constexpr float kReachMeters=.90f; // advisory only
  std::array<bool,2> gripLast{},held{},wasPastReach{};
  std::array<unsigned,2> starts{},releases{};
  WorldGrabProbeEvents update(const WorldGrabProbeInput& in) noexcept {
    WorldGrabProbeEvents ev{};
    const bool allowed=in.enabled&&in.gameplay&&in.controllersReady
      &&!in.supportBusy&&!in.wheelBusy;
    for(std::size_t h=0;h<2;++h){
      const bool valid=allowed&&in.tracked[h]&&in.freeHand[h]
        &&std::isfinite(in.distanceFromHead[h])&&in.distanceFromHead[h]>=0.f;
      const bool newPress=in.grip[h]&&!gripLast[h];
      const bool nowHeld=valid&&in.grip[h]&&(held[h]||newPress);
      ev.started[h]=nowHeld&&!held[h];ev.released[h]=held[h]&&!nowHeld;
      if(ev.started[h])++starts[h];
      if(ev.released[h])++releases[h];
      const bool over=nowHeld&&in.distanceFromHead[h]>kReachMeters;
      ev.pastReach[h]=over&&!wasPastReach[h];
      held[h]=nowHeld;wasPastReach[h]=over;
      gripLast[h]=in.grip[h]; // no auto-attach when enabling with held Grip
    }
    return ev;
  }
};
}
