#pragma once
#include <cstdint>

namespace kharvox {
// The native CampaignDeath menu can receive repeated ShowScreen calls during
// a single death. Only a new closed->open transition (or a new level
// generation) is a fresh death. No polling or speculative health checks.
inline constexpr bool campaignDeathOpeningIsNew(
    bool wasVisible,std::uint64_t previousGeneration,
    std::uint64_t currentGeneration) noexcept {
    return !wasVisible||previousGeneration!=currentGeneration;
}
// Simple top/bottom and left/right positions in the existing 960x720 XR
// overlay. This is intentionally a discreet head-locked status, not another
// large HUD card.
inline constexpr int deathCounterPanelX(int corner,int width,int panelWidth,
    int margin=32) noexcept {
    return corner==0||corner==2?margin:width-panelWidth-margin;
}
inline constexpr int deathCounterPanelY(int corner,int height,int panelHeight,
    int margin=28) noexcept {
    return corner==0||corner==1?margin:height-panelHeight-margin;
}
inline constexpr const char* deathCounterCornerName(int corner) noexcept {
    switch(corner){
        case 0:return "TOP LEFT";
        case 1:return "TOP RIGHT";
        case 2:return "BOTTOM LEFT";
        case 3:return "BOTTOM RIGHT";
        default:return "TOP RIGHT";
    }
}
inline constexpr bool deathCounterShouldDraw(bool enabled,bool gameplay,
    bool vrMenuClosed,bool onboardingVisible,bool reconnectVisible) noexcept {
    return enabled&&gameplay&&vrMenuClosed
        &&!onboardingVisible&&!reconnectVisible;
}
} // namespace kharvox
