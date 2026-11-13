using System;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace KharvoxMods;

// One portable local file. A bridge-aware KHARVOX launcher reads this file
// whenever DOOM starts, but the Mods EXE NEVER edits the original installation.
internal sealed class StandaloneModPreferences
{
    private const string PreviousGloryTitle = "Glory Kill Speed Override";
    private const string PreviousRipTearTitle = "RIP & TEAR: KILLTIME";
    private const string KilltimeTitle = "KILLTIME: RIP & TEAR";
    private const string GunslingerTitle = "Gunslinger";
    internal const string WeaponGripCalibrationTitle = "Weapon Grip Calibration";
    private const string PreviousGripTitle = "Off-Hand Grip Capture (Test)";
    private static string UpgradeModTitle(string title)
    {
        if (string.Equals(title, PreviousGloryTitle, StringComparison.Ordinal) ||
            string.Equals(title, PreviousRipTearTitle, StringComparison.Ordinal))
            return KilltimeTitle;
        if (title == "Behind-the-Head Weapon Wheel" ||
            title == "Dynamic Quick Holsters")
            return GunslingerTitle;
        if (title == PreviousGripTitle) return WeaponGripCalibrationTitle;
        return title;
    }

    public int Schema { get; set; } = 1;
    // Deprecated schema field retained only to read old bridge JSON.
    // It is always FALSE and cannot activate a standalone native remap.
    public bool WeaponWheelRemap { get; set; } = false;
    public bool HandFocus { get; set; } = true;
    public bool Gunslinger { get; set; } = true;
    // Read old bridge files; these flags are never written as active mods again.
    public bool BehindHeadWeaponWheel { get; set; }
    public bool DynamicQuickHolsters { get; set; }
    public bool PhysicalCrouch { get; set; } = true;
    public bool PainsawRipshiftOverdrive { get; set; }
    public bool OffHandGripCapture { get; set; } = true;
    public bool GloryKillSpeedOverride { get; set; } = true;
    public int GloryKillHz { get; set; } = 60; // New installs default KILLTIME to 60 Hz.
    // Presentation order only: the bridge ignores this metadata. Enables
    // original click/drag reorder without changing mod dependencies or timing.
    public string[] ActiveOrder { get; set; } = Array.Empty<string>();
    // UI-only order for inactive and paused mods; does not enable anything.
    public string[] AvailableOrder { get; set; } = Array.Empty<string>();
    // UI-only; keys are A:category or V:category. No native dependency impact.
    // First run: available categories collapsed; selected Active Mods stays open.
    // A saved preference array (even empty) always overrides this default.
    public string[] CollapsedCategories { get; set; } =
        ModsForm.DefaultCollapsedAvailableCategories();
    // A fresh revision identifies saved runtime changes until a verified DOOM
    // start writes a corresponding receipt. Never infer applied from a Save.
    public string RuntimeRevision { get; set; } = "";
    internal static string AppliedReceiptPath =>
        PortableUserData.PathFor("kharvox-mods-applied.txt");

    internal static string DefaultPath =>
        PortableUserData.PathFor("kharvox-mods-bridge.json");

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
            // Migrate either old physical mod to the new all-in-one switch.
            if (value.BehindHeadWeaponWheel || value.DynamicQuickHolsters)
                value.Gunslinger = true;
            value.BehindHeadWeaponWheel = false;
            value.DynamicQuickHolsters = false;
            value.WeaponWheelRemap = false; // discard any legacy on-disk selection
            value.HandFocus = true;
            // Locked core features cannot be disabled by legacy saved OFF flags.
            value.Gunslinger = true;
            value.OffHandGripCapture = true;
            value.GloryKillSpeedOverride = true;
            // Physical Crouch and PAINSAW remain user-toggleable.
            value.ActiveOrder = Array.ConvertAll(value.ActiveOrder ??
                Array.Empty<string>(), UpgradeModTitle)
                .Where(title => title != "Weapon Wheel Button Remap")
                .Distinct(StringComparer.Ordinal).ToArray();
            value.AvailableOrder = Array.ConvertAll(value.AvailableOrder ??
                Array.Empty<string>(), UpgradeModTitle)
                .Where(title => title != "Weapon Wheel Button Remap")
                .Distinct(StringComparer.Ordinal).ToArray();
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
        HandFocus = true;
        Gunslinger = true;
        OffHandGripCapture = true;
        GloryKillSpeedOverride = true;
        // Preserve the user's Physical Crouch and PAINSAW choices.
        // Standalone wheel remap was retired; Gunslinger alone owns the wheel.
        WeaponWheelRemap = false;
        BehindHeadWeaponWheel = false;
        DynamicQuickHolsters = false;
        ActiveOrder = Array.ConvertAll(ActiveOrder ??
            Array.Empty<string>(), UpgradeModTitle)
            .Where(title => title != "Weapon Wheel Button Remap")
            .Distinct(StringComparer.Ordinal).ToArray();
        AvailableOrder = Array.ConvertAll(AvailableOrder ??
            Array.Empty<string>(), UpgradeModTitle)
            .Where(title => title != "Weapon Wheel Button Remap")
            .Distinct(StringComparer.Ordinal).ToArray();
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
            if (x.WeaponWheelRemap || !x.HandFocus ||
                !x.Gunslinger || x.BehindHeadWeaponWheel || x.DynamicQuickHolsters ||
                !x.PhysicalCrouch || x.PainsawRipshiftOverdrive || !x.OffHandGripCapture ||
                !x.GloryKillSpeedOverride || x.GloryKillHz != 60 ||
                x.CollapsedCategories.Length !=
                    ModsForm.DefaultCollapsedAvailableCategories().Length ||
                x.CollapsedCategories.Any(category => !category.StartsWith("V:")) )
                throw new InvalidDataException("Unsafe default mod choices.");
            x.HandFocus = true;
            x.Gunslinger = true;
            x.WeaponWheelRemap = false;
            x.PainsawRipshiftOverdrive = true;
            x.OffHandGripCapture = true;
            x.GloryKillSpeedOverride = true;
            x.GloryKillHz = 35;
            // Legacy mod names should retain their saved positions.
            x.ActiveOrder = new[] { "Hand Focus", PreviousGloryTitle,
                PreviousGripTitle, "PAINSAW: RIPSHIFT OVERDRIVE" };
            x.AvailableOrder = new[] { "Directional Dash", PreviousRipTearTitle,
                "Boxing Guard Mode" };
            x.CollapsedCategories = new[] { "A:✥  Hands & Arms" };
            x.RuntimeRevision = "00000000000000000000000000000001";
            x.Save(path);
            var y = Load(path);
            if (!y.HandFocus || !y.Gunslinger || y.WeaponWheelRemap ||
                y.BehindHeadWeaponWheel || y.DynamicQuickHolsters ||
                !y.PainsawRipshiftOverdrive || !y.OffHandGripCapture ||
                !y.GloryKillSpeedOverride || y.GloryKillHz != 35 ||
                y.ActiveOrder.Length != 4 ||
                y.ActiveOrder[0] != "Hand Focus" ||
                y.ActiveOrder[1] != KilltimeTitle ||
                y.ActiveOrder[2] != WeaponGripCalibrationTitle ||
                y.ActiveOrder[3] != "PAINSAW: RIPSHIFT OVERDRIVE" ||
                y.AvailableOrder.Length != 3 ||
                y.AvailableOrder[0] != "Directional Dash" ||
                y.AvailableOrder[1] != KilltimeTitle ||
                y.CollapsedCategories.Length != 1 ||
                y.RuntimeRevision != "00000000000000000000000000000001")
                throw new InvalidDataException("Portable bridge setting roundtrip failed.");
            y.Gunslinger = false;
            y.OffHandGripCapture = false;
            y.GloryKillSpeedOverride = false;
            y.PhysicalCrouch = false;
            y.Save(path);
            var disabled = Load(path);
            if (!disabled.Gunslinger || !disabled.OffHandGripCapture ||
                !disabled.GloryKillSpeedOverride || disabled.PhysicalCrouch ||
                disabled.BehindHeadWeaponWheel || disabled.DynamicQuickHolsters)
                throw new InvalidDataException("Core mods must remain ON; Physical Crouch OFF must persist.");
            // Old sidecar state merges both legacy rows into one, disabling
            // standalone remap without disturbing the stored ordering.
            File.WriteAllText(path, new JavaScriptSerializer().Serialize(new
            {
                Schema = 1, WeaponWheelRemap = true,
                BehindHeadWeaponWheel = true, DynamicQuickHolsters = true,
                ActiveOrder = new[] { "Behind-the-Head Weapon Wheel",
                    "Dynamic Quick Holsters" },
                AvailableOrder = new[] { "Weapon Wheel Button Remap" }
            }));
            var legacy = Load(path);
            if (!legacy.Gunslinger || legacy.WeaponWheelRemap ||
                legacy.BehindHeadWeaponWheel || legacy.DynamicQuickHolsters ||
                legacy.ActiveOrder.Length != 1 ||
                legacy.ActiveOrder[0] != GunslingerTitle ||
                legacy.AvailableOrder.Length != 0)
                throw new InvalidDataException("Legacy physical-mod migration failed.");
        }
        finally { if (Directory.Exists(dir)) Directory.Delete(dir, true); }
    }
}
