using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace KharvoxLauncher;

// This bridge also honours the MrEGS_ profile's permanent Shoulder Weapon
// lock, independently of the Gunslinger mod state.
internal static class GunslingerShoulderWeaponLock
{
    private static readonly BindingFlags PrivateInstance =
        BindingFlags.Instance | BindingFlags.NonPublic;
    private const string DisabledHelp =
        "Gunslinger is built into KHARVOX. Configure it from the in-headset MODS menu.";

    private static ComboBox? FindPicker(MainForm form) =>
        typeof(MainForm).GetField("backWeapon", PrivateInstance)
            ?.GetValue(form) as ComboBox;

    private static Label? FindLabel(Control root)
    {
        foreach (Control child in root.Controls)
        {
            if (child is Label label && string.Equals(label.Text,
                "Shoulder Weapon", StringComparison.Ordinal))
                return label;
            var nested = FindLabel(child);
            if (nested != null) return nested;
        }
        return null;
    }

    private static void SetLocked(ComboBox picker, Label caption,
        Color defaultCaptionColor, ToolTip help, bool gunslinger, string? reason = null)
    {
        picker.Enabled = !gunslinger;
        caption.ForeColor = gunslinger
            ? Color.FromArgb(123, 130, 142) : defaultCaptionColor;
        help.SetToolTip(picker, gunslinger ? reason ?? DisabledHelp : "");
        help.SetToolTip(caption, gunslinger ? reason ?? DisabledHelp : "");
    }

    internal static void Watch(MainForm form)
    {
        var picker = FindPicker(form);
        var caption = FindLabel(form);
        if (picker == null || caption == null)
            throw new InvalidDataException(
                "Cannot locate original Shoulder Weapon controls in modified KHARVOX.");

        var defaultCaptionColor = caption.ForeColor;
        var help = new ToolTip { ShowAlways = true };
        bool? lastLock = null;

        void Refresh()
        {
            // Core Gunslinger is always enabled by the retail launcher.
            // Never query the removed Mod Manager bridge or its JSON.
            bool lockShoulder = MrEgsLaunchProfileBridge.IsActive(form) ||
                RetailVrDefaults.GunslingerAlwaysOn;

            if (lastLock == lockShoulder) return;
            SetLocked(picker, caption, defaultCaptionColor, help, lockShoulder,
                MrEgsLaunchProfileBridge.IsActive(form) ? "Locked by the MrEGS_ launch profile." : null);
            lastLock = lockShoulder;
        }

        // Apply before the launcher appears. The in-game configuration never
        // needs to re-enable this original desktop Shoulder Weapon picker.
        Refresh();
        var poll = new System.Windows.Forms.Timer { Interval = 500 };
        poll.Tick += (_, _) =>
        {
            if (!form.IsDisposed && !form.Disposing)
                Refresh();
        };
        form.Shown += (_, _) => poll.Start();
        form.FormClosed += (_, _) =>
        {
            poll.Stop();
            poll.Dispose();
            help.Dispose();
        };
    }

    internal static int RunSelfTest()
    {
        var testSettingsPath = Path.Combine(Path.GetTempPath(),
            "KHARVOX-shoulder-lock-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            using (var form = new MainForm(testSettingsPath))
            using (var help = new ToolTip())
            {
                var picker = FindPicker(form);
                var caption = FindLabel(form);
                MrEgsLaunchProfileBridge.SelectProfileForSelfTest(form, 0);
                if (picker == null || caption == null || !picker.Enabled)
                    throw new InvalidDataException("Shoulder Weapon picker missing.");

                var originalColor = caption.ForeColor;
                SetLocked(picker, caption, originalColor, help, true);
                if (picker.Enabled || caption.ForeColor == originalColor)
                    throw new InvalidDataException(
                        "Gunslinger did not disable Shoulder Weapon dropdown and label.");

                SetLocked(picker, caption, originalColor, help, false);
                if (!picker.Enabled || caption.ForeColor != originalColor)
                    throw new InvalidDataException(
                        "Turning Gunslinger off did not restore Shoulder Weapon.");

                var retail = new CustomModSettings();
                RetailVrDefaults.Apply(retail);
                if (!retail.BehindHeadWeaponWheel ||
                    !retail.DynamicShoulderHolster ||
                    !RetailVrDefaults.GunslingerAlwaysOn)
                    throw new InvalidDataException("Built-in Gunslinger policy is inactive.");
            }

            Console.WriteLine(
                "Gunslinger Shoulder Weapon dropdown disabled/re-enabled without editing Cactus UI.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}

// Keep the upstream Cactus MainForm exactly unmodified. Physical grip
// calibration is built in, so the old Virtual Gunstock option remains locked
// and the runtime independently forces gunstock OFF.
internal static class VirtualGunstockGripCalibrationLock
{
    private const string DisabledHelp =
        "Weapon Grip Calibration is built in. Virtual Gunstock is disabled.";

    private static CheckBox? FindToggle(MainForm form) =>
        typeof(MainForm).GetField("virtualGunstock",
            BindingFlags.Instance | BindingFlags.NonPublic)
            ?.GetValue(form) as CheckBox;

    private static void SetLocked(CheckBox toggle, ToolTip help, bool blocked, string? reason = null)
    {
        // Never restore a previously-checked setting automatically. An old
        // checked gunstock cannot silently reactivate after calibration ends.
        if (blocked) toggle.Checked = false;
        toggle.Enabled = !blocked;
        help.SetToolTip(toggle, blocked ? reason ?? DisabledHelp : "");
    }

    internal static void Watch(MainForm form)
    {
        var toggle = FindToggle(form);
        if (toggle == null)
            throw new InvalidDataException("Original Virtual Gunstock control missing.");

        var help = new ToolTip { ShowAlways = true };
        bool? lastLock = null;
        void Refresh()
        {
            bool blocked = MrEgsLaunchProfileBridge.IsActive(form) ||
                RetailVrDefaults.WeaponGripAlwaysOn;
            if (lastLock == blocked) return;
            SetLocked(toggle, help, blocked,
                MrEgsLaunchProfileBridge.IsActive(form) ? "Locked off by the MrEGS_ launch profile." : null);
            lastLock = blocked;
        }

        Refresh();
        var poll = new System.Windows.Forms.Timer { Interval = 500 };
        poll.Tick += (_, _) =>
        {
            if (!form.IsDisposed && !form.Disposing) Refresh();
        };
        form.Shown += (_, _) => poll.Start();
        form.FormClosed += (_, _) =>
        {
            poll.Stop();
            poll.Dispose();
            help.Dispose();
        };
    }

    internal static int RunSelfTest()
    {
        var path = Path.Combine(Path.GetTempPath(),
            "KHARVOX-gunstock-lock-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            using var form = new MainForm(path);
            using var help = new ToolTip();
            var toggle = FindToggle(form)
                ?? throw new InvalidDataException("Cactus Virtual Gunstock checkbox missing.");
            MrEgsLaunchProfileBridge.SelectProfileForSelfTest(form, 0);
            toggle.Checked = true;
            SetLocked(toggle, help, true);
            if (toggle.Enabled || toggle.Checked)
                throw new InvalidDataException(
                    "Calibration must disable and uncheck original Virtual Gunstock.");
            SetLocked(toggle, help, false);
            if (!toggle.Enabled || toggle.Checked)
                throw new InvalidDataException(
                    "Turning calibration off must not silently reactivate gunstock.");
            Console.WriteLine(
                "Built-in Weapon Grip Calibration locks Virtual Gunstock; Cactus MainForm unchanged.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}
