using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Discovers user-owned resources without loading or modifying them.
internal static class DoomUserMods
{
    internal static string Folder =>
        Path.Combine(AppContext.BaseDirectory, "mods", "doom", "user");

    internal static string SelectionsFile =>
        Path.Combine(AppContext.BaseDirectory, "mods", "doom", "user-selections.json");

    internal sealed class ModEntry
    {
        internal string Id { get; }
        internal string Name { get; }
        internal string FullPath { get; }
        internal bool IsFromDoom { get; }

        internal ModEntry(string id, string name, string fullPath, bool isFromDoom)
        {
            Id = id;
            Name = name;
            FullPath = fullPath;
            IsFromDoom = isFromDoom;
        }
    }

    internal sealed class SelectionFileData
    {
        public int Schema { get; set; } = 1;
        public string[] Selected { get; set; } = Array.Empty<string>();
    }

    internal static IReadOnlyList<ModEntry> Scan(string? gameDirectory,
        string? userDirectoryOverride = null)
    {
        var result = new List<ModEntry>();
        ScanFolder(userDirectoryOverride ?? Folder, "user:", false, result);
        if (!string.IsNullOrWhiteSpace(gameDirectory))
        {
            var gameMods = Path.Combine(gameDirectory, "Mods");
            if (!string.Equals(Path.GetFullPath(gameMods),
                    Path.GetFullPath(userDirectoryOverride ?? Folder),
                    StringComparison.OrdinalIgnoreCase))
                ScanFolder(gameMods, "game:", true, result);
        }

        return result.OrderBy(x => x.IsFromDoom)
            .ThenBy(x => x.Name, StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }

    private static void ScanFolder(
        string folder, string prefix, bool isFromDoom, List<ModEntry> entries)
    {
        if (!Directory.Exists(folder)) return;
        // Direct children only. Internal mod resource directories are not mods.
        foreach (var path in Directory.EnumerateFileSystemEntries(folder,
                     "*", SearchOption.TopDirectoryOnly))
        {
            var attributes = File.GetAttributes(path);
            if ((attributes & FileAttributes.ReparsePoint) != 0)
                continue; // Don't follow junctions/symlinks to unexpected locations.

            var isDirectory = (attributes & FileAttributes.Directory) != 0;
            if (!isDirectory &&
                !Path.GetExtension(path).Equals(".zip", StringComparison.OrdinalIgnoreCase))
                continue;

            var name = Path.GetFileName(path);
            if (string.IsNullOrWhiteSpace(name) || name.StartsWith(".",
                    StringComparison.Ordinal)) continue;

            entries.Add(new ModEntry(prefix + name.ToLowerInvariant(),
                name, path, isFromDoom));
        }
    }

    internal static HashSet<string> LoadSelections(string? path = null)
    {
        try
        {
            var file = path ?? SelectionsFile;
            if (!File.Exists(file)) return new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var selection = new JavaScriptSerializer().Deserialize<SelectionFileData>(
                File.ReadAllText(file));
            return new HashSet<string>(selection?.Selected ?? Array.Empty<string>(),
                StringComparer.OrdinalIgnoreCase);
        }
        catch
        {
            // Corrupt config must never silently enable unknown mods.
            return new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        }
    }

    internal static void SaveSelections(HashSet<string> values, string? path = null)
    {
        var file = path ?? SelectionsFile;
        Directory.CreateDirectory(Path.GetDirectoryName(file)!);
        var settings = new SelectionFileData
        {
            Schema = 1,
            Selected = values.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray()
        };
        var temporary = file + ".tmp";
        File.WriteAllText(temporary, new JavaScriptSerializer().Serialize(settings));
        if (File.Exists(file))
            File.Replace(temporary, file, null);
        else
            File.Move(temporary, file);
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-Doom-Mod-Discovery-" + Guid.NewGuid().ToString("N"));
        try
        {
            var user = Path.Combine(root, "user");
            var game = Path.Combine(root, "game");
            var original = Path.Combine(game, "Mods");
            Directory.CreateDirectory(user);
            Directory.CreateDirectory(original);
            File.WriteAllText(Path.Combine(user, "B_Mod.ZIP"), "placeholder");
            Directory.CreateDirectory(Path.Combine(user, "MyUnpackedMod"));
            File.WriteAllText(Path.Combine(user, "readme.txt"), "not a mod");
            File.WriteAllText(Path.Combine(original, "Existing.zip"), "placeholder");
            Directory.CreateDirectory(Path.Combine(original, "OldMod"));
            var found = Scan(game, user);
            if (found.Count != 4
                || found.Count(x => x.IsFromDoom) != 2
                || found.Count(x => !x.IsFromDoom) != 2
                || !found.Any(x => x.Id == "user:b_mod.zip")
                || !found.Any(x => x.Id == "game:existing.zip"))
                throw new InvalidOperationException(
                    "Expected 2 user and 2 existing DOOM mods, ignoring text files.");

            var settings = Path.Combine(root, "selections.json");
            var selected = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
                { "user:b_mod.zip", "game:existing.zip" };
            SaveSelections(selected, settings);
            var persisted = LoadSelections(settings);
            if (!selected.SetEquals(persisted))
                throw new InvalidOperationException("Checkbox selections did not round-trip.");
            persisted.Remove("user:b_mod.zip");
            SaveSelections(persisted, settings);
            if (!LoadSelections(settings).SetEquals(persisted))
                throw new InvalidOperationException("Checkbox deselection did not persist.");
            File.WriteAllText(settings, "{invalid");
            if (LoadSelections(settings).Count != 0)
                throw new InvalidOperationException("Invalid selection config must fail closed.");

            Console.WriteLine("DOOM user mod discovery and selection tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("DOOM user mod discovery self-test failed: " + error);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
