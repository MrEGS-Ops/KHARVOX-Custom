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

// One-time automatic self-refresh of the separate updater after KharvoxMods
// itself is updated. The running updater cannot overwrite its own EXE; this
// application can, once the older updater process has exited.
// Never modifies user settings, game files, or native integration folders.
internal static class UpdaterBootstrap
{
    private const string ReleaseUrl =
        "https://api.github.com/repos/MrEGS-Ops/KHARVOX-Custom/releases/tags/kharvox-mods-standalone-preview";
    private const string ArchiveName = "KHARVOX-Mods-Updater.zip";
    private const string UpdaterName = "KharvoxModsUpdater.exe";
    private const long MaxSize = 20L * 1024 * 1024;

    private sealed class Release
    {
        public Asset[] assets { get; set; } = Array.Empty<Asset>();
    }

    private sealed class Asset
    {
        public string name { get; set; } = "";
        public long size { get; set; }
        public string browser_download_url { get; set; } = "";
    }

    private static string FindUrl(Release release, string name)
    {
        var asset = release.assets.SingleOrDefault(x => x.name == name);
        if (asset == null || asset.size <= 0 || asset.size > MaxSize)
            throw new InvalidDataException("Missing updater refresh asset.");
        if (!Uri.TryCreate(asset.browser_download_url, UriKind.Absolute, out var url) ||
            url.Scheme != Uri.UriSchemeHttps ||
            !string.Equals(url.Host, "github.com", StringComparison.OrdinalIgnoreCase) ||
            !url.AbsolutePath.StartsWith(
                "/MrEGS-Ops/KHARVOX-Custom/releases/download/", StringComparison.Ordinal))
            throw new InvalidDataException("Updater refresh URL was not from the project.");
        return url.AbsoluteUri;
    }

    private static string Digest(string path)
    {
        using (var sha = SHA256.Create())
        using (var stream = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(stream))
                .Replace("-", "").ToLowerInvariant();
    }

    private static bool IsUpdaterRunning()
    {
        foreach (var p in Process.GetProcessesByName("KharvoxModsUpdater"))
        {
            try { if (!p.HasExited) return true; }
            catch (InvalidOperationException) { }
            finally { p.Dispose(); }
        }
        return false;
    }

    internal static async Task TryRefreshAsync()
    {
        try
        {
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
            using (var client = new HttpClient { Timeout = TimeSpan.FromSeconds(75) })
            {
                client.DefaultRequestHeaders.UserAgent.ParseAdd("KharvoxModsUpdaterBootstrap/1.0");
                client.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
                var json = await client.GetStringAsync(ReleaseUrl);
                var release = new JavaScriptSerializer().Deserialize<Release>(json);
                if (release == null) return;
                var zipUrl = FindUrl(release, ArchiveName);
                var shaUrl = FindUrl(release, ArchiveName + ".sha256");
                var manifest = (await client.GetStringAsync(shaUrl)).Trim();
                var match = Regex.Match(manifest,
                    @"\A([a-fA-F0-9]{64})(?:\s+\*?KHARVOX-Mods-Updater\.zip)?\s*\z");
                if (!match.Success) return;

                var root = AppContext.BaseDirectory;
                var staging = Path.Combine(root, "UpdateCache",
                    "updater-self-" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(staging);
                try
                {
                    var zip = Path.Combine(staging, ArchiveName);
                    using (var response = await client.GetAsync(zipUrl,
                        HttpCompletionOption.ResponseHeadersRead))
                    {
                        response.EnsureSuccessStatusCode();
                        if (response.Content.Headers.ContentLength > MaxSize)
                            throw new InvalidDataException("Updater download too large.");
                        using (var input = await response.Content.ReadAsStreamAsync())
                        using (var output = new FileStream(zip, FileMode.CreateNew, FileAccess.Write))
                        {
                            var buffer = new byte[81920];
                            int count;
                            long total = 0;
                            while ((count = await input.ReadAsync(buffer, 0, buffer.Length)) > 0)
                            {
                                total += count;
                                if (total > MaxSize) throw new InvalidDataException("Updater ZIP too large.");
                                await output.WriteAsync(buffer, 0, count);
                            }
                        }
                    }
                    if (!string.Equals(Digest(zip), match.Groups[1].Value,
                        StringComparison.OrdinalIgnoreCase))
                        throw new InvalidDataException("Updater refresh checksum mismatch.");
                    var next = Path.Combine(staging, UpdaterName);
                    using (var archive = ZipFile.OpenRead(zip))
                    {
                        if (archive.Entries.Count != 1 ||
                            !string.Equals(archive.Entries[0].FullName,
                                UpdaterName, StringComparison.Ordinal) ||
                            archive.Entries[0].Length > MaxSize)
                            throw new InvalidDataException("Unexpected updater archive contents.");
                        archive.Entries[0].ExtractToFile(next);
                    }

                    var current = Path.Combine(root, UpdaterName);
                    if (File.Exists(current) && Digest(next) == Digest(current)) return;

                    var backups = Path.Combine(root, "Backups", "Updater");
                    Directory.CreateDirectory(backups);
                    var backup = Path.Combine(backups, "previous-updater.exe");
                    for (var attempt = 0; attempt < 20; attempt++)
                    {
                        if (IsUpdaterRunning())
                        {
                            await Task.Delay(400);
                            continue;
                        }
                        try
                        {
                            if (File.Exists(current))
                                File.Replace(next, current, backup, true);
                            else
                                File.Move(next, current);
                            return;
                        }
                        catch (IOException)
                        {
                            await Task.Delay(400);
                        }
                    }
                }
                finally { try { Directory.Delete(staging, true); } catch { } }
            }
        }
        catch (Exception ex) when (ex is HttpRequestException || ex is IOException ||
            ex is UnauthorizedAccessException || ex is InvalidDataException ||
            ex is InvalidOperationException || ex is ArgumentException)
        {
            // A failed updater refresh must never interrupt KHARVOX startup.
            try
            {
                var directory = Path.Combine(AppContext.BaseDirectory, "Logs");
                Directory.CreateDirectory(directory);
                File.AppendAllText(Path.Combine(directory, "updater-refresh.log"),
                    DateTime.UtcNow.ToString("o") + " " + ex.Message + Environment.NewLine);
            }
            catch { }
        }
    }
}
