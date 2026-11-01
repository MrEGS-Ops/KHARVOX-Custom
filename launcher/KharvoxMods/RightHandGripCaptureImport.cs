using System.Globalization;

namespace KharvoxMods;

// One-time migration of the owner's most recent Gauss/Chaingun RIGHT-hand
// model positions. This never touches normal left-hand support profiles or
// weapon aiming. User changes after migration are respected forever.
internal static class RightHandGripCaptureImport
{
    internal const string SourceName = "offhand_right_hand_positions_latest.cfg";
    internal const string TargetName = "offhand_right_hand_positions.cfg";
    private const string MarkerName = "right-hand-grips-20261101.applied";
    private static readonly string[] Keys = { "gauss_cannon", "chaingun" };

    private static string? ReadKey(string line)
    {
        var value = line.TrimStart();
        if (value.Length == 0 || value[0] == '#') return null;
        var equal = value.IndexOf('=');
        return equal < 0 ? null : value.Substring(0, equal).Trim();
    }

    private static string[] ReadAndValidate(string path)
    {
        if (new FileInfo(path).Length > 4096)
            throw new InvalidDataException("Owner right-hand measurement file exceeds safe size.");
        var lines = File.ReadAllLines(path);
        var found = new HashSet<string>(StringComparer.Ordinal);
        foreach (var line in lines)
        {
            var key = ReadKey(line);
            if (key == null) continue;
            if (key == "right_hand_drop_m")
            {
                var raw = line.Substring(line.IndexOf('=') + 1).Trim();
                if (!float.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture, out var drop)
                    || float.IsNaN(drop) || float.IsInfinity(drop) || drop < 0 || drop > 0.20f)
                    throw new InvalidDataException("Invalid right-hand drop calibration.");
                continue;
            }
            if (!Keys.Contains(key, StringComparer.Ordinal) || !found.Add(key))
                throw new InvalidDataException("Unexpected or duplicate right-hand measurement.");
            var columns = line.Substring(line.IndexOf('=') + 1)
                .Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (columns.Length != 3)
                throw new InvalidDataException("Right-hand measurement must contain three coordinates.");
            var xyz = new float[3];
            for (var i = 0; i < 3; i++)
            {
                if (!float.TryParse(columns[i], NumberStyles.Float,
                        CultureInfo.InvariantCulture, out xyz[i]) ||
                    float.IsNaN(xyz[i]) || float.IsInfinity(xyz[i]))
                    throw new InvalidDataException("Right-hand measurement is not a finite number.");
            }
            if (Math.Sqrt(xyz.Sum(x => (double)x * x)) > 0.85)
                throw new InvalidDataException("Right-hand measurement exceeds 85 cm safe limit.");
        }
        if (found.Count != Keys.Length)
            throw new InvalidDataException("Gauss and Chaingun measurement entries are required.");
        return lines;
    }

    internal static bool ApplyOnce(string nativeDirectory, string backupsDirectory)
    {
        var source = Path.Combine(nativeDirectory, SourceName);
        if (!File.Exists(source)) return false; // old but valid bridge
        var markerDir = Path.Combine(backupsDirectory, "GripProfiles");
        var marker = Path.Combine(markerDir, MarkerName);
        if (File.Exists(marker)) return false;
        var measurements = ReadAndValidate(source);
        var target = Path.Combine(nativeDirectory, TargetName);
        var previous = File.Exists(target) ? File.ReadAllLines(target) : Array.Empty<string>();
        if (File.Exists(target) && new FileInfo(target).Length > 16384)
            throw new InvalidDataException("Existing right-hand grip settings exceed safe size.");

        var kept = previous.Where(x => !Keys.Contains(ReadKey(x), StringComparer.Ordinal))
            .ToList();
        // Use the shipped drop only on new configurations. Never overwrite a
        // hand height the user has subsequently adjusted.
        if (!kept.Any(x => ReadKey(x) == "right_hand_drop_m"))
            kept.Add(measurements.Single(x => ReadKey(x) == "right_hand_drop_m"));
        foreach (var key in Keys)
            kept.Add(measurements.Single(x => ReadKey(x) == key));

        Directory.CreateDirectory(markerDir);
        var snapshot = Path.Combine(markerDir, "before-right-hand-grips-" +
            DateTime.UtcNow.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) +
            "-" + Guid.NewGuid().ToString("N").Substring(0, 8) + ".cfg");
        if (File.Exists(target)) File.Copy(target, snapshot, false);
        var temp = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllLines(temp, kept);
            if (File.Exists(target)) File.Replace(temp, target, null);
            else File.Move(temp, target);
            File.WriteAllText(marker,
                "Owner's November 1 Gauss and Chaingun right-hand placements imported once.\r\n" +
                "Earlier profile: " + (File.Exists(snapshot) ? snapshot : "(none)") + "\r\n");
            return true;
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }

    internal static void SelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "kharvox-righthand-grip-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            var native = Path.Combine(root, "NativeBridge");
            var backups = Path.Combine(root, "Backups");
            Directory.CreateDirectory(native);
            File.WriteAllText(Path.Combine(native, SourceName),
                "right_hand_drop_m = 0.08\ngauss_cannon = -0.09830919 0.17283046 0.52416211\n" +
                "chaingun = -0.11940779 0.27708763 0.67574835\n");
            var target = Path.Combine(native, TargetName);
            var old = "# preserve comments\nright_hand_drop_m = 0.11\n" +
                "gauss_cannon = 0 0 0\nchaingun = 0 0 0\nother_weapon = keep\n";
            File.WriteAllText(target, old);
            if (!ApplyOnce(native, backups))
                throw new InvalidDataException("Right-hand placement not imported.");
            var applied = File.ReadAllText(target);
            if (!applied.Contains("gauss_cannon = -0.09830919") ||
                !applied.Contains("chaingun = -0.11940779") ||
                !applied.Contains("right_hand_drop_m = 0.11") ||
                !applied.Contains("other_weapon = keep") ||
                !Directory.GetFiles(Path.Combine(backups, "GripProfiles"),
                    "before-right-hand-grips-*.cfg").Any(p => File.ReadAllText(p) == old))
                throw new InvalidDataException("Right-hand import overwrote user settings/backup.");
            File.AppendAllText(target, "# manual edit after migration\n");
            if (ApplyOnce(native, backups) ||
                !File.ReadAllText(target).Contains("# manual edit after migration"))
                throw new InvalidDataException("Right-hand positions were re-imported after user edit.");
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }
}
