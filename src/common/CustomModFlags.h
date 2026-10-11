#pragma once

#include <windows.h>
#include <cstring>

namespace kharvox {

inline bool customModEnvironmentEnabled(const char* name) {
    char value[16]{};
    const DWORD copied = GetEnvironmentVariableA(name, value, sizeof(value));
    if (!copied || copied >= sizeof(value)) return false;
    return !_stricmp(value, "1") || !_stricmp(value, "true")
        || !_stricmp(value, "yes") || !_stricmp(value, "on");
}

struct CustomModFlags {
    bool disableHud{};
    bool disableWeaponWheel{};
    bool gaussChargeSlowMovement{};
    bool backOfHandHud{};
    bool handFocusedRs{};
    bool directionalDash{};
    bool behindHeadWeaponWheel{};
    bool behindHeadWheelHandSelection{true};
    bool physicalCrouch{};
    bool revengeDemon{};
    bool dynamicShoulderHolster{};
    bool physicalGrenadeThrow{};
    bool motionGloryKillSpeed{};
    bool physicalChainsawGestures{};
    bool hapticOverhaul{};
};

inline CustomModFlags loadCustomModFlags() {
    CustomModFlags flags;
    flags.disableHud = customModEnvironmentEnabled("KHARVOX_MOD_DISABLE_HUD");
    flags.disableWeaponWheel = customModEnvironmentEnabled("KHARVOX_MOD_DISABLE_WEAPON_WHEEL");
    flags.gaussChargeSlowMovement = customModEnvironmentEnabled("KHARVOX_MOD_GAUSS_SLOW_CHARGE");
    flags.backOfHandHud = customModEnvironmentEnabled("KHARVOX_MOD_BACK_OF_HAND_HUD");
    flags.handFocusedRs = customModEnvironmentEnabled("KHARVOX_MOD_HAND_FOCUS_RS");
    flags.directionalDash = customModEnvironmentEnabled("KHARVOX_MOD_DIRECTIONAL_DASH");
    flags.behindHeadWeaponWheel = customModEnvironmentEnabled("KHARVOX_MOD_BEHIND_HEAD_WHEEL");
    char behindHeadHandSelection[16]{};
    const DWORD behindHeadHandSelectionLength = GetEnvironmentVariableA(
        "KHARVOX_MOD_BEHIND_HEAD_WHEEL_HAND_SELECTION", behindHeadHandSelection,
        sizeof(behindHeadHandSelection));
    flags.behindHeadWheelHandSelection = behindHeadHandSelectionLength == 0
        || (!_stricmp(behindHeadHandSelection, "1")
            || !_stricmp(behindHeadHandSelection, "true")
            || !_stricmp(behindHeadHandSelection, "yes")
            || !_stricmp(behindHeadHandSelection, "on"));
    flags.physicalCrouch = customModEnvironmentEnabled("KHARVOX_MOD_PHYSICAL_CROUCH");
    flags.revengeDemon = customModEnvironmentEnabled("KHARVOX_MOD_REVENGE_DEMON");
    flags.dynamicShoulderHolster = customModEnvironmentEnabled("KHARVOX_MOD_DYNAMIC_SHOULDER");
    flags.physicalGrenadeThrow = customModEnvironmentEnabled("KHARVOX_MOD_PHYSICAL_GRENADE");
    flags.motionGloryKillSpeed = customModEnvironmentEnabled("KHARVOX_MOD_MOTION_GLORY_SPEED");
    flags.physicalChainsawGestures = customModEnvironmentEnabled("KHARVOX_MOD_CHAINSAW_GESTURES");
    flags.hapticOverhaul = customModEnvironmentEnabled("KHARVOX_MOD_HAPTIC_OVERHAUL");
    return flags;
}

} // namespace kharvox
