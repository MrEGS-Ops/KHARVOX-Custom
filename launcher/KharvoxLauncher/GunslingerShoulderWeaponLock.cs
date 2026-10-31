using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace KharvoxLauncher;

// The official Cactus MainForm.cs remains byte-for-byte untouched.
// Only our bridge-enabled launcher greys out the old Shoulder Weapon picker
// while Gunslinger owns the physical hip and behind-head weapon selection.
internal static class GunslingerShoulderWeaponLock
{
    private static readonly BindingFlags PrivateInstance =
        BindingFlags.Instance | BindingFlags.NonPublic;
    private const string DisabledHelp =
        "Unavailable while Gunslinger is enabled in KHARVOX VR Mod Manager.";

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
        Color defaultCaptionColor, ToolTip help, bool gunslinger)
    {
        picker.Enabled = !gunslinger;
        caption.ForeColor = gunslinger
            ? Color.FromArgb(123, 130, 142) : defaultCaptionColor;
        help.SetToolTip(picker, gunslinger ? DisabledHelp : "");
        help.SetToolTip(caption, gunslinger ? DisabledHelp : "");
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
            // Mod choices are written beside the manager, never to AppData or
            // the user-selected original KHARVOX installation. An incomplete
            // concurrent JSON save is retried at the next tick.
            bool lockShoulder;
            try
            {
                lockShoulder = StandaloneModsBridge.LoadFromEnvironment()
                    ?.EffectiveGunslinger == true;
            }
            catch (Exception exception) when (
                exception is IOException ||
                exception is UnauthorizedAccessException ||
                exception is InvalidDataException ||
                exception is InvalidOperationException ||
                exception is ArgumentException)
            {
                return;
            }

            if (lastLock == lockShoulder) return;
            SetLocked(picker, caption, defaultCaptionColor, help, lockShoulder);
            lastLock = lockShoulder;
        }

        // Apply before the first display, then update when the user changes
        // Gunslinger in the adjacent manager window, without relaunching.
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

                // Either old individual physical gesture still maps to the
                // combo, preserving compatibility with pre-merge profiles.
                var older = new StandaloneModsBridge
                {
                    DynamicQuickHolsters = true
                };
                if (!older.EffectiveGunslinger)
                    throw new InvalidDataException("Legacy holster bridge not recognised.");
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
