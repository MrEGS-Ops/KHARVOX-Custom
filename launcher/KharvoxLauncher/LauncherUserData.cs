using System;
using System.IO;

namespace KharvoxLauncher;

// Portable, user-owned data stays outside NativeBridge and is never packaged.
// Existing flat UserData files are COPIED, never moved or deleted. If migration
// cannot complete, readers may continue using the original flat file.
internal static class LauncherUserData
{
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

    // Keep this explicit manifest in sync with runtimePath() in
    // src/common/RuntimePaths.h. Runtime launch markers, shader assets and
    // factory *_default.cfg files must NEVER be classified as user data.
    private static readonly string[] PortableFiles = {
        "kharvox-launcher-settings.json", "kharvox-custom-mods.json",
        "original-kharvox-location.txt", "known-configurations.json",
        "vr_live_mods_saved.cfg", "hud_headlock.cfg",
        "offhand_hud.cfg", "offhand_right_hand_positions.cfg",
        "two_hand_weapon_profiles_saved.cfg",
        "two_hand_weapon_profiles_left_primary_saved.cfg",
        "two_hand_shotgun_saved.cfg",
        "hud_role_calibration_saved.cfg", "hud_roles_saved.cfg",
        "hud_flat_calibration_saved.cfg", "hud_profile_calibration_saved.cfg",
        "hud_quad_scale_saved.cfg", "hand_models_calibration_saved.cfg",
        "weapon_yaw_saved.cfg", "weapon_roll_saved.cfg",
        "weapon_pitch_saved.cfg", "weapon_recoil_saved.cfg",
        "campaign_death_count.cfg", "campaign_run_timer.cfg",
        "level_best_records.cfg"
    };

    internal static string? CategoryFor(string file)
    {
        switch (file.ToLowerInvariant())
        {
            case "kharvox-launcher-settings.json":
            case "kharvox-custom-mods.json":
            case "original-kharvox-location.txt":
            case "vr_live_mods_saved.cfg":
            case "hud_headlock.cfg":
                return "Settings";

            case "offhand_hud.cfg":
            case "offhand_right_hand_positions.cfg":
            case "two_hand_weapon_profiles_saved.cfg":
            case "two_hand_weapon_profiles_left_primary_saved.cfg":
            case "two_hand_shotgun_saved.cfg":
            case "hud_role_calibration_saved.cfg":
            case "hud_roles_saved.cfg":
            case "hud_flat_calibration_saved.cfg":
            case "hud_profile_calibration_saved.cfg":
            case "hud_quad_scale_saved.cfg":
            case "hand_models_calibration_saved.cfg":
            case "weapon_yaw_saved.cfg":
            case "weapon_roll_saved.cfg":
            case "weapon_pitch_saved.cfg":
            case "weapon_recoil_saved.cfg":
                return "Calibrations";

            case "campaign_death_count.cfg":
            case "campaign_run_timer.cfg":
                return "Statistics";

            case "level_best_records.cfg":
                return "Records";

            case "known-configurations.json":
                return "Profiles";

            default:
                return null;
        }
    }

    private static string PreferredPath(string userRoot, string file)
    {
        var category = CategoryFor(file);
        return category is null
            ? Path.Combine(userRoot, file)
            : Path.Combine(userRoot, category, file);
    }

    // Used by every launcher settings consumer. Prefer the new location only
    // when its file exists; failed migrations leave the old file functional.
    internal static string ResolvePath(string userRoot, string file)
    {
        var preferred = PreferredPath(userRoot, file);
        if (File.Exists(preferred) || CategoryFor(file) is null)
            return preferred;
        var legacy = Path.Combine(userRoot, file);
        return File.Exists(legacy) ? legacy : preferred;
    }

    internal static string PathFor(string file) => ResolvePath(Root, file);

    private static bool HasPersonalVersion(string userRoot, string file) =>
        File.Exists(PreferredPath(userRoot, file))
        || File.Exists(Path.Combine(userRoot, file));

    // Stage inside the destination directory and rename without overwriting.
    // On interruption neither the existing target nor the original is harmed.
    private static void CopyIfMissing(string source, string target)
    {
        if (!File.Exists(source) || File.Exists(target)) return;
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        var staging = target + "." + Guid.NewGuid().ToString("N") + ".migration.tmp";
        try
        {
            File.Copy(source, staging, false);
            try { File.Move(staging, target); }
            catch (IOException) when (File.Exists(target)) { }
        }
        finally
        {
            if (File.Exists(staging)) File.Delete(staging);
        }
    }

    private static void MigrateFlatFile(string userRoot, string file)
    {
        if (CategoryFor(file) is null) return;
        CopyIfMissing(Path.Combine(userRoot, file), PreferredPath(userRoot, file));
    }

    internal static void Migrate()
    {
        if (!BridgeMode) return;
        Directory.CreateDirectory(Root);

        // All five folders are safe to create. Do not touch unknown files.
        foreach (var folder in new[] {
            "Calibrations", "Settings", "Statistics", "Records", "Profiles"
        })
            Directory.CreateDirectory(Path.Combine(Root, folder));

        // Highest priority: existing user's portable UserData files.
        foreach (var file in PortableFiles)
            MigrateFlatFile(Root, file);

        // Next priority: pre-UserData settings stored by the old desktop
        // launcher. Keep an intact copy in place for downgrade/rollback.
        foreach (var name in new[] {
            "kharvox-launcher-settings.json", "kharvox-custom-mods.json"
        })
        {
            var old = Path.Combine(AppContext.BaseDirectory, name);
            if (!File.Exists(old)) continue;
            var target = PreferredPath(Root, name);
            if (!HasPersonalVersion(Root, name))
                CopyIfMissing(old, target);
            else if (!string.Equals(File.ReadAllText(old),
                     File.ReadAllText(ResolvePath(Root, name)),
                     StringComparison.Ordinal))
            {
                CopyIfMissing(old, PathCombineLegacy(name));
            }
        }

        // Prior KHARVOX installations used AppData, which remains untouched.
        var appData = Path.Combine(Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData), "KHARVOX");
        var legacySettings = Path.Combine(appData, "settings.json");
        if (!HasPersonalVersion(Root, "kharvox-launcher-settings.json"))
            CopyIfMissing(legacySettings,
                PreferredPath(Root, "kharvox-launcher-settings.json"));

        // Fresh installs inherit approved owner-calibrated defaults only
        // after checking BOTH the old flat and new categorized locations.
        var defaultsDirectory = Path.Combine(AppContext.BaseDirectory,
            "CalibrationDefaults");
        if (!HasPersonalVersion(Root, "kharvox-launcher-settings.json"))
            CopyIfMissing(Path.Combine(defaultsDirectory,
                "kharvox-launcher-settings.json"),
                PreferredPath(Root, "kharvox-launcher-settings.json"));
        if (Directory.Exists(defaultsDirectory))
        {
            foreach (var factory in Directory.GetFiles(defaultsDirectory, "*.cfg"))
            {
                var file = Path.GetFileName(factory);
                if (!HasPersonalVersion(Root, file))
                    CopyIfMissing(factory, PreferredPath(Root, file));
            }
        }
        if (!HasPersonalVersion(Root, "known-configurations.json"))
            CopyIfMissing(Path.Combine(appData, "known-configurations.json"),
                PreferredPath(Root, "known-configurations.json"));
    }

    private static string PathCombineLegacy(string filename) =>
        Path.Combine(Root, "LegacyBackups", filename + ".before-userdata");

    // Isolated filesystem regression test: never touches a real installation.
    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "ripshift-userdata-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            foreach (var file in PortableFiles)
                if (CategoryFor(file) is null)
                    throw new InvalidDataException("Unmapped persistent file: " + file);
            if (CategoryFor("render_scale.cfg") != null
                || CategoryFor("back_weapon.cfg") != null
                || CategoryFor("hand_models_calibration_default.cfg") != null)
                throw new InvalidDataException("Runtime or factory file was classified as personal.");

            var old = Path.Combine(root, "offhand_right_hand_positions.cfg");
            File.WriteAllText(old, "original-calibration");
            if (ResolvePath(root, "offhand_right_hand_positions.cfg") != old)
                throw new InvalidDataException("Flat legacy fallback was lost.");
            MigrateFlatFile(root, "offhand_right_hand_positions.cfg");
            var grouped = Path.Combine(root, "Calibrations",
                "offhand_right_hand_positions.cfg");
            if (ResolvePath(root, "offhand_right_hand_positions.cfg") != grouped
                || File.ReadAllText(grouped) != "original-calibration"
                || File.ReadAllText(old) != "original-calibration")
                throw new InvalidDataException("Calibration migration changed the original.");
            File.WriteAllText(grouped, "new-user-edit");
            MigrateFlatFile(root, "offhand_right_hand_positions.cfg");
            if (File.ReadAllText(grouped) != "new-user-edit"
                || File.ReadAllText(old) != "original-calibration")
                throw new InvalidDataException("Rerun overwrote a user's edited profile.");

            foreach (var item in new[] {
                (File: "vr_live_mods_saved.cfg", Folder: "Settings"),
                (File: "campaign_death_count.cfg", Folder: "Statistics"),
                (File: "level_best_records.cfg", Folder: "Records"),
                (File: "known-configurations.json", Folder: "Profiles"),
                (File: "original-kharvox-location.txt", Folder: "Settings")
            })
            {
                File.WriteAllText(Path.Combine(root, item.File), item.File);
                MigrateFlatFile(root, item.File);
                if (ResolvePath(root, item.File) !=
                        Path.Combine(root, item.Folder, item.File)
                    || File.ReadAllText(ResolvePath(root, item.File)) != item.File
                    || File.ReadAllText(Path.Combine(root, item.File)) != item.File)
                    throw new InvalidDataException("Migration failed for " + item.File);
            }

            var unknown = Path.Combine(root, "some-old-unknown.cfg");
            File.WriteAllText(unknown, "untouched");
            MigrateFlatFile(root, "some-old-unknown.cfg");
            if (ResolvePath(root, "some-old-unknown.cfg") != unknown
                || File.ReadAllText(unknown) != "untouched")
                throw new InvalidDataException("Unknown legacy configuration moved.");
            return 0;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine(e);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
