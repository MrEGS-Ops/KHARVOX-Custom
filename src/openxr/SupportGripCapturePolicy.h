#pragma once
namespace kharvox {
// One capture for each press of BOTH thumbsticks, release either to rearm.
struct SupportGripCaptureChord {
    bool held{};
    bool pressed(bool leftClick,bool rightClick) noexcept {
        if(!leftClick||!rightClick){held=false;return false;}
        if(held)return false;
        held=true;
        return true;
    }
};
} // namespace kharvox
