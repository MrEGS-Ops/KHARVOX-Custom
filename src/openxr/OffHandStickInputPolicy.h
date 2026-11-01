#pragma once
#include <cstdint>

namespace kharvox {
// Both physical thumbstick clicks are reserved for capture only when
// pressed together. Single right-click pauses, single left-click melees.
// Allow a brief simultaneous-click window so capturing cannot pause the game.
struct OffHandDelayedClick {
    bool held{}, blocked{}, emitted{};
    std::uint64_t startedAt{};
    bool update(bool down,bool other,std::uint64_t ms) noexcept {
        if(!down) {
            const bool pulse=held&&!blocked&&!emitted;
            held=blocked=emitted=false;
            return pulse;
        }
        if(!held) {held=true;startedAt=ms;blocked=other;emitted=false;}
        if(other) blocked=true;
        if(blocked)return false;
        if(!emitted&&ms-startedAt>=100)emitted=true;
        return emitted;
    }
};
struct OffHandStickOutput {bool rightPause{},leftMelee{};};
struct OffHandStickInput {
    OffHandDelayedClick right{},left{};
    OffHandStickOutput update(bool rightPressed,bool leftPressed,
                              bool enabled,std::uint64_t ms) noexcept {
        if(!enabled) {right={};left={};return {};}
        return {right.update(rightPressed,leftPressed,ms),
                left.update(leftPressed,rightPressed,ms)};
    }
};
} // namespace kharvox
