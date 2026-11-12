using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace KharvoxMods;

// Bundled LOCAL custom runtime; older installations can prepare it in-place.
// Stock KHARVOX is never run, replaced, modified, or deleted.
internal static class NativeBridgeInstaller
{
    private const string ReleaseApi =
        "https://api.github.com/repos/MrEGS-Ops/KHARVOX-Custom/releases/tags/kharvox-mods-standalone-preview";
    // The publication workflow verifies native source ancestry, its GitHub
    // Actions result and both artifact provenance manifests. Runtime identity
    // is supplied by that signed-off package rather than a frozen old SHA.
    private static bool ValidNativeCommitIdentity(string manifest) =>
        Regex.Matches(manifest, @"(?m)^baseNativeCommit=[0-9a-f]{40}\r?$").Count == 1 &&
        Regex.Matches(manifest, @"(?m)^baseNativeCommit=").Count == 1;
    private const string ZipName = "KHARVOX-Mods-NativeBridge.zip";
    private const string RequiredLauncherUi = "cactus-original-v1.1.1-unmodified-ui-v2";
    private const long MaxArchiveBytes = 250L * 1024 * 1024;
    private const long MaxExtractBytes = 1200L * 1024 * 1024;

    private sealed class ReleaseInfo
    {
        public Asset[] assets { get; set; } = Array.Empty<Asset>();
    }

    private sealed class Asset
    {
        public string name { get; set; } = "";
        public string browser_download_url { get; set; } = "";
        public long size { get; set; }
    }

    internal static string TargetDirectory =>
        Path.Combine(AppContext.BaseDirectory, "NativeBridge");
    internal static string TargetLauncher =>
        Path.Combine(TargetDirectory, "KharvoxLauncher.exe");

    internal static string ExpectedManagerBuild()
    {
        var assembly = typeof(NativeBridgeInstaller).Assembly;
        var attribute = (System.Reflection.AssemblyInformationalVersionAttribute?)
            Attribute.GetCustomAttribute(assembly,
                typeof(System.Reflection.AssemblyInformationalVersionAttribute));
        var raw = attribute?.InformationalVersion ?? "";
        var plus = raw.IndexOf('+');
        if (plus >= 0) raw = raw.Substring(0, plus);
        return long.TryParse(raw, out var value) && value > 0 ? value.ToString() : "";
    }

    internal static bool BridgeMatchesManagerBuild(string manifest, string managerBuild) =>
        managerBuild.Length > 0 &&
        manifest.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
            .Any(line => string.Equals(line.Trim(), "managerBuild=" + managerBuild,
                StringComparison.Ordinal));

    internal static void RunManagerBuildCompatibilitySelfTest()
    {
        const string example = "KHARVOX_MODS_BRIDGE_V1\nmanagerBuild=41175228703\n";
        if (!BridgeMatchesManagerBuild(example, "41175228703") ||
            BridgeMatchesManagerBuild(example, "41174630526") ||
            BridgeMatchesManagerBuild(example, "4117522870") ||
            BridgeMatchesManagerBuild(example, ""))
            throw new InvalidDataException("Bridge manager version matching failed.");
        RunPortableCalibrationFactorySelfTest();
    }

    private static void RunPortableCalibrationFactorySelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "kharvox-factory-userdata-" + Guid.NewGuid().ToString("N"));
        try
        {
            var oldBridge = Path.Combine(root, "OldNativeBridge");
            var newBridge = Path.Combine(root, "NewNativeBridge");
            var immutable = Path.Combine(newBridge, "CalibrationDefaults");
            var user = Path.Combine(root, "UserData");
            Directory.CreateDirectory(oldBridge);
            Directory.CreateDirectory(immutable);
            foreach (var name in OwnerFactoryNames)
                File.WriteAllText(Path.Combine(immutable, name), "factory-" + name);
            // A previously shipped bridge contains a NEUTRAL anchor CFG
            // that must NOT become the first-run user's custom calibration.
            File.WriteAllText(Path.Combine(oldBridge,
                "offhand_right_hand_positions.cfg"),
                "right_hand_drop_m = 0.08\n"
                + "gauss_cannon = 0.00 0.00 0.00\n"
                + "chaingun = 0.00 0.00 0.00\n");
            // Existing shotgun calibration must beat the NEW shipped factory.
            File.WriteAllText(Path.Combine(oldBridge,
                "two_hand_weapon_profiles_saved.cfg"), "owner-custom");
            // An unrelated editable user config must also leave NativeBridge.
            File.WriteAllText(Path.Combine(oldBridge, "world_scale.cfg"), "42");
            EnsurePortableUserDataAt(oldBridge, newBridge, user);
            if (File.ReadAllText(Path.Combine(user,
                    "two_hand_weapon_profiles_saved.cfg")) != "owner-custom"
                || File.ReadAllText(Path.Combine(user, "world_scale.cfg")) != "42"
                || File.ReadAllText(Path.Combine(user,
                    "offhand_right_hand_positions.cfg"))
                    != "factory-offhand_right_hand_positions.cfg"
                || File.ReadAllText(Path.Combine(user,
                    "two_hand_weapon_profiles_left_primary_saved.cfg"))
                    != "factory-two_hand_weapon_profiles_left_primary_saved.cfg")
                throw new InvalidDataException("First-run factory or migration lost user configs.");
            File.WriteAllText(Path.Combine(user,
                "offhand_right_hand_positions.cfg"), "edited-after-install");
            EnsurePortableUserDataAt(oldBridge, newBridge, user);
            if (File.ReadAllText(Path.Combine(user,
                    "offhand_right_hand_positions.cfg")) != "edited-after-install")
                throw new InvalidDataException("Updater overwrote edited calibration.");
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }

    internal static bool IsReady()
    {
        try
        {
            var manifest = Path.Combine(TargetDirectory, "KharvoxModsBridge.v1");
            return File.Exists(TargetLauncher) &&
                File.Exists(Path.Combine(TargetDirectory, "KharvoxLayer.dll")) &&
                File.Exists(manifest) &&
                ValidNativeCommitIdentity(File.ReadAllText(manifest)) &&
                File.ReadAllText(manifest).Contains(
                    "launcherUi=" + RequiredLauncherUi) &&
                BridgeMatchesManagerBuild(File.ReadAllText(manifest),
                    ExpectedManagerBuild());
        }
        catch (IOException) { return false; }
        catch (UnauthorizedAccessException) { return false; }
    }

    private static string AssetUrl(ReleaseInfo info, string name)
    {
        var entry = info.assets.SingleOrDefault(item =>
            string.Equals(item.name, name, StringComparison.Ordinal));
        if (entry == null || entry.size < 1 || entry.size > MaxArchiveBytes)
            throw new InvalidDataException("Native bridge release asset missing or too large.");
        if (!Uri.TryCreate(entry.browser_download_url, UriKind.Absolute, out var url) ||
            url.Scheme != Uri.UriSchemeHttps ||
            !string.Equals(url.Host, "github.com", StringComparison.OrdinalIgnoreCase) ||
            !url.AbsolutePath.StartsWith(
                "/MrEGS-Ops/KHARVOX-Custom/releases/download/", StringComparison.Ordinal))
            throw new InvalidDataException("Native bridge download must come from our GitHub release.");
        return url.AbsoluteUri;
    }

    private static string HashFile(string path)
    {
        using (var sha = SHA256.Create())
        using (var file = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(file))
                .Replace("-", "").ToLowerInvariant();
    }

    private static string ParseHash(string text)
    {
        var match = Regex.Match(text.Trim(),
            @"\A([a-fA-F0-9]{64})(?:\s+\*?KHARVOX-Mods-NativeBridge\.zip)?\s*\z");
        if (!match.Success) throw new InvalidDataException("Native bridge checksum is invalid.");
        return match.Groups[1].Value.ToLowerInvariant();
    }

    private static void ExtractVerified(string archiveFile, string destination)
    {
        var fullRoot = Path.GetFullPath(destination)
            .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        Directory.CreateDirectory(destination);
        long total = 0;
        using (var archive = ZipFile.OpenRead(archiveFile))
        {
            foreach (var entry in archive.Entries)
            {
                var relative = entry.FullName.Replace('\\', '/');
                if (string.IsNullOrWhiteSpace(relative) || relative.StartsWith("/") ||
                    relative.Contains(":") || relative.Split('/').Any(x => x == ".."))
                    throw new InvalidDataException("Invalid bridge archive path: " + relative);
                if (((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000)
                    throw new InvalidDataException("Symbolic links are not permitted in bridge package.");
                var output = Path.GetFullPath(Path.Combine(destination,
                    relative.Replace('/', Path.DirectorySeparatorChar)));
                if (!output.StartsWith(fullRoot, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Unsafe archive path: " + relative);
                total += entry.Length;
                if (total > MaxExtractBytes)
                    throw new InvalidDataException("Uncompressed bridge package is too large.");
                if (relative.EndsWith("/"))
                {
                    Directory.CreateDirectory(output);
                    continue;
                }
                Directory.CreateDirectory(Path.GetDirectoryName(output)!);
                using (var src = entry.Open())
                using (var dest = new FileStream(output, FileMode.CreateNew, FileAccess.Write))
                    src.CopyTo(dest);
            }
        }
        if (!File.Exists(Path.Combine(destination, "KharvoxLauncher.exe")) ||
            !File.Exists(Path.Combine(destination, "KharvoxLayer.dll")))
            throw new InvalidDataException("Bridge package is missing the launcher or native layer.");
        var manifest = Path.Combine(destination, "KharvoxModsBridge.v1");
        if (!File.Exists(manifest) ||
            !ValidNativeCommitIdentity(File.ReadAllText(manifest)) ||
            !File.ReadAllText(manifest).Contains(
                "launcherUi=" + RequiredLauncherUi) ||
            !BridgeMatchesManagerBuild(File.ReadAllText(manifest), ExpectedManagerBuild()))
            throw new InvalidDataException("Native bridge build identity does not match our verified core.");
    }

    // The retail DOOM chainsaw samples are user-owned files, NOT part of
    // any GitHub release. Retain these three local WAVs when replacing the
    // NativeBridge directory so that a manager update never discards them.
    private static void PreserveLocalPainsawAudio(string extracted)
    {
        var oldAudio = Path.Combine(TargetDirectory, "audio", "painsaw");
        if (!Directory.Exists(oldAudio)) return;
        var newAudio = Path.Combine(extracted, "audio", "painsaw");
        Directory.CreateDirectory(newAudio);
        foreach (var name in new[] { "rev_1.wav", "rev_2.wav", "rev_3.wav" })
        {
            var source = Path.Combine(oldAudio, name);
            if (!File.Exists(source)) continue;
            var bytes = new FileInfo(source).Length;
            if (bytes < 44 || bytes > 8L * 1024 * 1024)
                throw new InvalidDataException(
                    "Owner-provided PAINSAW WAV has an invalid size: " + name);
            File.Copy(source, Path.Combine(newAudio, name), false);
        }
    }

    // Old NativeBridge snapshots duplicate local user-supplied PAINSAW WAVs.
    // After successful installation, remove ONLY those known WAV names from
    // updater-managed BACKUP snapshots, never active NativeBridge audio.
    private static void CleanupArchivedPainsawAudio()
    {
        var active = Path.Combine(TargetDirectory, "audio", "painsaw");
        var names = new[] { "rev_1.wav", "rev_2.wav", "rev_3.wav" };
        if (names.Any(name => !File.Exists(Path.Combine(active, name)))) return;
        var backups = Path.Combine(AppContext.BaseDirectory, "Backups", "NativeBridge");
        if (!Directory.Exists(backups)) return;
        try
        {
            foreach (var snapshot in Directory.EnumerateDirectories(backups))
            {
                try
                {
                    if ((File.GetAttributes(snapshot) & FileAttributes.ReparsePoint) != 0)
                        continue;
                    var audio = Path.Combine(snapshot, "audio");
                    var painsaw = Path.Combine(audio, "painsaw");
                    if (!Directory.Exists(painsaw) ||
                        (File.GetAttributes(audio) & FileAttributes.ReparsePoint) != 0 ||
                        (File.GetAttributes(painsaw) & FileAttributes.ReparsePoint) != 0)
                        continue;
                    foreach (var name in names)
                    {
                        var copy = Path.Combine(painsaw, name);
                        if (File.Exists(copy) &&
                            (File.GetAttributes(copy) & FileAttributes.ReparsePoint) == 0)
                            File.Delete(copy);
                    }
                    if (!Directory.EnumerateFileSystemEntries(painsaw).Any())
                        Directory.Delete(painsaw);
                    if (!Directory.EnumerateFileSystemEntries(audio).Any())
                        Directory.Delete(audio);
                }
                catch (IOException) { /* Keep backup if locked. */ }
                catch (UnauthorizedAccessException) { /* Keep backup if protected. */ }
            }
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }

    // The native bridge is replaceable. User-created CFG files MUST live
    // one level up, in top-level UserData beside the Mods Manager. During
    // upgrade, copy legacy local CFGs first. Never replace a user's current
    // calibration or settings with the shipped factory defaults.
    private static readonly string[] OwnerFactoryNames = {
        "offhand_right_hand_positions.cfg",
        "two_hand_weapon_profiles_saved.cfg",
        "two_hand_weapon_profiles_left_primary_saved.cfg"
    };

    internal static void EnsurePortableUserData(string? factoryBridge = null) =>
        EnsurePortableUserDataAt(TargetDirectory,
            factoryBridge ?? TargetDirectory, PortableUserData.Root);

    // Parameterized for isolated tests: proving first-run seeding, upgrade
    // migration and preserving edits without touching an actual installation.
    private static void EnsurePortableUserDataAt(
        string existingBridge,string packagedBridge,string folder)
    {
        Directory.CreateDirectory(folder);
        // Old native releases wrote saved profiles and other user configs
        // directly beside KharvoxLayer.dll. Migrate ALL editable .cfg files,
        // including manually customized settings, but not read-only assets
        // or the older one-time capture import sources.
        if (Directory.Exists(existingBridge))
        {
            foreach (var source in Directory.GetFiles(existingBridge, "*.cfg"))
            {
                var name = Path.GetFileName(source);
                if (name.EndsWith("_default.cfg", StringComparison.OrdinalIgnoreCase)
                    || name.Equals("hand_models.cfg", StringComparison.OrdinalIgnoreCase)
                    || name.Equals("two_hand_weapon_profiles_captured.cfg",
                        StringComparison.OrdinalIgnoreCase)
                    || name.Equals("offhand_right_hand_positions_latest.cfg",
                        StringComparison.OrdinalIgnoreCase))
                    continue;
                // Historic bridge bundles shipped a deliberately NEUTRAL
                // Gauss/Chaingun file with no owner calibration. Do not
                // mistakenly migrate that as an intentional saved profile:
                // it would block the user's exact uploaded factory preset
                // from being copied into a fresh UserData directory.
                if (name.Equals("offhand_right_hand_positions.cfg",
                    StringComparison.OrdinalIgnoreCase))
                {
                    var active = File.ReadAllLines(source)
                        .Select(line => line.Trim())
                        .Where(line => line.Length > 0 && !line.StartsWith("#"))
                        .ToArray();
                    if (active.Length == 3
                        && active.Contains("right_hand_drop_m = 0.08")
                        && active.Contains("gauss_cannon = 0.00 0.00 0.00")
                        && active.Contains("chaingun = 0.00 0.00 0.00"))
                        continue; // Known uncalibrated shipping fallback, NOT a save.
                }
                var dest = Path.Combine(folder, name);
                if (File.Exists(dest)) continue;
                if (new FileInfo(source).Length > 1024 * 1024)
                    throw new InvalidDataException("Legacy user CFG too large: " + name);
                File.Copy(source, dest, false);
            }
        }
        // The three actual uploaded November calibration files are shipped
        // read-only in CalibrationDefaults. First-run gets exact saved values;
        // all subsequent user edits always take precedence.
        var packaged = Path.Combine(packagedBridge, "CalibrationDefaults");
        foreach (var name in OwnerFactoryNames)
        {
            var factory = Path.Combine(packaged, name);
            if (!File.Exists(factory))
                throw new InvalidDataException("Missing owner factory calibration: " + name);
            if (new FileInfo(factory).Length == 0 ||
                new FileInfo(factory).Length > 65536)
                throw new InvalidDataException("Factory calibration invalid: " + name);
            var editable = Path.Combine(folder, name);
            if (!File.Exists(editable)) File.Copy(factory, editable, false);
        }
    }

    private static void PreserveLocalGripData(string extracted)
    {
        // Diagnostics remain with diagnostics; user-owned CFGs have already
        // been copied into UserData and are NEVER put in the new NativeBridge.
        var relative = Path.Combine("logs", "diagnostics", "OFFHAND-GRIP-CAPTURE.csv");
        var csv = Path.Combine(TargetDirectory, relative);
        if (File.Exists(csv))
        {
            if (new FileInfo(csv).Length > 8L * 1024 * 1024)
                throw new InvalidDataException("Owner grip capture CSV exceeds safe size.");
            var target = Path.Combine(extracted, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(csv, target, true);
        }
    }

    private static bool BridgeLauncherRunning()
    {
        foreach (var p in Process.GetProcessesByName("KharvoxLauncher"))
        {
            try
            {
                if (p.HasExited) continue;
                var path = p.MainModule?.FileName;
                if (!string.IsNullOrWhiteSpace(path) &&
                    string.Equals(Path.GetFullPath(path), Path.GetFullPath(TargetLauncher),
                        StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            catch (Exception ex) when (ex is System.ComponentModel.Win32Exception ||
                ex is InvalidOperationException) { }
            finally { p.Dispose(); }
        }
        return false;
    }

    internal static async Task<string> InstallAsync(Action<string> status)
    {
        if (BridgeLauncherRunning())
            throw new InvalidOperationException(
                "Close the KHARVOX NativeBridge launcher before updating it.");
        ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
        using (var client = new HttpClient { Timeout = TimeSpan.FromMinutes(6) })
        {
            client.DefaultRequestHeaders.UserAgent.ParseAdd("KHARVOXModsNativeBridge/1.0");
            client.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
            status("Finding verified native bridge release…");
            var release = new JavaScriptSerializer().Deserialize<ReleaseInfo>(
                await client.GetStringAsync(ReleaseApi));
            if (release == null) throw new InvalidDataException("Empty GitHub release metadata.");
            var zipUrl = AssetUrl(release, ZipName);
            var shaUrl = AssetUrl(release, ZipName + ".sha256");
            var expected = ParseHash(await client.GetStringAsync(shaUrl));

            var working = Path.Combine(AppContext.BaseDirectory,
                "UpdateCache", "native-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(working);
            try
            {
                var archive = Path.Combine(working, ZipName);
                status("Downloading mod-enabled KHARVOX native runtime…");
                using (var resp = await client.GetAsync(zipUrl,
                    HttpCompletionOption.ResponseHeadersRead))
                {
                    resp.EnsureSuccessStatusCode();
                    if (resp.Content.Headers.ContentLength > MaxArchiveBytes)
                        throw new InvalidDataException("Native bridge archive exceeds size limit.");
                    using (var input = await resp.Content.ReadAsStreamAsync())
                    using (var output = new FileStream(archive, FileMode.CreateNew, FileAccess.Write))
                    {
                        long total = 0;
                        var buffer = new byte[81920];
                        int read;
                        while ((read = await input.ReadAsync(buffer, 0, buffer.Length)) > 0)
                        {
                            total += read;
                            if (total > MaxArchiveBytes)
                                throw new InvalidDataException("Native bridge archive too large.");
                            await output.WriteAsync(buffer, 0, read);
                        }
                    }
                }
                status("Verifying checksum and unpacking local bridge…");
                if (!string.Equals(HashFile(archive), expected, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Native bridge download checksum mismatch.");

                var extracted = Path.Combine(working, "extracted");
                ExtractVerified(archive, extracted);
                if (BridgeLauncherRunning())
                    throw new InvalidOperationException("Close the active NativeBridge launcher.");

                // Migrate existing user CFGs out of NativeBridge, then seed
                // uploaded factory defaults ONLY for missing user files.
                // Complete this before replacing the previous native runtime.
                EnsurePortableUserData(extracted);
                PreserveLocalPainsawAudio(extracted);
                PreserveLocalGripData(extracted);

                var backup = "";
                if (Directory.Exists(TargetDirectory))
                {
                    var dest = Path.Combine(AppContext.BaseDirectory,
                        "Backups", "NativeBridge");
                    Directory.CreateDirectory(dest);
                    backup = Path.Combine(dest, DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")
                        + "-" + Guid.NewGuid().ToString("N").Substring(0, 5));
                    Directory.Move(TargetDirectory, backup);
                }
                try { Directory.Move(extracted, TargetDirectory); }
                catch
                {
                    if (backup.Length > 0 && Directory.Exists(backup) &&
                        !Directory.Exists(TargetDirectory))
                        Directory.Move(backup, TargetDirectory);
                    throw;
                }
                // Only after the new runtime and current local WAVs are safe.
                CleanupArchivedPainsawAudio();
                return "Native bridge installed in local NativeBridge folder. Original KHARVOX untouched.";
            }
            finally { try { Directory.Delete(working, true); } catch { } }
        }
    }
}
