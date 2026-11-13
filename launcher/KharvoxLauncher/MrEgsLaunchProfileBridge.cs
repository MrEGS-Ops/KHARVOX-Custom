using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Launcher-specific profile overlay. Cactus's original MainForm.cs is kept
// byte-for-byte unchanged; the modified launcher adds behaviour at runtime.
internal static class MrEgsLaunchProfileBridge
{
    private const BindingFlags PrivateInstance =
        BindingFlags.Instance | BindingFlags.NonPublic;
    private static readonly Color DisabledCaptionColor =
        Color.FromArgb(123, 130, 142);
    private const string LockedHelp =
        "Fixed by the MrEGS_ launch profile.";

    private static T Get<T>(MainForm form, string field) where T : Control =>
        typeof(MainForm).GetField(field, PrivateInstance)
            ?.GetValue(form) as T
        ?? throw new InvalidDataException("KHARVOX launcher profile control missing: " + field);

    internal static bool IsActive(MainForm form) =>
        Get<ComboBox>(form, "preset").SelectedIndex == LauncherPresetPolicy.MrEgsPresetIndex;

    internal static void SelectProfileForSelfTest(MainForm form, int index) =>
        Get<ComboBox>(form, "preset").SelectedIndex = index;

    private static Label? FindShoulderLabel(Control root)
    {
        foreach (Control child in root.Controls)
        {
            if (child is Label label
                && label.Text.Equals("Shoulder Weapon", StringComparison.Ordinal))
                return label;
            var descendant = FindShoulderLabel(child);
            if (descendant is not null) return descendant;
        }
        return null;
    }

    internal static void Watch(MainForm form)
    {
        var preset = Get<ComboBox>(form, "preset");
        if (preset.Items.Count != 4)
            throw new InvalidDataException("KHARVOX launch profile count changed unexpectedly.");

        // The fourth slot retains its old numeric ID for existing settings.
        preset.Items[LauncherPresetPolicy.MrEgsPresetIndex] = "MrEGS_";

        var immersive = Get<CheckBox>(form, "intense");
        var freelook = Get<CheckBox>(form, "cinematicFreelook");
        var regularCineWindow = Get<CheckBox>(form, "otherCinematicsInQuad");
        var followHeadset = Get<CheckBox>(form, "cinewindowFollowsHeadset");
        var gloryKills = Get<CheckBox>(form, "physicalGlorykill");
        var shoulderWeapon = Get<ComboBox>(form, "backWeapon");
        var gunstock = Get<CheckBox>(form, "virtualGunstock");
        var laserSight = Get<CheckBox>(form, "laserSight");
        var hands = Get<CheckBox>(form, "showHands");
        var handsJump = Get<CheckBox>(form, "handsJump");
        var leftHanded = Get<CheckBox>(form, "leftHanded");
        var leftHandSwap = Get<ComboBox>(form, "leftHandSwapMode");
        var turnMode = Get<ComboBox>(form, "turnMode");
        var turnSpeed = Get<TrackBar>(form, "smoothSpeed");
        var shoulderLabel = FindShoulderLabel(form);
        var shoulderLabelColor = shoulderLabel?.ForeColor ?? Color.Empty;
        var toolTip = new ToolTip { ShowAlways = true };
        var protectedCheckboxes = new[]
        {
            immersive, freelook, regularCineWindow, followHeadset, gloryKills,
            gunstock, laserSight, hands, handsJump, leftHanded
        };
        bool applying = false;

        void Apply(bool resetTurnSpeed)
        {
            if (applying || form.IsDisposed || form.Disposing) return;
            applying = true;
            try
            {
                var locked = IsActive(form);
                if (locked)
                {
                    // The desired values must be applied, not merely drawn as
                    // disabled: older JSON and programmatic events can set them.
                    immersive.Checked = true;
                    freelook.Checked = true;
                    regularCineWindow.Checked = false;
                    followHeadset.Checked = false;
                    gloryKills.Checked = true;
                    gunstock.Checked = false;
                    laserSight.Checked = false;
                    hands.Checked = true;
                    handsJump.Checked = true;
                    leftHanded.Checked = false;

                    if (resetTurnSpeed)
                    {
                        turnMode.SelectedIndex = 0; // Smooth turn, not snap.
                        turnSpeed.Value = LauncherPresetPolicy.MrEgsTurnSpeed;
                    }
                }
                immersive.Enabled = !locked;
                freelook.Enabled = !locked && immersive.Checked;
                regularCineWindow.Enabled = !locked && immersive.Checked;
                followHeadset.Enabled = !locked;
                gloryKills.Enabled = !locked;
                shoulderWeapon.Enabled = !locked;
                gunstock.Enabled = !locked;
                laserSight.Enabled = !locked;
                hands.Enabled = !locked;
                handsJump.Enabled = !locked;
                leftHanded.Enabled = !locked;
                leftHandSwap.Enabled = !locked && leftHanded.Checked;

                foreach (var checkbox in protectedCheckboxes)
                    toolTip.SetToolTip(checkbox, locked ? LockedHelp : "");
                toolTip.SetToolTip(shoulderWeapon, locked ? LockedHelp : "");
                if (shoulderLabel is not null)
                {
                    shoulderLabel.ForeColor = locked
                        ? DisabledCaptionColor : shoulderLabelColor;
                    toolTip.SetToolTip(shoulderLabel, locked ? LockedHelp : "");
                }
            }
            finally { applying = false; }
        }

        // Track explicit profile changes. No source patch to Cactus MainForm.
        preset.SelectedIndexChanged += (_, _) => Apply(resetTurnSpeed: IsActive(form));
        foreach (var checkbox in protectedCheckboxes)
            checkbox.CheckedChanged += (_, _) => Apply(resetTurnSpeed: false);

        // Original Immersive Mode and left-handed handlers can re-enable
        // child controls. Reapply after those handlers and on a lightweight
        // timer, including when adjacent mod-manager settings are refreshed.
        var poll = new System.Windows.Forms.Timer { Interval = 500 };
        poll.Tick += (_, _) => Apply(resetTurnSpeed: false);
        form.Shown += (_, _) => poll.Start();
        form.FormClosed += (_, _) =>
        {
            poll.Stop();
            poll.Dispose();
            toolTip.Dispose();
        };

        // The original launcher selects Recommended when no JSON exists.
        // Select MrEGS_ on first run; retain the actual selected preset when
        // existing user settings are loaded, to avoid losing preferences.
        var settingsPath = typeof(MainForm).GetField("SettingsPath", PrivateInstance)
            ?.GetValue(form) as string;
        var hasSavedSettings = !string.IsNullOrWhiteSpace(settingsPath)
            && File.Exists(settingsPath);
        if (!hasSavedSettings)
            preset.SelectedIndex = LauncherPresetPolicy.DefaultPresetIndex;
        Apply(resetTurnSpeed: !hasSavedSettings && IsActive(form));
    }
}
