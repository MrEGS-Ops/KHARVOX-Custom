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
            customBoxingGuard, customHandFocusedRs, customDirectionalDash,
            customBehindHeadWeaponWheel,
            customPhysicalCrouch, customRevengeDemon, customDynamicShoulderHolster,
            customPhysicalGrenadeThrow, customPersistentSsgReload,
            customHapticOverhaul, cinematicSpeedSelector
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
            // The hidden native full-speed flag is derived from the selector.
            // Restore it explicitly because events are suppressed here.
            cinematicFullSpeedMode=false; // shared full-speed selector retired
            cinematicKillSpeed.Value=1;
            // WinForms restoration bypasses user click dependency handlers.
            ApplyCustomModDependencies(null);
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
            using var installPreview = new DoomModLoaderInstallDialog();
            if (!installPreview.PromptMatchesAction)
                throw new InvalidDataException(
                    "DOOMModLoader instruction does not match its button caption.");
            var window = main.customOptionsForm ??
                throw new InvalidOperationException("Custom Mods window was not created.");
            TestMainLauncherModLocks(main);
            if (window.ClientSize.Width < 1170 || window.ClientSize.Width > 1190)
                throw new InvalidDataException("Custom Mods initial width is incorrect.");
            // Larger width is fixed while preserving docked move/height behavior.
            main.FitCustomOptionsToContent(11);
            var expectedWidth = Math.Min(1194, Screen.FromControl(main).WorkingArea.Width);
            if (window.MinimumSize.Width != expectedWidth
                || window.Width < expectedWidth || window.Width > expectedWidth + 32
                || window.FormBorderStyle != FormBorderStyle.FixedSingle)
                throw new InvalidDataException(
                    "Custom Mods must preserve its fixed wider action-slider layout."
                    + " min=" + window.MinimumSize.Width
                    + " actual=" + window.Width);
            // Match actual outside borders exactly with no separator gap,
            // including moving both windows when space is constrained.
            var desktop = new Rectangle(0, 0, 2560, 1080);
            var dock = CalculateCustomModsDock(
                new Rectangle(1800, 57, 515, 910), window.Width, desktop);
            if (dock.Mods.Right != dock.Launcher.Left
                || dock.Mods.Top != dock.Launcher.Top
                || dock.Mods.Bottom != dock.Launcher.Bottom
                || dock.Mods.Width != window.Width
                || dock.Launcher.Width != 515
                || dock.Launcher.X != 1800)
                throw new InvalidDataException(
                    "Docked Custom Mods must touch launcher and match its height.");
            // Windows includes ~7-9px invisible resize borders in each
            // window's Bounds. The old zero-overlap calculation left a
            // conspicuous strip of desktop between their VISIBLE frames.
            // Assert the DWM inset compensation removes it without resizing
            // either window or breaking movement near the desktop edge.
            var overlap = CalculateVisibleSeamOverlap(8, 9);
            if (overlap != 17
                || CalculateVisibleSeamOverlap(-5, 7) != 7
                || CalculateVisibleSeamOverlap(80, 80) != 48)
                throw new InvalidDataException(
                    "DWM visible-frame seam overlap must be bounded and additive.");
            var snapped = CalculateCustomModsDock(
                new Rectangle(1800, 57, 515, 910), window.Width, desktop, overlap);
            if (snapped.Mods.Right - snapped.Launcher.Left != overlap
                || snapped.Mods.Width != window.Width
                || snapped.Mods.Top != snapped.Launcher.Top
                || snapped.Mods.Bottom != snapped.Launcher.Bottom
                || snapped.Launcher.Width != 515
                // Derive visible-frame positions from the two insets:
                || (snapped.Mods.Right - 8) != (snapped.Launcher.Left + 9))
                throw new InvalidDataException(
                    "DWM window frames must visibly touch with no desktop gap.");
            var edgeSnapped = CalculateCustomModsDock(
                new Rectangle(10, 20, 515, 860), window.Width, desktop, overlap);
            if (edgeSnapped.Mods.Left < desktop.Left
                || edgeSnapped.Mods.Right - edgeSnapped.Launcher.Left != overlap
                || edgeSnapped.Mods.Bottom != edgeSnapped.Launcher.Bottom)
                throw new InvalidDataException(
                    "Invisible-border overlap must survive left screen-edge docking.");

            var cramped = CalculateCustomModsDock(
                new Rectangle(10, 20, 515, 860), window.Width, desktop);
            if (cramped.Mods.Right != cramped.Launcher.Left
                || cramped.Mods.Left < desktop.Left
                || cramped.Mods.Top != cramped.Launcher.Top
                || cramped.Mods.Bottom != cramped.Launcher.Bottom)
                throw new InvalidDataException(
                    "Docking near left screen edge must move the pair together.");

            IEnumerable<Control> Descendants(Control root)
            {
                foreach (Control child in root.Controls)
                {
                    yield return child;
                    foreach (var nested in Descendants(child)) yield return nested;
                }
            }

            var footerClose = Descendants(window).OfType<Button>()
                .SingleOrDefault(button => button.Text == "Close");
            var footerRow = footerClose?.Parent as FlowLayoutPanel;
            var creatorCredit = footerRow?.Controls.OfType<Label>()
                .SingleOrDefault(label =>
                    label.AccessibleName == "Custom Mods creator credit");
            if (footerRow is null || footerRow.FlowDirection != FlowDirection.RightToLeft
                || creatorCredit is null || creatorCredit.Text != "Made by MrEGS"
                || creatorCredit.TextAlign != ContentAlignment.MiddleRight
                || creatorCredit.ForeColor != Color.Gray
                || (window.Visible && !creatorCredit.Visible) || creatorCredit.Height < 20
                || creatorCredit.Right >= footerClose!.Left
                || footerClose.Right > footerRow.ClientSize.Width
                || footerRow.Controls.Count != 2)
                throw new InvalidDataException(
                    "Made by MrEGS credit must be next to Close in the Custom Mods footer."
                    + " row=" + (footerRow is not null)
                    + " direction=" + footerRow?.FlowDirection
                    + " credit=" + (creatorCredit is not null)
                    + " text=" + creatorCredit?.Text
                    + " align=" + creatorCredit?.TextAlign
                    + " color=" + creatorCredit?.ForeColor
                    + " visible=" + creatorCredit?.Visible
                    + " height=" + creatorCredit?.Height
                    + " creditRight=" + creatorCredit?.Right
                    + " closeLeft=" + footerClose?.Left
                    + " closeRight=" + footerClose?.Right
                    + " rowWidth=" + footerRow?.ClientSize.Width
                    + " count=" + footerRow?.Controls.Count);

            // Exercise both verified and missing/repair-required DML states
            // without touching the real game or installing the loader.
            // A previously selected mod must remain checked while disabled,
            // then become selectable again after DML verification.
            foreach (var isMissing in new[] { false, true })
            {
                using var selection = new CheckBox { Checked = true };
                SetDoomModCheckboxAvailability(selection,
                    loaderVerified: false, missing: isMissing);
                if (selection.Enabled || !selection.Checked
                    || selection.ForeColor != Color.Gray)
                    throw new InvalidDataException(
                        "Unverified DML must disable and grey all resource mods.");
                SetDoomModCheckboxAvailability(selection,
                    loaderVerified: true, missing: isMissing);
                if (!selection.Enabled || !selection.Checked
                    || selection.ForeColor != (isMissing ? Color.Orange : Color.Gainsboro))
                    throw new InvalidDataException(
                        "DML verification must restore mod selection without losing saved checks.");
            }

            // The watcher must survive the deletion and recreation of both
            // the loader folder and its "tools" parent directory.
            var install = Path.Combine(Path.GetTempPath(), "KHARVOX-DML-TEST",
                "tools", "doommodloader");
            if (!IsDoomModLoaderChange(install, install)
                || !IsDoomModLoaderChange(Path.Combine(install, "DOOMModLoader.exe"), install)
                || !IsDoomModLoaderChange(Path.Combine(install, "KHARVOX-SOURCE.txt"), install)
                || !IsDoomModLoaderChange(Path.GetDirectoryName(install)!, install)
                || IsDoomModLoaderChange(Path.Combine(Path.GetDirectoryName(install)!,
                    "unrelated-tool", "file.txt"), install)
                || IsDoomModLoaderChange(Path.Combine(Path.GetTempPath(),
                    "KHARVOX-DML-TEST", "tools", "doommodloader-old"), install))
                throw new InvalidDataException(
                    "DML watcher must handle folder/file delete, restore and rename without reacting to other tools.");
            // Both lists are updated in-place (no CheckedChanged events and
            // no loss of saved checks) when the DML folder disappears.
            using (var userList = new FlowLayoutPanel())
            using (var packagedList = new FlowLayoutPanel())
            {
                var userChoice = new CheckBox { Checked = true, Tag = false };
                var missingChoice = new CheckBox { Checked = true, Tag = true };
                var packagedChoice = new CheckBox { Checked = true, Tag = false };
                userList.Controls.Add(userChoice);
                userList.Controls.Add(missingChoice);
                packagedList.Controls.Add(packagedChoice);
                ApplyDoomModLoaderGate(userList, packagedList, verified: false);
                if (userList.Controls.OfType<CheckBox>()
                        .Concat(packagedList.Controls.OfType<CheckBox>())
                        .Any(choice => choice.Enabled || !choice.Checked
                            || choice.ForeColor != Color.Gray))
                    throw new InvalidDataException(
                        "Deleting DML must immediately grey both mod lists without clearing checks.");
                ApplyDoomModLoaderGate(userList, packagedList, verified: true);
                if (!userChoice.Enabled || userChoice.ForeColor != Color.Gainsboro
                    || !packagedChoice.Enabled || packagedChoice.ForeColor != Color.Gainsboro
                    || !missingChoice.Enabled || missingChoice.ForeColor != Color.Orange
                    || !userChoice.Checked || !packagedChoice.Checked || !missingChoice.Checked)
                    throw new InvalidDataException(
                        "Restored DML must re-enable selections and preserve missing-mod colour.");
            }
            main.EnsureDoomModLoaderWatcher();
            if (main.doomModLoaderWatcher is null
                || !main.doomModLoaderWatcher.IncludeSubdirectories
                || !main.doomModLoaderWatcher.EnableRaisingEvents
                || !string.Equals(Path.GetFullPath(main.doomModLoaderWatcher.Path),
                    Path.GetFullPath(AppContext.BaseDirectory),
                    StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException(
                    "DML watcher must stay anchored to the launcher even if tools is deleted.");

            var groups = Descendants(window).OfType<GroupBox>().ToArray();
            var doom = groups.Single(x => x.Text == "DOOM MODS");
            var vr = groups.Single(x => x.Text == "VR MODS");
            if (doom.Parent is not TableLayoutPanel columns
                || !ReferenceEquals(vr.Parent, columns)
                || columns.GetColumn(doom) != 0 || columns.GetColumn(vr) != 1)
                throw new InvalidDataException("DOOM / VR column ordering is incorrect.");

            if (!RunModReorderPolicySelfTest() || !RunModPolishPolicySelfTest()
                || !RunDoomDisplayOrderPolicySelfTest())
                throw new InvalidDataException("Mod group ordering/status policy failed.");
            // The VR group also has a separate rating-header Panel.
            var scroll = vr.Controls.OfType<Panel>()
                .Single(panel => panel.Controls.OfType<TableLayoutPanel>().Any());
            var layout = scroll.Controls.OfType<TableLayoutPanel>().Single();
            var orderedRows = main.modOrderRows.ToArray();
            // Three prototype checkboxes are single-select and reside in
            // the dedicated chainsaw pacing panel, not the draggable mod list.
            if(new[] {main.chainsawGripPull,main.chainsawPointer}.Count(c=>c.Checked)!=1
                || main.SelectedChainsawExperimentMode is not (1 or 3)
                || main.chainsawGripPull.Parent!=main.speedAdvancedPanel
                || main.chainsawPointer.Parent!=main.speedAdvancedPanel
                || main.chainsawShowGripZones.Parent!=main.speedAdvancedPanel)
                throw new InvalidDataException(
                    "Chainsaw motion modes must be exclusive and remain under Advanced.");
            if (main.PausedCustomModControls().Any(mod => mod.Enabled || mod.Checked)
                || !main.weaponWheelRemap.Enabled || !main.customHandFocusedRs.Enabled
                || !main.customBehindHeadWeaponWheel.Enabled || !main.customPhysicalCrouch.Enabled
                || !main.customTriggerChainsawThrottle.Enabled
                || !main.cinematicSpeedSelector.Enabled)
                throw new InvalidDataException("Paused features must be grey while active features remain selectable.");
            var checks = orderedRows.Select(row => row.Box).ToArray();
            var expectedChecks = new[]
            {
                main.weaponWheelRemap,
                main.customHandFocusedRs,
                main.customBoxingGuard,
                main.customBehindHeadWeaponWheel,
                main.customDynamicShoulderHolster,
                main.customPhysicalCrouch,
                main.customDirectionalDash,
                main.customPhysicalGrenadeThrow,
                main.customGaussChargeSlowMovement,
                main.customPersistentSsgReload,
                main.customTriggerChainsawThrottle,
                main.customStoryCinematicSkip,
                main.customStoryFreeWalk,
                main.customStoryCameraFocus,
                main.customUninterruptedStory,
                main.customActionExitHandoff,
                main.customBackOfHandHud,
                main.customDisableHud,
                main.customDisableWeaponWheel,
                main.customRevengeDemon,
                main.customHapticOverhaul
            };
            // A persisted layout may reorder WITHIN a group. Never allow a mod
            // to cross into another category, disappear or become duplicated.
            if (!scroll.AutoScroll || layout.AutoScroll
                || checks.Length != expectedChecks.Length || checks.Distinct().Count() != expectedChecks.Length
                || !checks.OrderBy(x => Array.IndexOf(expectedChecks, x))
                    .SequenceEqual(expectedChecks)
                || orderedRows.Where((row, index) => row.Group
                    != (index < 5 ? 0 : index < 7 ? 1 : index < 11 ? 2
                        : index < 19 ? 3 : index < 20 ? 4 : 5)).Any()
                || main.modOrderSlots.Count != expectedChecks.Length
                || main.modOrderSlots.Where((slot, index) =>
                    slot.Index != index
                    || slot.Number.Text != (index + 1).ToString("00") + "."
                    || slot.Group != orderedRows[index].Group
                    || slot.Panel.Controls.OfType<CheckBox>().SingleOrDefault()
                        != orderedRows[index].Box
                    || layout.GetRow(slot.Panel) < 1).Any()
                || checks.Any(check => check.Text.StartsWith("01. ")
                    || check.Text.StartsWith("02. "))
                || !main.customHandFocusedRs.Text.StartsWith(
                    "Hand Focus", StringComparison.Ordinal)
                || main.modOrderHeaders.Count != 6
                || main.modOrderHeaders.Any(header =>
                    layout.GetRow(header) < 1 || !header.Text.Contains("  "))
                || layout.Height < 6 + expectedChecks.Length * 28 + 6 * 20
                || main.modOrderSlots.Any(slot => slot.Dependency is null
                    || slot.Grip is null)
                || main.modOrderSlots.Any(slot => slot.Panel.RowCount != 1
                    || slot.Panel.Height > 32))
                throw new InvalidDataException(
                    "VR group headings, drag grips, order or numbering changed.");
            string ModNumber(CheckBox check) => "#"
                + (Array.IndexOf(checks, check) + 1);
            string LinksOf(CheckBox mod) => main.modOrderSlots[
                Array.IndexOf(checks, mod)].Dependency.Text;
            if (!LinksOf(main.customDirectionalDash).Contains(
                    "R " + ModNumber(main.weaponWheelRemap) + ","
                    + ModNumber(main.customBehindHeadWeaponWheel)
                    + "  D " + ModNumber(main.customDisableWeaponWheel))
                || !LinksOf(main.customDisableWeaponWheel).Contains(
                    "D " + ModNumber(main.customBehindHeadWeaponWheel)
                    + "," + ModNumber(main.customDirectionalDash))
                || main.modOrderSlots.Where(slot =>
                    slot.Dependency.Text.Contains("#")
                    && slot.Dependency.Links.Count == 0).Any())
                throw new InvalidDataException(
                    "Compact clickable dependencies no longer match mod positions.");

            // Drag a referenced mod inside Hands & Arms and assert its new
            // fixed slot number is reflected in other mods' labels AND tips.
            // Restore the list in memory without touching user preferences.
            var previousModOrder = main.modOrderRows.ToArray();
            var movedMod = main.modOrderRows.Single(row =>
                row.Box == main.customBehindHeadWeaponWheel);
            var dropTarget = main.modOrderRows.First(row =>
                row.Group == movedMod.Group && row.Id != movedMod.Id);
            var priorNumber = ModNumber(main.customBehindHeadWeaponWheel);
            if (!MoveModWithinGroup(main.modOrderRows, movedMod.Id, dropTarget.Id))
                throw new InvalidDataException("Same-group mod drag refused.");
            main.RefreshGroupedModRows();
            var updatedNumber = "#" + (main.modOrderRows.FindIndex(row =>
                row.Box == main.customBehindHeadWeaponWheel) + 1);
            if (updatedNumber == priorNumber
                || !main.modOrderSlots[main.modOrderRows.FindIndex(row =>
                    row.Box == main.customDisableWeaponWheel)].Dependency.Text.Contains(
                    "D " + updatedNumber)
                || !main.detachedModToolTip.GetToolTip(
                    main.customDisableWeaponWheel).Contains(
                        "Disables " + updatedNumber)
                || main.modOrderSlots.Where((slot, index) =>
                    slot.Number.Text != (index + 1).ToString("00") + ".").Any())
                throw new InvalidDataException(
                    "Dragged mod must change dependent references, never slot numbers.");
            main.modOrderRows.Clear();
            main.modOrderRows.AddRange(previousModOrder);
            main.RefreshGroupedModRows();

            var wheelRemapOption = checks.SingleOrDefault(option =>
                option.Text.Contains("Weapon Wheel Button Remap"));
            var wheelRemapTip = wheelRemapOption is null ? string.Empty
                : main.detachedModToolTip.GetToolTip(wheelRemapOption);
            if (!wheelRemapTip.Contains("Tap A to quick-switch")
                || !wheelRemapTip.Contains("Hold A to open the weapon wheel")
                || !wheelRemapTip.Contains("left stick to choose")
                || !wheelRemapTip.Contains("right thumbstick DOWN to toggle crouch")
                || !wheelRemapTip.Contains("Behind-the-Head Weapon Wheel")
                || !wheelRemapTip.Contains("Directional Dash is currently disabled")
                || !wheelRemapTip.Contains("restore the original KHARVOX controls"))
                throw new InvalidDataException(
                    "Weapon Wheel Remap tooltip must explain the actual controls and dependencies.");

            if (main.cinematicSpeedSelector.Items.Count != 4
                || !main.cinematicSpeedSelector.Items[2]!.ToString()!.Contains("Maximum")
                || !main.customBoxingGuard.Text.Contains("Boxing Guard")
                || main.modOrderRows.Any(row=>row.Id=="fast-glory-kills"||row.Id=="fast-chainsaw"))
                throw new InvalidDataException("Fast Kills must live in Sync Action Speed, not draggable VR mods.");
            if (!FirstPartyDoomModGenerator.RunSelfTest())
                throw new InvalidDataException("First-party DOOM mod generator integration is invalid.");
            if (!SfsProfileRecovery.RunSelfTest())
                throw new InvalidDataException(
                    "SFS recovery must preserve the original profile and back up replaced files.");
            if (!NexusFolderCompanion.RunFileDropPolicySelfTest())
                throw new InvalidDataException(
                    "ZIP drag/drop must preserve downloads and avoid overwriting existing mods.");
            if (!main.detachedModToolTip.ShowAlways
                || main.detachedModToolTip.AutoPopDelay < 7000)
                throw new InvalidDataException(
                    "VR mod tooltips must remain readable in the detached panel.");
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
            var miniFace = Descendants(window).OfType<SkullKeyEasterEgg>()
                .SingleOrDefault();
            if (miniFace is null || !miniFace.Visible || miniFace.Width < 40
                || miniFace.Height < 28)
                throw new InvalidDataException("Centred skull key Easter egg is missing.");
            // The CI desktop can be narrower than launcher + 930px sidecar:
            // docking may shrink it to fit, without changing the preferred size.
            if (window.ClientSize.Width < 520 || window.ClientSize.Width > 950
                || main.cinematicSpeedSelector.Width < 190)
                throw new InvalidDataException(
                    "Rendered Custom Mods panel or Sync Action Speed selector is clipped."
                    + " clientWidth=" + window.ClientSize.Width
                    + " selectorWidth=" + main.cinematicSpeedSelector.Width
                    + " desktopWidth=" + Screen.FromControl(window).WorkingArea.Width);
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
            if (doomGrid.RowCount != 6
                || doomGrid.RowStyles[2].SizeType != SizeType.Absolute
                || doomGrid.RowStyles[2].Height != 32
                || doomGrid.RowStyles[4].SizeType != SizeType.Absolute
                || doomGrid.RowStyles[4].Height != 34
                || doomGrid.RowStyles[5].SizeType != SizeType.Absolute
                || doomGrid.RowStyles[5].Height != 27)
                throw new InvalidDataException(
                    "DOOM toolbar and bottom Nexus Mods link rows are not fixed-height.");
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
            var packagedContent = packagedRow as FlowLayoutPanel;
            var generatorButton = packagedContent?.Controls.OfType<TableLayoutPanel>()
                .SelectMany(toolbar => toolbar.Controls.OfType<Button>())
                .SingleOrDefault(button => button.AccessibleName
                    == "Generate first-party KHARVOX resource mods");
            if (generatorButton is null || !generatorButton.Visible
                || !generatorButton.Text.Contains("Generate"))
                throw new InvalidDataException(
                    "First-party DOOM resource generator must be visible even without ZIPs.");
            var heading = userHeader.Controls.OfType<Label>().SingleOrDefault(label =>
                label.Text == "USER MODS");
            var divider = userHeader.Controls.OfType<Panel>()
                .SingleOrDefault(panel =>
                    panel.AccessibleName == "User mods header divider");
            if (heading is null)
                throw new InvalidDataException("USER MODS heading is missing.");
            var measuredHeadingWidth = TextRenderer.MeasureText(heading.Text,
                heading.Font, Size.Empty,
                TextFormatFlags.NoPadding | TextFormatFlags.SingleLine).Width;
            if (userHeader.ColumnStyles[0].SizeType != SizeType.Absolute
                || userHeader.ColumnStyles[0].Width < measuredHeadingWidth + 4
                || userHeader.ColumnStyles[0].Width > measuredHeadingWidth + 10)
                throw new InvalidDataException(
                    "USER MODS heading must fit tightly so the detected count is truly centred.");
            var countLabel = main.userDoomModCount;
            // Verify the actual paint surface rather than just the control
            // tree: our previous nested 1px panels passed layout checks but
            // rendered as tiny isolated dashes on the user's Windows display.
            if (divider is null || countLabel is null
                || countLabel.Parent != divider
                || divider.Controls.Count != 1
                || countLabel.AccessibleName != "Detected user mod count"
                || countLabel.TextAlign != ContentAlignment.MiddleCenter
                || !countLabel.Visible || countLabel.Height < 18
                || countLabel.Left < 0 || countLabel.Right > divider.ClientSize.Width
                || Math.Abs(countLabel.Left + countLabel.Width / 2
                    - divider.ClientSize.Width / 2) > 1
                || FormatDetectedUserMods(17) != "17 detected"
                || FormatDetectedUserMods(0) != "0 detected"
                || !countLabel.Text.EndsWith(" detected", StringComparison.Ordinal)
                || Descendants(modListPanel).OfType<Label>().Any(label =>
                    label.Text.Contains("mod(s) detected.")))
                throw new InvalidDataException(
                    "Detected count must fit the header with one continuous divider surface.");

            // Plain centered detected text: no painted decoration on either side.
            var refresh = userHeader.Controls.OfType<Button>().SingleOrDefault(button =>
                button.AccessibleName == "Rescan mods");
            var folder = userHeader.Controls.OfType<Button>().SingleOrDefault(button =>
                button.Text == "Mod Folder");
            if (heading is null || refresh is null || folder is null
                || !heading.Visible || heading.Height < 15
                || !main.statusToolTip.GetToolTip(refresh).Contains("Rescan USER MODS")
                || !main.statusToolTip.GetToolTip(refresh).Contains("updates automatically")
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

            // Test both Firefox and Chromium-family Windows default browsers.
            // The link must request a new window, NOT a new tab and NOT a
            // hardwired Edge app-mode launch, with no actual browser on CI.
            if (!NexusModsPopup.TestDefaultBrowserArguments())
                throw new InvalidDataException(
                    "Default-browser new-window command generation regressed.");
            var firefoxCommand = NexusModsPopup.MakeDefaultBrowserStartInfo(
                @"C:\Program Files\Mozilla Firefox\firefox.exe", window.Bounds);
            var chromeCommand = NexusModsPopup.MakeDefaultBrowserStartInfo(
                @"C:\Program Files\Google\Chrome\Application\chrome.exe",
                window.Bounds);
            if (!firefoxCommand.Arguments.Contains("--new-window")
                || !chromeCommand.Arguments.Contains("--new-window")
                || chromeCommand.Arguments.Contains("--new-tab")
                || firefoxCommand.Arguments.Contains("--new-tab")
                || !chromeCommand.Arguments.Contains("--window-position="
                    + window.Left.ToString(System.Globalization.CultureInfo.InvariantCulture)
                    + "," + window.Top.ToString(System.Globalization.CultureInfo.InvariantCulture))
                || !chromeCommand.Arguments.Contains("--window-size="
                    + window.Width.ToString(System.Globalization.CultureInfo.InvariantCulture)
                    + "," + window.Height.ToString(System.Globalization.CultureInfo.InvariantCulture)))
                throw new InvalidDataException(
                    "Nexus new-window commands must match the displayed Custom Mods bounds.");

            var saveControls = doomGrid.Controls.OfType<TableLayoutPanel>()
                .SingleOrDefault(row => doomGrid.GetRow(row)==4);
            if (saveControls is null
                || saveControls.Controls.OfType<Button>().Count()!=2
                || saveControls.Controls.OfType<Button>().All(button =>
                    button.AccessibleName!="Backup DOOM saves")
                || saveControls.Controls.OfType<Button>().All(button =>
                    button.AccessibleName!="Manage or restore DOOM save backups"))
                throw new InvalidDataException(
                    "Persistent Save Protection controls must be visible above Nexus.");
            var nexusLink = doomGrid.Controls.OfType<LinkLabel>()
                .SingleOrDefault(label => doomGrid.GetRow(label) == 5);
            if (nexusLink is null || nexusLink.Parent != doomGrid
                || !nexusLink.Visible || nexusLink.Height < 18
                || nexusLink.AccessibleName != "Browse DOOM mods on Nexus Mods"
                || nexusLink.AccessibleDescription
                    != "https://www.nexusmods.com/games/doom/mods"
                || nexusLink.LinkArea.Start != 0
                || nexusLink.LinkArea.Length != nexusLink.Text.Length
                || nexusLink.LinkBehavior != LinkBehavior.HoverUnderline
                || nexusLink.Bottom > doomGrid.ClientSize.Height
                || nexusLink.Top < modListPanel.Bottom
                || nexusLink.LinkColor != Color.Gray)
                throw new InvalidDataException(
                    "Nexus Mods hyperlink must remain clickable at the bottom of DOOM MODS."
                    + " visible=" + nexusLink?.Visible
                    + " height=" + nexusLink?.Height
                    + " top=" + nexusLink?.Top
                    + " bottom=" + nexusLink?.Bottom
                    + " gridHeight=" + doomGrid.ClientSize.Height
                    + " modListBottom=" + modListPanel.Bottom
                    + " available=" + nexusLink?.Enabled
                    + " color=" + nexusLink?.LinkColor);
            // The browser must retain its original Custom Mods bounds.
            Rectangle? clickedBounds = null;
            Rectangle? requestedFolderBounds = null;
            NexusModsPopup.TestOpenRequested = bounds => clickedBounds = bounds;
            NexusFolderCompanion.TestModsPanelOpenRequested =
                bounds => requestedFolderBounds = bounds;
            try
            {
                var clickMethod = typeof(LinkLabel).GetMethod("OnLinkClicked",
                    System.Reflection.BindingFlags.Instance
                    | System.Reflection.BindingFlags.NonPublic)
                    ?? throw new InvalidDataException("LinkLabel click dispatch unavailable.");
                if (nexusLink.Links.Count != 1)
                    throw new InvalidDataException("Nexus link hit area is missing.");
                clickMethod.Invoke(nexusLink, new object[]
                {
                    new LinkLabelLinkClickedEventArgs(nexusLink.Links[0])
                });
                Application.DoEvents();
                if (clickedBounds != window.Bounds
                    || requestedFolderBounds != main.Bounds
                    || nexusLink.Text != "Browse DOOM mods on Nexus Mods"
                    || !nexusLink.Enabled)
                    throw new InvalidDataException(
                        "Clicking Nexus Mods must actually dispatch the popup launch.");
            }
            finally
            {
                NexusModsPopup.TestOpenRequested = null;
                NexusFolderCompanion.TestModsPanelOpenRequested = null;
            }
            // Opening the folder repeatedly, including via Browse Nexus, must
            // not create additional folder instances or delete source ZIPs.
            var isolated = Path.Combine(Path.GetTempPath(),
                "KHARVOX-UI-folder-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(isolated);
            try
            {
                NexusFolderCompanion.OpenStandalone(isolated, main.Bounds);
                var instance = NexusFolderCompanion.ActiveWindow;
                NexusFolderCompanion.OpenStandalone(isolated, main.Bounds);
                NexusFolderCompanion.OpenForPopupAsync(
                    isolated, main.Bounds, IntPtr.Zero).GetAwaiter().GetResult();
                if (instance is null
                    || !ReferenceEquals(instance, NexusFolderCompanion.ActiveWindow)
                    || NexusFolderCompanion.ActiveWindowCount != 1
                    || !string.Equals(instance.BackColor.Name, Color.Black.Name,
                        StringComparison.OrdinalIgnoreCase)
                    || instance.ContextMenuStrip is null
                    || instance.ContextMenuStrip.Items.Count < 4)
                    throw new InvalidDataException(
                        "MODS FOLDER must be dark, reusable and support a file context menu.");
            }
            finally
            {
                NexusFolderCompanion.ActiveWindow?.Close();
                try { Directory.Delete(isolated, true); } catch { }
            }
            // Both title-bar X and footer Close route through the same
            // launcher-focus restoration method. Reopening must stay possible.
            var closeButton = Descendants(window).OfType<Button>()
                .SingleOrDefault(button => button.Text == "Close");
            if (closeButton is null) throw new InvalidDataException(
                "Custom Mods Close button missing.");
            main.Show();
            window.Activate();
            Application.DoEvents();
            closeButton.PerformClick();
            Application.DoEvents();
            if (window.Visible || !main.Visible || !main.ContainsFocus)
                throw new InvalidDataException(
                    "Closing Custom Mods must hide it and focus the launcher.");

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

    // Exercise cross-window dependencies without a DOOM runtime or modifying
    // actual user settings. Hand calibration, hip holsters and A/B remapping
    // are the only features that currently own these launcher controls.
    private static void TestMainLauncherModLocks(MainForm main)
    {
        var tracked = new[]
        {
            main.showHands, main.calibrateHands,
            main.customDynamicShoulderHolster, main.weaponWheelRemap,
            main.swapJumpCrouch, main.customBehindHeadWeaponWheel,
            main.customDisableWeaponWheel, main.customPersistentSsgReload,
            main.customBoxingGuard, main.speedAdvanced,
            main.customActionExitHandoff
        };
        var original = tracked.Select(box => box.Checked).ToArray();
        var previousRestore = main.restoringConfiguration;
        var originalLeftMode=main.leftHanded.Checked;
        var originalFullSpeed=main.cinematicFullSpeedMode;
        var originalSpeedLevel=main.cinematicKillSpeed.Value;
        main.restoringConfiguration = true;
        try
        {
            // Force a clean baseline without dispatching custom-mod saves.
            main.applyingCustomModDependencies = true;
            main.calibrateHands.Checked = false;
            main.customDynamicShoulderHolster.Checked = false;
            main.customPersistentSsgReload.Checked = false;
            main.customBehindHeadWeaponWheel.Checked = false;
            main.customDisableWeaponWheel.Checked = false;
            main.weaponWheelRemap.Checked = false;
            main.showHands.Checked = false;
            main.swapJumpCrouch.Checked = true;
            main.cinematicFullSpeedMode=false;
            main.cinematicKillSpeed.Value=1;
            main.customBoxingGuard.Checked=false;
            main.customActionExitHandoff.Checked=false;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            if (main.customBackOfHandHud.Checked || main.customBackOfHandHud.Enabled)
                throw new InvalidDataException("Distorted Back of Hand HUD mirror must remain paused.");
            if (main.customActionExitHandoff.Enabled
                || main.ReadCustomModSettingsFromControls().CinematicExitHandoff
                || main.ReadCustomModSettingsFromControls().CinematicExitHandoffDelayMs != 150)
                throw new InvalidDataException("Action exit must default OFF with the 150ms regular GK test calibration.");
            main.customActionExitHandoff.Checked=true;
            main.ApplyCustomModDependencies(null);
            if (main.customActionExitHandoff.Checked || main.customActionExitHandoff.Enabled)
                throw new InvalidDataException("Paused action exit may not be enabled.");
            // Three action presets, and a read-only Custom status.
            if (main.cinematicSpeedSelector.Items.Count!=4
                || main.cinematicSpeedSelector.Parent?.Parent is not Control presetPanel
                || main.speedCustomModsGrid is null
                || main.speedCustomModsGrid.GetRow(presetPanel)!=0
                || main.speedAdvancedPanel is null
                || main.speedCustomModsGrid.GetRow(main.speedAdvancedPanel)!=1
                || main.speedAdvancedPanel.ColumnStyles[1].SizeType!=SizeType.Percent)
                throw new InvalidDataException("Three action presets must remain directly above Advanced.");
            if (ActionSpeedCatalog.Controllable.Length!=2
                || main.speedActionChecks.Count!=2 || main.speedActionSliders.Count!=2
                || main.speedActionChecks.ContainsKey("ledge") || main.speedActionSliders.ContainsKey("ledge")
                || main.speedActionChecks.ContainsKey("cinematic")
                || main.speedActionLogTest.Checked
                || main.speedActionSliders.Values.Any(slider=>slider.TickStyle!=TickStyle.None))
                throw new InvalidDataException("Ledges and scripted cinematics must have no user tick override.");
            main.ApplyActionSpeedPreset(0);
            var native=main.ReadCustomModSettingsFromControls();
            if (main.cinematicSpeedSelector.SelectedIndex!=0
                || native.ActionSpeedProfiles.ContainsKey("ledge")
                || ActionSpeedCatalog.HasActiveOverrides(native)
                || native.FastGloryKills || native.FastChainsawKills
                || native.CinematicKillSpeedLevel!=1)
                throw new InvalidDataException("Native preset must use original DOOM action timing.");
            main.ApplyActionSpeedPreset(1);
            var fast=main.ReadCustomModSettingsFromControls();
            if (main.cinematicSpeedSelector.SelectedIndex!=1
                || !fast.ActionSpeedProfiles.Values.All(p=>p.Enabled&&p.TargetHz==70)
                || !ActionSpeedCatalog.HasActiveOverrides(fast))
                throw new InvalidDataException("Fast preset must enable both Glory Kill/Chainsaw profiles.");
            main.speedAdvanced.Checked=false;
            main.ApplyCustomModDependencies(null);
            var collapsed=main.ReadCustomModSettingsFromControls();
            if (main.speedAdvancedVisible || main.speedAdvancedRowStyle!.Height!=0
                || main.cinematicSpeedSelector.SelectedIndex!=1
                || !ActionSpeedCatalog.HasActiveOverrides(collapsed)
                || main.speedActionSliders["glory"].Enabled)
                throw new InvalidDataException("Collapsing Advanced must not disable selected presets.");
            main.speedAdvanced.Checked=true;
            main.ApplyCustomModDependencies(null);
            main.speedActionSliders["glory"].Value=270-140;
            main.UpdateCinematicKillSpeedLabel();
            if (main.cinematicSpeedSelector.SelectedIndex!=3
                || main.ReadCustomModSettingsFromControls().ActionSpeedProfiles["glory"].TargetHz!=140)
                throw new InvalidDataException("Manual slider changes must show Custom.");
            main.ApplyActionSpeedPreset(2);
            var maximum=main.ReadCustomModSettingsFromControls();
            if (main.cinematicSpeedSelector.SelectedIndex!=2
                || maximum.ActionSpeedProfiles["glory"].TargetHz!=30
                || maximum.ActionSpeedProfiles["chainsaw"].TargetHz!=40
                || maximum.ActionSpeedProfiles.ContainsKey("ledge"))
                throw new InvalidDataException("Maximum preset must use 30/40 Hz and no ledge override.");
            main.speedActionLogTest.Checked=true;
            if (!main.ReadCustomModSettingsFromControls().ActionSpeedLogTestEnabled)
                throw new InvalidDataException("Optional action diagnostic selection was lost.");
            main.ApplyActionSpeedPreset(0);
            main.speedActionLogTest.Checked=false;
            if (ActionSpeedCatalog.HasActiveOverrides(main.ReadCustomModSettingsFromControls())
                || main.cinematicSpeedSelector.SelectedIndex!=0)
                throw new InvalidDataException("Native preset must clear all active action overrides.");

            if (!main.showHands.Enabled || !main.swapJumpCrouch.Enabled
                || main.showHands.Checked || !main.swapJumpCrouch.Checked)
                throw new InvalidDataException(
                    "Unused launcher options must be editable and retain values.");

            // The paused SSG test cannot cause invisible input delays or force dependencies.
            main.applyingCustomModDependencies = true;
            main.customPersistentSsgReload.Checked = true;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            if (!main.showHands.Enabled || main.showHands.Checked
                || main.customDynamicShoulderHolster.Checked
                || main.weaponWheelRemap.Checked
                || main.ReadCustomModSettingsFromControls().PersistentSuperShotgunReload
                || main.customPersistentSsgReload.Enabled)
                throw new InvalidDataException(
                    "Paused Super Shotgun Reload must remain disabled.");
            main.applyingCustomModDependencies = true;
            main.customPersistentSsgReload.Checked = false;
            main.customDynamicShoulderHolster.Checked = true;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            if (!main.showHands.Enabled || main.showHands.Checked
                || main.customDynamicShoulderHolster.Checked || main.customDynamicShoulderHolster.Enabled)
                throw new InvalidDataException(
                    "Paused Quick Holsters must not force Enable Hands.");
            main.applyingCustomModDependencies = true;
            main.customDynamicShoulderHolster.Checked = false;
            main.calibrateHands.Checked = true;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            if (main.showHands.Enabled || !main.showHands.Checked)
                throw new InvalidDataException(
                    "Hand calibration must independently lock Enable Hands.");
            main.applyingCustomModDependencies = true;
            main.calibrateHands.Checked = false;
            main.weaponWheelRemap.Checked = true;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            if (!main.showHands.Enabled || main.swapJumpCrouch.Enabled
                || !main.swapJumpCrouch.Checked
                || !main.swapJumpCrouch.AccessibleDescription.Contains("preserved"))
                throw new InvalidDataException(
                    "Weapon Wheel Button Remap must grey out ineffective Jump/Crouch swap without erasing it.");
            main.applyingCustomModDependencies = true;
            main.weaponWheelRemap.Checked = false;
            main.customBehindHeadWeaponWheel.Checked = true;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            if (!main.swapJumpCrouch.Enabled || !main.swapJumpCrouch.Checked
                || !main.customBehindHeadWeaponWheel.Checked
                || !main.leftHanded.Enabled || main.leftHanded.Checked != originalLeftMode)
                throw new InvalidDataException(
                    "Disabling a mod must unlock relevant launcher options; left wheel cannot force global handedness.");

            // Boxing remains independent of the scoped tick overrides.
            main.ApplyActionSpeedPreset(2);
            main.customBoxingGuard.Checked=true;
            main.ApplyCustomModDependencies(null);
            if (main.customBoxingGuard.Checked || main.customBoxingGuard.Enabled
                || main.gloryKillSlowmo.Value!=10
                || main.cinematicSpeedSelector.SelectedIndex!=2
                || main.ReadCustomModSettingsFromControls().FastGloryKills)
                throw new InvalidDataException("Scoped action presets must not resurrect the retired shared speed mode.");
            main.customBoxingGuard.Checked=false;
            main.ApplyActionSpeedPreset(0);
        }
        finally
        {
            main.applyingCustomModDependencies = true;
            // Set calibration first: turning off Hands may otherwise clear it.
            main.calibrateHands.Checked = original[1];
            for (var index = 0; index < tracked.Length; index++)
            {
                if (index != 1) tracked[index].Checked = original[index];
            }
            main.cinematicFullSpeedMode=originalFullSpeed;
            main.cinematicKillSpeed.Value=originalSpeedLevel;
            main.applyingCustomModDependencies = false;
            main.ApplyCustomModDependencies(null);
            main.restoringConfiguration = previousRestore;
        }
    }

}
