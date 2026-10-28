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
    public bool PhysicalCrouch { get; set; }
    public bool PainsawRipshiftOverdrive { get; set; }
    public bool GloryKillSpeedOverride { get; set; }
    public int GloryKillHz { get; set; } = 200;

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
        return value;
    }

    internal void ApplyTo(CustomModSettings mods)
    {
        mods.HandFocusedRs = HandFocus;
        mods.PhysicalCrouch = PhysicalCrouch;
        mods.ChainsawTriggerThrottle = PainsawRipshiftOverdrive;
        ActionSpeedCatalog.Validate(mods);
        if (mods.ActionSpeedProfiles.TryGetValue("glory", out var profile))
        {
            profile.Enabled = GloryKillSpeedOverride;
            profile.TargetHz = GloryKillHz;
        }
    }
}
