using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace KharvoxMods;

// One-time, reversible import of eight owner-measured off-hand grip locations.
// This edits ONLY the local NativeBridge saved two-hand profiles (never the
// original KHARVOX installation). A copy is retained in local Backups.
internal static class CapturedGripProfiles
{
    internal const string SourceName = "two_hand_weapon_profiles_captured.cfg";
    private const string TargetName = "two_hand_weapon_profiles_saved.cfg";
    private const string AppliedMarker = "captured-grips-v1.applied";
    private static readonly string[] ExpectedKeys =
    {
        "shotgun", "super_shotgun", "heavy_assault_rifle", "chaingun",
        "plasma_rifle", "gauss_cannon", "rocket_launcher", "pistol"
    };

    private static string[] ParseCaptures(string path)
    {
        var info = new FileInfo(path);
        if (info.Length <= 0 || info.Length > 16384)
            throw new InvalidDataException("Captured support-grip profile file has an invalid size.");
        var rows = File.ReadAllLines(path).Select(s => s.Trim())
            .Where(s => s.Length > 0).ToArray();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var row in rows)
        {
            var columns = row.Split((char[]?)null,
                StringSplitOptions.RemoveEmptyEntries);
            if (columns.Length != 8 || columns[0] != "2" ||
                !ExpectedKeys.Contains(columns[1], StringComparer.Ordinal) ||
                !seen.Add(columns[1]))
                throw new InvalidDataException("Captured support-grip record is missing, repeated, or unsupported.");
            var expectedMode = columns[1] == "chaingun" || columns[1] == "gauss_cannon"
                ? "side-grip" : "barrel";
            if (columns[2] != expectedMode)
                throw new InvalidDataException("Captured support-grip mode does not match the weapon.");
            for (int i = 3; i < 8; ++i)
            {
                if (!float.TryParse(columns[i], NumberStyles.Float,
                    CultureInfo.InvariantCulture, out var v) ||
                    float.IsNaN(v) || float.IsInfinity(v) ||
                    (i <= 5 && Math.Abs(v) > 0.75f) ||
                    (i == 6 && (v < 0.08f || v > 0.4f)) ||
                    (i == 7 && (v < 0f || v > 1f)))
                    throw new InvalidDataException("Captured support-grip measurement is invalid.");
            }
        }
        if (rows.Length != ExpectedKeys.Length)
            throw new InvalidDataException("Exactly eight captured weapon profiles are required.");
        return rows;
    }

    internal static bool ApplyOnce(string nativeDir, string backupsDir)
    {
        var source = Path.Combine(nativeDir, SourceName);
        if (!File.Exists(source)) return false; // Previous stable bridge.
        var markerDir = Path.Combine(backupsDir, "GripProfiles");
        var marker = Path.Combine(markerDir, AppliedMarker);
        if (File.Exists(marker)) return false; // Future adjustments stay intact.
        var captures = ParseCaptures(source); // Validate before changing anything.
        var target = Path.Combine(nativeDir, TargetName);
        var previous = File.Exists(target) ? File.ReadAllLines(target) :
            Array.Empty<string>();
        if (File.Exists(target) && new FileInfo(target).Length > 65536)
            throw new InvalidDataException("Existing two-hand profiles exceed safe import size.");

        // Only replace the eight measured weapon rows; BFG, Chainsaw and any
        // other user-saved lines remain unchanged.
        var kept = previous.Where(row =>
        {
            var parts = row.Trim().Split((char[]?)null,
                StringSplitOptions.RemoveEmptyEntries);
            return parts.Length < 2 || parts[0] != "2" ||
                !ExpectedKeys.Contains(parts[1], StringComparer.Ordinal);
        }).ToList();
        kept.AddRange(captures);
        Directory.CreateDirectory(markerDir);
        var snapshot = Path.Combine(markerDir,
            "before-captured-grips-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss",
                CultureInfo.InvariantCulture) + "-" +
            Guid.NewGuid().ToString("N").Substring(0, 8) + ".cfg");
        if (File.Exists(target)) File.Copy(target, snapshot, false);
        var temp = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllLines(temp, kept);
            if (File.Exists(target)) File.Replace(temp, target, null);
            else File.Move(temp, target);
            File.WriteAllText(marker,
                "Applied eight experimental off-hand grip positions once.\r\n" +
                "NativeBridge profile: " + TargetName + "\r\n" +
                "Original saved profile backup: " +
                (File.Exists(snapshot) ? snapshot : "(no previous file)") + "\r\n");
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
        return true;
    }

    internal static void SelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "kharvox-captured-grips-" + Guid.NewGuid().ToString("N"));
        try
        {
            var native = Path.Combine(root, "NativeBridge");
            var backups = Path.Combine(root, "Backups");
            Directory.CreateDirectory(native);
            var source = Path.Combine(native, SourceName);
            const string example =
                "2 shotgun barrel -0.00248955 -0.01730382 -0.21754722 0.2 1\n" +
                "2 super_shotgun barrel 0.00432786 -0.02304399 -0.24853745 0.2 1\n" +
                "2 heavy_assault_rifle barrel -0.00055963 -0.03434449 -0.26972672 0.2 1\n" +
                "2 chaingun side-grip -0.30414763 0.05044765 -0.02056184 0.2 1\n" +
                "2 plasma_rifle barrel 0.01633241 -0.02299422 -0.25941357 0.2 1\n" +
                "2 gauss_cannon side-grip -0.32022661 -0.00210297 -0.03496519 0.2 1\n" +
                "2 rocket_launcher barrel -0.04141347 -0.15555370 -0.20393729 0.2 1\n" +
                "2 pistol barrel -0.03753062 -0.06298845 0.06208171 0.2 1\n";
            File.WriteAllText(source, example);
            var target = Path.Combine(native, TargetName);
            var saved = "2 bfg barrel 0.01 0.02 -0.28 0.2 1\n" +
                "2 shotgun barrel -0.09 0.02 -0.3 0.2 1\n" +
                "2 chainsaw side-grip -0.13 0.2 0.0 0.2 1\n";
            File.WriteAllText(target, saved);
            if (!ApplyOnce(native, backups) || ApplyOnce(native, backups))
                throw new InvalidDataException("Captured grip import must run once.");
            var merged = File.ReadAllText(target);
            if (!merged.Contains("2 bfg barrel 0.01") ||
                !merged.Contains("2 chainsaw side-grip -0.13") ||
                !merged.Contains("2 shotgun barrel -0.00248955") ||
                merged.Contains("2 shotgun barrel -0.09") ||
                File.ReadAllText(Directory.GetFiles(
                    Path.Combine(backups, "GripProfiles"), "*.cfg").Single()) != saved)
                throw new InvalidDataException("Captured grip import lost original profiles or backup.");
            // A later user correction must not be overwritten.
            File.AppendAllText(target, "# user adjustment\n");
            if (ApplyOnce(native, backups) || !File.ReadAllText(target).Contains("# user adjustment"))
                throw new InvalidDataException("Captured grip importer overwrote a later user edit.");
            File.Delete(Path.Combine(backups, "GripProfiles", AppliedMarker));
            File.WriteAllText(source, example.Replace("0.2 1", "NaN 1"));
            try { ApplyOnce(native, backups); throw new Exception("Invalid capture accepted."); }
            catch (InvalidDataException) { /* fail closed */ }
            if (!File.ReadAllText(target).Contains("# user adjustment"))
                throw new InvalidDataException("Invalid capture modified existing profiles.");
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }
}
