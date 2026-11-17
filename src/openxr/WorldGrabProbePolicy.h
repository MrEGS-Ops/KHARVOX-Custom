#pragma once
#include <array>
#include <cmath>
#include <cstddef>

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
struct WorldGrabProbeInput {
  bool enabled{},gameplay{},controllersReady{},supportBusy{},wheelBusy{};
  std::array<bool,2> tracked{},freeHand{},grip{}; // LEFT, RIGHT
  std::array<float,2> distanceFromHead{};
};
struct WorldGrabProbeEvents {
  std::array<bool,2> started{},released{},pastReach{};
};
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
