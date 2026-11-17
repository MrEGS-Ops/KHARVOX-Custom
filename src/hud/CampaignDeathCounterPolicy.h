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
// Fine placement uses a screen-shaped 12 x 10 map (120 potential slots).
// Old saved corner values 0..3 remain valid and retain their original pixels.
inline constexpr int kStatsGridColumns=12;
inline constexpr int kStatsGridRows=10;
inline constexpr int kStatsGridCells=kStatsGridColumns*kStatsGridRows;
inline constexpr int kStatsFinePositionBase=100;
inline constexpr int kStatsMenuGridX=538;
inline constexpr int kStatsMenuGridY=248;
inline constexpr int kStatsMenuGridCellWidth=31;
inline constexpr int kStatsMenuGridCellHeight=30;
inline constexpr bool statsGridCellSelectable(int cell) noexcept {
    if(cell<0||cell>=kStatsGridCells)return false;
    const int col=cell%kStatsGridColumns,row=cell/kStatsGridColumns;
    // Leave the exact centre clear; side and surrounding cells are available.
    return !((col==5||col==6)&&(row==4||row==5));
}
inline constexpr int statsGridCellFromPosition(int position) noexcept {
    switch(position) {
        case 0:return 0;                                  // old TOP LEFT
        case 1:return kStatsGridColumns-1;                // old TOP RIGHT
        case 2:return kStatsGridCells-kStatsGridColumns; // old BOTTOM LEFT
        case 3:return kStatsGridCells-1;                 // old BOTTOM RIGHT
        default:break;
    }
    const int cell=position-kStatsFinePositionBase;
    return statsGridCellSelectable(cell)?cell:-1;
}
inline constexpr int statsPositionFromGridCell(int cell) noexcept {
    return statsGridCellSelectable(cell)?kStatsFinePositionBase+cell:-1;
}
inline constexpr bool statsPositionValid(int position) noexcept {
    return statsGridCellFromPosition(position)>=0;
}
inline constexpr int statsMenuGridCellAt(int x,int y) noexcept {
    const int dx=x-kStatsMenuGridX,dy=y-kStatsMenuGridY;
    if(dx<0||dy<0||dx>=kStatsGridColumns*kStatsMenuGridCellWidth
        ||dy>=kStatsGridRows*kStatsMenuGridCellHeight)return -1;
    const int cell=(dy/kStatsMenuGridCellHeight)*kStatsGridColumns
        +dx/kStatsMenuGridCellWidth;
    return statsGridCellSelectable(cell)?cell:-1;
}
inline constexpr int statsPanelX(int position,int width,int panelWidth,
    int margin=32) noexcept {
    if(position>=kStatsFinePositionBase){
        const int cell=statsGridCellFromPosition(position);
        if(cell<0)return deathCounterPanelX(1,width,panelWidth,margin);
        const int travel=width-panelWidth-2*margin;
        return margin+(travel>0?travel*(cell%kStatsGridColumns)
            /(kStatsGridColumns-1):0);
    }
    return deathCounterPanelX(position,width,panelWidth,margin);
}
inline constexpr int statsPanelY(int position,int height,int panelHeight,
    int margin=28) noexcept {
    if(position>=kStatsFinePositionBase){
        const int cell=statsGridCellFromPosition(position);
        if(cell<0)return deathCounterPanelY(1,height,panelHeight,margin);
        const int travel=height-panelHeight-2*margin;
        return margin+(travel>0?travel*(cell/kStatsGridColumns)
            /(kStatsGridRows-1):0);
    }
    return deathCounterPanelY(position,height,panelHeight,margin);
}
inline constexpr bool deathCounterShouldDraw(bool enabled,bool gameplay,
    bool vrMenuClosed,bool onboardingVisible,bool reconnectVisible) noexcept {
    return enabled&&gameplay&&vrMenuClosed
        &&!onboardingVisible&&!reconnectVisible;
}
} // namespace kharvox
