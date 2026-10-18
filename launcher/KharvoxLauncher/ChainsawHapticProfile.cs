using System;
using System.IO;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

/// <summary>
/// Temporary development calibration data. This file is deliberately isolated so
/// the Haptic Lab can be removed after Quest 3 tuning without changing game settings.
/// Values are normalized targets, not promises of device-supported pulse frequency.
/// </summary>
internal sealed class ChainsawHapticProfile
{
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
            case "Diesel Idle":
                p = new ChainsawHapticProfile { Name = name, IdlePercent = 35, CuttingPercent = 65, PulseRate = 25, PulseDurationMs = 35, RoughnessPercent = 10, MotionSensitivityPercent = 55 }; break;
            case "Demon Shredder":
                p = new ChainsawHapticProfile { Name = name, IdlePercent = 15, CuttingPercent = 100, PulseRate = 65, PulseDurationMs = 15, RoughnessPercent = 75, MotionSensitivityPercent = 95 }; break;
            case "Jackhammer":
                p = new ChainsawHapticProfile { Name = name, IdlePercent = 10, CuttingPercent = 95, PulseRate = 35, PulseDurationMs = 25, RoughnessPercent = 55, MotionSensitivityPercent = 90 }; break;
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

    public void Save(string path)
    {
        Validate();
        var directory = Path.GetDirectoryName(Path.GetFullPath(path));
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
        File.WriteAllText(path, new JavaScriptSerializer().Serialize(this));
    }
}
