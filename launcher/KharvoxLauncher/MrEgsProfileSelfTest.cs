using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Windows-only UI regression: no headset, DOOM install, or VR mods required.
internal static class MrEgsProfileSelfTest
{
    private static T Control<T>(MainForm form, string name) where T : Control =>
        typeof(MainForm).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)
            ?.GetValue(form) as T
        ?? throw new InvalidDataException("Missing launcher control: " + name);

    private static void Assert(CheckBox box, bool expected)
    {
        if (box.Checked != expected || box.Enabled)
            throw new InvalidDataException(
                "MrEGS_ lock mismatch: " + box.Text + " checked=" + box.Checked
                + " enabled=" + box.Enabled);
    }

    internal static int Run()
    {
        var path = Path.Combine(Path.GetTempPath(),
            "KHARVOX-mregs-profile-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            using var form = new MainForm(path);
            MrEgsLaunchProfileBridge.Watch(form);
            var profile = Control<ComboBox>(form, "preset");
            if (profile.SelectedIndex != LauncherPresetPolicy.DefaultPresetIndex
                || !string.Equals(profile.Text, "MrEGS_", StringComparison.Ordinal))
                throw new InvalidDataException("MrEGS_ must be the fresh-install default.");

            void CheckLocks()
            {
                Assert(Control<CheckBox>(form, "intense"), true);
                Assert(Control<CheckBox>(form, "cinematicFreelook"), true);
                Assert(Control<CheckBox>(form, "otherCinematicsInQuad"), false);
                Assert(Control<CheckBox>(form, "cinewindowFollowsHeadset"), false);
                Assert(Control<CheckBox>(form, "physicalGlorykill"), true);
                Assert(Control<CheckBox>(form, "virtualGunstock"), false);
                Assert(Control<CheckBox>(form, "laserSight"), false);
                Assert(Control<CheckBox>(form, "showHands"), true);
                Assert(Control<CheckBox>(form, "handsJump"), true);
                Assert(Control<CheckBox>(form, "leftHanded"), false);
                if (Control<ComboBox>(form, "backWeapon").Enabled)
                    throw new InvalidDataException("Shoulder Weapon must be greyed out.");
            }

            CheckLocks();
            // Owner factory presets must not lock or impose render resolution
            // or FSR. The ordinary launcher baseline stays 100% / unchecked,
            // and the user can override each setting under MrEGS_.
            var renderScale = Control<NumericUpDown>(form, "renderScale");
            var useFsr = Control<CheckBox>(form, "useFsrUpscaling");
            if (renderScale.Value != 100m || useFsr.Checked
                || !renderScale.Enabled || !useFsr.Enabled)
                throw new InvalidDataException(
                    "RenderScale and FSR must retain the normal editable launcher baseline.");
            renderScale.Value = 125m;
            useFsr.Checked = true;
            CheckLocks();
            if (renderScale.Value != 125m || !useFsr.Checked)
                throw new InvalidDataException(
                    "MrEGS_ must not override explicitly selected RenderScale or FSR.");
            if (Control<CheckBox>(form, "swapJumpCrouch").Checked)
                throw new InvalidDataException("Jump/Crouch swap must default to OFF.");
            // A saved v36 override used to keep this switch on unexpectedly.
            // Upgrade only once; future deliberate user choices remain valid.
            var oldSettingsPath = path + ".legacy";
            try
            {
                File.WriteAllText(oldSettingsPath,
                    "{\"SettingsVersion\":36,\"SwapJumpCrouch\":true,\"RendererMode\":\"VULKAN_SFS\"}");
                if (LauncherSettingsStore.Load(oldSettingsPath).SwapJumpCrouch)
                    throw new InvalidDataException("v36 jump/crouch swap migration failed.");
                File.WriteAllText(oldSettingsPath,
                    "{\"SettingsVersion\":37,\"SwapJumpCrouch\":true,\"RendererMode\":\"VULKAN_SFS\"}");
                if (!LauncherSettingsStore.Load(oldSettingsPath).SwapJumpCrouch)
                    throw new InvalidDataException("Explicit v37 jump/crouch user choice was lost.");
            }
            finally { try { File.Delete(oldSettingsPath); } catch { } }
            var turnSpeed = Control<TrackBar>(form, "smoothSpeed");
            if (turnSpeed.Value != 387)
                throw new InvalidDataException("New profile smooth turn speed must be 387 degrees/s.");
            // Disabled controls remain mutable from code. The selected profile
            // must reject a conflicting update, not just disable mouse clicks.
            Control<CheckBox>(form, "virtualGunstock").Checked = true;
            Control<CheckBox>(form, "showHands").Checked = false;
            Control<CheckBox>(form, "intense").Checked = false;
            CheckLocks();
            if (form.CreateLaunchOptions().VirtualGunstock
                || !form.CreateLaunchOptions().ShowHands
                || !form.CreateLaunchOptions().PhysicalGlorykill)
                throw new InvalidDataException("Runtime options differ from visible profile locks.");

            MrEgsLaunchProfileBridge.SelectProfileForSelfTest(form, 0);
            if (!Control<CheckBox>(form, "intense").Enabled
                || !Control<CheckBox>(form, "handsJump").Enabled
                || !Control<ComboBox>(form, "backWeapon").Enabled)
                throw new InvalidDataException("Original profiles were not unlocked.");
            Control<CheckBox>(form, "laserSight").Checked = true;
            if (profile.SelectedIndex != 0)
                throw new InvalidDataException(
                    "Editing a different profile must not silently select MrEGS_.");

            // Switching to MrEGS_ from another profile must NOT enforce a
            // saved owner's render scale or Use FSR preference.
            renderScale.Value = 135m;
            useFsr.Checked = true;
            MrEgsLaunchProfileBridge.SelectProfileForSelfTest(form, LauncherPresetPolicy.MrEgsPresetIndex);
            CheckLocks();
            if (renderScale.Value != 135m || !useFsr.Checked
                || !renderScale.Enabled || !useFsr.Enabled)
                throw new InvalidDataException(
                    "Choosing MrEGS_ unexpectedly changed render scale or FSR.");
            if (turnSpeed.Value != 387)
                throw new InvalidDataException("Profile switch did not reset smooth turn speed.");
            turnSpeed.Value = 390;
            Control<CheckBox>(form, "laserSight").Checked = true;
            CheckLocks();
            if (turnSpeed.Value != 390)
                throw new InvalidDataException("Turn speed must remain user-adjustable.");

            Console.WriteLine("MrEGS_ default profile, locked values, mod-conflict recovery and switching passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
        finally
        {
            try { if (File.Exists(path)) File.Delete(path); } catch { }
        }
    }
}
