using System;
using System.IO;

namespace KharvoxLauncher;

// Retail KHARVOX stores editable settings beside the launcher in UserData.
// This is the same portable directory that the retired desktop manager used.
// Never overwrite an owner's existing settings or calibration.
internal static class LauncherUserData
{
    // Keep the existing portable UserData path even without the retired
    // KHARVOX_MODS_BRIDGE_FILE environment variable.
    internal static bool BridgeMode => true;

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
        // Existing standalone KHARVOX installations used AppData for the
        // original launch profile. Copy it only if portable settings do not
        // already exist. Never alter the source.
        var legacySettings = Path.Combine(Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData),
            "KHARVOX", "settings.json");
        var portableSettings = PathFor("kharvox-launcher-settings.json");
        if (File.Exists(legacySettings) && !File.Exists(portableSettings))
            File.Copy(legacySettings, portableSettings, false);
        // The retired manager previously copied the owner-calibrated
        // launcher profile on the first run. Preserve that exact first-run
        // behaviour without a manager/updater and only after checking for
        // user data from previous installations.
        var factorySettings = Path.Combine(AppContext.BaseDirectory,
            "CalibrationDefaults", "kharvox-launcher-settings.json");
        if (File.Exists(factorySettings) && !File.Exists(portableSettings))
            File.Copy(factorySettings, portableSettings, false);
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
