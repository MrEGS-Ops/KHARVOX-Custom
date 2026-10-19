using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Five fixed choices for repeatable in-headset A/B feedback.
// Deliberately no unsupported 'live controller preview' or calibration sliders.
internal sealed class ChainsawHapticLabForm : Form
{
    private readonly ComboBox preset = new()
    {
        Name = "ChainsawHapticPresetDropdown",
        AccessibleName = "Chainsaw haptic preset",
        Dock = DockStyle.Fill,
        DropDownStyle = ComboBoxStyle.DropDownList
    };
    private readonly Label summary = new()
    {
        Dock = DockStyle.Fill,
        ForeColor = Color.WhiteSmoke,
        Font = new Font("Segoe UI", 10f),
        TextAlign = ContentAlignment.MiddleLeft
    };
    private readonly Label parameters = new()
    {
        Dock = DockStyle.Fill,
        ForeColor = Color.FromArgb(187, 188, 193),
        Font = new Font("Segoe UI", 9f),
        TextAlign = ContentAlignment.MiddleLeft
    };

    internal ChainsawHapticLabForm()
    {
        Text = "KHARVOX — Chainsaw Haptic Presets";
        ClientSize = new Size(600, 358);
        MinimumSize = new Size(520, 340);
        StartPosition = FormStartPosition.CenterParent;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        ShowInTaskbar = false;
        BackColor = Color.FromArgb(27, 27, 30);
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9.5f);
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1, RowCount = 7,
            Padding = new Padding(20, 16, 20, 12),
            BackColor = BackColor
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 71));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
        Controls.Add(root);

        root.Controls.Add(new Label
        {
            Text = "CHAINSAW HAPTICS  /  FIVE PRESETS",
            Font = new Font("Segoe UI", 13f, FontStyle.Bold),
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = Color.WhiteSmoke
        }, 0, 0);

        var choice = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2,
            Margin = Padding.Empty, Padding = Padding.Empty
        };
        choice.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 85));
        choice.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        choice.Controls.Add(new Label
        {
            Text = "Preset",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = Color.Gainsboro
        }, 0, 0);
        preset.Items.AddRange(ChainsawHapticProfile.RecommendedPresets.Cast<object>().ToArray());
        choice.Controls.Add(preset, 1, 0);
        root.Controls.Add(choice, 0, 1);

        var savedText = new Label
        {
            Dock = DockStyle.Fill, AutoEllipsis = true,
            ForeColor = Color.FromArgb(186, 188, 193),
            Font = new Font("Segoe UI", 8.8f),
            TextAlign = ContentAlignment.MiddleLeft
        };
        root.Controls.Add(savedText, 0, 2);
        root.Controls.Add(summary, 0, 3);
        root.Controls.Add(parameters, 0, 4);
        root.Controls.Add(new Label
        {
            Dock = DockStyle.Fill,
            ForeColor = Color.FromArgb(185, 185, 192),
            Font = new Font("Segoe UI", 9f),
            TextAlign = ContentAlignment.MiddleLeft,
            Text = "Save a preset, then restart DOOM to feel it during a chainsaw "
                + "execution. Enable a chainsaw motion/trigger mod in Custom Mods. "
                + "No sliders or simulated controller-test buttons."
        }, 0, 5);

        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Margin = new Padding(0, 6, 0, 0)
        };
        var save = MakeButton("Save preset", true);
        save.Name = "SaveChainsawHapticPreset";
        var cancel = MakeButton("Cancel", false);
        buttons.Controls.Add(save);
        buttons.Controls.Add(cancel);
        root.Controls.Add(buttons, 0, 6);
        AcceptButton = save;
        CancelButton = cancel;
        cancel.Click += (_, _) => Close();

        ChainsawHapticProfile? existing = null;
        try { existing = ChainsawHapticProfile.LoadOrDefault(); }
        catch (Exception ex) when (ex is IOException || ex is ArgumentException
            || ex is InvalidDataException || ex is InvalidOperationException
            || ex is UnauthorizedAccessException)
        {
            savedText.Text = "Saved profile unreadable. Choose a preset; the old file won't "
                + "be replaced until Save is pressed.";
        }
        if (existing is not null)
        {
            savedText.Text = "Currently saved: " + existing.Name
                + "  ·  Choosing a different preset won't save it automatically.";
            // Do not mislabel hand-edited profiles as the exact standard preset.
            var exact = ChainsawHapticProfile.RecommendedPresets.FirstOrDefault(name =>
            {
                var known = ChainsawHapticProfile.Preset(name);
                return existing.Name == name
                    && existing.IdlePercent == known.IdlePercent
                    && existing.CuttingPercent == known.CuttingPercent
                    && existing.PulseRate == known.PulseRate
                    && existing.PulseDurationMs == known.PulseDurationMs
                    && existing.RoughnessPercent == known.RoughnessPercent
                    && existing.MotionSensitivityPercent == known.MotionSensitivityPercent;
            });
            if (exact is not null)
                preset.SelectedItem = exact;
        }

        void RefreshChoice()
        {
            if (preset.SelectedItem is not string name)
            {
                save.Enabled = false;
                summary.Text = "Choose one of the five recommended profiles.";
                parameters.Text = "";
                return;
            }
            var selected = ChainsawHapticProfile.Preset(name);
            summary.Text = ChainsawHapticProfile.ExplainPreset(name);
            parameters.Text =
                "Idle " + selected.IdlePercent + "%    Cutting "
                + selected.CuttingPercent + "%    Roughness "
                + selected.RoughnessPercent + "%    Pulse "
                + selected.PulseRate + "/s";
            save.Enabled = true;
        }
        preset.SelectedIndexChanged += (_, _) => RefreshChoice();
        RefreshChoice();
        save.Click += (_, _) =>
        {
            if (preset.SelectedItem is not string name) return;
            try
            {
                ChainsawHapticProfile.Preset(name).Save(
                    ChainsawHapticProfile.DefaultPath);
                DialogResult = DialogResult.OK;
                Close();
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException
                || ex is ArgumentException || ex is InvalidDataException)
            {
                MessageBox.Show(this, "Could not save the preset: " + ex.Message,
                    "Chainsaw Haptics", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        };
    }

    private static Button MakeButton(string title, bool primary) => new()
    {
        Text = title, Size = new Size(139, 31),
        FlatStyle = FlatStyle.Flat,
        ForeColor = Color.WhiteSmoke,
        BackColor = primary ? Color.FromArgb(115, 54, 38)
            : Color.FromArgb(48, 48, 52),
        Margin = new Padding(8, 0, 0, 0)
    };
}
