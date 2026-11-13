using System;
using System.IO;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Opt-in bridge: only a KHARVOX build with this source reads the standalone
// controls. Stock Cactus KHARVOX is deliberately not claimed compatible.
// The path is supplied when KharvoxMods starts KHARVOX. No registry/AppData data.
internal sealed class StandaloneModsBridge
{
    public int Schema { get; set; } = 1;
    // Deprecated JSON compatibility field. Never activates a standalone mod.
    public bool WeaponWheelRemap { get; set; } = false;
    public bool HandFocus { get; set; }
    public bool Gunslinger { get; set; }
    // Kept for older sidecar JSON files; either maps to the full combo.
    public bool BehindHeadWeaponWheel { get; set; }
    public bool DynamicQuickHolsters { get; set; }
    internal bool EffectiveGunslinger =>
        Gunslinger || BehindHeadWeaponWheel || DynamicQuickHolsters;
    internal bool EffectiveWheelRemap => EffectiveGunslinger;
    public bool PhysicalCrouch { get; set; }
    public bool PainsawRipshiftOverdrive { get; set; }
    public bool OffHandGripCapture { get; set; }
    public bool GloryKillSpeedOverride { get; set; }
    public int GloryKillHz { get; set; } = 60; // Fallback when no user bridge preference exists.
    public string RuntimeRevision { get; set; } = "";
    private string _selectedBridgePath = "";

    internal static StandaloneModsBridge? LoadFromEnvironment()
    {
        var file = Environment.GetEnvironmentVariable("KHARVOX_MODS_BRIDGE_FILE");
        if (string.IsNullOrWhiteSpace(file)) return null;
        if (!Path.IsPathRooted(file) ||
            !string.Equals(Path.GetFileName(file), "kharvox-mods-bridge.json",
                StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("KHARVOX Mods bridge path is invalid.");
        if (!File.Exists(file)) return null; // no selections made
        var value = new JavaScriptSerializer().Deserialize<StandaloneModsBridge>(
            File.ReadAllText(file));
        if (value == null || value.Schema != 1)
            throw new InvalidDataException("Unknown standalone KHARVOX Mods bridge schema.");
        value.GloryKillHz = Math.Max(30, Math.Min(240, value.GloryKillHz));
        value._selectedBridgePath = file;
        return value;
    }

    internal void MarkDoomLaunched()
    {
        // Only the bridge-aware custom launcher writes this receipt, and only
        // after DOOM has started. This is strictly within the portable sidecar.
        if (string.IsNullOrWhiteSpace(_selectedBridgePath) ||
            string.IsNullOrWhiteSpace(RuntimeRevision)) return;
        try
        {
            var receipt = Path.Combine(Path.GetDirectoryName(_selectedBridgePath)!,
                "kharvox-mods-applied.txt");
            File.WriteAllText(receipt, RuntimeRevision);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }

    internal void ApplyTo(CustomModSettings mods)
    {
        mods.HandFocusedRs = HandFocus;
        // Reuse the exact verified native wheel and hip implementations.
        // Gunslinger exclusively owns the compatible merged button routing;
        // old saved wheel-remap checkboxes are ignored.
        mods.BehindHeadWeaponWheel = EffectiveGunslinger;
        mods.LeftWristWeaponWheel = EffectiveGunslinger; // existing alias
        mods.BehindHeadWheelHandSelection = EffectiveGunslinger;
        mods.DynamicShoulderHolster = EffectiveGunslinger;
        mods.PhysicalCrouch = PhysicalCrouch;
        mods.ChainsawTriggerThrottle = PainsawRipshiftOverdrive;
        mods.OffHandGripCapture = OffHandGripCapture;
        ActionSpeedCatalog.Validate(mods);
        if (mods.ActionSpeedProfiles.TryGetValue("glory", out var profile))
        {
            profile.Enabled = GloryKillSpeedOverride;
            profile.TargetHz = GloryKillHz;
        }
    }

    internal static int RunSelfTest()
    {
        var disabled = new StandaloneModsBridge();
        if (disabled.GloryKillHz != 60) return 1;
        var baseline = new CustomModSettings();
        disabled.ApplyTo(baseline);
        if (baseline.OffHandGripCapture || baseline.BehindHeadWeaponWheel || baseline.LeftWristWeaponWheel ||
            baseline.BehindHeadWheelHandSelection || baseline.DynamicShoulderHolster)
            return 1;
        var enabled = new StandaloneModsBridge
        {
            Gunslinger = true,
            WeaponWheelRemap = false,
            OffHandGripCapture = true
        };
        if (!enabled.EffectiveGunslinger || !enabled.EffectiveWheelRemap)
            return 1;
        enabled.ApplyTo(baseline);
        if (!baseline.OffHandGripCapture || !baseline.BehindHeadWeaponWheel || !baseline.LeftWristWeaponWheel ||
            !baseline.BehindHeadWheelHandSelection || !baseline.DynamicShoulderHolster)
            return 1;
        disabled.ApplyTo(baseline);
        if (baseline.OffHandGripCapture || baseline.BehindHeadWeaponWheel || baseline.LeftWristWeaponWheel ||
            baseline.BehindHeadWheelHandSelection || baseline.DynamicShoulderHolster)
            return 1;
        // Existing JSON from the two separate mod switches also migrates.
        var legacy = new StandaloneModsBridge
        {
            BehindHeadWeaponWheel = true, DynamicQuickHolsters = true
        };
        legacy.ApplyTo(baseline);
        return baseline.BehindHeadWeaponWheel && baseline.LeftWristWeaponWheel &&
            baseline.BehindHeadWheelHandSelection && baseline.DynamicShoulderHolster
            ? 0 : 1;
    }
}
