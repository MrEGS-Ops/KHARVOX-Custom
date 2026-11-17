using System;
using System.IO;

namespace KharvoxLauncher;

// Desktop Mod Manager is retired. Native in-headset settings now own all
// user-editable adjustments, while the core gameplay mods always launch.
// Keep this policy independent of any sidecar/updater installation.
internal static class RetailVrDefaults
{
    internal const bool GunslingerAlwaysOn = true;
    internal const bool WeaponGripAlwaysOn = true;

    internal static void Apply(CustomModSettings mods)
    {
        if (mods == null) throw new ArgumentNullException(nameof(mods));
        // The former standalone manager forced these five core capabilities
        // on every launch. The in-game menu provides their configuration.
        mods.HandFocusedRs = true;
        mods.BehindHeadWeaponWheel = GunslingerAlwaysOn;
        mods.LeftWristWeaponWheel = true;
        mods.BehindHeadWheelHandSelection = true;
        mods.DynamicShoulderHolster = true;
        mods.OffHandGripCapture = WeaponGripAlwaysOn;
        // Remain individually toggleable from the in-headset MODS page. The
        // native UserData/vr_live_mods_saved.cfg overrides can still set 0.
        mods.PhysicalCrouch = true;
        mods.ChainsawTriggerThrottle = true;
        mods.MotionChainsawKillSpeed = false;
        // The old standalone manager enabled KILLTIME's GK override by
        // default at 60Hz. The headset's own persisted per-action choice wins
        // inside the native runtime.
        ActionSpeedCatalog.Validate(mods);
        mods.ActionSpeedProfiles["glory"].Enabled = true;
        mods.ActionSpeedProfiles["glory"].TargetHz = 60;
    }

    internal static int RunSelfTest()
    {
        var first = new CustomModSettings
        {
            HandFocusedRs = false,
            BehindHeadWeaponWheel = false,
            LeftWristWeaponWheel = false,
            BehindHeadWheelHandSelection = false,
            DynamicShoulderHolster = false,
            OffHandGripCapture = false,
            PhysicalCrouch = false,
            ChainsawTriggerThrottle = false,
            MotionChainsawKillSpeed = true
        };
        Apply(first);
        if (!first.HandFocusedRs || !first.BehindHeadWeaponWheel ||
            !first.LeftWristWeaponWheel || !first.BehindHeadWheelHandSelection ||
            !first.DynamicShoulderHolster || !first.OffHandGripCapture ||
            !first.PhysicalCrouch || !first.ChainsawTriggerThrottle ||
            first.MotionChainsawKillSpeed ||
            !first.ActionSpeedProfiles["glory"].Enabled ||
            first.ActionSpeedProfiles["glory"].TargetHz != 60)
            throw new InvalidDataException("Retail core mods differ from the last good standalone baseline.");
        // A reload must be idempotent. No mod-wrapper/bridge file is required.
        Apply(first);
        if (first.ActionSpeedProfiles["glory"].TargetHz != 60)
            throw new InvalidDataException("Retail defaults changed on second application.");
        return 0;
    }
}
