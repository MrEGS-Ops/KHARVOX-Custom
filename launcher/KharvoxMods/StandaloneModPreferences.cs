using System;
using System.IO;
using System.Web.Script.Serialization;

namespace KharvoxMods;

// One portable local file. A bridge-aware KHARVOX launcher reads this file
// whenever DOOM starts, but the Mods EXE NEVER edits the original installation.
internal sealed class StandaloneModPreferences
{
    public int Schema { get; set; } = 1;
    public bool WeaponWheelRemap { get; set; } = true;
    public bool HandFocus { get; set; }
    public bool PhysicalCrouch { get; set; }
    public bool PainsawRipshiftOverdrive { get; set; }
    public bool GloryKillSpeedOverride { get; set; }
    public int GloryKillHz { get; set; } = 200;
    // Presentation order only: the bridge ignores this metadata. Enables
    // original click/drag reorder without changing mod dependencies or timing.
    public string[] ActiveOrder { get; set; } = Array.Empty<string>();
    // UI-only order for inactive and paused mods; does not enable anything.
    public string[] AvailableOrder { get; set; } = Array.Empty<string>();
    // UI-only; keys are A:category or V:category. No native dependency impact.
    public string[] CollapsedCategories { get; set; } = Array.Empty<string>();
    // A fresh revision identifies saved runtime changes until a verified DOOM
    // start writes a corresponding receipt. Never infer applied from a Save.
    public string RuntimeRevision { get; set; } = "";
    internal static string AppliedReceiptPath =>
        Path.Combine(AppContext.BaseDirectory, "kharvox-mods-applied.txt");

    internal static string DefaultPath =>
        Path.Combine(AppContext.BaseDirectory, "kharvox-mods-bridge.json");

    internal static StandaloneModPreferences Load(string? path = null)
    {
        path ??= DefaultPath;
        if (!File.Exists(path)) return new StandaloneModPreferences();
        try
        {
            var value = new JavaScriptSerializer().Deserialize<StandaloneModPreferences>(
                File.ReadAllText(path));
            if (value == null || value.Schema != 1)
                throw new InvalidDataException("Unknown KHARVOX Mods bridge schema.");
            value.GloryKillHz = Math.Max(30, Math.Min(240, value.GloryKillHz));
            value.ActiveOrder ??= Array.Empty<string>();
            value.AvailableOrder ??= Array.Empty<string>();
            value.CollapsedCategories ??= Array.Empty<string>();
            value.RuntimeRevision ??= "";
            return value;
        }
        catch (Exception ex) when (ex is IOException || ex is InvalidOperationException ||
            ex is ArgumentException || ex is InvalidDataException)
        {
            throw new InvalidDataException(
                "Local KHARVOX Mods selections could not be read: " + path, ex);
        }
    }

    internal void Save(string? path = null)
    {
        path ??= DefaultPath;
        GloryKillHz = Math.Max(30, Math.Min(240, GloryKillHz));
        ActiveOrder ??= Array.Empty<string>();
        AvailableOrder ??= Array.Empty<string>();
        CollapsedCategories ??= Array.Empty<string>();
        RuntimeRevision ??= "";
        var directory = Path.GetDirectoryName(Path.GetFullPath(path))!;
        Directory.CreateDirectory(directory);
        var tmp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(tmp, new JavaScriptSerializer().Serialize(this));
            if (File.Exists(path)) File.Replace(tmp, path, null);
            else File.Move(tmp, path);
        }
        finally { if (File.Exists(tmp)) File.Delete(tmp); }
    }

    internal static void SelfTest()
    {
        var dir = Path.Combine(Path.GetTempPath(), "kharvox-modprefs-" +
            Guid.NewGuid().ToString("N"));
        try
        {
            var path = Path.Combine(dir, "mods.json");
            var x = Load(path);
            if (!x.WeaponWheelRemap || x.HandFocus ||
                x.PhysicalCrouch || x.PainsawRipshiftOverdrive ||
                x.GloryKillSpeedOverride)
                throw new InvalidDataException("Unsafe default mod choices.");
            x.HandFocus = true;
            x.PainsawRipshiftOverdrive = true;
            x.GloryKillSpeedOverride = true;
            x.GloryKillHz = 35;
            x.ActiveOrder = new[] { "Hand Focus", "PAINSAW: RIPSHIFT OVERDRIVE" };
            x.AvailableOrder = new[] { "Directional Dash", "Boxing Guard Mode" };
            x.CollapsedCategories = new[] { "A:✥  Hands Arms" };
            x.RuntimeRevision = "00000000000000000000000000000001";
            x.Save(path);
            var y = Load(path);
            if (!y.HandFocus || !y.PainsawRipshiftOverdrive ||
                !y.GloryKillSpeedOverride || y.GloryKillHz != 35 ||
                y.ActiveOrder.Length != 2 ||
                y.ActiveOrder[0] != "Hand Focus" ||
                y.ActiveOrder[1] != "PAINSAW: RIPSHIFT OVERDRIVE" ||
                y.AvailableOrder.Length != 2 ||
                y.AvailableOrder[0] != "Directional Dash" ||
                y.CollapsedCategories.Length != 1 ||
                y.RuntimeRevision != "00000000000000000000000000000001")
                throw new InvalidDataException("Portable bridge setting roundtrip failed.");
        }
        finally { if (Directory.Exists(dir)) Directory.Delete(dir, true); }
    }
}
