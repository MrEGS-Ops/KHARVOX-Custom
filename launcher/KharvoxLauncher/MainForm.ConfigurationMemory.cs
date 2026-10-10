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
            using var installPreview = new DoomModLoaderInstallDialog();
            if (!installPreview.PromptMatchesAction)
                throw new InvalidDataException(
                    "DOOMModLoader instruction does not match its button caption.");
            var window = main.customOptionsForm ??
                throw new InvalidOperationException("Custom Mods window was not created.");
            if (window.ClientSize.Width > 780)
                throw new InvalidDataException("Custom Mods window is too wide.");
            // Start at the EXISTING resize minimum instead of forcing users
            // to manually drag the dialog narrower each launch.
            main.FitCustomOptionsToContent(11);
            if (window.MinimumSize.Width != 730 || window.Width != 730)
                throw new InvalidDataException(
                    "Custom Mods must default to its original 730px minimum width."
                    + " min=" + window.MinimumSize.Width
                    + " actual=" + window.Width);
            // Match actual outside borders exactly with no separator gap,
            // including moving both windows when space is constrained.
            var desktop = new Rectangle(0, 0, 1920, 1080);
            var dock = CalculateCustomModsDock(
                new Rectangle(990, 57, 515, 910), window.Width, desktop);
            if (dock.Mods.Right != dock.Launcher.Left
                || dock.Mods.Top != dock.Launcher.Top
                || dock.Mods.Bottom != dock.Launcher.Bottom
                || dock.Mods.Width != window.Width
                || dock.Launcher.Width != 515
                || dock.Launcher.X != 990)
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
                new Rectangle(990, 57, 515, 910), window.Width, desktop, overlap);
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

            if (!RunModReorderPolicySelfTest())
                throw new InvalidDataException("Mod group ordering policy failed.");
            // The VR group also has a separate rating-header Panel.
            var scroll = vr.Controls.OfType<Panel>()
                .Single(panel => panel.Controls.OfType<TableLayoutPanel>().Any());
            var layout = scroll.Controls.OfType<TableLayoutPanel>().Single();
            var orderedRows = main.modOrderRows.ToArray();
            var checks = orderedRows.Select(row => row.Box).ToArray();
            var expectedChecks = new[]
            {
                main.weaponWheelRemap,
                main.customHandFocusedRs,
                main.customBackOfHandHud,
                main.customBehindHeadWeaponWheel,
                main.customBehindHeadWheelHandSelection,
                main.customDynamicShoulderHolster,
                main.customPhysicalCrouch,
                main.customDirectionalDash,
                main.customPhysicalGrenadeThrow,
                main.customGaussChargeSlowMovement,
                main.customMotionGloryKillSpeed,
                main.customPhysicalChainsawGestures,
                main.customDisableHud,
                main.customDisableWeaponWheel,
                main.customRevengeDemon
            };
            // A persisted layout may reorder WITHIN a group. Never allow a mod
            // to cross into another category, disappear or become duplicated.
            if (!scroll.AutoScroll || layout.AutoScroll
                || checks.Length != 15 || checks.Distinct().Count() != 15
                || !checks.OrderBy(x => Array.IndexOf(expectedChecks, x))
                    .SequenceEqual(expectedChecks)
                || orderedRows.Where((row, index) => row.Group
                    != (index < 6 ? 0 : index < 8 ? 1 : index < 12 ? 2
                        : index < 14 ? 3 : 4)).Any()
                || main.modOrderSlots.Count != 15
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
                || main.modOrderHeaders.Count != 5
                || main.modOrderHeaders.Any(header =>
                    layout.GetRow(header) < 1 || !header.Text.Contains("   "))
                || layout.Height < 32 + 15 * 28 + 5 * 24)
                throw new InvalidDataException(
                    "VR group headings, drag grips, order or numbering changed.");
            string ModNumber(CheckBox check) => "#"
                + (Array.IndexOf(checks, check) + 1);
            if (!main.customDirectionalDash.Text.Contains(
                    "Requires " + ModNumber(main.weaponWheelRemap) + ", "
                    + ModNumber(main.customBehindHeadWeaponWheel)
                    + "; disables " + ModNumber(main.customDisableWeaponWheel))
                || !main.customDisableWeaponWheel.Text.Contains(
                    "Disables " + ModNumber(main.customBehindHeadWeaponWheel)
                    + ", " + ModNumber(main.customDirectionalDash))
                || !main.customDisableHud.Text.Contains(
                    "Disables " + ModNumber(main.customBackOfHandHud))
                || !main.customBackOfHandHud.Text.Contains(
                    "Disables " + ModNumber(main.customDisableHud))
                || !main.customBehindHeadWheelHandSelection.Text.Contains(
                    "Requires " + ModNumber(main.customBehindHeadWeaponWheel)))
                throw new InvalidDataException("Numbered VR dependencies no longer follow layout.");

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
                || !main.customDisableWeaponWheel.Text.Contains(
                    "Disables " + updatedNumber)
                || !main.statusToolTip.GetToolTip(
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
                option.Text.Contains("Weapon Wheel Remap"));
            var wheelRemapTip = wheelRemapOption is null ? string.Empty
                : main.statusToolTip.GetToolTip(wheelRemapOption);
            if (!wheelRemapTip.Contains("Tap A to quick-switch")
                || !wheelRemapTip.Contains("Hold A to open the weapon wheel")
                || !wheelRemapTip.Contains("left stick to choose")
                || !wheelRemapTip.Contains("right thumbstick DOWN to toggle crouch")
                || !wheelRemapTip.Contains("Behind-Head Weapon Wheel")
                || !wheelRemapTip.Contains("Directional Dash")
                || !wheelRemapTip.Contains("restore the original KHARVOX controls"))
                throw new InvalidDataException(
                    "Weapon Wheel Remap tooltip must explain the actual controls and dependencies.");

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
            if (doomGrid.RowCount != 5
                || doomGrid.RowStyles[2].SizeType != SizeType.Absolute
                || doomGrid.RowStyles[2].Height != 32
                || doomGrid.RowStyles[4].SizeType != SizeType.Absolute
                || doomGrid.RowStyles[4].Height != 27)
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

            // With enough horizontal space, confirm that both real line
            // segments were painted on either side of the centered text.
            var leftLength = countLabel.Left - 6;
            var rightLength = divider.ClientSize.Width - countLabel.Right - 6;
            if (leftLength >= 6 && rightLength >= 6 && divider.Height >= 18)
            {
                using var preview = new Bitmap(divider.Width, divider.Height);
                divider.DrawToBitmap(preview, new Rectangle(0, 0,
                    divider.Width, divider.Height));
                var y = divider.Height / 2;
                bool IsDividerInk(int x)
                {
                    for (var dy = -1; dy <= 1; dy++)
                    {
                        var py = y + dy;
                        if (py < 0 || py >= preview.Height) continue;
                        var pixel = preview.GetPixel(x, py);
                        if (Math.Abs(pixel.R - Color.DimGray.R) <= 25
                            && Math.Abs(pixel.G - Color.DimGray.G) <= 25
                            && Math.Abs(pixel.B - Color.DimGray.B) <= 25)
                            return true;
                    }
                    return false;
                }
                var leftX = (2 + countLabel.Left - 4) / 2;
                var rightX = (countLabel.Right + 4
                    + divider.ClientSize.Width - 2) / 2;
                if (!IsDividerInk(leftX) || !IsDividerInk(rightX))
                    throw new InvalidDataException(
                        "USER MODS divider must paint visible hairlines on both sides.");
            }
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

            var nexusLink = doomGrid.Controls.OfType<LinkLabel>()
                .SingleOrDefault(label => doomGrid.GetRow(label) == 4);
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
                || nexusLink.LinkColor != Color.LightSkyBlue)
                throw new InvalidDataException(
                    "Nexus Mods hyperlink must remain clickable at the bottom of DOOM MODS.");
            // Previous tests only checked that a LinkLabel existed: the user
            // could click it without a browser appearing. Fire the real
            // LinkClicked event with the browser launch intercepted, and
            // verify it requests this displayed form's actual outer bounds.
            Rectangle? clickedBounds = null;
            NexusModsPopup.TestOpenRequested = bounds => clickedBounds = bounds;
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
                    || nexusLink.Text != "Browse DOOM mods on Nexus Mods"
                    || !nexusLink.Enabled)
                    throw new InvalidDataException(
                        "Clicking Nexus Mods must actually dispatch the popup launch.");
            }
            finally
            {
                NexusModsPopup.TestOpenRequested = null;
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

}
