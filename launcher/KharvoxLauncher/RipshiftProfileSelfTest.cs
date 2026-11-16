using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Windows-only UI regression: no headset, DOOM install, or VR mods required.
internal static class RipshiftProfileSelfTest
{
    private static T Control<T>(MainForm form, string name) where T : Control =>
        typeof(MainForm).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)
            ?.GetValue(form) as T
        ?? throw new InvalidDataException("Missing launcher control: " + name);

    private static void Assert(CheckBox box, bool expected)
    {
        if (box.Checked != expected || box.Enabled)
            throw new InvalidDataException(
                "RIPSHIFT lock mismatch: " + box.Text + " checked=" + box.Checked
                + " enabled=" + box.Enabled);
    }

    internal static int Run()
    {
        var path = Path.Combine(Path.GetTempPath(),
            "RIPSHIFT-profile-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            using var form = new MainForm(path);
            RipshiftLaunchProfileBridge.Watch(form);
            var profile = Control<ComboBox>(form, "preset");
            if (profile.SelectedIndex != LauncherPresetPolicy.DefaultPresetIndex
                || !string.Equals(profile.Text, "_RIPSHIFT", StringComparison.Ordinal))
                throw new InvalidDataException("RIPSHIFT must be the fresh-install default.");

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
            // Extra file/loader diagnostics are a developer opt-in, not a
            // saved RIPSHIFT launch-profile default. Do not force the switch
            // off on every launch: an explicit user choice still wins.
            var extendedLogging = Control<CheckBox>(form, "extendedLogging");
            if (extendedLogging.Checked || form.CreateLaunchOptions().ExtendedLogging)
                throw new InvalidDataException(
                    "Fresh RIPSHIFT profile must launch with Extended Logging OFF.");
            extendedLogging.Checked = true;
            CheckLocks();
            if (!form.CreateLaunchOptions().ExtendedLogging)
                throw new InvalidDataException(
                    "An explicit Extended Logging opt-in must still be supported.");
            extendedLogging.Checked = false;
            if (form.CreateLaunchOptions().ExtendedLogging)
                throw new InvalidDataException(
                    "Disabling Extended Logging must be reflected in launch options.");
            // Owner factory presets must not lock or impose render resolution
            // or FSR. The ordinary launcher baseline stays 100% / unchecked,
            // and the user can override each setting under RIPSHIFT.
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
                    "RIPSHIFT must not override explicitly selected RenderScale or FSR.");
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
            if (turnSpeed.Value != 400)
                throw new InvalidDataException("New profile smooth turn speed must be 400 degrees/s.");
            var punchSpeed = Control<TrackBar>(form, "physicalGlorykillSpeed");
            if (punchSpeed.Value != 9)
                throw new InvalidDataException("Physical Glory Kill default must be 2.8 m/s.");
            punchSpeed.Value = 10;
            CheckLocks();
            if (punchSpeed.Value != 10 || !punchSpeed.Enabled)
                throw new InvalidDataException("Physical Glory Kill threshold must stay adjustable.");
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

            RipshiftLaunchProfileBridge.SelectProfileForSelfTest(form, 0);
            if (!Control<CheckBox>(form, "intense").Enabled
                || !Control<CheckBox>(form, "handsJump").Enabled
                || !Control<ComboBox>(form, "backWeapon").Enabled)
                throw new InvalidDataException("Original profiles were not unlocked.");
            Control<CheckBox>(form, "laserSight").Checked = true;
            if (profile.SelectedIndex != 0)
                throw new InvalidDataException(
                    "Editing a different profile must not silently select RIPSHIFT.");

            // Switching to RIPSHIFT from another profile must NOT enforce a
            // saved owner's render scale or Use FSR preference.
            renderScale.Value = 135m;
            useFsr.Checked = true;
            RipshiftLaunchProfileBridge.SelectProfileForSelfTest(form, LauncherPresetPolicy.RipshiftPresetIndex);
            CheckLocks();
            if (renderScale.Value != 135m || !useFsr.Checked
                || !renderScale.Enabled || !useFsr.Enabled)
                throw new InvalidDataException(
                    "Choosing RIPSHIFT unexpectedly changed render scale or FSR.");
            if (turnSpeed.Value != 400)
                throw new InvalidDataException("Profile switch did not reset smooth turn speed.");
            turnSpeed.Value = 390;
            Control<CheckBox>(form, "laserSight").Checked = true;
            CheckLocks();
            if (turnSpeed.Value != 390)
                throw new InvalidDataException("Turn speed must remain user-adjustable.");

            Console.WriteLine("RIPSHIFT default profile, locked values, mod-conflict recovery and switching passed.");
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
