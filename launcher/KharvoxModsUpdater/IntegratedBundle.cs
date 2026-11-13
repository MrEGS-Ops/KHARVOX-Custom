using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;

namespace KharvoxModsUpdater;

// The updater must replace the matched native runtime AND manager from the
// exact same ReadyToRun archive. Previous releases silently updated only the
// EXE, leaving NativeBridge for a later (possibly blocked) startup refresh.
internal static class IntegratedBundle
{
    private const long MaxEntryBytes = 256L * 1024 * 1024;
    private const long MaxExtractedBytes = 1200L * 1024 * 1024;
    private static readonly string[] AllowedRootFiles = {
        "KharvoxVRModManager.exe", "KharvoxVRModManager.exe.config",
        "KharvoxVRModManagerUpdater.exe", "README.md", "build-info.txt"
    };
    private static readonly string[] FactoryProfiles = {
        "offhand_right_hand_positions.cfg",
        "two_hand_weapon_profiles_saved.cfg",
        "two_hand_weapon_profiles_left_primary_saved.cfg"
    };

    internal static void ValidateAndExtract(string archivePath, string destination)
    {
        Directory.CreateDirectory(destination);
        var prefix = Path.GetFullPath(destination)
            .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        long extracted = 0;
        using (var zip = ZipFile.OpenRead(archivePath))
        foreach (var entry in zip.Entries)
        {
            var name = entry.FullName.Replace('\\', '/');
            var directory = name.EndsWith("/", StringComparison.Ordinal);
            var leaf = directory ? name.TrimEnd('/') : name;
            var native = leaf.StartsWith("NativeBridge/", StringComparison.Ordinal);
            var permitted = AllowedRootFiles.Contains(leaf, StringComparer.Ordinal)
                || native || (directory && (leaf == "NativeBridge" || leaf == "UserData"));
            if (!permitted || string.IsNullOrWhiteSpace(leaf) ||
                leaf.StartsWith("/", StringComparison.Ordinal) ||
                leaf.Contains(":") ||
                leaf.Split('/').Any(part => part == "" || part == "." || part == "..") ||
                !found.Add(name) || entry.Length > MaxEntryBytes ||
                ((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000)
                throw new InvalidDataException("Unsafe or unexpected integrated ZIP entry: " + name);
            extracted += entry.Length;
            if (extracted > MaxExtractedBytes)
                throw new InvalidDataException("Integrated ZIP exceeds safe extracted size.");
            var output = Path.GetFullPath(Path.Combine(destination,
                leaf.Replace('/', Path.DirectorySeparatorChar)));
            if (!output.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("ZIP path escaped installation staging.");
            if (directory)
            {
                Directory.CreateDirectory(output);
                continue;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(output)!);
            using (var source = entry.Open())
            using (var target = new FileStream(output, FileMode.CreateNew, FileAccess.Write))
                source.CopyTo(target);
        }
        if (!File.Exists(Path.Combine(destination, "KharvoxVRModManager.exe")) ||
            !File.Exists(Path.Combine(destination, "NativeBridge", "KharvoxLayer.dll")) ||
            !File.Exists(Path.Combine(destination, "NativeBridge", "KharvoxLauncher.exe")) ||
            !File.Exists(Path.Combine(destination, "NativeBridge", "KharvoxModsBridge.v1")))
            throw new InvalidDataException("Complete manager/native bundle is missing a required executable or manifest.");
    }

    private static void VerifyManifest(string stagedNative, string expectedBuild)
    {
        var manifest = File.ReadAllLines(Path.Combine(stagedNative, "KharvoxModsBridge.v1"));
        if (!manifest.Contains("KHARVOX_MODS_BRIDGE_V1") ||
            !manifest.Contains("managerBuild=" + expectedBuild) ||
            !manifest.Any(line => line.StartsWith("baseNativeCommit=", StringComparison.Ordinal)))
            throw new InvalidDataException("Native bridge does not match the Mod Manager release build.");
        foreach (var name in FactoryProfiles)
            if (!File.Exists(Path.Combine(stagedNative, "CalibrationDefaults", name)))
                throw new InvalidDataException("Missing original factory profile: " + name);
    }

    private static bool NativeLauncherRunning(string root)
    {
        var expected = Path.GetFullPath(Path.Combine(root, "NativeBridge", "KharvoxLauncher.exe"));
        foreach (var process in Process.GetProcessesByName("KharvoxLauncher"))
        {
            using (process)
            {
                try
                {
                    if (!process.HasExited && string.Equals(
                        Path.GetFullPath(process.MainModule?.FileName ?? ""),
                        expected, StringComparison.OrdinalIgnoreCase))
                        return true;
                }
                catch (Exception error) when (error is System.ComponentModel.Win32Exception ||
                    error is InvalidOperationException || error is FileNotFoundException) { }
            }
        }
        return false;
    }

    internal static void EnsureLauncherNotRunning(string root)
    {
        if (NativeLauncherRunning(root))
            throw new InvalidOperationException(
                "Close the modified KHARVOX launcher before updating or rolling back its native runtime.");
    }

    private static void PreserveUserData(string original, string staged, string root)
    {
        var user = Path.Combine(root, "UserData");
        Directory.CreateDirectory(user);
        if (Directory.Exists(original))
        {
            foreach (var source in Directory.GetFiles(original, "*.cfg"))
            {
                var name = Path.GetFileName(source);
                if (name.EndsWith("_default.cfg", StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("hand_models.cfg", StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("two_hand_weapon_profiles_captured.cfg", StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("offhand_right_hand_positions_latest.cfg", StringComparison.OrdinalIgnoreCase))
                    continue;
                // Do not migrate a historic neutral factory placeholder as
                // an intentional saved calibration (same rule as manager).
                if (name.Equals("offhand_right_hand_positions.cfg", StringComparison.OrdinalIgnoreCase))
                {
                    var lines = File.ReadAllLines(source).Select(x => x.Trim())
                        .Where(x => x.Length > 0 && !x.StartsWith("#")).ToArray();
                    if (lines.Length == 3 &&
                        lines.Contains("right_hand_drop_m = 0.08") &&
                        lines.Contains("gauss_cannon = 0.00 0.00 0.00") &&
                        lines.Contains("chaingun = 0.00 0.00 0.00")) continue;
                }
                var target = Path.Combine(user, name);
                if (!File.Exists(target))
                {
                    if (new FileInfo(source).Length > 1024 * 1024)
                        throw new InvalidDataException("User calibration CFG too large: " + name);
                    File.Copy(source, target, false);
                }
            }
            // PAINSAW audio is local user input, not part of the published
            // runtime. Preserve it exactly and never overwrite a newer file.
            var audio = Path.Combine(original, "audio", "painsaw");
            if (Directory.Exists(audio))
                foreach (var source in Directory.GetFiles(audio, "*.wav"))
                {
                    if (new FileInfo(source).Length > 16 * 1024 * 1024)
                        throw new InvalidDataException("Local PAINSAW audio too large.");
                    var destination = Path.Combine(staged, "audio", "painsaw", Path.GetFileName(source));
                    Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                    if (!File.Exists(destination)) File.Copy(source, destination, false);
                }
            var capture = Path.Combine(original, "logs", "diagnostics", "OFFHAND-GRIP-CAPTURE.csv");
            if (File.Exists(capture))
            {
                if (new FileInfo(capture).Length > 8L * 1024 * 1024)
                    throw new InvalidDataException("Local grip capture is too large.");
                var target = Path.Combine(staged, "logs", "diagnostics", "OFFHAND-GRIP-CAPTURE.csv");
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                if (!File.Exists(target)) File.Copy(capture, target, false);
            }
        }
        foreach (var name in FactoryProfiles)
        {
            var target = Path.Combine(user, name);
            if (!File.Exists(target))
                File.Copy(Path.Combine(staged, "CalibrationDefaults", name), target, false);
        }
    }

    // The existing manager binary update routine already snapshots and rolls
    // back its managed EXE files. This wraps it in a native-runtime swap and
    // restores the old NativeBridge if EXE replacement fails.
    internal static void ReplaceTogether(string root, string staged, string build,
        Action replaceManager)
    {
        var stagedNative = Path.Combine(staged, "NativeBridge");
        VerifyManifest(stagedNative, build);
        if (NativeLauncherRunning(root))
            throw new InvalidOperationException(
                "Close the modified KHARVOX launcher before installing the complete update.");
        var liveNative = Path.Combine(root, "NativeBridge");
        if (Directory.Exists(liveNative) &&
            (File.GetAttributes(liveNative) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidDataException("NativeBridge must be a real local directory.");
        PreserveUserData(liveNative, stagedNative, root);
        var backups = Path.Combine(root, "Backups", "Updater");
        Directory.CreateDirectory(backups);
        var backupName = "native-" + Guid.NewGuid().ToString("N");
        var backupNative = Path.Combine(backups, backupName);
        var priorNative = Directory.Exists(liveNative);
        var newNative = false;
        if (priorNative) Directory.Move(liveNative, backupNative);
        try
        {
            Directory.Move(stagedNative, liveNative);
            newNative = true;
            replaceManager();
            // Only publish the rollback pointer after both runtime and
            // manager executable have been updated successfully.
            try
            {
                var pointer = Path.Combine(backups, "previous-native.txt");
                if (priorNative) File.WriteAllText(pointer, backupName);
                else if (File.Exists(pointer)) File.Delete(pointer);
            }
            catch (IOException) { /* Installed bundle is already valid. */ }
        }
        catch
        {
            if (newNative && Directory.Exists(liveNative))
                Directory.Move(liveNative, stagedNative);
            if (priorNative && Directory.Exists(backupNative))
                Directory.Move(backupNative, liveNative);
            throw;
        }
    }

    internal static void RestorePreviousNative(string root)
    {
        var backups = Path.Combine(root, "Backups", "Updater");
        var pointer = Path.Combine(backups, "previous-native.txt");
        if (!File.Exists(pointer)) return;
        var name = File.ReadAllText(pointer).Trim();
        if (!System.Text.RegularExpressions.Regex.IsMatch(name, @"\Anative-[a-f0-9]{32}\z"))
            throw new InvalidDataException("Native rollback pointer is invalid.");
        var previous = Path.Combine(backups, name);
        if (!Directory.Exists(previous))
            throw new InvalidDataException("Previous native runtime backup is missing.");
        if (NativeLauncherRunning(root))
            throw new InvalidOperationException("Close the modified KHARVOX launcher before rollback.");
        var current = Path.Combine(root, "NativeBridge");
        var displaced = Path.Combine(backups, "pre-rollback-native-" + Guid.NewGuid().ToString("N"));
        if (Directory.Exists(current)) Directory.Move(current, displaced);
        try { Directory.Move(previous, current); }
        catch
        {
            if (Directory.Exists(displaced) && !Directory.Exists(current))
                Directory.Move(displaced, current);
            throw;
        }
        File.Delete(pointer);
    }

    internal static void SelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(), "kharvox-bundle-test-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            var zipPath = Path.Combine(root, "integrated.zip");
            using (var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create))
            {
                foreach (var name in new[] {
                    "KharvoxVRModManager.exe", "NativeBridge/KharvoxLayer.dll",
                    "NativeBridge/KharvoxLauncher.exe", "NativeBridge/KharvoxModsBridge.v1" })
                {
                    var item = zip.CreateEntry(name);
                    using (var writer = new StreamWriter(item.Open()))
                        writer.Write(name.EndsWith("v1") ? "KHARVOX_MODS_BRIDGE_V1\nmanagerBuild=12345678900\nbaseNativeCommit=abc\n" : name);
                }
                foreach (var name in FactoryProfiles)
                {
                    var item = zip.CreateEntry("NativeBridge/CalibrationDefaults/" + name);
                    using (var writer = new StreamWriter(item.Open())) writer.Write("owner-factory");
                }
            }
            var staged = Path.Combine(root, "staged");
            ValidateAndExtract(zipPath, staged);
            var old = Path.Combine(root, "NativeBridge");
            Directory.CreateDirectory(old);
            File.WriteAllText(Path.Combine(old, "my_saved.cfg"), "owner-edited");
            File.WriteAllText(Path.Combine(root, "KharvoxVRModManager.exe"), "old");
            ReplaceTogether(root, staged, "12345678900", () =>
                File.Copy(Path.Combine(staged, "KharvoxVRModManager.exe"),
                    Path.Combine(root, "KharvoxVRModManager.exe"), true));
            if (!File.Exists(Path.Combine(root, "NativeBridge", "KharvoxLayer.dll")) ||
                File.ReadAllText(Path.Combine(root, "UserData", "my_saved.cfg")) != "owner-edited")
                throw new Exception("Integrated install lost matched native binary or owner CFG.");
            RestorePreviousNative(root);
            if (!File.Exists(Path.Combine(root, "NativeBridge", "my_saved.cfg")))
                throw new Exception("Native rollback lost the original runtime.");
            var malicious = Path.Combine(root, "traversal.zip");
            using (var zip = ZipFile.Open(malicious, ZipArchiveMode.Create))
                zip.CreateEntry("NativeBridge/../../outside.txt");
            var rejected = false;
            try { ValidateAndExtract(malicious, Path.Combine(root, "bad")); }
            catch (InvalidDataException) { rejected = true; }
            if (!rejected) throw new Exception("Traversal in integrated archive was accepted.");
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }
}
