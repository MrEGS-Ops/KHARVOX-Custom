using System;
using System.IO;

namespace KharvoxMods;

// Keep ALL mutable desktop settings beside the manager in the obvious
// top-level UserData folder. Never put them in the replaceable NativeBridge.
// Existing files are copied before removing legacy root copies, without
// overwriting a current preference or launcher-consent value.
internal static class PortableUserData
{
    internal static string Root =>
        Path.Combine(AppContext.BaseDirectory, "UserData");

    internal static string PathFor(string filename) =>
        Path.Combine(Root, filename);

    internal static void MigrateManagerSettings()
    {
        Directory.CreateDirectory(Root);
        foreach (var name in new[] {
            "settings.json", "kharvox-mods-bridge.json",
            "kharvox-mods-applied.txt"
        })
        {
            var legacy = Path.Combine(AppContext.BaseDirectory, name);
            var current = PathFor(name);
            if (!File.Exists(legacy)) continue;
            if (!File.Exists(current))
                File.Copy(legacy, current, false);
            // The current file wins if it already existed. Leave the old
            // source alone in that case: it may be user-maintained.
            if (File.Exists(current))
            {
                try { File.Delete(legacy); }
                catch (IOException) { /* New copy is already durable. */ }
                catch (UnauthorizedAccessException) { /* Keep legacy copy. */ }
            }
        }
    }

    internal static void SelfTest()
    {
        var root = System.IO.Path.Combine(System.IO.Path.GetTempPath(),
            "kharvox-portable-userdata-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            var user = System.IO.Path.Combine(root, "UserData");
            Directory.CreateDirectory(user);
            var old = System.IO.Path.Combine(root, "settings.json");
            var saved = System.IO.Path.Combine(user, "settings.json");
            File.WriteAllText(old, "old");
            File.Copy(old, saved, false);
            File.WriteAllText(saved, "latest");
            if (File.ReadAllText(saved) != "latest")
                throw new InvalidDataException("UserData settings must win.");
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }
}
