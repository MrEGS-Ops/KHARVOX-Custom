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

            form.SelectLaunchProfileForSelfTest(0);
            if (!Control<CheckBox>(form, "intense").Enabled
                || !Control<CheckBox>(form, "handsJump").Enabled
                || !Control<ComboBox>(form, "backWeapon").Enabled)
                throw new InvalidDataException("Original profiles were not unlocked.");
            Control<CheckBox>(form, "laserSight").Checked = true;
            if (profile.SelectedIndex != 0)
                throw new InvalidDataException(
                    "Editing a different profile must not silently select MrEGS_.");

            form.SelectLaunchProfileForSelfTest(LauncherPresetPolicy.MrEgsPresetIndex);
            CheckLocks();
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
