using System;
using System.IO;
using System.Linq;

namespace KharvoxLauncher;

// Restore Cactus's original KHARVOX v1.1.1 hand-HUD once. Native HudHook,
// OffhandHudPolicy and the actual 6DoF hand-pose path are the original upstream
// implementation. An old, separately saved offhand_hud.cfg can still override
// the source-equivalent values after a DLL/build update, so preserve a backup
// and replace that one persisted override on the first launch only.
internal static class CactusHandHudRestore
{
    private const string DefaultFile = "offhand_hud_default.cfg";
    private const string SavedFile = "offhand_hud.cfg";
    private const string Marker = ".kharvox-cactus-v1.1.1-hand-hud-restored";
    private const string CalibrationMarker = "enable_live_ammo_calibration";

    // Exact CactusVRStudios/KHARVOX tag v1.1.1, config/offhand_hud_default.cfg.
    // Fail closed on an unexpected template rather than silently installing
    // an experimental calibration and calling it the upstream preset.
    private static readonly string[] Cactus111 = {
        "3 1",
        "-9.5 -5 24.5 -65 90 15 0.4",
        "-10 -14 0 -75 -65 20 0.4",
        "-4.5 20.5 17.5 -65 60 15 0.4",
        "-13 12.5 11 -55 -100 0 0.4",
        "-11.5 7.5 18 -130 95 -50 0.4",
        "-18 -1 6 -85 -65 30 0.4"
    };

    internal static void EnsureRestored(string runtimeDir, Action<string>? report = null)
    {
        var source = Path.Combine(runtimeDir, DefaultFile);
        var target = Path.Combine(runtimeDir, SavedFile);
        var marker = Path.Combine(runtimeDir, Marker);
        if (!File.Exists(source))
            throw new FileNotFoundException("Cactus KHARVOX v1.1.1 offhand HUD calibration is missing from this build.", source);
        if (!File.ReadAllLines(source).Select(x => x.Trim()).SequenceEqual(
                Cactus111, StringComparer.Ordinal))
            throw new InvalidDataException(
                "Shipped offhand_hud_default.cfg differs from Cactus KHARVOX v1.1.1; refusing unsafe HUD replacement.");

        // Already migrated: intentionally preserve any user calibration edits.
        // If they later delete the active config, safely recreate the original.
        if (File.Exists(marker) && File.Exists(target)) return;

        if (File.Exists(target))
        {
            var current = File.ReadAllLines(target).Select(x => x.Trim());
            if (!current.SequenceEqual(Cactus111, StringComparer.Ordinal))
            {
                var backup = target + ".pre-cactus-v1.1.1-" +
                    DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" +
                    Guid.NewGuid().ToString("N").Substring(0, 8) + ".bak";
                File.Copy(target, backup, overwrite: false);
                report?.Invoke("Old hand HUD calibration safely backed up: " + Path.GetFileName(backup));
            }
        }

        var temp = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.Copy(source, temp);
            if (File.Exists(target)) File.Replace(temp, target, null);
            else File.Move(temp, target);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }

        // This optional upstream calibration hotkey marker can be left over
        // from the experimental wrist-position tests; disable it reversibly.
        var liveMarker = Path.Combine(runtimeDir, CalibrationMarker);
        if (File.Exists(liveMarker))
        {
            var backup = liveMarker + ".pre-cactus-v1.1.1-" +
                Guid.NewGuid().ToString("N").Substring(0, 8) + ".bak";
            File.Move(liveMarker, backup);
            report?.Invoke("Previous hand HUD calibration mode disabled; marker backed up.");
        }

        File.WriteAllText(marker,
            "CactusVRStudios/KHARVOX tag v1.1.1 hand HUD default restored." +
            Environment.NewLine + "Original overrides preserved as .bak files.");
        report?.Invoke("Cactus KHARVOX v1.1.1 original hand HUD restored (Life, Ammo and ProgMeter).");
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-Cactus-Hand-HUD-Test-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            File.WriteAllLines(Path.Combine(root, DefaultFile), Cactus111);
            var active = Path.Combine(root, SavedFile);
            File.WriteAllText(active, "3 0\n1 2 3 4 5 6 0.1\n");
            File.WriteAllText(Path.Combine(root, CalibrationMarker), "");
            EnsureRestored(root);
            if (!File.ReadAllLines(active).SequenceEqual(Cactus111)
                || !File.Exists(Path.Combine(root, Marker))
                || Directory.GetFiles(root, SavedFile + ".pre-cactus-v1.1.1-*.bak").Length != 1
                || File.Exists(Path.Combine(root, CalibrationMarker))
                || Directory.GetFiles(root, CalibrationMarker + ".pre-cactus-v1.1.1-*.bak").Length != 1)
                return 1;
            // A user's later manual calibration must NOT be reset on every run.
            File.WriteAllText(active, "custom calibration after initial restoration");
            EnsureRestored(root);
            if (File.ReadAllText(active) != "custom calibration after initial restoration")
                return 2;
            // Removing that file is safely repaired from the original.
            File.Delete(active);
            EnsureRestored(root);
            if (!File.ReadAllLines(active).SequenceEqual(Cactus111))
                return 3;
            File.WriteAllText(Path.Combine(root, DefaultFile), "wrong template");
            var rejected = false;
            try { EnsureRestored(root); } catch (InvalidDataException) { rejected = true; }
            return rejected ? 0 : 4;
        }
        catch (Exception e) { Console.Error.WriteLine(e); return 5; }
        finally { try { Directory.Delete(root, recursive: true); } catch { } }
    }
}
