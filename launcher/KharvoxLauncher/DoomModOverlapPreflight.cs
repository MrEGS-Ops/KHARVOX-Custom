using System.IO.Compression;

namespace KharvoxLauncher;

// Active DML fallback cannot choose resources by file priority without telling
// the user. Detect unambiguous resource overlaps before handing files to DML.
// This intentionally does not attempt a semantic merge.
internal static class DoomModOverlapPreflight
{
    private const int MaxEntriesPerMod = 100000;
    private static bool IsGameResource(string relative) =>
        relative.StartsWith("generated/", StringComparison.OrdinalIgnoreCase)
        || relative.StartsWith("maps/", StringComparison.OrdinalIgnoreCase);

    private static IEnumerable<string> Paths(DoomUserMods.ModEntry mod)
    {
        if (File.Exists(mod.FullPath))
        {
            using (var zip = ZipFile.OpenRead(mod.FullPath))
            {
                if (zip.Entries.Count > MaxEntriesPerMod)
                    throw new InvalidDataException("Mod ZIP has too many entries: " + mod.Name);
                foreach (var entry in zip.Entries)
                    if (!entry.FullName.EndsWith("/", StringComparison.Ordinal))
                        yield return entry.FullName;
            }
        }
        else if (Directory.Exists(mod.FullPath))
        {
            var root = Path.GetFullPath(mod.FullPath)
                .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            var pending = new Stack<string>();
            pending.Push(Path.GetFullPath(mod.FullPath));
            while (pending.Count != 0)
            {
                var folder = pending.Pop();
                if ((File.GetAttributes(folder) & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidOperationException("Linked DOOM mod folder: " + mod.Name);
                foreach (var file in Directory.GetFiles(folder))
                {
                    if ((File.GetAttributes(file) & FileAttributes.ReparsePoint) != 0)
                        throw new InvalidOperationException("Linked DOOM mod file: " + file);
                    yield return file.Substring(root.Length);
                }
                foreach (var subfolder in Directory.GetDirectories(folder))
                    pending.Push(subfolder);
            }
        }
    }

    internal static void EnsureNoResourceOverlap(IEnumerable<DoomUserMods.ModEntry> mods)
    {
        var owners = new Dictionary<string, DoomUserMods.ModEntry>(StringComparer.OrdinalIgnoreCase);
        var allConflicts = new List<string>();
        foreach (var mod in mods)
        {
            var insideMod = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var filename in Paths(mod))
            {
                var path = filename.Replace('\\', '/').TrimStart('/');
                if (path.Split('/').Any(part => part == ".." || part == ".")
                    || path.Contains(":"))
                    throw new InvalidDataException("Unsafe resource name inside " + mod.Name);
                if (!IsGameResource(path)) continue;
                if (!insideMod.Add(path))
                    throw new InvalidDataException(
                        "Duplicate game resource within mod " + mod.Name + ": " + path);
                if (owners.TryGetValue(path, out var previous)
                    && !string.Equals(previous.Id, mod.Id, StringComparison.OrdinalIgnoreCase))
                    allConflicts.Add(path + Environment.NewLine
                        + "  Mod A: " + previous.Name + " [" + previous.Id + "]"
                        + Environment.NewLine + "  Mod B: " + mod.Name + " [" + mod.Id + "]");
                else owners[path] = mod;
            }
        }
        if (allConflicts.Count != 0)
            throw new InvalidOperationException(
                "Two selected DOOM mods replace the same game resource."
                + Environment.NewLine + "The current DML fallback cannot safely merge these."
                + Environment.NewLine + "Uncheck one of the conflicting mods before launching."
                + Environment.NewLine + "Nothing was installed."
                + Environment.NewLine + string.Join(Environment.NewLine,
                    allConflicts.Take(8)));
    }

    internal static int RunSelfTest()
    {
        var dir = Path.Combine(Path.GetTempPath(),
            "Kharvox-Overlap-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(dir);
            var first = Path.Combine(dir, "alpha.zip");
            var second = Path.Combine(dir, "beta.zip");
            using (var zip = ZipFile.Open(first, ZipArchiveMode.Create))
            {
                var a = zip.CreateEntry("generated/decls/entitydef/imp.decl");
                using (var output = new StreamWriter(a.Open())) output.Write("first");
                var readme = zip.CreateEntry("README.md");
                using (var output = new StreamWriter(readme.Open())) output.Write("instructions");
            }
            using (var zip = ZipFile.Open(second, ZipArchiveMode.Create))
            {
                var a = zip.CreateEntry("generated/decls/entitydef/imp.decl");
                using (var output = new StreamWriter(a.Open())) output.Write("second");
                var readme = zip.CreateEntry("README.md");
                using (var output = new StreamWriter(readme.Open())) output.Write("different readme");
            }
            var mods = new[]
            {
                new DoomUserMods.ModEntry("user:a", "alpha.zip", first, false),
                new DoomUserMods.ModEntry("user:b", "beta.zip", second, false)
            };
            EnsureNoResourceOverlap(mods.Take(1));
            var refused = false;
            try { EnsureNoResourceOverlap(mods); }
            catch (InvalidOperationException ex)
            {
                refused = ex.Message.Contains("imp.decl")
                    && ex.Message.Contains("alpha.zip")
                    && ex.Message.Contains("beta.zip");
            }
            if (!refused)
                throw new InvalidDataException("A resource conflict passed without an error.");
            Console.WriteLine("KHARVOX DML fallback overlap checks passed.");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("DML fallback overlap test failed: " + ex);
            return 1;
        }
        finally
        {
            try { Directory.Delete(dir, true); } catch { }
        }
    }
}
