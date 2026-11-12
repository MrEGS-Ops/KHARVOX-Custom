using System;
using System.IO;

namespace KharvoxLauncher;

// Bridge-enabled custom KHARVOX keeps its own editable settings NEXT TO the
// standalone Mod Manager, outside the replaceable NativeBridge directory.
// Direct/original non-bridge launcher storage remains unchanged.
internal static class LauncherUserData
{
    internal static bool BridgeMode =>
        !string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable(
            "KHARVOX_MODS_BRIDGE_FILE"));

    internal static string Root
    {
        get
        {
            var native = Path.GetFullPath(AppContext.BaseDirectory)
                .TrimEnd(Path.DirectorySeparatorChar);
            return Path.Combine(
                string.Equals(Path.GetFileName(native), "NativeBridge",
                    StringComparison.OrdinalIgnoreCase)
                    ? Path.GetDirectoryName(native)! : native, "UserData");
        }
    }

    internal static string PathFor(string file) => Path.Combine(Root, file);

    internal static void Migrate()
    {
        if (!BridgeMode) return;
        Directory.CreateDirectory(Root);
        foreach (var name in new[] {
            "kharvox-launcher-settings.json",
            "kharvox-custom-mods.json"
        })
        {
            var old = Path.Combine(AppContext.BaseDirectory, name);
            if (!File.Exists(old)) continue;
            var newer = PathFor(name);
            if (!File.Exists(newer))
            {
                File.Copy(old, newer, false);
                // Original remains in bridge until its normal update
                // replaces the native directory, which is already backed up.
            }
            else if (!string.Equals(File.ReadAllText(old),
                     File.ReadAllText(newer), StringComparison.Ordinal))
            {
                var legacy = PathCombineLegacy(name);
                if (!File.Exists(legacy)) File.Copy(old, legacy);
            }
        }
        // Prior config marks were in AppData. Copy the owner's history into
        // portable UserData once but do not modify the original app data.
        var marksSource = Path.Combine(Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData),
            "KHARVOX", "known-configurations.json");
        var marksDest = PathFor("known-configurations.json");
        if (File.Exists(marksSource) && !File.Exists(marksDest))
            File.Copy(marksSource, marksDest, false);
    }

    private static string PathCombineLegacy(string filename)
    {
        var folder = Path.Combine(Root, "LegacyBackups");
        Directory.CreateDirectory(folder);
        return Path.Combine(folder, filename+".before-userdata");
    }
}
