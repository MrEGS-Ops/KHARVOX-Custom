#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

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
inline std::string campaignRunTimeText(std::uint64_t milliseconds){
    const auto seconds=milliseconds/1000ULL;
    const auto hours=seconds/3600ULL;
    const auto minutes=(seconds/60ULL)%60ULL;
    const auto secs=seconds%60ULL;
    char result[64]{};
    std::snprintf(result,sizeof(result),"%02llu:%02llu:%02llu",
        static_cast<unsigned long long>(hours),
        static_cast<unsigned long long>(minutes),
        static_cast<unsigned long long>(secs));
    return result;
}
inline constexpr int campaignStatsPanelWidth(int item) noexcept {
    // All four compact readouts use the same width, allowing clean
    // left-edge alignment of Campaign/Session Deaths and both timers.
    (void)item;
    return 226;
}
} // namespace kharvox
