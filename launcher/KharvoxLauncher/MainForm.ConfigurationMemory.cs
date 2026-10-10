namespace KharvoxLauncher;

// Live, user-authored compatibility memory. This is deliberately a separate
// launch-UI concern, not VR gameplay/controller remapping.
public sealed partial class MainForm
{
    private readonly Label configurationStatusMods = MakeConfigurationStatus();
    private bool configurationTrackingReady;
    private bool restoringConfiguration;
    private bool showingConfigurationWarning;
    private string lastConfigurationKey = "";
    private ConfigurationSnapshot? lastAcceptedConfiguration;

    private sealed class ConfigurationSnapshot
    {
        internal Dictionary<Control, object> Values = new();
        internal HashSet<string> DoomMods = new(StringComparer.OrdinalIgnoreCase);
    }

    private static Label MakeConfigurationStatus() => new()
    {
        Text = "? UNMARKED",
        AutoSize = false,
        Width = 136,
        Height = 28,
        TextAlign = ContentAlignment.MiddleCenter,
        ForeColor = Color.Silver,
        BackColor = Color.FromArgb(32, 32, 35),
        Font = new Font("Segoe UI", 9F, FontStyle.Bold),
        AccessibleName = "Current KHARVOX configuration is unmarked"
    };

    private Button BuildCompactConfigurationControl()
    {
        var button = new Button
        {
            Text = "? Unmarked  ▾", Width = 126, Height = 27, AutoSize = false,
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(39, 39, 42),
            ForeColor = Color.Silver, Font = new Font("Segoe UI", 8.25F, FontStyle.Bold),
            AccessibleName = "Mark this configuration good, bad or unmarked"
        };
        button.FlatAppearance.BorderColor = Color.FromArgb(80, 80, 85);
        configurationStatusMods.Visible = false;
        var menu = new ContextMenuStrip
        {
            BackColor = Color.FromArgb(35, 35, 38),
            ForeColor = Color.Gainsboro, ShowImageMargin = false
        };
        menu.Items.Add("✓  Mark Good", null,
            (_, _) => MarkCurrentConfiguration(KharvoxConfigMarks.Verdict.Good));
        menu.Items.Add("✕  Mark Bad", null,
            (_, _) => MarkCurrentConfiguration(KharvoxConfigMarks.Verdict.Bad));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("?  Clear Mark", null,
            (_, _) => MarkCurrentConfiguration(KharvoxConfigMarks.Verdict.Unmarked));
        button.Click += (_, _) => menu.Show(button, new Point(0, button.Height));
        configurationStatusButton = button;
        statusToolTip.SetToolTip(button,
            "Personal rating for these settings and mod contents."
            + Environment.NewLine + "Click to mark this combination Good, Bad or Unmarked.");
        return button;
    }

    private Button? configurationStatusButton;

    private IEnumerable<Control> ConfigurationInputs()
    {
        foreach (var control in new Control[]
        {
            doomPath, preset, rendererMode, turnMode, movementDirection, weaponMode,
            calibrationWeapon, gripAlignment, leftHandSwapMode, physicalGlorykillHands,
            backWeapon, renderScale, snapAngle, smoothSpeed, physicalGlorykillSpeed,
            gloryKillSlowmo, intense, cinematicFreelook, otherCinematicsInQuad,
            cinewindowFollowsHeadset, virtualGunstock, physicalGlorykill, laserSight,
            leftHanded, swapJumpCrouch, hudDebugging, extendedLogging, captureEyes,
            disableAa, handsJump, disableVrIntro, showHands, calibrateHands,
            enableBhaptics, usePsvr2Toolkit, useFsrUpscaling, weaponWheelRemap,
            customDisableHud, customDisableWeaponWheel, customGaussChargeSlowMovement,
            customBackOfHandHud, customHandFocusedRs, customDirectionalDash,
            customBehindHeadWeaponWheel, customBehindHeadWheelHandSelection,
            customPhysicalCrouch, customRevengeDemon, customDynamicShoulderHolster,
            customPhysicalGrenadeThrow, customMotionGloryKillSpeed,
            customPhysicalChainsawGestures
        }) yield return control;
    }

    private ConfigurationSnapshot CaptureConfiguration()
    {
        var snapshot = new ConfigurationSnapshot
        {
            DoomMods = DoomUserMods.LoadSelections()
        };
        foreach (var control in ConfigurationInputs())
        {
            snapshot.Values[control] = control switch
            {
                CheckBox check => check.Checked,
                ComboBox combo => combo.SelectedIndex,
                TrackBar slider => slider.Value,
                NumericUpDown number => number.Value,
                TextBox field => field.Text,
                _ => throw new InvalidOperationException(
                    "Unsupported KHARVOX configuration control.")
            };
        }
        return snapshot;
    }

    private void RestoreConfiguration(ConfigurationSnapshot snapshot)
    {
        restoringConfiguration = true;
        try
        {
            // Programmatic WinForms changes raise the same events as clicks.
            // Suppress reputation warnings until every value is restored.
            foreach (var entry in snapshot.Values)
            {
                switch (entry.Key)
                {
                    case CheckBox check: check.Checked = (bool)entry.Value; break;
                    case ComboBox combo: combo.SelectedIndex = (int)entry.Value; break;
                    case TrackBar slider: slider.Value = (int)entry.Value; break;
                    case NumericUpDown number: number.Value = (decimal)entry.Value; break;
                    case TextBox field: field.Text = (string)entry.Value; break;
                }
            }
            DoomUserMods.SaveSelections(new HashSet<string>(snapshot.DoomMods,
                StringComparer.OrdinalIgnoreCase));
            SaveCustomModSettings();
            SaveSettings();
            // Refresh after the checkbox event has finished; don't dispose a
            // mod checkbox while its own CheckedChanged event is executing.
            if (IsHandleCreated && !IsDisposed)
                BeginInvoke((Action)RefreshUserDoomMods);
            UpdateSpeedSliderLabels();
        }
        finally { restoringConfiguration = false; }
    }

    private string ConfigurationKey()
    {
        // Properties describe the actual effective launch settings. The
        // preset name or checkbox order alone does not change game behaviour.
        return KharvoxConfigMarks.Fingerprint(
            CreateLaunchOptions(), ReadCustomModSettingsFromControls(),
            KharvoxModContentFingerprints.ResolveSelected(
                doomPath.Text, DoomUserMods.LoadSelections()));
    }

    private void DisplayConfigurationVerdict(KharvoxConfigMarks.Verdict verdict)
    {
        var (label, colour) = verdict switch
        {
            KharvoxConfigMarks.Verdict.Good => ("✓ GOOD", Color.FromArgb(119, 224, 144)),
            KharvoxConfigMarks.Verdict.Bad => ("✕ BAD", Color.FromArgb(255, 117, 117)),
            _ => ("? UNMARKED", Color.Silver)
        };
        configurationStatusMods.Text = label;
        configurationStatusMods.ForeColor = colour;
        if (configurationStatusButton is { } button)
        {
            button.Text = verdict switch
            {
                KharvoxConfigMarks.Verdict.Good => "✓ Good  ▾",
                KharvoxConfigMarks.Verdict.Bad => "✕ Bad  ▾",
                _ => "? Unmarked  ▾"
            };
            button.ForeColor = colour;
            button.AccessibleName = "Current configuration: " + label
                + ". Click to change its personal rating.";
            statusToolTip.SetToolTip(button, "Personal rating: " + label
                + ". Click to mark Good, Bad or Unmarked.");
        }
    }

    private void StartConfigurationTracking()
    {
        configurationTrackingReady = true;
        CheckLiveConfiguration(promptOnBad: false);
    }

    // Called synchronously from each option/checkbox event, not at Launch.
    private void CheckLiveConfiguration(bool promptOnBad = true)
    {
        if (customModsSaveFailed)
        {
            configurationStatusMods.Text = "! UNSAVED";
            if (configurationStatusButton is { } button)
            {
                button.Text = "! Unsaved";
                button.ForeColor = Color.Orange;
                statusToolTip.SetToolTip(button,
                    "VR mod settings were not saved. Correct the error before rating this configuration.");
            }
            return;
        }
        if (!configurationTrackingReady || restoringConfiguration
            || showingConfigurationWarning || applyingPreset || applyingCustomModDependencies)
            return;
        try
        {
            var key = ConfigurationKey();
            var verdict = KharvoxConfigMarks.Lookup(key);
            DisplayConfigurationVerdict(verdict);

            if (string.Equals(key, lastConfigurationKey, StringComparison.Ordinal))
                return;

            if (promptOnBad && verdict == KharvoxConfigMarks.Verdict.Bad
                && lastAcceptedConfiguration is not null)
            {
                showingConfigurationWarning = true;
                DialogResult choice;
                try
                {
                    choice = MessageBox.Show(this,
                        "You've previously marked this exact KHARVOX configuration as BAD."
                        + Environment.NewLine + Environment.NewLine
                        + "These settings and selected mods may cause problems or prevent DOOM from running."
                        + Environment.NewLine + Environment.NewLine
                        + "PROCEED (OK): Keep this configuration so you can continue experimenting."
                        + Environment.NewLine
                        + "CANCEL: Undo the change that brought you back to this configuration.",
                        "KHARVOX — Known bad configuration",
                        MessageBoxButtons.OKCancel, MessageBoxIcon.Warning,
                        MessageBoxDefaultButton.Button2);
                }
                finally { showingConfigurationWarning = false; }

                if (choice != DialogResult.OK)
                {
                    RestoreConfiguration(lastAcceptedConfiguration);
                    DisplayConfigurationVerdict(KharvoxConfigMarks.Lookup(lastConfigurationKey));
                    return;
                }
            }

            var currentSnapshot = CaptureConfiguration();
            // Same checkbox/settings values with a changed fingerprint mean
            // that a selected mod changed on disk. Keep this informational.
            if (verdict == KharvoxConfigMarks.Verdict.Unmarked
                && lastAcceptedConfiguration is not null
                && lastAcceptedConfiguration.DoomMods.SetEquals(currentSnapshot.DoomMods)
                && lastAcceptedConfiguration.Values.Count == currentSnapshot.Values.Count
                && lastAcceptedConfiguration.Values.All(previous =>
                    currentSnapshot.Values.TryGetValue(previous.Key, out var value)
                    && Equals(previous.Value, value)))
            {
                configurationStatusMods.Text = "? MOD CHANGED";
                if (configurationStatusButton is { } button)
                {
                    button.Text = "? Mod changed  ▾";
                    statusToolTip.SetToolTip(button, "Selected mod content changed."
                        + Environment.NewLine + "This exact combination is unmarked.");
                }
            }
            lastConfigurationKey = key;
            lastAcceptedConfiguration = currentSnapshot;
        }
        catch (Exception error)
        {
            configurationStatusMods.Text = "! UNKNOWN";
            if (configurationStatusButton is { } button)
            {
                button.Text = "! Unknown  ▾";
                button.ForeColor = Color.Orange;
                statusToolTip.SetToolTip(button, error.Message);
            }
            // An unreadable state is not equivalent to an unmarked state.
            // Don't erase saved information or block the core launcher.
        }
    }

    private void MarkCurrentConfiguration(KharvoxConfigMarks.Verdict verdict)
    {
        if (!configurationTrackingReady) return;
        if (customModsSaveFailed)
        {
            MessageBox.Show(this, "Save VR mod settings before rating this configuration.",
                "KHARVOX — Unsaved settings", MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
            return;
        }
        try
        {
            var key = ConfigurationKey();
            KharvoxConfigMarks.Set(key, verdict);
            lastConfigurationKey = key;
            lastAcceptedConfiguration = CaptureConfiguration();
            DisplayConfigurationVerdict(verdict);
        }
        catch (Exception error)
        {
            MessageBox.Show(this, error.Message, "KHARVOX — Cannot save configuration mark",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
    // Windows-only smoke test for visual structure. No DOOM install, OpenXR
    // runtime, game process or controller remapping is needed.
    internal static int RunCustomModsUiSelfTest()
    {
        var path = Path.Combine(Path.GetTempPath(),
            "KHARVOX-UI-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            using var main = new MainForm(path);
            var window = main.customOptionsForm ??
                throw new InvalidOperationException("Custom Mods window was not created.");
            if (window.ClientSize.Width > 930)
                throw new InvalidDataException("Custom Mods window is too wide.");

            IEnumerable<Control> Descendants(Control root)
            {
                foreach (Control child in root.Controls)
                {
                    yield return child;
                    foreach (var nested in Descendants(child)) yield return nested;
                }
            }

            var groups = Descendants(window).OfType<GroupBox>().ToArray();
            var doom = groups.Single(x => x.Text == "DOOM MODS");
            var vr = groups.Single(x => x.Text == "VR MODS");
            if (doom.Parent is not TableLayoutPanel columns
                || !ReferenceEquals(vr.Parent, columns)
                || columns.GetColumn(doom) != 0 || columns.GetColumn(vr) != 1)
                throw new InvalidDataException("DOOM / VR column ordering is incorrect.");

            var layout = vr.Controls.OfType<TableLayoutPanel>().Single();
            var checks = layout.Controls.OfType<CheckBox>()
                .OrderBy(check => layout.GetRow(check)).ToArray();
            if (checks.Length != 15 || layout.AutoScroll
                || layout.GetRow(checks[14]) != layout.GetRow(checks[13]) + 1
                || layout.RowStyles[layout.GetRow(checks[14])].Height != 28)
                throw new InvalidDataException("VR mod list count or scrolling changed.");
            var labels = checks.Select(check =>
            {
                var label = check.Text;
                var delimiter = label.IndexOf(". ", StringComparison.Ordinal);
                if (delimiter < 0) throw new InvalidDataException("Mod is not numbered.");
                return label.Substring(delimiter + 2).Split('—')[0].Trim();
            }).ToArray();
            if (!labels.SequenceEqual(labels.OrderBy(x => x,
                    StringComparer.OrdinalIgnoreCase)))
                throw new InvalidDataException("VR mod names are not alphabetical.");
            if (!checks.Any(x => x.Text.Contains("Requires #"))
                || !checks.Any(x => x.Text.Contains("Disables #")))
                throw new InvalidDataException("VR dependencies are not numbered.");

            if (main.gloryKillSpeedMenu.DropDownStyle != ComboBoxStyle.DropDownList
                || main.gloryKillSpeedMenu.Items.Count != 5
                || !Equals(main.gloryKillSpeedMenu.Items[0], "1")
                || !Equals(main.gloryKillSpeedMenu.Items[4], "5")
                || GKMenuToEngine(1) != 10 || GKMenuToEngine(5) != 0)
                throw new InvalidDataException("Glory Kill speed dropdown is missing.");
            if (main.configurationStatusButton?.Parent is null
                || Descendants(main).Any(x => x == main.configurationStatusButton))
                throw new InvalidDataException(
                    "Rating menu must only appear inside Custom Mods.");

            // The Windows Forms layout must be realized to catch rows that
            // look correct in source but render empty at runtime.
            window.Show();
            Application.DoEvents();
            window.PerformLayout();
            var doomGrid = doom.Controls.OfType<TableLayoutPanel>().Single();
            doomGrid.PerformLayout();
            if (doomGrid.RowCount != 4
                || doomGrid.RowStyles[2].SizeType != SizeType.Absolute
                || doomGrid.RowStyles[2].Height != 32)
                throw new InvalidDataException("DOOM mod toolbar row isn't fixed-height.");
            var loaderRow = doomGrid.GetControlFromPosition(0, 0)
                as TableLayoutPanel;
            if (loaderRow is null || loaderRow.ColumnCount != 2)
                throw new InvalidDataException("DOOMModLoader status row changed.");
            var installer = loaderRow.Controls.OfType<Button>().SingleOrDefault(button =>
                loaderRow.GetColumn(button) == 1);
            if (installer is null)
                throw new InvalidDataException("DML install button isn't right-aligned.");

            var packagedRow = doomGrid.GetControlFromPosition(0, 1);
            var userHeader = doomGrid.GetControlFromPosition(0, 2)
                as TableLayoutPanel;
            var modListPanel = doomGrid.GetControlFromPosition(0, 3);
            if (packagedRow is null || userHeader is null || modListPanel is null
                || userHeader.Parent != doomGrid || !userHeader.Visible
                || userHeader.ColumnCount != 4
                || userHeader.Controls.OfType<Panel>().All(panel =>
                    panel.AccessibleName != "User mods header divider"
                    || userHeader.GetColumn(panel) != 1)
                || Descendants(modListPanel).OfType<Label>().Any(label =>
                    label.Text.IndexOf("PLANNED MODS", StringComparison.OrdinalIgnoreCase) >= 0))
                throw new InvalidDataException("USER MODS header/section layout regressed.");
            userHeader.PerformLayout();
            var heading = userHeader.Controls.OfType<Label>().SingleOrDefault(label =>
                label.Text == "USER MODS");
            var refresh = userHeader.Controls.OfType<Button>().SingleOrDefault(button =>
                button.AccessibleName == "Rescan mods");
            var folder = userHeader.Controls.OfType<Button>().SingleOrDefault(button =>
                button.Text == "Mod Folder");
            if (heading is null || refresh is null || folder is null
                || !heading.Visible || heading.Height < 15
                || refresh.Height < 18 || folder.Height < 18
                || userHeader.GetColumn(refresh) != 2
                || userHeader.GetColumn(folder) != 3
                || userHeader.ColumnStyles[3].Width != loaderRow.ColumnStyles[1].Width
                || installer.Width != folder.Width || installer.Height != folder.Height
                || refresh.ForeColor != Color.LightSkyBlue)
                throw new InvalidDataException(
                    "USER MODS heading, buttons, sizing or colour regressed.");
            if (folder.Bounds.Right <= refresh.Bounds.Right
                || refresh.Bounds.Right <= heading.Bounds.Right
                || installer.Left + loaderRow.Left != folder.Left + userHeader.Left)
                throw new InvalidDataException(
                    "DML install and Mod Folder button alignment regressed.");
            var status = Descendants(modListPanel).OfType<Label>()
                .SingleOrDefault(label => label.Text.Contains("mod(s) detected."));
            if (status is not null && status.Text != status.Text.Substring(
                    0, status.Text.IndexOf("detected.", StringComparison.Ordinal)
                    + "detected.".Length))
                throw new InvalidDataException("Mod count includes unwanted suffix.");

            Console.WriteLine("KHARVOX Custom Mods layout smoke test passed.");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("KHARVOX Custom Mods UI test failed: " + ex);
            return 1;
        }
        finally
        {
            try { if (File.Exists(path)) File.Delete(path); } catch { }
        }
    }

}
