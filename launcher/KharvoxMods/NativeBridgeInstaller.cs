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
    private const string PinnedNativeCommit =
        "9e30175531524c308d38d61c43b0fe2b1f16c6e4";
    private const string ZipName = "KHARVOX-Mods-NativeBridge.zip";
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

    internal static bool IsReady()
    {
        try
        {
            var manifest = Path.Combine(TargetDirectory, "KharvoxModsBridge.v1");
            return File.Exists(TargetLauncher) &&
                File.Exists(Path.Combine(TargetDirectory, "KharvoxLayer.dll")) &&
                File.Exists(manifest) &&
                File.ReadAllText(manifest).Contains(
                    "baseNativeCommit=" + PinnedNativeCommit);
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
            !File.ReadAllText(manifest).Contains(
                "baseNativeCommit=" + PinnedNativeCommit))
            throw new InvalidDataException("Native bridge build identity does not match our verified core.");
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
                return "Native bridge installed in local NativeBridge folder. Original KHARVOX untouched.";
            }
            finally { try { Directory.Delete(working, true); } catch { } }
        }
    }
}
