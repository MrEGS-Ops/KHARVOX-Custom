#pragma once
#include <cstdint>

namespace kharvox {
struct CampaignRunTime {
    std::uint64_t elapsedMs{};
    std::uint64_t lastSampleMs{};
    bool activeLastSample{};
};
inline constexpr void advanceCampaignRunTime(
    CampaignRunTime& timer,bool active,std::uint64_t nowMs) noexcept {
    if(timer.activeLastSample&&active&&timer.lastSampleMs
        &&nowMs>timer.lastSampleMs){
        // Prevent a blocked rendering frame or a suspended headset from
        // silently adding seconds of non-playable time.
        const auto delta=nowMs-timer.lastSampleMs;
        timer.elapsedMs+=delta<250ULL?delta:250ULL;
    }
    timer.lastSampleMs=nowMs;
    timer.activeLastSample=active;
}
inline constexpr void resetCampaignRunTime(CampaignRunTime& timer) noexcept {
    timer.elapsedMs=0;
    timer.lastSampleMs=0;
    timer.activeLastSample=false;
}
inline constexpr bool campaignStatsCanDraw(bool gameplay,bool menuClosed,
    bool onboarding,bool disconnected) noexcept {
    return gameplay&&menuClosed&&!onboarding&&!disconnected;
}
inline constexpr bool campaignStatsDrawItem(bool enabled,bool canDraw) noexcept {
    return enabled&&canDraw;
}
inline constexpr int campaignStatsPanelWidth(int item) noexcept {
    return item==1?316:226;
}
} // namespace kharvox
