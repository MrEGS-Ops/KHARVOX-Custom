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
}
