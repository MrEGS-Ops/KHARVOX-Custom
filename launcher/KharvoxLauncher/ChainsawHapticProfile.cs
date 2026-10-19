using System;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

/// <summary>
/// Temporary development calibration data. This file is deliberately isolated so
/// the Haptic Lab can be removed after Quest 3 tuning without changing game settings.
/// Values are normalized targets, not promises of device-supported pulse frequency.
/// </summary>
internal sealed class ChainsawHapticProfile
{
    internal static string DefaultPath => Path.Combine(AppContext.BaseDirectory, "kharvox-chainsaw-haptics.json");

    public int SchemaVersion { get; set; } = 1;
    public string Name { get; set; } = "Heavy Chainsaw";
    public int IdlePercent { get; set; } = 20;
    public int CuttingPercent { get; set; } = 90;
    public int PulseRate { get; set; } = 45;
    public int PulseDurationMs { get; set; } = 20;
    public int RoughnessPercent { get; set; } = 35;
    public int MotionSensitivityPercent { get; set; } = 75;
    public int MotionSmoothingMs { get; set; } = 50;
    public int ReleaseMs { get; set; } = 70;

    // The only five supported UI choices. Legacy names remain loadable
    // without losing an old saved profile; the user explicitly selects
    // one of these new choices to replace it.
    internal static readonly string[] RecommendedPresets =
    {
        "Heavy Chainsaw", "Deep Rumble", "Jackhammer", "Ripping Teeth", "Brutal Chop"
    };

    internal static string ExplainPreset(string name) => name switch
    {
        "Heavy Chainsaw" =>
            "Baseline · Balanced continuous engine rumble with a light gritty edge. "
            + "Useful for comparing whether the new presets actually feel stronger.",
        "Deep Rumble" =>
            "Low, weighty vibration with longer beats and a stronger engine idle. "
            + "Prioritises the sensation of weight over sharp teeth.",
        "Jackhammer" =>
            "Punchy, interrupted impacts rather than a smooth buzz. "
            + "Best for hard contact and repeated hits.",
        "Ripping Teeth" =>
            "Rapid, serrated tearing texture: short sharp pulses, "
            + "high cutting intensity, little idle vibration.",
        "Brutal Chop" =>
            "Maximum cutting intensity and roughness with slow, chunky pulses. "
            + "Recommended first test for aggressive chopping feedback.",
        _ => "Choose one of the five recommended haptic presets."
    };

    public void Validate()
    {
        if (SchemaVersion != 1) throw new InvalidDataException("Unsupported haptic profile schema.");
        Name = string.IsNullOrWhiteSpace(Name) ? "Custom" : Name.Trim();
        if (Name.Length > 80) Name = Name.Substring(0, 80);
        IdlePercent = Clamp(IdlePercent, 0, 100);
        CuttingPercent = Clamp(CuttingPercent, 0, 100);
        PulseRate = Clamp(PulseRate, 5, 100);
        PulseDurationMs = Clamp(PulseDurationMs, 5, 100);
        RoughnessPercent = Clamp(RoughnessPercent, 0, 100);
        MotionSensitivityPercent = Clamp(MotionSensitivityPercent, 0, 100);
        MotionSmoothingMs = Clamp(MotionSmoothingMs, 0, 500);
        ReleaseMs = Clamp(ReleaseMs, 0, 500);
    }

    private static int Clamp(int value, int min, int max) => Math.Max(min, Math.Min(max, value));

    public static ChainsawHapticProfile Preset(string name)
    {
        ChainsawHapticProfile p;
        switch (name)
        {
            case "Deep Rumble":
                p = new ChainsawHapticProfile
                {
                    Name = name, IdlePercent = 40, CuttingPercent = 82,
                    PulseRate = 18, PulseDurationMs = 45,
                    RoughnessPercent = 25, MotionSensitivityPercent = 65
                }; break;
            case "Jackhammer":
                p = new ChainsawHapticProfile
                {
                    Name = name, IdlePercent = 15, CuttingPercent = 97,
                    PulseRate = 38, PulseDurationMs = 20,
                    RoughnessPercent = 80, MotionSensitivityPercent = 90
                }; break;
            case "Ripping Teeth":
                p = new ChainsawHapticProfile
                {
                    Name = name, IdlePercent = 16, CuttingPercent = 100,
                    PulseRate = 65, PulseDurationMs = 10,
                    RoughnessPercent = 90, MotionSensitivityPercent = 95
                }; break;
            case "Brutal Chop":
                p = new ChainsawHapticProfile
                {
                    Name = name, IdlePercent = 28, CuttingPercent = 100,
                    PulseRate = 12, PulseDurationMs = 32,
                    RoughnessPercent = 100, MotionSensitivityPercent = 100,
                    MotionSmoothingMs = 18, ReleaseMs = 42
                }; break;
            // Original stock behaviour is kept intact as an A/B control.
            case "Heavy Chainsaw":
            default:
                p = new ChainsawHapticProfile(); break;
        }
        p.Validate();
        return p;
    }

    public static ChainsawHapticProfile Load(string path)
    {
        var p = new JavaScriptSerializer().Deserialize<ChainsawHapticProfile>(File.ReadAllText(path));
        if (p == null) throw new InvalidDataException("Empty haptic profile.");
        p.Validate();
        return p;
    }

    public static ChainsawHapticProfile LoadOrDefault(string? path = null)
    {
        path ??= DefaultPath;
        return File.Exists(path) ? Load(path) : new ChainsawHapticProfile();
    }

    // Atomic profile updates: closing/cancelling the Lab never writes anything,
    // and a write failure cannot destroy the previous working calibration.
    public void Save(string path)
    {
        Validate();
        var directory = Path.GetDirectoryName(Path.GetFullPath(path));
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
        var temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temporary, new JavaScriptSerializer().Serialize(this));
            if (File.Exists(path)) File.Replace(temporary, path, null);
            else File.Move(temporary, path);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    internal static void RunHapticProfileSelfTest(string directory)
    {
        var path = Path.Combine(directory, "test-chainsaw-haptics.json");
        if (LoadOrDefault(path).IdlePercent != 20)
            throw new InvalidDataException("Haptic Lab default changed.");
        var chosen = Preset("Jackhammer");
        chosen.IdlePercent = 22;
        chosen.RoughnessPercent = 83;
        chosen.MotionSensitivityPercent = 94;
        chosen.Save(path);
        var restored = Load(path);
        if (restored.Name != "Jackhammer" || restored.IdlePercent != 22
            || restored.RoughnessPercent != 83 || restored.MotionSensitivityPercent != 94)
            throw new InvalidDataException("Haptic Lab failed to persist calibration.");
        foreach (var preset in RecommendedPresets)
            if (Preset(preset).IdlePercent < 0 || Preset(preset).CuttingPercent > 100)
                throw new InvalidDataException("Haptic Lab preset out of bounds.");
        if(RecommendedPresets.Length!=5
            || RecommendedPresets.Distinct().Count()!=5)
            throw new InvalidDataException("Haptic preset dropdown must contain exactly five distinct profiles.");
        var distinctValues = RecommendedPresets.Select(name => Preset(name))
            .Select(p => p.IdlePercent + "/" + p.CuttingPercent + "/" + p.PulseRate
                + "/" + p.PulseDurationMs + "/" + p.RoughnessPercent + "/" + p.MotionSensitivityPercent)
            .Distinct().Count();
        if(distinctValues!=5)
            throw new InvalidDataException("All five haptic presets must differ.");
        foreach(var name in RecommendedPresets)
        {
            if(string.IsNullOrWhiteSpace(ExplainPreset(name)))
                throw new InvalidDataException("Haptic preset description missing.");
            var selected=Preset(name);
            selected.Save(path);
            var stored=Load(path);
            if(stored.Name!=selected.Name || stored.IdlePercent!=selected.IdlePercent
                || stored.CuttingPercent!=selected.CuttingPercent || stored.PulseRate!=selected.PulseRate
                || stored.PulseDurationMs!=selected.PulseDurationMs
                || stored.RoughnessPercent!=selected.RoughnessPercent
                || stored.MotionSensitivityPercent!=selected.MotionSensitivityPercent)
                throw new InvalidDataException("Haptic preset did not survive a save/load cycle.");
        }
        var brutal=Preset("Brutal Chop");
        if(brutal.CuttingPercent!=100||brutal.RoughnessPercent!=100
            ||brutal.PulseRate!=12)
            throw new InvalidDataException("Brutal Chop haptic preset changed.");
        var legacy = new ChainsawHapticProfile { PulseRate = 200, PulseDurationMs = -5 };
        legacy.Validate();
        if (legacy.PulseRate != 100 || legacy.PulseDurationMs != 5)
            throw new InvalidDataException("Haptic Lab failed to clamp imported calibration.");
    }
}
