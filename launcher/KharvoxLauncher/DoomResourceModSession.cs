using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Applies individually selected resource mods only after backing up KHARVOX-owned
// patches. Does not modify the original user mods or launch DOOM itself.
internal static class DoomResourceModSession
{
    private const int Schema = 1;
    private const int MaxMods = 128;
    private static readonly TimeSpan LoaderTimeout = TimeSpan.FromMinutes(5);

    private sealed class Manifest
    {
        public int Schema { get; set; } = 1;
        public string GameDirectory { get; set; } = "";
        public string Fingerprint { get; set; } = "";
        public string LoaderVersion { get; set; } = "";
        public Dictionary<string, string> Files { get; set; } =
            new(StringComparer.OrdinalIgnoreCase);
    }

    private static string CacheRoot(string runtimeDir) =>
        Path.Combine(runtimeDir, "cache", "doom-modloader");

    private static string StateFile(string runtimeDir, string gameDir) =>
        Path.Combine(CacheRoot(runtimeDir),
            "game-" + HashText(Path.GetFullPath(gameDir).TrimEnd(
                Path.DirectorySeparatorChar).ToUpperInvariant()) + ".json");

    private static string ActiveFolder(string runtimeDir) =>
        Path.Combine(CacheRoot(runtimeDir), "active");

    private static string HashText(string text)
    {
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(text)))
                .Replace("-", "").ToLowerInvariant();
    }

    private static string HashFile(string path)
    {
        using (var stream = File.OpenRead(path))
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(stream))
                .Replace("-", "").ToLowerInvariant();
    }

    private static void NoLinks(string path)
    {
        if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidOperationException(
                "DOOM mod contains a symbolic link or junction: " + path);
    }

    private static string[] FilesInMod(string root)
    {
        var result = new List<string>();
        var pending = new Stack<string>();
        pending.Push(root);
        while (pending.Count != 0)
        {
            var current = pending.Pop();
            NoLinks(current);
            foreach (var file in Directory.GetFiles(current))
            {
                NoLinks(file);
                result.Add(file);
            }
            foreach (var dir in Directory.GetDirectories(current))
            {
                NoLinks(dir);
                pending.Push(dir);
            }
        }
        result.Sort(StringComparer.OrdinalIgnoreCase);
        return result.ToArray();
    }

    private static string Fingerprint(IReadOnlyList<DoomUserMods.ModEntry> selected)
    {
        var sb = new StringBuilder("KHARVOX-DOOM-MODS-v1\n");
        foreach (var mod in selected.OrderBy(x => x.Id, StringComparer.OrdinalIgnoreCase))
        {
            NoLinks(mod.FullPath);
            sb.Append(mod.Id).Append('|');
            if (File.Exists(mod.FullPath))
            {
                sb.Append(HashFile(mod.FullPath)).Append('\n');
                continue;
            }
            if (!Directory.Exists(mod.FullPath))
                throw new FileNotFoundException("Selected DOOM mod is missing.", mod.FullPath);
            foreach (var file in FilesInMod(mod.FullPath))
                sb.Append(file.Substring(mod.FullPath.Length + 1).Replace('\\', '/'))
                    .Append('=').Append(HashFile(file)).Append('\n');
            sb.Append("[end-directory]\n");
        }
        return HashText(sb.ToString());
    }

    private static void CopyMod(DoomUserMods.ModEntry mod, string destinationRoot)
    {
        // Prefixes prevent a ZIP from a user's DOOM/Mods directory clashing
        // with an identically named ZIP in KHARVOX/mods/doom/user.
        var prefix = HashText(mod.Id).Substring(0, 12);
        var filename = prefix + "-" + mod.Name;
        var output = Path.Combine(destinationRoot, filename);
        if (File.Exists(mod.FullPath))
        {
            NoLinks(mod.FullPath);
            File.Copy(mod.FullPath, output);
            return;
        }
        Directory.CreateDirectory(output);
        foreach (var source in FilesInMod(mod.FullPath))
        {
            var relative = source.Substring(mod.FullPath.Length + 1);
            var target = Path.Combine(output, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(source, target);
        }
    }

    // Prepare from scratch then replace the previous selection atomically.
    // The source folders are never modified.
    private static void Stage(string runtimeDir, IReadOnlyList<DoomUserMods.ModEntry> selected)
    {
        var root = CacheRoot(runtimeDir);
        Directory.CreateDirectory(root);
        var staging = Path.Combine(root, ".prepare-" + Guid.NewGuid().ToString("N"));
        var previous = Path.Combine(root, ".previous-" + Guid.NewGuid().ToString("N"));
        var active = ActiveFolder(runtimeDir);
        var movedPrevious = false;
        var committed = false;
        try
        {
            Directory.CreateDirectory(staging);
            foreach (var mod in selected) CopyMod(mod, staging);
            if (Directory.Exists(active))
            {
                Directory.Move(active, previous);
                movedPrevious = true;
            }
            try
            {
                Directory.Move(staging, active);
                committed = true;
            }
            catch
            {
                if (movedPrevious && !Directory.Exists(active))
                {
                    Directory.Move(previous, active);
                    movedPrevious = false;
                }
                throw;
            }
        }
        finally
        {
            if (Directory.Exists(staging)) Directory.Delete(staging, true);
            if (committed && movedPrevious && Directory.Exists(previous))
                Directory.Delete(previous, true);
        }
    }

    private static Dictionary<string, string> GetGeneratedResources(string gameDir)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var baseDir = Path.Combine(gameDir, "base");
        if (!Directory.Exists(baseDir)) return result;

        foreach (var path in Directory.GetFiles(baseDir, "gameresources*",
                     SearchOption.TopDirectoryOnly))
        {
            var name = Path.GetFileName(path);
            // DOOMModLoader only manipulates generated .patch/.pindex containers.
            if (!name.EndsWith(".patch", StringComparison.OrdinalIgnoreCase)
                && !name.EndsWith(".pindex", StringComparison.OrdinalIgnoreCase))
                continue;
            var stem = Path.GetFileNameWithoutExtension(name);
            if (!stem.Equals("gameresources", StringComparison.OrdinalIgnoreCase)
                && !(stem.StartsWith("gameresources_", StringComparison.OrdinalIgnoreCase)
                     && stem.Length == "gameresources_000".Length
                     && stem.Substring(stem.Length - 3).All(char.IsDigit)))
                continue;
            if (File.Exists(path + ".verify")) continue; // Official game content.
            NoLinks(path);
            result["base/" + name] = HashFile(path);
        }

        var video = Path.Combine(baseDir, "video", "mods");
        if (Directory.Exists(video))
        {
            foreach (var file in FilesInMod(video))
            {
                var relative = file.Substring(gameDir.Length).TrimStart(
                    Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
                result[relative.Replace('\\', '/')] = HashFile(file);
            }
        }
        return result;
    }

    private static Manifest? LoadManifest(string path)
    {
        if (!File.Exists(path)) return null;
        try
        {
            var value = new JavaScriptSerializer().Deserialize<Manifest>(
                File.ReadAllText(path));
            if (value is null || value.Schema != Schema || value.Files is null)
                throw new InvalidDataException("Invalid mod ownership record.");
            return value;
        }
        catch (Exception e)
        {
            throw new InvalidOperationException(
                "KHARVOX mod state cannot be read safely: " + path
                + ". Back up and inspect it before proceeding.", e);
        }
    }

    private static void SaveManifest(string path, Manifest state)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var tmp = path + ".tmp";
        File.WriteAllText(tmp, new JavaScriptSerializer().Serialize(state));
        if (File.Exists(path)) File.Replace(tmp, path, null);
        else File.Move(tmp, path);
    }

    private static bool SameFiles(
        Dictionary<string, string> expected, Dictionary<string, string> actual)
    {
        return expected.Count == actual.Count
            && expected.All(pair => actual.TryGetValue(pair.Key, out var digest)
                && string.Equals(pair.Value, digest, StringComparison.OrdinalIgnoreCase));
    }

    private static void VerifyOwnership(
        Manifest? state, Dictionary<string, string> existing, string gameDir)
    {
        if (state is null)
        {
            if (existing.Count == 0) return;
            throw new InvalidOperationException(
                "Existing DOOM resource patches were detected, but KHARVOX did not create them."
                + Environment.NewLine + "Nothing was changed. Remove/restore external mods yourself,"
                + " or use a clean DOOM installation before enabling KHARVOX resource mods."
                + Environment.NewLine + string.Join(Environment.NewLine, existing.Keys.Take(12)));
        }
        if (!string.Equals(state.GameDirectory, Path.GetFullPath(gameDir),
                StringComparison.OrdinalIgnoreCase)
            || !SameFiles(state.Files, existing))
            throw new InvalidOperationException(
                "DOOM resource patches changed outside KHARVOX. Nothing was changed."
                + Environment.NewLine + "Restore your previous mod setup before launching with KHARVOX resource mods.");
    }

    private static async Task<string> RunLoaderAsync(
        string gameDir, string modDir, string loaderExe)
    {
        using (var process = new Process())
        {
            process.StartInfo = new ProcessStartInfo(loaderExe)
            {
                Arguments = "-moddir \"" + modDir + "\" -nolaunchgame -nocheckforupdates -nopatchgame -nosnapmap -nouncapcutscenes",
                WorkingDirectory = gameDir, // DOOMModLoader resolves ./base relative to this.
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            if (!process.Start())
                throw new InvalidOperationException("DOOMModLoader could not start.");
            process.StandardInput.Close(); // No interactive prompts.
            var stdout = process.StandardOutput.ReadToEndAsync();
            var stderr = process.StandardError.ReadToEndAsync();
            var completed = await Task.WhenAny(
                Task.Run(() => process.WaitForExit()), Task.Delay(LoaderTimeout))
                .ConfigureAwait(false);
            if (!process.HasExited)
            {
                try { process.Kill(); } catch { }
                process.WaitForExit();
                throw new TimeoutException("DOOMModLoader exceeded the five-minute installation limit.");
            }
            var output = await stdout.ConfigureAwait(false);
            var errors = await stderr.ConfigureAwait(false);
            var combined = output + Environment.NewLine + errors;
            File.WriteAllText(RuntimeStorage.LogPath("KHARVOX-DOOMModLoader.log"), combined);
            if (process.ExitCode != 0
                || (combined.IndexOf("Successfully installed mods!",
                        StringComparison.OrdinalIgnoreCase) < 0
                    && combined.IndexOf("Successfully uninstalled mods!",
                        StringComparison.OrdinalIgnoreCase) < 0))
                throw new InvalidOperationException(
                    "DOOMModLoader failed or did not confirm success. See KHARVOX-DOOMModLoader.log."
                    + Environment.NewLine + combined.Substring(
                        Math.Max(0, combined.Length - Math.Min(900, combined.Length))));
            return combined;
        }
    }

    private static void RemoveGenerated(string gameDir, IEnumerable<string> paths)
    {
        foreach (var relative in paths)
        {
            // Paths originate in GetGeneratedResources; never use arbitrary user paths.
            var resolved = Path.GetFullPath(Path.Combine(
                gameDir, relative.Replace('/', Path.DirectorySeparatorChar)));
            var gamePrefix = Path.GetFullPath(gameDir).TrimEnd(
                Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (!resolved.StartsWith(gamePrefix, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Unsafe resource path encountered.");
            if (File.Exists(resolved)) File.Delete(resolved);
        }
    }

    private static void RestoreBackup(string gameDir, string backup,
        IReadOnlyDictionary<string, string> before)
    {
        var after = GetGeneratedResources(gameDir);
        RemoveGenerated(gameDir, after.Keys);
        foreach (var relative in before.Keys)
        {
            var source = Path.Combine(backup,
                relative.Replace('/', Path.DirectorySeparatorChar));
            var target = Path.Combine(gameDir,
                relative.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(source, target, true);
        }
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-DML-Staging-" + Guid.NewGuid().ToString("N"));
        try
        {
            var runtime = Path.Combine(root, "runtime");
            var game = Path.Combine(root, "game");
            var user = Path.Combine(root, "user");
            Directory.CreateDirectory(runtime);
            Directory.CreateDirectory(user);
            Directory.CreateDirectory(Path.Combine(game, "base"));
            var fileMod = Path.Combine(user, "SomeMod.zip");
            var otherMod = Path.Combine(user, "AnotherMod");
            File.WriteAllText(fileMod, "zip bytes placeholder");
            Directory.CreateDirectory(otherMod);
            File.WriteAllText(Path.Combine(otherMod, "resource.decl"), "value=1");
            var entries = new[]
            {
                new DoomUserMods.ModEntry("user:somemod.zip", "SomeMod.zip", fileMod, false),
                new DoomUserMods.ModEntry("user:anothermod", "AnotherMod", otherMod, false)
            };
            var firstHash = Fingerprint(entries);
            Stage(runtime, entries);
            var active = ActiveFolder(runtime);
            if (Directory.GetFileSystemEntries(active).Length != 2)
                throw new InvalidOperationException("Expected both checked mods in staging.");
            Stage(runtime, new[] { entries[0] });
            if (Directory.GetFileSystemEntries(active).Length != 1
                || !File.Exists(fileMod) || !File.Exists(
                    Path.Combine(otherMod, "resource.decl")))
                throw new InvalidOperationException(
                    "Unchecking a mod must remove only the staging copy.");
            if (Fingerprint(entries) != firstHash)
                throw new InvalidOperationException("Source hash changed without edits.");
            File.WriteAllText(Path.Combine(otherMod, "resource.decl"), "value=2");
            if (Fingerprint(entries) == firstHash)
                throw new InvalidOperationException("Edited mod content must invalidate cache.");
            Stage(runtime, Array.Empty<DoomUserMods.ModEntry>());
            if (Directory.GetFileSystemEntries(active).Length != 0)
                throw new InvalidOperationException("Unchecking every mod must empty staging.");

            var foreign = Path.Combine(game, "base", "gameresources_002.patch");
            File.WriteAllText(foreign, "third-party patch");
            var existing = GetGeneratedResources(game);
            if (existing.Count != 1)
                throw new InvalidOperationException("Foreign resource patch not detected.");
            var blocked = false;
            try { VerifyOwnership(null, existing, game); }
            catch (InvalidOperationException) { blocked = true; }
            if (!blocked)
                throw new InvalidOperationException("Unowned resources must block the loader.");
            var owned = new Manifest
            {
                GameDirectory = Path.GetFullPath(game),
                Files = existing,
                Fingerprint = firstHash
            };
            VerifyOwnership(owned, existing, game);
            File.WriteAllText(foreign, "mutated outside KHARVOX");
            blocked = false;
            try { VerifyOwnership(owned, GetGeneratedResources(game), game); }
            catch (InvalidOperationException) { blocked = true; }
            if (!blocked)
                throw new InvalidOperationException("Modified owned resources must block apply.");
            Console.WriteLine("DOOM staging, uncheck, checksum and patch ownership tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("DOOM staging self-test failed: " + error);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }

    internal static async Task<bool> PrepareAsync(string runtimeDir, string gameDir,
        Action<string>? statusUpdate)
    {
        var selectedIds = DoomUserMods.LoadSelections();
        var entries = DoomUserMods.Scan(gameDir);
        var selected = entries.Where(e => selectedIds.Contains(e.Id)).ToArray();
        var missing = selectedIds.Except(selected.Select(e => e.Id),
            StringComparer.OrdinalIgnoreCase).ToArray();
        if (missing.Length != 0)
            throw new InvalidOperationException(
                "Selected DOOM mods are missing. Re-add them or uncheck them in Custom Mods: "
                + string.Join(", ", missing));

        if (selected.Length > MaxMods)
            throw new InvalidOperationException("Too many selected DOOM mods (maximum 128).");
        var statePath = StateFile(runtimeDir, gameDir);
        var manifest = LoadManifest(statePath);

        // If nothing has ever been installed by KHARVOX, leave DOOM completely alone.
        if (selected.Length == 0 && manifest is null) return false;

        if (!DoomModLoaderInstaller.IsInstalled)
            throw new InvalidOperationException(
                "DOOMModLoader is missing or failed integrity verification."
                + Environment.NewLine + "Open Custom Mods > DOOM Mods to install or repair it.");

        statusUpdate?.Invoke("Checking selected DOOM mods...");
        var fingerprint = Fingerprint(selected);
        var current = GetGeneratedResources(gameDir);
        VerifyOwnership(manifest, current, gameDir);

        if (manifest is not null
            && manifest.LoaderVersion == DoomModLoaderInstaller.Version
            && manifest.Fingerprint == fingerprint)
        {
            statusUpdate?.Invoke("DOOM resource mods already match selected checkboxes.");
            return selected.Length != 0;
        }

        statusUpdate?.Invoke("Preparing " + selected.Length + " selected DOOM mods...");
        Stage(runtimeDir, selected);
        if (Fingerprint(selected) != fingerprint)
            throw new InvalidOperationException(
                "A source DOOM mod changed while it was being copied. Try launching again.");

        // A loader failure cannot leave partially rebuilt KHARVOX patches
        // silently active: back up and restore the exact pre-run resources.
        var backup = Path.Combine(CacheRoot(runtimeDir),
            ".resource-backup-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(backup);
        try
        {
            foreach (var path in current.Keys)
            {
                var source = Path.Combine(gameDir,
                    path.Replace('/', Path.DirectorySeparatorChar));
                var target = Path.Combine(backup,
                    path.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                File.Copy(source, target);
            }

            try
            {
                statusUpdate?.Invoke("Applying DOOM resource mods...");
                var output = await RunLoaderAsync(gameDir, ActiveFolder(runtimeDir),
                    DoomModLoaderInstaller.ExecutablePath).ConfigureAwait(false);
                var after = GetGeneratedResources(gameDir);
                SaveManifest(statePath, new Manifest
                {
                    Schema = Schema,
                    GameDirectory = Path.GetFullPath(gameDir),
                    LoaderVersion = DoomModLoaderInstaller.Version,
                    Fingerprint = fingerprint,
                    Files = after
                });
                statusUpdate?.Invoke("DOOM mods updated (" + selected.Length + " enabled).");
            }
            catch
            {
                try { RestoreBackup(gameDir, backup, current); }
                catch (Exception recovery)
                {
                    throw new InvalidOperationException(
                        "DOOMModLoader failed and KHARVOX could not fully restore the previous patches."
                        + Environment.NewLine + "A recovery copy remains in: " + backup, recovery);
                }
                throw;
            }
        }
        finally
        {
            // Preserve the backup if recovery failed; never delete the only copy.
            try
            {
                var latest = LoadManifest(statePath);
                var resources = GetGeneratedResources(gameDir);
                if (latest is not null && SameFiles(latest.Files, resources)
                    && Directory.Exists(backup))
                    Directory.Delete(backup, true);
            }
            catch { /* Never shadow a failed apply/recovery with cleanup errors. */ }
        }
        return selected.Length != 0;
    }
}
