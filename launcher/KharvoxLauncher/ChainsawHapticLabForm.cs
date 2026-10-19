using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Explicit opt-in calibration for Chainsaw Trigger Throttle. This window does
// not open an OpenXR session: saved values are sent to the game at next launch.
internal sealed class ChainsawHapticLabForm : Form
{
    private readonly ChainsawHapticProfile profile;
    private readonly ComboBox preset = new()
    {
        DropDownStyle = ComboBoxStyle.DropDownList,
        Dock = DockStyle.Fill
    };
    private readonly TrackBar[] sliders = new TrackBar[6];
    private readonly Label[] values = new Label[6];
    private bool updating;

    internal ChainsawHapticLabForm()
    {
        Text = "KHARVOX — Chainsaw Haptic Lab";
        ClientSize = new Size(720, 610);
        MinimumSize = new Size(650, 600);
        BackColor = Color.FromArgb(25, 25, 28);
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9.5f);
        StartPosition = FormStartPosition.CenterParent;
        FormBorderStyle = FormBorderStyle.Sizable;
        MaximizeBox = false;
        try { profile = ChainsawHapticProfile.LoadOrDefault(); }
        catch (Exception e) when (e is IOException || e is ArgumentException
            || e is InvalidDataException || e is InvalidOperationException)
        {
            profile = new ChainsawHapticProfile();
            MessageBox.Show(
                "Could not read the saved chainsaw haptic profile. The defaults are shown, "
                + "but your existing file will only be replaced if you select Save."
                + Environment.NewLine + e.Message,
                "Haptic Lab", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 10,
            Padding = new Padding(20, 15, 20, 15),
            BackColor = BackColor
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 47));
        for (int i = 0; i < 6; i++)
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100f / 6f));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 72));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 49));
        Controls.Add(root);

        var heading = new Label
        {
            Dock = DockStyle.Fill,
            Text = "CHAINSAW HAPTIC LAB  ·  QUEST CONTROLLERS",
            Font = new Font("Segoe UI", 13f, FontStyle.Bold),
            ForeColor = Color.WhiteSmoke,
            TextAlign = ContentAlignment.MiddleLeft
        };
        root.Controls.Add(heading, 0, 0);

        var presetRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2 };
        presetRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        presetRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        presetRow.Controls.Add(new Label
        {
            Text = "Preset", Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft, ForeColor = Color.Gainsboro
        }, 0, 0);
        preset.Items.AddRange(new object[]
        {
            "Heavy Chainsaw", "Diesel Idle", "Demon Shredder", "Jackhammer", "Custom"
        });
        presetRow.Controls.Add(preset, 1, 0);
        root.Controls.Add(presetRow, 0, 1);

        AddSlider(root, 2, 0, "Idle intensity", 0, 100,
            () => profile.IdlePercent, x => profile.IdlePercent = x,
            "Vibration when the chainsaw is equipped but the trigger is released.");
        AddSlider(root, 3, 1, "Cutting intensity", 0, 100,
            () => profile.CuttingPercent, x => profile.CuttingPercent = x,
            "Maximum cutting vibration. The trigger and hand motion still shape output.");
        AddSlider(root, 4, 2, "Pulse rate", 5, 100,
            () => profile.PulseRate, x => profile.PulseRate = x,
            "Rough vibration pulse cycles per second. This is NOT simulation tick Hz.");
        AddSlider(root, 5, 3, "Pulse duration", 5, 100,
            () => profile.PulseDurationMs, x => profile.PulseDurationMs = x,
            "Pulse ON duration in milliseconds, limited to the selected pulse period.");
        AddSlider(root, 6, 4, "Roughness", 0, 100,
            () => profile.RoughnessPercent, x => profile.RoughnessPercent = x,
            "How strongly pulse intensity varies. Zero = steady vibration.");
        AddSlider(root, 7, 5, "Motion sensitivity", 0, 100,
            () => profile.MotionSensitivityPercent, x => profile.MotionSensitivityPercent = x,
            "Strength of the vibration increase as you make cutting strokes. "
            + "This does not change execution speed.");

        var notice = new Label
        {
            Dock = DockStyle.Fill, AutoEllipsis = true,
            ForeColor = Color.FromArgb(184, 185, 191),
            Font = new Font("Segoe UI", 9f),
            Text = "Changes apply on the NEXT DOOM launch when Chainsaw Trigger Throttle "
                + "is enabled. Only the chainsaw weapon hand receives this custom haptic. "
                + "No vibration is sent to a disconnected headset from this window. "
                + "Hardware may limit pulse frequency and texture.",
            TextAlign = ContentAlignment.MiddleLeft
        };
        root.Controls.Add(notice, 0, 8);

        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false, Padding = new Padding(0, 6, 0, 0)
        };
        var save = MakeButton("Save calibration", true);
        var cancel = MakeButton("Cancel", false);
        var reset = MakeButton("Restore defaults", false);
        buttons.Controls.Add(save);
        buttons.Controls.Add(cancel);
        buttons.Controls.Add(reset);
        root.Controls.Add(buttons, 0, 9);

        preset.SelectedIndexChanged += (_, _) =>
        {
            if (updating || preset.SelectedIndex < 0 || preset.Text == "Custom") return;
            var newProfile = ChainsawHapticProfile.Preset(preset.Text);
            profile.Name = newProfile.Name;
            profile.IdlePercent = newProfile.IdlePercent;
            profile.CuttingPercent = newProfile.CuttingPercent;
            profile.PulseRate = newProfile.PulseRate;
            profile.PulseDurationMs = newProfile.PulseDurationMs;
            profile.RoughnessPercent = newProfile.RoughnessPercent;
            profile.MotionSensitivityPercent = newProfile.MotionSensitivityPercent;
            SyncSliders();
        };
        updating = true;
        preset.SelectedItem = preset.Items.Contains(profile.Name) ? profile.Name : "Custom";
        updating = false;
        SyncSliders();
        save.Click += (_, _) =>
        {
            try
            {
                profile.Validate();
                profile.Save(ChainsawHapticProfile.DefaultPath);
                DialogResult = DialogResult.OK;
                Close();
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException
                || ex is ArgumentException || ex is InvalidDataException)
            {
                MessageBox.Show(this, "Could not save the calibration: " + ex.Message,
                    "Haptic Lab", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        };
        cancel.Click += (_, _) => Close();
        reset.Click += (_, _) => preset.SelectedItem = "Heavy Chainsaw";
        AcceptButton = save;
        CancelButton = cancel;
    }

    private static Button MakeButton(string title, bool emphasis) => new()
    {
        Text = title, AutoSize = false, Size = new Size(emphasis ? 154 : 134, 31),
        FlatStyle = FlatStyle.Flat,
        ForeColor = Color.WhiteSmoke,
        BackColor = emphasis ? Color.FromArgb(115, 54, 38) : Color.FromArgb(48, 48, 52),
        Margin = new Padding(7, 0, 0, 0)
    };

    private void AddSlider(TableLayoutPanel root, int row, int index,
        string name, int minimum, int maximum, Func<int> read, Action<int> write,
        string tooltip)
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 3, Margin = new Padding(0, 2, 0, 2)
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 170));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 68));
        var title = new Label
        {
            Text = name, Dock = DockStyle.Fill, ForeColor = Color.Gainsboro,
            TextAlign = ContentAlignment.MiddleLeft
        };
        var bar = new TrackBar
        {
            Minimum = minimum, Maximum = maximum,
            Value = Math.Max(minimum, Math.Min(maximum, read())),
            TickStyle = TickStyle.None,
            SmallChange = 1, LargeChange = 5, Dock = DockStyle.Fill,
            Margin = new Padding(0, 8, 2, 0)
        };
        var valueLabel = new Label
        {
            Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleRight,
            ForeColor = Color.WhiteSmoke
        };
        panel.Controls.Add(title, 0, 0);
        panel.Controls.Add(bar, 1, 0);
        panel.Controls.Add(valueLabel, 2, 0);
        root.Controls.Add(panel, 0, row);
        sliders[index] = bar;
        values[index] = valueLabel;
        title.AccessibleDescription = tooltip;
        bar.AccessibleDescription = tooltip;
        bar.ValueChanged += (_, _) =>
        {
            valueLabel.Text = bar.Value.ToString() + (index == 3 ? " ms" : "%");
            if (index == 2) valueLabel.Text = bar.Value + " /s";
            if (updating) return;
            write(bar.Value);
            profile.Name = "Custom";
            updating = true;
            preset.SelectedItem = "Custom";
            updating = false;
        };
        valueLabel.Text = bar.Value + (index == 3 ? " ms" : index == 2 ? " /s" : "%");
    }

    private void SyncSliders()
    {
        updating = true;
        try
        {
            sliders[0].Value = profile.IdlePercent;
            sliders[1].Value = profile.CuttingPercent;
            sliders[2].Value = profile.PulseRate;
            sliders[3].Value = profile.PulseDurationMs;
            sliders[4].Value = profile.RoughnessPercent;
            sliders[5].Value = profile.MotionSensitivityPercent;
        }
        finally { updating = false; }
    }
}
