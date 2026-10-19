#pragma once
#include "ChainsawTriggerThrottlePolicy.h"
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <cctype>

namespace kharvox {
// Small, bounded protocol for launcher-to-game Haptic Lab testing.
// Windows FILETIME is the common cross-process clock (100ns since 1601).
// Requests older than 10 seconds NEVER play after a crash/relaunch.
// Unlike the normal chainsaw haptics, test requests intentionally select
// a physical left/right controller, independent of currently equipped weapon.
struct ChainsawHapticPreviewRequest {
    std::uint64_t fileTime{};
    std::string id{};
    int hand{2}; // 0 left, 1 right, 2 stop
    bool cutting{};
    ChainsawHapticConfig config{};
};
inline bool parseChainsawHapticPreviewRequest(
    const std::string& line, std::uint64_t nowFileTime,
    ChainsawHapticPreviewRequest& output) {
    if(line.empty() || line.size()>256)return false;
    std::istringstream in(line);
    std::string magic,id,extra;
    std::uint64_t created{};
    int hand=-1,mode=-1,idle=-1,cut=-1,rate=-1,pulse=-1,rough=-1,motion=-1;
    if(!(in>>magic>>created>>id>>hand>>mode>>idle>>cut>>rate>>pulse>>rough>>motion)
        ||in>>extra||magic!="KHARVOX_HAPTIC_PREVIEW_V1"
        ||id.size()!=32||hand<0||hand>2||mode<0||mode>1
        ||idle<0||idle>100||cut<0||cut>100
        ||rate<5||rate>100||pulse<5||pulse>100
        ||rough<0||rough>100||motion<0||motion>100)return false;
    for(const unsigned char c:id)
        if(!std::isxdigit(c))return false;
    constexpr std::uint64_t ticksPerSecond=10000000ull;
    if(created>nowFileTime+2*ticksPerSecond
        ||nowFileTime>created+10*ticksPerSecond)return false;
    output={created,id,hand,mode==1,{idle,cut,rate,pulse,rough,motion}};
    return true;
}
inline ChainsawTriggerThrottleOutput chainsawHapticPreviewSignal(
    const ChainsawHapticPreviewRequest& request,std::uint64_t nowMs){
    // Full-throttle moving-chain simulation versus steady idle.
    // The produced tickHz is never installed in the game's simulation.
    auto sample=evaluateChainsawTriggerThrottle(
        request.cutting?1.f:0.f,request.cutting?1.8f:0.f,
        1.8f,request.config);
    return textureChainsawTriggerHaptics(sample,nowMs,request.config);
}
} // namespace kharvox
