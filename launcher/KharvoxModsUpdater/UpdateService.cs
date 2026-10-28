using System;
using System.Collections.Generic;
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

namespace KharvoxModsUpdater;

// Only trusted public release assets from our own repository are accepted.
// The updater NEVER deletes user-owned folders or arbitrary files.
internal static class UpdateService
{
    private const string ReleaseApi =
        "https://api.github.com/repos/MrEGS-Ops/KHARVOX-Custom/releases/tags/kharvox-mods-standalone-preview";
    private const string AssetPrefix =
        "https://github.com/MrEGS-Ops/KHARVOX-Custom/releases/download/";
    private const string ZipName = "KHARVOX-Mods-Standalone.zip";
    private const string HashName = "KHARVOX-Mods-Standalone.zip.sha256";
    private const long MaxZipBytes = 128L * 1024 * 1024;
    private const long MaxFileBytes = 64L * 1024 * 1024;
    private static readonly string[] ManagedFiles =
        { "KharvoxMods.exe", "KharvoxMods.exe.config", "README.md" };

    internal static string InstallFolder => Path.GetFullPath(AppContext.BaseDirectory);
    internal static string AppExe => Path.Combine(InstallFolder, "KharvoxMods.exe");

    private sealed class ReleaseInfo
    {
        public ReleaseAsset[] assets { get; set; } = Array.Empty<ReleaseAsset>();
    }

    private sealed class ReleaseAsset
    {
        public string name { get; set; } = "";
        public string browser_download_url { get; set; } = "";
        public long size { get; set; }
    }

    private static string GetAssetUrl(ReleaseInfo release, string name)
    {
        var asset = release.assets?.SingleOrDefault(x =>
            string.Equals(x.name, name, StringComparison.Ordinal));
        if (asset == null || asset.size < 1 || asset.size > MaxZipBytes)
            throw new InvalidDataException("Missing or incorrectly sized release asset: " + name);
        if (!Uri.TryCreate(asset.browser_download_url, UriKind.Absolute, out var uri) ||
            uri.Scheme != Uri.UriSchemeHttps ||
            !string.Equals(uri.Host, "github.com", StringComparison.OrdinalIgnoreCase) ||
            !uri.AbsolutePath.StartsWith(
                "/MrEGS-Ops/KHARVOX-Custom/releases/download/", StringComparison.Ordinal))
            throw new InvalidDataException("Update download URL is not from the KHARVOX project.");
        return uri.AbsoluteUri;
    }

    private static string ParseExpectedHash(string text)
    {
        var match = Regex.Match(text.Trim(),
            @"\A([A-Fa-f0-9]{64})(?:\s+\*?KHARVOX-Mods-Standalone\.zip)?\s*\z");
        if (!match.Success)
            throw new InvalidDataException("Release SHA-256 manifest is invalid.");
        return match.Groups[1].Value.ToLowerInvariant();
    }

    private static string HashFile(string path)
    {
        using (var sha = SHA256.Create())
        using (var file = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(file))
                .Replace("-", "").ToLowerInvariant();
    }

    private static void ValidateAndExtract(string archivePath, string targetDirectory)
    {
        Directory.CreateDirectory(targetDirectory);
        var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using (var archive = ZipFile.OpenRead(archivePath))
        {
            foreach (var entry in archive.Entries)
            {
                // Flat, explicit allowlist; reject traversal, symlinks, unknown files
                // and duplicate or unexpectedly large application binaries.
                var entryName = entry.FullName.Replace('\\', '/');
                if (entryName.Contains("/") || !ManagedFiles.Contains(entryName,
                        StringComparer.Ordinal) || !found.Add(entryName) ||
                    entry.Length > MaxFileBytes)
                    throw new InvalidDataException("Unexpected file in update archive: " + entryName);
                var output = Path.Combine(targetDirectory, entryName);
                using (var source = entry.Open())
                using (var dest = new FileStream(output, FileMode.CreateNew, FileAccess.Write))
                    source.CopyTo(dest);
            }
        }
        if (!found.Contains("KharvoxMods.exe"))
            throw new InvalidDataException("Update archive does not contain the application executable.");
    }

    private static void ReplaceManagedFiles(string root, string stagedDirectory)
    {
        var names = ManagedFiles.Where(name =>
            File.Exists(Path.Combine(stagedDirectory, name))).ToArray();
        if (!names.Contains("KharvoxMods.exe"))
            throw new InvalidDataException("New application executable missing.");

        var backup = Path.Combine(root, "Backups", "Updater", "previous");
        Directory.CreateDirectory(backup);
        var existing = new Dictionary<string, bool>(StringComparer.Ordinal);
        // Complete ALL backups before replacing a single production file.
        foreach (var name in names)
        {
            var path = Path.Combine(root, name);
            var present = File.Exists(path);
            existing.Add(name, present);
            if (present) File.Copy(path, Path.Combine(backup, name), true);
        }

        var replaced = new List<string>();
        try
        {
            foreach (var name in names)
            {
                var dest = Path.Combine(root, name);
                File.Copy(Path.Combine(stagedDirectory, name), dest, true);
                replaced.Add(name);
            }
        }
        catch
        {
            // Roll back only files touched in this attempt. Never touch settings,
            // user mods, logs or any other folders.
            foreach (var name in replaced.AsEnumerable().Reverse())
            {
                try
                {
                    var target = Path.Combine(root, name);
                    if (existing[name]) File.Copy(Path.Combine(backup, name), target, true);
                    else if (File.Exists(target)) File.Delete(target);
                }
                catch { /* preserve backups so the user can recover manually */ }
            }
            throw;
        }
    }

    private static bool AppIsRunning()
    {
        foreach (var process in Process.GetProcessesByName("KharvoxMods"))
        {
            try
            {
                if (!process.HasExited && process.Id != Process.GetCurrentProcess().Id)
                    return true;
            }
            finally { process.Dispose(); }
        }
        return false;
    }

    internal static async Task<string> RunAsync(Action<string> status)
    {
        ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
        using (var handler = new HttpClientHandler
        {
            AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate
        })
        using (var client = new HttpClient(handler) { Timeout = TimeSpan.FromMinutes(3) })
        {
            client.DefaultRequestHeaders.UserAgent.ParseAdd("KharvoxModsPortableUpdater/0.1");
            client.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
            status("Checking latest successful KHARVOX Mods release…");
            var json = await client.GetStringAsync(ReleaseApi);
            var release = new JavaScriptSerializer().Deserialize<ReleaseInfo>(json);
            if (release == null)
                throw new InvalidDataException("GitHub release response was empty.");
            var zipUrl = GetAssetUrl(release, ZipName);
            var hashUrl = GetAssetUrl(release, HashName);

            status("Verifying published update checksum…");
            var expected = ParseExpectedHash(await client.GetStringAsync(hashUrl));
            var marker = Path.Combine(InstallFolder, "updater-state.txt");
            if (File.Exists(AppExe) && File.Exists(marker) &&
                string.Equals(File.ReadAllText(marker).Trim(), expected,
                    StringComparison.OrdinalIgnoreCase))
                return "Already up to date.";

            if (AppIsRunning())
                throw new InvalidOperationException(
                    "Close KHARVOX Mods first, then run the updater again. The KHARVOX launcher may remain open.");

            // All working directories live beside the updater, not AppData or TEMP.
            var cache = Path.Combine(InstallFolder, "UpdateCache");
            var stage = Path.Combine(cache, "staging-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(stage);
            try
            {
                var archivePath = Path.Combine(stage, ZipName);
                status("Downloading latest application…");
                using (var response = await client.GetAsync(zipUrl,
                    HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();
                    if (response.Content.Headers.ContentLength > MaxZipBytes)
                        throw new InvalidDataException("Release ZIP exceeds the updater safety limit.");
                    using (var stream = await response.Content.ReadAsStreamAsync())
                    using (var dest = new FileStream(archivePath, FileMode.CreateNew, FileAccess.Write))
                    {
                        var buffer = new byte[81920];
                        long total = 0;
                        int count;
                        while ((count = await stream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                        {
                            total += count;
                            if (total > MaxZipBytes)
                                throw new InvalidDataException("Update ZIP exceeds the safety limit.");
                            await dest.WriteAsync(buffer, 0, count);
                        }
                    }
                }

                status("Checking SHA-256 and extracting files…");
                if (!string.Equals(HashFile(archivePath), expected,
                        StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Update checksum mismatch; nothing was installed.");
                var extracted = Path.Combine(stage, "extracted");
                ValidateAndExtract(archivePath, extracted);

                if (AppIsRunning())
                    throw new InvalidOperationException(
                        "KHARVOX Mods was started during the download. Close it before updating.");

                status("Replacing application files; keeping settings and Mods folder…");
                ReplaceManagedFiles(InstallFolder, extracted);
                // Marker is only a download optimization; failure to write this
                // should not cause damage to already-committed application files.
                try { File.WriteAllText(marker, expected); } catch (IOException) { }
                return "KHARVOX Mods updated successfully.";
            }
            finally
            {
                try { Directory.Delete(stage, true); } catch { }
            }
        }
    }

    internal static void RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(), "kharvox-updater-test-" +
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            if (ParseExpectedHash(new string('a', 64) + " *" + ZipName)
                != new string('a', 64))
                throw new Exception("SHA-256 validation failed.");
            var settingsPath = Path.Combine(root, "settings.json");
            File.WriteAllText(settingsPath, "test-settings-should-stay");
            foreach (var folder in new[] { "Mods", "Logs", "Backups" })
                Directory.CreateDirectory(Path.Combine(root, folder));
            File.WriteAllText(Path.Combine(root, "Mods", "my-mod.txt"), "keep");
            File.WriteAllText(Path.Combine(root, "KharvoxMods.exe"), "old-exe");
            var staged = Path.Combine(root, "staged");
            Directory.CreateDirectory(staged);
            File.WriteAllText(Path.Combine(staged, "KharvoxMods.exe"), "new-exe");
            ReplaceManagedFiles(root, staged);
            if (File.ReadAllText(Path.Combine(root, "KharvoxMods.exe")) != "new-exe" ||
                File.ReadAllText(settingsPath) != "test-settings-should-stay" ||
                File.ReadAllText(Path.Combine(root, "Mods", "my-mod.txt")) != "keep" ||
                File.ReadAllText(Path.Combine(root, "Backups", "Updater",
                    "previous", "KharvoxMods.exe")) != "old-exe")
                throw new Exception("Portable update overwrote application settings or mod data.");

            var archive = Path.Combine(root, "test.zip");
            using (var zip = ZipFile.Open(archive, ZipArchiveMode.Create))
            {
                var entry = zip.CreateEntry("KharvoxMods.exe");
                using (var writer = new StreamWriter(entry.Open()))
                    writer.Write("new");
            }
            ValidateAndExtract(archive, Path.Combine(root, "extracted"));
            if (!File.Exists(Path.Combine(root, "extracted", "KharvoxMods.exe")))
                throw new Exception("Verified update extraction did not work.");
            var traversal = Path.Combine(root, "traversal.zip");
            using (var zip = ZipFile.Open(traversal, ZipArchiveMode.Create))
                zip.CreateEntry("../settings.json");
            var rejected = false;
            try { ValidateAndExtract(traversal, Path.Combine(root, "reject")); }
            catch (InvalidDataException) { rejected = true; }
            if (!rejected) throw new Exception("Unsafe ZIP entry was accepted.");
            Console.WriteLine("Updater tests passed: portable backup, preserved settings, checksum, ZIP allowlist.");
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }
}
