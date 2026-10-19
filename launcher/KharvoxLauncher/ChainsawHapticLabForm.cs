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
    private readonly System.Windows.Forms.Timer previewAckTimer = new() { Interval = 250 };
    private readonly Label previewStatus = new()
    {
        Dock = DockStyle.Fill, AutoEllipsis = true,
        ForeColor = Color.FromArgb(190, 191, 198),
        Font = new Font("Segoe UI", 9f),
        TextAlign = ContentAlignment.MiddleLeft
    };
    private string? activePreviewId;
    private int activePreviewHand;
    private int previewPollCount;

    internal ChainsawHapticLabForm()
    {
        Text = "KHARVOX — Chainsaw Haptic Lab";
        ClientSize = new Size(720, 680);
        MinimumSize = new Size(650, 650);
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
            Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 12,
            Padding = new Padding(20, 15, 20, 15),
            BackColor = BackColor
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 47));
        for (int i = 0; i < 6; i++)
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100f / 6f));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 74)); // hardware preview controls
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 43)); // acknowledgement
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 62)); // disclaimer
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 49)); // Save and Cancel
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
            "Heavy Chainsaw", "Diesel Idle", "Demon Shredder", "Jackhammer", "Brutal Chop", "Custom"
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

        // Uncommitted slider values are sent directly to KHARVOX's running
        // OpenXR session. No game start, settings save or controller remap.
        var previewPanel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 4, RowCount = 2,
            Margin = new Padding(0, 4, 0, 1),
            Padding = Padding.Empty
        };
        previewPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 145));
        previewPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        previewPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 153));
        previewPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 136));
        previewPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 33));
        previewPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 33));
        var testTitle = new Label
        {
            Text = "CONTROLLER TEST", Dock = DockStyle.Fill,
            Font = new Font("Segoe UI", 8.5f, FontStyle.Bold),
            ForeColor = Color.Gainsboro, TextAlign = ContentAlignment.MiddleLeft
        };
        previewPanel.Controls.Add(testTitle, 0, 0);
        var testMode = new ComboBox
        {
            Name = "HapticLabTestMode",
            AccessibleName = "Haptic preview mode",
            Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList
        };
        testMode.Items.AddRange(new object[] { "Cutting simulation", "Idle simulation" });
        testMode.SelectedIndex = 0;
        previewPanel.Controls.Add(testMode, 1, 0);
        var testLeft = MakeButton("Test LEFT · 6 s", true);
        testLeft.Name = "HapticLabTestLeft";
        testLeft.AccessibleName = "Test left VR controller vibration";
        testLeft.Dock = DockStyle.Fill;
        previewPanel.Controls.Add(testLeft, 2, 0);
        var testRight = MakeButton("Test RIGHT · 6 s", true);
        testRight.Name = "HapticLabTestRight";
        testRight.AccessibleName = "Test right VR controller vibration";
        testRight.Dock = DockStyle.Fill;
        previewPanel.Controls.Add(testRight, 3, 0);
        var stopTest = MakeButton("Stop test", false);
        stopTest.Name = "HapticLabStopTest";
        stopTest.AccessibleName = "Stop Haptic Lab controller test";
        stopTest.Dock = DockStyle.Fill;
        previewPanel.Controls.Add(stopTest, 2, 1);
        previewPanel.SetColumnSpan(stopTest, 2);
        root.Controls.Add(previewPanel, 0, 8);

        previewStatus.Text =
            "Launch DOOM with KHARVOX first. Press Test, then return focus to the headset. "
            + "OpenXR confirmation appears here.";
        root.Controls.Add(previewStatus, 0, 9);
        void SendPreview(int hand)
        {
            if (!KharvoxRunner.IsRunning)
            {
                previewStatus.Text = "No running KHARVOX DOOM session. Launch the game before testing controllers.";
                previewStatus.ForeColor = Color.Orange;
                return;
            }
            try
            {
                var cutting = testMode.SelectedIndex == 0;
                activePreviewId = ChainsawHapticPreview.Send(profile, hand, cutting);
                activePreviewHand = hand;
                previewPollCount = 0;
                previewStatus.Text = "Test queued for " + (hand == 0 ? "LEFT" : "RIGHT")
                    + " controller. Return to your active VR session now (up to 6 seconds).";
                previewStatus.ForeColor = Color.Gainsboro;
                previewAckTimer.Start();
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException
                || ex is ArgumentException)
            {
                previewStatus.Text = "Could not send the test: " + ex.Message;
                previewStatus.ForeColor = Color.Orange;
            }
        }
        testLeft.Click += (_, _) => SendPreview(0);
        testRight.Click += (_, _) => SendPreview(1);
        stopTest.Click += (_, _) =>
        {
            previewAckTimer.Stop();
            activePreviewId = null;
            if (KharvoxRunner.IsRunning)
            {
                try { ChainsawHapticPreview.Send(profile, 2, false); }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
            previewStatus.Text = "Stop requested. Existing DOOM weapon feedback is unaffected.";
        };
        previewAckTimer.Tick += (_, _) =>
        {
            if (activePreviewId is null) { previewAckTimer.Stop(); return; }
            if (!ChainsawHapticPreview.Acknowledged(activePreviewId, activePreviewHand))
            {
                if (++previewPollCount < 48) return;
                previewAckTimer.Stop();
                activePreviewId = null;
                previewStatus.ForeColor = Color.Orange;
                previewStatus.Text =
                    "No OpenXR acknowledgement. Keep the VR game active/focused and try again; "
                    + "the runtime may not accept haptics while unfocused.";
                return;
            }
            previewAckTimer.Stop();
            previewStatus.ForeColor = Color.LightGreen;
            previewStatus.Text = "OpenXR accepted the " +
                (activePreviewHand == 0 ? "LEFT" : "RIGHT") +
                " controller haptic command. Whether you felt it depends on the VR runtime.";
        };
        Disposed += (_, _) => { previewAckTimer.Stop(); previewAckTimer.Dispose(); };

        var notice = new Label
        {
            Dock = DockStyle.Fill, AutoEllipsis = true,
            ForeColor = Color.FromArgb(184, 185, 191),
            Font = new Font("Segoe UI", 9f),
            Text = "TEST uses the unsaved slider values on LEFT or RIGHT controller "
                + "with KHARVOX DOOM running, without affecting animation speed. "
                + "SAVE applies calibration to normal chainsaw haptics on the NEXT launch. "
                + "Controllers and runtimes may differ in pulse fidelity.",
            TextAlign = ContentAlignment.MiddleLeft
        };
        root.Controls.Add(notice, 0, 10);

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
        root.Controls.Add(buttons, 0, 11);

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
