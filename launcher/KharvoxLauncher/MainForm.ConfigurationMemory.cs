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

    private Panel? configurationRatingHeader;
    private Button? configurationUnmarkedButton;
    private Button? configurationGoodButton;
    private Button? configurationBadButton;

    // The native GroupBox draws its own top-border line. Keep it visible
    // from the VR MODS caption all the way to these stock icon buttons: no
    // extra divider of a different color, thickness or vertical alignment.
    private void BuildConfigurationRatingHeader(GroupBox group)
    {
        configurationStatusMods.Visible = false;
        var bar = new Panel
        {
            AccessibleName = "VR Mods configuration rating header",
            BackColor = PanelColor,
            Size = new Size(80, 23),
            Margin = Padding.Empty, Padding = Padding.Empty,
            TabStop = false
        };
        Button AddRatingButton(Image image, string symbol, string accessibleName,
            KharvoxConfigMarks.Verdict verdict)
        {
            var button = new Button
            {
                Text = "",
                Image = image,
                ImageAlign = ContentAlignment.MiddleCenter,
                AccessibleName = accessibleName,
                AccessibleDescription = symbol,
                Size = new Size(24, 22),
                AutoSize = false,
                FlatStyle = FlatStyle.Flat,
                BackColor = PanelColor,
                Margin = Padding.Empty,
                TabStop = true,
                UseVisualStyleBackColor = false
            };
            button.FlatAppearance.BorderSize = 0;
            button.FlatAppearance.MouseOverBackColor = Color.FromArgb(55, 55, 59);
            button.FlatAppearance.MouseDownBackColor = Color.FromArgb(66, 66, 70);
            button.Click += (_, _) => MarkCurrentConfiguration(verdict);
            bar.Controls.Add(button);
            return button;
        }

        configurationUnmarkedButton = AddRatingButton(StockUiIcons.Question,
            "?", "Clear configuration rating", KharvoxConfigMarks.Verdict.Unmarked);
        configurationGoodButton = AddRatingButton(StockUiIcons.Check,
            "✓", "Mark configuration good", KharvoxConfigMarks.Verdict.Good);
        configurationBadButton = AddRatingButton(StockUiIcons.Cross,
            "✕", "Mark configuration bad", KharvoxConfigMarks.Verdict.Bad);

        void FitHeader()
        {
            if (bar.IsDisposed) return;
            const int iconWidth = 24;
            const int gap = 3;
            const int width = iconWidth * 3 + gap * 2 + 2;
            // Leave the native GroupBox border untouched from the caption to
            // the first icon. Only mask the part immediately under the icons.
            bar.SetBounds(Math.Max(90, group.ClientSize.Width - width - 10),
                0, width, 23);
            configurationUnmarkedButton?.SetBounds(1, 0, iconWidth, 22);
            configurationGoodButton?.SetBounds(1 + iconWidth + gap, 0,
                iconWidth, 22);
            configurationBadButton?.SetBounds(1 + (iconWidth + gap) * 2,
                0, iconWidth, 22);
        }
        group.Controls.Add(bar);
        configurationRatingHeader = bar;
        group.SizeChanged += (_, _) => FitHeader();
        FitHeader();
        bar.BringToFront();
        UpdateConfigurationRatingButtons(KharvoxConfigMarks.Verdict.Unmarked);
    }

    private void UpdateConfigurationRatingButtons(
        KharvoxConfigMarks.Verdict? verdict, string? detail = null)
    {
        var buttons = new[]
        {
            (Button: configurationUnmarkedButton, Verdict: KharvoxConfigMarks.Verdict.Unmarked,
                Hue: Color.Silver, Action: "Clear rating (unmarked)"),
            (Button: configurationGoodButton, Verdict: KharvoxConfigMarks.Verdict.Good,
                Hue: Color.FromArgb(119, 224, 144), Action: "Mark this configuration good"),
            (Button: configurationBadButton, Verdict: KharvoxConfigMarks.Verdict.Bad,
                Hue: Color.FromArgb(255, 117, 117), Action: "Mark this configuration bad")
        };
        var status = detail ?? (verdict switch
        {
            KharvoxConfigMarks.Verdict.Good => "Current rating: Good.",
            KharvoxConfigMarks.Verdict.Bad => "Current rating: Bad.",
            KharvoxConfigMarks.Verdict.Unmarked => "Current rating: Unmarked.",
            _ => "The current rating cannot be determined."
        });
        foreach (var entry in buttons)
        {
            var button = entry.Button;
            if (button is null) continue;
            var active = verdict.HasValue && entry.Verdict == verdict.Value;
            button.Enabled = verdict.HasValue;
            // The upstream Fluent UI PNG carries the symbol, so there is no
            // Paint handler, fallback character or home-made icon geometry.
            // The active rating is indicated by a subtle background highlight.
            button.BackColor = active ? Color.FromArgb(59, 59, 63) : PanelColor;
            button.FlatAppearance.BorderSize = 0;
            statusToolTip.SetToolTip(button,
                entry.Action + "." + Environment.NewLine + status
                + Environment.NewLine + "Ratings apply to this exact settings and mod combination.");
            button.Invalidate();
        }
    }

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
        UpdateConfigurationRatingButtons(verdict);
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
            UpdateConfigurationRatingButtons(null,
                "VR mod settings were not saved. Correct the error before rating this configuration.");
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
                UpdateConfigurationRatingButtons(KharvoxConfigMarks.Verdict.Unmarked,
                    "Selected mod content changed. This exact combination is unmarked.");
            }
            lastConfigurationKey = key;
            lastAcceptedConfiguration = currentSnapshot;
        }
        catch (Exception error)
        {
            configurationStatusMods.Text = "! UNKNOWN";
            UpdateConfigurationRatingButtons(null, error.Message);
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
            if (window.ClientSize.Width > 780)
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
                || GKMenuToEngine(1) != 10 || GKMenuToEngine(5) != 0
                || main.gloryKillSpeedMenu.Width > 40
                || main.gloryKillSpeedMenu.DropDownWidth > 40
                || main.gloryKillSpeedMenu.Dock != DockStyle.None)
                throw new InvalidDataException("Glory Kill speed dropdown is missing.");
            var ratingBar = main.configurationRatingHeader;
            var ratingButtons = new[]
            {
                main.configurationUnmarkedButton,
                main.configurationGoodButton,
                main.configurationBadButton
            };
            if (ratingBar?.Parent != vr
                || ratingBar.AccessibleName != "VR Mods configuration rating header"
                || ratingButtons.Any(button => button is null)
                || ratingButtons[0]!.AccessibleDescription != "?"
                || ratingButtons[1]!.AccessibleDescription != "✓"
                || ratingButtons[2]!.AccessibleDescription != "✕"
                || ratingButtons[0]!.Image != StockUiIcons.Question
                || ratingButtons[1]!.Image != StockUiIcons.Check
                || ratingButtons[2]!.Image != StockUiIcons.Cross
                || ratingButtons.Any(button => button!.Text.Length != 0
                    || button.FlatAppearance.BorderSize != 0)
                || ratingButtons.Any(button => Descendants(main).Contains(button!))
                || ratingButtons.Any(button => !Descendants(ratingBar).Contains(button!)))
                throw new InvalidDataException(
                    "Stock image rating buttons must live in the VR MODS caption.");
            // No custom divider: the actual native GroupBox border must supply
            // the entire horizontal rule (identical thickness and colour).
            if (ratingBar.Controls.Count != 3
                || ratingBar.Controls.OfType<Panel>().Any())
                throw new InvalidDataException(
                    "A custom line is obscuring the native VR MODS border.");

            // The Windows Forms layout must be realized to catch rows that
            // look correct in source but render empty at runtime.
            window.Show();
            Application.DoEvents();
            window.PerformLayout();
            if (window.ClientSize.Width > 780
                || main.gloryKillSpeedMenu.Width > 40
                || main.gloryKillSpeedMenu.DropDownWidth > 40)
                throw new InvalidDataException(
                    "Rendered Custom Mods window or Glory Kill selector too wide.");
            ratingBar.PerformLayout();
            if (!ratingBar.Visible || ratingBar.Top > 7
                || ratingBar.Left < vr.ClientSize.Width / 2
                || ratingBar.Right > vr.ClientSize.Width
                || ratingBar.Height < 21
                || ratingButtons.Any(button => !button!.Visible
                    || button.Width != 24 || button.Height != 22
                    || button.Image is null
                    || button.FlatAppearance.BorderSize != 0
                    || string.IsNullOrEmpty(main.statusToolTip.GetToolTip(button)))
                || ratingButtons[0]!.Left >= ratingButtons[1]!.Left
                || ratingButtons[1]!.Left >= ratingButtons[2]!.Left
                || ratingButtons[2]!.Right > ratingBar.ClientSize.Width)
                throw new InvalidDataException(
                    "Rating buttons are clipped, misaligned or lack tooltips.");
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

            // GetControlFromPosition() may return null for intentionally hidden
            // KHARVOX MODS controls, even though the row is occupied.
            // Inspect actual child assignments rather than only visible cells.
            var packagedRow = doomGrid.Controls.Cast<Control>().FirstOrDefault(control =>
                doomGrid.GetRow(control) == 1);
            var userHeader = doomGrid.Controls.Cast<Control>()
                .OfType<TableLayoutPanel>().FirstOrDefault(control =>
                    doomGrid.GetRow(control) == 2);
            var modListPanel = doomGrid.Controls.Cast<Control>().FirstOrDefault(control =>
                doomGrid.GetRow(control) == 3);
            if (packagedRow is null || userHeader is null || modListPanel is null
                || userHeader.Parent != doomGrid || !userHeader.Visible
                || userHeader.ColumnCount != 4
                || userHeader.Controls.OfType<Panel>().All(panel =>
                    panel.AccessibleName != "User mods header divider"
                    || userHeader.GetColumn(panel) != 1)
                || Descendants(modListPanel).OfType<Label>().Any(label =>
                    label.Text.IndexOf("PLANNED MODS", StringComparison.OrdinalIgnoreCase) >= 0))
                throw new InvalidDataException("USER MODS header/section layout regressed."
                    + " packaged=" + (packagedRow is not null)
                    + " header=" + (userHeader is not null)
                    + " list=" + (modListPanel is not null)
                    + " visible=" + (userHeader?.Visible ?? false)
                    + " columns=" + userHeader?.ColumnCount);
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
