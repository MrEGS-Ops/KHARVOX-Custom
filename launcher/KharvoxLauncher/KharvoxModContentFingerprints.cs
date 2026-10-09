using System.Security.Cryptography;
using System.Text;

namespace KharvoxLauncher;

// Identifies the actual bytes selected by the user, without inspecting
// compatibility or rewriting the mod. Repeated checkbox edits reuse the
// SHA-256 of files whose size/timestamp (or folder inventory) is unchanged.
internal static class KharvoxModContentFingerprints
{
    private const int MaxDirectoryFiles = 100000;
    private sealed class CachedDigest
    {
        internal string Stamp = "";
        internal string Hash = "";
    }

    private static readonly Dictionary<string, CachedDigest> Cache =
        new(StringComparer.OrdinalIgnoreCase);
    private static readonly object CacheLock = new();

    internal static void Invalidate()
    {
        lock (CacheLock) Cache.Clear();
    }

    internal static KeyValuePair<string, string>[] ResolveSelected(
        string? gameDirectory, IEnumerable<string> selectedIds)
    {
        var selected = selectedIds.Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray();
        if (selected.Length == 0) return Array.Empty<KeyValuePair<string, string>>();
        var available = DoomUserMods.Scan(gameDirectory)
            .ToDictionary(x => x.Id, StringComparer.OrdinalIgnoreCase);
        var output = new List<KeyValuePair<string, string>>(selected.Length);
        foreach (var id in selected)
        {
            if (!available.TryGetValue(id, out var mod))
                throw new InvalidDataException(
                    "A selected DOOM mod is missing: " + id
                    + ". Its previous Good/Bad rating will not be reused.");
            output.Add(new KeyValuePair<string, string>(id, Digest(mod.FullPath)));
        }
        return output.ToArray();
    }

    internal static string Digest(string path)
    {
        path = Path.GetFullPath(path);
        if (!File.Exists(path) && !Directory.Exists(path))
            throw new FileNotFoundException("DOOM mod is missing.", path);
        if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidDataException("Linked mod files/folders cannot be fingerprinted: " + path);

        var file = File.Exists(path);
        string stamp;
        List<(string Relative, string Full, long Length, long WriteTicks)>? files = null;
        if (file)
        {
            var info = new FileInfo(path);
            stamp = "file|" + info.Length + "|" + info.LastWriteTimeUtc.Ticks;
        }
        else
        {
            files = ListFiles(path);
            // The manifest is a cheap metadata check; do not read file
            // contents again when users simply tick another checkbox.
            stamp = "folder|" + string.Join("\n", files.Select(x =>
                x.Relative + "|" + x.Length + "|" + x.WriteTicks));
        }

        lock (CacheLock)
        {
            if (Cache.TryGetValue(path, out var previous) && previous.Stamp == stamp)
                return previous.Hash;
        }

        string digest;
        if (file)
        {
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var sha = SHA256.Create();
            digest = Convert.ToBase64String(sha.ComputeHash(stream));
            var info = new FileInfo(path);
            if (stamp != "file|" + info.Length + "|" + info.LastWriteTimeUtc.Ticks)
                throw new IOException("Mod changed while it was being fingerprinted: " + path);
        }
        else
        {
            using var sha = SHA256.Create();
            using var buffer = new MemoryStream();
            using (var writer = new BinaryWriter(buffer, new UTF8Encoding(false), true))
            {
                foreach (var item in files!)
                {
                    writer.Write(item.Relative);
                    using var stream = new FileStream(item.Full, FileMode.Open,
                        FileAccess.Read, FileShare.Read);
                    using var fileSha = SHA256.Create();
                    var hash = fileSha.ComputeHash(stream);
                    var after = new FileInfo(item.Full);
                    if (!after.Exists || after.Length != item.Length
                        || after.LastWriteTimeUtc.Ticks != item.WriteTicks)
                        throw new IOException("Mod folder changed while being fingerprinted: " + path);
                    writer.Write(hash.Length);
                    writer.Write(hash);
                }
            }
            buffer.Position = 0;
            digest = Convert.ToBase64String(sha.ComputeHash(buffer));
            // Detect a change to the folder inventory during hashing.
            var check = ListFiles(path);
            if (check.Count != files!.Count || check.Where((x, i) =>
                x.Relative != files[i].Relative || x.Length != files[i].Length
                || x.WriteTicks != files[i].WriteTicks).Any())
                throw new IOException("Mod folder changed while being fingerprinted: " + path);
        }

        lock (CacheLock) Cache[path] = new CachedDigest { Stamp = stamp, Hash = digest };
        return digest;
    }

    private static List<(string Relative, string Full, long Length, long WriteTicks)>
        ListFiles(string root)
    {
        var result = new List<(string Relative, string Full, long Length, long WriteTicks)>();
        var pending = new Stack<string>();
        pending.Push(root);
        var basePath = root.TrimEnd(Path.DirectorySeparatorChar,
            Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
        while (pending.Count != 0)
        {
            var folder = pending.Pop();
            if ((File.GetAttributes(folder) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Linked directory in DOOM mod: " + folder);
            foreach (var file in Directory.EnumerateFiles(folder))
            {
                var info = new FileInfo(file);
                if ((info.Attributes & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Linked file in DOOM mod: " + file);
                var relative = file.Substring(basePath.Length)
                    .Replace('\\', '/').ToLowerInvariant();
                result.Add((relative, file, info.Length, info.LastWriteTimeUtc.Ticks));
                if (result.Count > MaxDirectoryFiles)
                    throw new InvalidDataException("Too many files in DOOM mod folder: " + root);
            }
            foreach (var child in Directory.EnumerateDirectories(folder))
                pending.Push(child);
        }
        result.Sort((x, y) => StringComparer.Ordinal.Compare(x.Relative, y.Relative));
        return result;
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-Mod-Digest-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            var mod = Path.Combine(root, "EnemyMod.zip");
            File.WriteAllText(mod, "first version");
            var first = Digest(mod);
            if (first != Digest(mod))
                throw new InvalidDataException("Cached ZIP content hash is unstable.");
            File.WriteAllText(mod, "second version with new data");
            var second = Digest(mod);
            if (first == second)
                throw new InvalidDataException("Updated ZIP was not detected.");
            File.WriteAllText(mod, "first version");
            Invalidate();
            if (first != Digest(mod))
                throw new InvalidDataException("Restoring old ZIP lost its identity.");

            var folder = Path.Combine(root, "Unpacked");
            Directory.CreateDirectory(Path.Combine(folder, "data"));
            File.WriteAllText(Path.Combine(folder, "data", "config.ini"), "health=100");
            var original = Digest(folder);
            if (original != Digest(folder))
                throw new InvalidDataException("Cached unpacked-mod hash is unstable.");
            File.WriteAllText(Path.Combine(folder, "data", "config.ini"), "health=200");
            if (original == Digest(folder))
                throw new InvalidDataException("Modified unpacked mod was not detected.");
            File.WriteAllText(Path.Combine(folder, "data", "config.ini"), "health=100");
            if (original != Digest(folder))
                throw new InvalidDataException("Restored unpacked mod lost its identity.");
            File.WriteAllText(Path.Combine(folder, "extra.txt"), "new");
            if (original == Digest(folder))
                throw new InvalidDataException("New file in unpacked mod went unnoticed.");

            var missingRejected = false;
            try { Digest(Path.Combine(root, "missing.zip")); }
            catch (FileNotFoundException) { missingRejected = true; }
            if (!missingRejected)
                throw new InvalidDataException("A missing mod was treated as compatible.");

            Console.WriteLine("KHARVOX ZIP and unpacked mod fingerprint tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("KHARVOX mod fingerprint test failed: " + error);
            return 1;
        }
        finally
        {
            Invalidate();
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
