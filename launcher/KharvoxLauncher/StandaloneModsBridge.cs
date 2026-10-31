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
    public bool WeaponWheelRemap { get; set; } = true;
    public bool HandFocus { get; set; }
    public bool BehindHeadWeaponWheel { get; set; }
    public bool DynamicQuickHolsters { get; set; }
    public bool PhysicalCrouch { get; set; }
    public bool PainsawRipshiftOverdrive { get; set; }
    public bool GloryKillSpeedOverride { get; set; }
    public int GloryKillHz { get; set; } = 200;
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
        mods.BehindHeadWeaponWheel = BehindHeadWeaponWheel;
        mods.LeftWristWeaponWheel = BehindHeadWeaponWheel; // existing alias
        mods.BehindHeadWheelHandSelection = BehindHeadWeaponWheel;
        mods.DynamicShoulderHolster = DynamicQuickHolsters;
        mods.PhysicalCrouch = PhysicalCrouch;
        mods.ChainsawTriggerThrottle = PainsawRipshiftOverdrive;
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
        var baseline = new CustomModSettings();
        disabled.ApplyTo(baseline);
        if (baseline.BehindHeadWeaponWheel || baseline.LeftWristWeaponWheel ||
            baseline.BehindHeadWheelHandSelection || baseline.DynamicShoulderHolster)
            return 1;
        var enabled = new StandaloneModsBridge
        {
            BehindHeadWeaponWheel = true,
            DynamicQuickHolsters = true
        };
        enabled.ApplyTo(baseline);
        if (!baseline.BehindHeadWeaponWheel || !baseline.LeftWristWeaponWheel ||
            !baseline.BehindHeadWheelHandSelection || !baseline.DynamicShoulderHolster)
            return 1;
        disabled.ApplyTo(baseline);
        return baseline.BehindHeadWeaponWheel || baseline.LeftWristWeaponWheel ||
            baseline.BehindHeadWheelHandSelection || baseline.DynamicShoulderHolster ? 1 : 0;
    }
}
