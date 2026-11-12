using System;
using System.IO;

namespace KharvoxMods;

// ONE visible, portable folder beside KharvoxVRModManager.exe.
// Preserve the original user-owned root preferences when migrating; if
// another copy already exists, archive the legacy one rather than deleting
// different settings, losing attachment consent, or overwriting mod choices.
internal static class PortableUserData
{
    internal static string Root => Path.Combine(AppContext.BaseDirectory, "UserData");
    internal static string PathFor(string filename) => Path.Combine(Root, filename);

    private static void MigrateOne(string root, string name)
    {
        var legacy = Path.Combine(root, name);
        if (!File.Exists(legacy)) return;
        var userData = Path.Combine(root, "UserData");
        Directory.CreateDirectory(userData);
        var current = Path.Combine(userData, name);
        if (!File.Exists(current))
        {
            File.Move(legacy, current);
            return;
        }
        // The newer UserData file ALWAYS wins; do not discard differing
        // old settings in case the owner wants to inspect them.
        if (FilesEqual(legacy, current))
        {
            try { File.Delete(legacy); }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            return;
        }
        var backupDir = Path.Combine(userData, "LegacyBackups");
        Directory.CreateDirectory(backupDir);
        var oldCopy = Path.Combine(backupDir,
            name + ".before-userdata-" + Guid.NewGuid().ToString("N"));
        File.Move(legacy, oldCopy);
    }

    private static bool FilesEqual(string a, string b)
    {
        var aBytes = File.ReadAllBytes(a);
        var bBytes = File.ReadAllBytes(b);
        if (aBytes.Length != bBytes.Length) return false;
        for (int i = 0; i < aBytes.Length; ++i)
            if (aBytes[i] != bBytes[i]) return false;
        return true;
    }

    private static void MigrateIn(string managerRoot)
    {
        Directory.CreateDirectory(Path.Combine(managerRoot, "UserData"));
        foreach (var name in new[] {
            "settings.json", "kharvox-mods-bridge.json",
            "kharvox-mods-applied.txt"
        })
            MigrateOne(managerRoot, name);
    }

    internal static void MigrateManagerSettings() => MigrateIn(AppContext.BaseDirectory);

    internal static void SelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "kharvox-portable-userdata-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            File.WriteAllText(Path.Combine(root, "settings.json"), "consented");
            File.WriteAllText(Path.Combine(root, "kharvox-mods-bridge.json"), "gunslinger-on");
            MigrateIn(root);
            var path = Path.Combine(root, "UserData");
            if (File.ReadAllText(Path.Combine(path, "settings.json")) != "consented"
                || File.ReadAllText(Path.Combine(path, "kharvox-mods-bridge.json"))
                    != "gunslinger-on"
                || File.Exists(Path.Combine(root, "settings.json")))
                throw new InvalidDataException("Portable user-data migration failed.");
            File.WriteAllText(Path.Combine(root, "settings.json"), "old-consent");
            MigrateIn(root);
            if (File.ReadAllText(Path.Combine(path, "settings.json")) != "consented"
                || Directory.GetFiles(Path.Combine(path, "LegacyBackups"),
                     "settings.json.before-userdata-*").Length != 1)
                throw new InvalidDataException("Conflicting legacy preferences lost.");
            MigrateIn(root); // idempotent, no more backups
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }
}
