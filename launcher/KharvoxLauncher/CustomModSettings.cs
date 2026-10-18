using System.Web.Script.Serialization;

namespace KharvoxLauncher;

internal sealed class ActionSpeedSetting
{
    public bool Enabled { get; set; }
    public int TargetHz { get; set; } = 30;
}

internal static class ActionSpeedCatalog
{
    internal static readonly string[] Controllable =
    { "glory", "chainsaw", "ledge" };
    // Story cinematics always keep native tick timing. Do not expose a
    // global tick override for their cutscene and execution-tail frames.
    // Only verified lifecycle actions are editable. Exploratory input probes
    // are read-only and live behind the single Log Test checkbox.
    internal static readonly string[] All = Controllable;

    internal static bool HasActiveOverrides(CustomModSettings settings) =>
        Controllable.Any(key =>
            settings.ActionSpeedProfiles != null
            && settings.ActionSpeedProfiles.TryGetValue(key, out var profile)
            && profile != null && profile.Enabled);


    internal static void Validate(CustomModSettings settings)
    {
        settings.ActionSpeedProfiles ??= new Dictionary<string, ActionSpeedSetting>();
        foreach (string key in All)
        {
            if (!settings.ActionSpeedProfiles.TryGetValue(key, out var profile)
                || profile is null)
                settings.ActionSpeedProfiles[key] = new ActionSpeedSetting();
            else
                profile.TargetHz = Math.Max(30, Math.Min(240, profile.TargetHz));
        }
        // Profiles from unknown or future builds never get launched.
        foreach (var key in settings.ActionSpeedProfiles.Keys.Except(All).ToArray())
            settings.ActionSpeedProfiles.Remove(key);
    }
}

internal sealed class CustomModSettings
{
    internal const int CurrentSchema = 1;

    public int Schema { get; set; } = CurrentSchema;
    public bool DisableHud { get; set; }
    public bool DisableWeaponWheel { get; set; }
    public bool GaussChargeSlowMovement { get; set; }
    // Per-build development-test selections are offered ON only once.
    // Users retain complete control after the first launch of a new test build.
    public int DevelopmentTestDefaultsRevision { get; set; }
    // Only the wrist HUD presentation is mirrored; Cactus pose is unchanged.
    public bool BackOfHandHud { get; set; }
    public bool BoxingGuard { get; set; }
    // Legacy serialized fields: the unified Full speed selector writes both
    // as true so the proven native kill-slowdown bypass remains available.
    public bool FastGloryKills { get; set; }
    public bool FastChainsawKills { get; set; }
    public bool CinematicExitHandoff { get; set; }
    public int CinematicExitHandoffDelayMs { get; set; } = 700; // fixed normal-speed GK delay; chainsaw/full speed remain 40ms
    // Native cinematic tick-Hz experiment: 1 original, 5 near-max adaptive.
    public int CinematicKillSpeedLevel { get; set; } = 1;
    public bool CinematicSpeedFineTune { get; set; }
    public bool SpeedAdvancedExpanded { get; set; }
    public int CinematicSpeedFineTuneHz { get; set; } = 35;
    public Dictionary<string, ActionSpeedSetting> ActionSpeedProfiles { get; set; } = new();
    public bool ActionSpeedLogTestEnabled { get; set; }
    public bool HandFocusedRs { get; set; }
    public bool DirectionalDash { get; set; }
    public bool BehindHeadWeaponWheel { get; set; }
    public bool LeftWristWeaponWheel { get; set; }
    public bool BehindHeadWheelHandSelection { get; set; } = false;
    public bool PhysicalCrouch { get; set; }
    public bool RevengeDemon { get; set; }
    public bool DynamicShoulderHolster { get; set; }
    public bool PersistentSuperShotgunReload { get; set; }
    public bool PhysicalGrenadeThrow { get; set; }
    public bool StoryCinematicSkip { get; set; } // legacy opt-in; timescale unreliable
    public bool StoryFreeWalk { get; set; } // separate explicit opt-in, never migrated from old skip
    public bool StoryCameraFocus { get; set; } // opt-in native authored story-camera yaw
    public bool UninterruptedStory { get; set; } // opt-in automatic, never migrated ON
    public bool MotionGloryKillSpeed { get; set; }
    public bool PhysicalChainsawGestures { get; set; }
    public bool HapticOverhaul { get; set; }

    public bool SupervisorRequired => RevengeDemon;
}

internal static class CustomModSettingsStore
{
    // Bump this only when the next KHARVOX build introduces a different
    // opt-in mod that should start checked for the user's test session.
    // Never force it on again after the user deliberately unticks it.
    internal const int CurrentDevelopmentTestDefaultsRevision = 4;
    private static readonly JavaScriptSerializer Serializer = new();

    internal static CustomModSettings ApplyDevelopmentTestDefaults(CustomModSettings value)
    {
        if (value.DevelopmentTestDefaultsRevision < 1)
        {
            // October 17: the Back of Hand HUD was accidentally left off for
            // its headset test. Enable it ONCE for both fresh and existing
            // configs, then allow the user's choice to persist normally.
            value.BackOfHandHud = true;
            value.DevelopmentTestDefaultsRevision = 1;
        }
        if (value.DevelopmentTestDefaultsRevision < 2)
        {
            // Advanced has one checkbox and is collapsed on fresh installs.
            value.CinematicSpeedFineTune = false;
            value.CinematicSpeedFineTuneHz = 35;
            value.DevelopmentTestDefaultsRevision = 2;
        }
        if (value.DevelopmentTestDefaultsRevision < 3)
        {
            // The experimental per-canvas HUD mirror distorted the stable
            // Cactus watch. Pause it for this build; enable the separate,
            // reversible camera-exit handoff for the next headset test.
            value.BackOfHandHud = false;
            value.CinematicExitHandoff = true;
            value.DevelopmentTestDefaultsRevision = 3;
        }
        if (value.DevelopmentTestDefaultsRevision < 4)
        {
            // Return to DOOM/game default once, even for users whose earlier
            // test builds forced the 40 ms exit on. Future manual choices stick.
            value.CinematicExitHandoff = false;
            value.DevelopmentTestDefaultsRevision = 4;
        }
        // Fixed measured regular-GK delay; changing JSON does not alter the native
        // action-specific 700ms/40ms selection in this experimental build.
        value.CinematicExitHandoffDelayMs = 700;
        // Legacy Fine Tune checked state is migrated into Advanced.
        value.SpeedAdvancedExpanded |= value.CinematicSpeedFineTune;
        value.CinematicSpeedFineTune = value.SpeedAdvancedExpanded;
        value.CinematicSpeedFineTuneHz = Math.Max(30, Math.Min(240, value.CinematicSpeedFineTuneHz));
        ActionSpeedCatalog.Validate(value);
        value.BackOfHandHud = false; // paused until composite image mirror exists
        return value;
    }

    internal static string DefaultPath =>
        Path.Combine(AppContext.BaseDirectory, "kharvox-custom-mods.json");

    internal static CustomModSettings Load(string? path = null)
    {
        path ??= DefaultPath;
        try
        {
            if (!File.Exists(path)) return ApplyDevelopmentTestDefaults(new CustomModSettings());
            var value = Serializer.Deserialize<CustomModSettings>(File.ReadAllText(path));
            if (value is null || value.Schema != CustomModSettings.CurrentSchema)
                throw new InvalidDataException("Invalid custom-mod settings schema.");
            value.CinematicKillSpeedLevel = Math.Max(1, Math.Min(5, value.CinematicKillSpeedLevel));
            value.CinematicSpeedFineTuneHz = Math.Max(30, Math.Min(240, value.CinematicSpeedFineTuneHz));
            return ApplyDevelopmentTestDefaults(value);
        }
        catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException
            || ex is InvalidDataException || ex is ArgumentException
            || ex is InvalidOperationException)
        {
            throw new InvalidDataException(
                "KHARVOX VR mod settings are unreadable. The existing file has not been changed."
                + Environment.NewLine + "Repair or restore: " + path, ex);
        }
    }

    internal static void Save(CustomModSettings value, string? path = null)
    {
        path ??= DefaultPath;
        value.Schema = CustomModSettings.CurrentSchema;
        value.CinematicKillSpeedLevel = Math.Max(1, Math.Min(5, value.CinematicKillSpeedLevel));
        value.CinematicSpeedFineTuneHz = Math.Max(30, Math.Min(240, value.CinematicSpeedFineTuneHz));
        // Respect user preference. Do not silently re-enable early action exit.
        value.CinematicExitHandoffDelayMs = 40;
        value.CinematicSpeedFineTune = value.SpeedAdvancedExpanded;
        ActionSpeedCatalog.Validate(value);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        var temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temp, Serializer.Serialize(value));
            if (File.Exists(path)) File.Replace(temp, path, null);
            else File.Move(temp, path);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-Custom-Settings-Test-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            var path = Path.Combine(root, "custom-mods.json");
            Save(new CustomModSettings { PhysicalCrouch = true, HapticOverhaul = true, PersistentSuperShotgunReload = true }, path);
            if (!Load(path).PhysicalCrouch || !Load(path).HapticOverhaul || !Load(path).PersistentSuperShotgunReload)
                throw new InvalidDataException("Valid custom mod option was lost.");
            // Old broad/scripted sliders must be dropped, even if saved ON.
            // Advanced is presentation only and may not deactivate presets.
            var profiles=new CustomModSettings { SpeedAdvancedExpanded=false };
            profiles.ActionSpeedProfiles["glory"]=new ActionSpeedSetting{
                Enabled=true,TargetHz=200
            };
            profiles.ActionSpeedProfiles["enemy"]=new ActionSpeedSetting{
                Enabled=true,TargetHz=240
            };
            profiles.ActionSpeedProfiles["reload"]=new ActionSpeedSetting{
                Enabled=true,TargetHz=240
            };
            profiles.ActionSpeedProfiles["scripted"]=new ActionSpeedSetting{
                Enabled=true,TargetHz=240
            };
            profiles.ActionSpeedProfiles["cinematic"]=new ActionSpeedSetting{
                Enabled=true,TargetHz=170
            };
            ActionSpeedCatalog.Validate(profiles);
            if(profiles.ActionSpeedProfiles.Count!=3
                || profiles.ActionSpeedProfiles.ContainsKey("scripted")
                || profiles.ActionSpeedProfiles.ContainsKey("cinematic")
                || profiles.ActionSpeedProfiles.ContainsKey("enemy")
                || profiles.ActionSpeedProfiles.ContainsKey("reload")
                || !profiles.ActionSpeedProfiles["glory"].Enabled
                || profiles.ActionSpeedProfiles["glory"].TargetHz!=200
                || !ActionSpeedCatalog.HasActiveOverrides(profiles))
                throw new InvalidDataException("Migrate old action settings without retaining cinematic tick override.");
            profiles.SpeedAdvancedExpanded=true;
            if(!ActionSpeedCatalog.HasActiveOverrides(profiles))
                throw new InvalidDataException("Advanced expansion must not change applied speeds.");
            profiles.SpeedAdvancedExpanded=false;
            profiles.ActionSpeedProfiles["glory"].Enabled=false;
            if(ActionSpeedCatalog.HasActiveOverrides(profiles))
                throw new InvalidDataException("Native timing must restore when all action overrides are disabled.");
            // Existing preferences receive newly tested mods exactly once.
            // A later explicit uncheck must survive all subsequent launches.
            Save(new CustomModSettings
            {
                BackOfHandHud = false, DevelopmentTestDefaultsRevision = 0
            }, path);
            var first = Load(path);
            if (first.BackOfHandHud || first.CinematicExitHandoff
                || first.CinematicExitHandoffDelayMs != 700
                || first.CinematicSpeedFineTune || first.SpeedAdvancedExpanded
                || first.CinematicSpeedFineTuneHz != 35
                || first.DevelopmentTestDefaultsRevision != CurrentDevelopmentTestDefaultsRevision)
                throw new InvalidDataException("Action exit must default to game timing on first launch.");
            first.BackOfHandHud = false;
            first.CinematicExitHandoff = true; // explicit diagnostic opt-in must persist
            first.CinematicExitHandoffDelayMs = 160; // retired override ignored
            first.SpeedAdvancedExpanded = true;
            first.CinematicSpeedFineTune = true;
            first.CinematicSpeedFineTuneHz = 38;
            Save(first, path);
            if (Load(path).BackOfHandHud || !Load(path).CinematicExitHandoff
                || Load(path).CinematicExitHandoffDelayMs != 40
                || !Load(path).CinematicSpeedFineTune || !Load(path).SpeedAdvancedExpanded
                || Load(path).CinematicSpeedFineTuneHz != 38)
                throw new InvalidDataException("Manual uncheck must persist across launcher restarts.");
            // Regression: previous builds forced this mod ON in settings.
            // Revision 4 must disable it exactly once, then retain a later ON.
            var oldActionExitPath = Path.Combine(root, "old-action-exit.json");
            Save(new CustomModSettings
            {
                DevelopmentTestDefaultsRevision = 3,
                CinematicExitHandoff = true
            }, oldActionExitPath);
            var migratedExit = Load(oldActionExitPath);
            if (migratedExit.CinematicExitHandoff
                || migratedExit.DevelopmentTestDefaultsRevision != 4)
                throw new InvalidDataException("Old forced early handoff must migrate OFF.");
            migratedExit.CinematicExitHandoff = true;
            Save(migratedExit, oldActionExitPath);
            if (!Load(oldActionExitPath).CinematicExitHandoff)
                throw new InvalidDataException("Manual action-exit ON did not persist.");
            var fresh = Load(Path.Combine(root, "new-user.json"));
            if (fresh.BackOfHandHud || fresh.CinematicExitHandoff
                || fresh.CinematicExitHandoffDelayMs != 40
                || fresh.CinematicSpeedFineTune || fresh.SpeedAdvancedExpanded)
                throw new InvalidDataException("Fresh install must retain game-default action exit.");
            File.WriteAllText(path, "{BROKEN");
            var refused = false;
            try { Load(path); }
            catch (InvalidDataException) { refused = true; }
            if (!refused || File.ReadAllText(path) != "{BROKEN")
                throw new InvalidDataException(
                    "Damaged custom mod settings must fail without resetting the file.");
            Console.WriteLine("KHARVOX custom VR mod settings safety tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("Custom VR mod config self-test failed: " + error);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
