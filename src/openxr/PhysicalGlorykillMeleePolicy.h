#pragma once
namespace kharvox::physicalgk {
// No RS gameplay melee or normal punch animation while physical GK is on.
// Never suppress independent physical punch pulses or UI/story bindings.
inline constexpr bool routeStickMelee(bool physicalGK,bool gameplay,bool pressed) noexcept {
    return pressed && !(physicalGK && gameplay);
}
inline constexpr bool routePhysicalPunch(bool physicalGK,bool gameplay,bool triggered) noexcept {
    return physicalGK && gameplay && triggered;
}
}
