using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxMods;

// VR-only standalone layout. The former DOOM Mods panel and its full source
// are parked outside this build. Four verified VR mod checkboxes plus a
// scoped Glory Kill override connect through the optional Native Bridge.
internal sealed partial class ModsForm
{
    private const int LegacyVrModCount = 22;
    private readonly StandaloneModPreferences _modPrefs = StandaloneModPreferences.Load();
    private bool _runtimePreparing;
    private readonly Label _pendingIndicator = new Label();
    private bool _savingSettings;

    // Readiness is still verified internally; no banner in the mod list.
    private void RefreshBridgeState()
    {
        if (_runtimePreparing) SetStatus("Preparing KHARVOX…");
    }

    private static bool IsConfirmedMod(string title) =>
        title == "Weapon Wheel Button Remap" ||
        title == "Hand Focus" ||
        title == "Physical Crouch" ||
        title == "PAINSAW: RIPSHIFT OVERDRIVE";

    private bool CurrentSelection(string name) => name switch
    {
        "Weapon Wheel Button Remap" => _modPrefs.WeaponWheelRemap,
        "Hand Focus" => _modPrefs.HandFocus,
        "Physical Crouch" => _modPrefs.PhysicalCrouch,
        "PAINSAW: RIPSHIFT OVERDRIVE" => _modPrefs.PainsawRipshiftOverdrive,
        _ => false
    };

    private void ApplySelection(string name, bool value)
    {
        if (value && !CanEnableMod(name, out var reason))
        {
            MessageBox.Show(this, reason, "KHARVOX Mods — Dependency check",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
            QueueVrRebuild(); // Revert the unapproved checkbox visually.
            return;
        }
        switch (name)
        {
            case "Weapon Wheel Button Remap": _modPrefs.WeaponWheelRemap = value; break;
            case "Hand Focus": _modPrefs.HandFocus = value; break;
            case "Physical Crouch": _modPrefs.PhysicalCrouch = value; break;
            case "PAINSAW: RIPSHIFT OVERDRIVE":
                _modPrefs.PainsawRipshiftOverdrive = value; break;
            default: return;
        }
        OnVrSelectionChanged(name, value);
    }

    private void SaveModSelections(bool runtimeChanged = false)
    {
        try
        {
            if (runtimeChanged)
                _modPrefs.RuntimeRevision = Guid.NewGuid().ToString("N");
            _modPrefs.Save();
            RefreshPendingIndicator();
            if (runtimeChanged)
                SetStatus("Saved · applies on next DOOM launch");
            else
                SetStatus("Layout saved locally");
        }
        catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException)
        {
            MessageBox.Show(this, "Cannot save portable mod settings: " + ex.Message,
                "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private sealed class PreviewMod
    {
        internal string Title { get; }
        internal string Help { get; }
        internal bool WasAvailable { get; }
        internal PreviewMod(string title, string help, bool wasAvailable = false)
        {
            Title = title;
            Help = help;
            WasAvailable = wasAvailable;
        }
    }

    // Same source order and categories as MainForm.CustomModOrdering.cs.
    // The old Chainsaw Revving label has its approved new display name.
    private static readonly (string Title, PreviewMod[] Mods)[] OriginalModGroups =
    {
        ("✥  Hands & Arms", new[]
        {
            new PreviewMod("Weapon Wheel Button Remap",
                "Tap A to quick-switch. Hold A for the weapon wheel; select with the left stick.", true),
            new PreviewMod("Hand Focus",
                "Controller-directed focus with RS activation.", true),
            new PreviewMod("Boxing Guard Mode",
                "Raises both hands to switch focus and apply slight time dilation. Paused."),
            new PreviewMod("Behind-the-Head Weapon Wheel",
                "Move either hand behind its own side of your head. Grip locks the selection.", true),
            new PreviewMod("Dynamic Quick Holsters",
                "Grip at either hip to store or retrieve a weapon. Paused.")
        }),
        ("➤  Legs & Movement", new[]
        {
            new PreviewMod("Physical Crouch",
                "Physical standing and crouching drive the DOOM crouch control.", true),
            new PreviewMod("Directional Dash",
                "Directional movement dash. Paused due to unsafe physics.")
        }),
        ("✦  Weapons & Combat", new[]
        {
            new PreviewMod("Physical Grenade Throw",
                "Cycle equipment in chest zone; grip, pull out and release to throw. Paused."),
            new PreviewMod("Gauss Charge Slow Movement",
                "Reduced movement speed while Gauss Siege is charging. Paused."),
            new PreviewMod("Persistent Super Shotgun Reload",
                "Experimental persistent native reload state. Paused."),
            new PreviewMod("PAINSAW: RIPSHIFT OVERDRIVE",
                "Rev chainsaw executions by releasing and pulling the trigger up to three times. Strong right-controller haptics.", true)
        }),
        ("▤  HUD & Immersion", new[]
        {
            new PreviewMod("Back of Hand HUD",
                "Experimental wrist-HUD mirror. Paused; the original Cactus HUD stays intact."),
            new PreviewMod("Story Cinematic Skip (RS)",
                "Attempt controlled story-scene fast-forward. Paused."),
            new PreviewMod("Story Free Walk (RS)",
                "Attempt movement during independent story scenes. Paused."),
            new PreviewMod("Story Camera Focus: Major Scenes Only",
                "Limit authored heading changes to major cinematics. Paused."),
            new PreviewMod("Uninterrupted Story",
                "Automatic movement-release experiment during scripted scenes. Paused."),
            new PreviewMod("Action Exit Handoff",
                "Experimental camera handoff after executions. Paused."),
            new PreviewMod("No HUD",
                "Hide the normal combat HUD but retain menus. Paused."),
            new PreviewMod("No Weapon Wheel",
                "Disable weapon-wheel presentation/selection. Paused.")
        }),
        ("⌕  Diagnostics", new[]
        {
            new PreviewMod("Revenge Demon",
                "Diagnostic-only death-time enemy logging. Paused."),
            new PreviewMod("Chainsaw Animation Investigator",
                "Read-only animation timing traces and per-kill CSV.", true)
        }),
        ("◎  Haptics & Feedback", new[]
        {
            new PreviewMod("Haptic Overhaul",
                "Experimental damage-pulse duration proportional to HP/AP loss. Paused.")
        })
    };

    private static readonly PreviewMod[] DevOverrides =
    {
        new PreviewMod("Disable Glory Kill Highlighting",
            "Remove blue/orange outlines around staggered demons."),
        new PreviewMod("Disable Glory Kill Motion Blur",
            "Disable DOOM's automatic motion blur during Glory Kills."),
        new PreviewMod("Disable Camera Kicks & Shaking",
            "Suppress artificial camera kicks and shaking."),
        new PreviewMod("Disable Native Weapon Kick",
            "Suppress DOOM-native recoil kick to preserve headset and hand alignment."),
        new PreviewMod("Disable Hand-Hit Reactions",
            "Prevent scripted hit reactions moving controller-tracked hands."),
        new PreviewMod("Protect Native Ledge Speed",
            "Exclude ledge climbs, falls and landings from experimental action-speed changes."),
        new PreviewMod("Protect Scripted Cinematic Speed",
            "Keep story sequences at their original timing.")
    };

    // Exact same group-box styling as KHARVOX MainForm.MakeGroup().
    private static GroupBox MakeOriginalGroup(string title) => new GroupBox
    {
        Text = title,
        Dock = DockStyle.Fill,
        ForeColor = Color.Gainsboro,
        BackColor = DarkPanel,
        Padding = new Padding(8),
        Font = new Font("Segoe UI", 9f)
    };

    private static Label MakeHeading(string title, int width = 400) => new Label
    {
        Text = title,
        Width = width,
        Height = 26,
        ForeColor = Color.WhiteSmoke,
        BackColor = DarkPanel,
        Font = new Font("Segoe UI", 9f, FontStyle.Bold),
        TextAlign = ContentAlignment.MiddleLeft,
        Margin = new Padding(3, 0, 3, 0)
    };

    private static Button MakeOriginalButton(string title, int width) => new Button
    {
        Text = title,
        Width = width,
        Height = 28,
        FlatStyle = FlatStyle.Flat,
        BackColor = Color.FromArgb(43, 43, 47),
        ForeColor = Color.Gainsboro,
        Margin = new Padding(3, 2, 3, 2)
    };

    private void BuildLayout()
    {
        // VR-only standalone build. DOOM resource-mod UI is retained in
        // ArchivedDoomMods.Parked.cs but excluded from compilation.
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(7),
            BackColor = Color.Black,
            RowCount = 2,
            ColumnCount = 1,
            Margin = Padding.Empty
        };
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));

        root.Controls.Add(BuildOriginalVrPanel(), 0, 0);
        root.Controls.Add(BuildOriginalFooter(), 0, 1);
        Controls.Add(root);
    }

    private void AppendSection(TableLayoutPanel list, string caption)
    {
        var label = new Label
        {
            Text = caption,
            Dock = DockStyle.Top,
            Height = 20,
            ForeColor = Color.FromArgb(157, 157, 162),
            BackColor = list.BackColor,
            Font = new Font("Segoe UI", 9f, FontStyle.Bold),
            TextAlign = ContentAlignment.BottomLeft,
            Margin = new Padding(2, 0, 0, 0)
        };
        AppendFixedRow(list, label, 20);
    }

    private void AppendDeveloperMod(TableLayoutPanel list, PreviewMod mod)
    {
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Top, Height = 28, ColumnCount = 2, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty, BackColor = DarkPanel
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 20));
        var check = MakePreviewCheck(mod.Title,
            mod.Help + Environment.NewLine + "Not connected; checkbox unavailable.",
            "dev-override");
        check.Dock = DockStyle.Fill;
        check.Margin = new Padding(6, 0, 0, 0);
        row.Controls.Add(check, 0, 0);
        row.Controls.Add(MakeHelpLabel(mod.Help), 1, 0);
        AppendFixedRow(list, row, 28);
    }

    private static void AppendFixedRow(TableLayoutPanel list, Control control, int height)
    {
        var rowIndex = list.RowCount++;
        list.RowStyles.Add(new RowStyle(SizeType.Absolute, height));
        list.Controls.Add(control, 0, rowIndex);
    }

    private Label MakeHelpLabel(string description)
    {
        var label = new Label
        {
            Text = "ⓘ",
            Dock = DockStyle.Fill,
            ForeColor = Color.FromArgb(157, 157, 162),
            BackColor = DarkPanel,
            TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 9f),
            Cursor = Cursors.Help
        };
        _tooltips.SetToolTip(label, description +
            Environment.NewLine + "Preview only; gameplay mods are not installed.");
        return label;
    }

    private CheckBox MakePreviewCheck(string title, string description, string key)
    {
        // Standard Windows themed CheckBox, like Cactus's native launcher.
        // Never overlay a disabled checkbox using Paint: it leaves ghost
        // glyphs at the start of text on some Windows scaling settings.
        var check = new CheckBox
        {
            Text = title, Checked = false, Enabled = false,
            FlatStyle = FlatStyle.Standard,
            UseVisualStyleBackColor = false,
            ForeColor = Color.Gainsboro, BackColor = DarkPanel,
            Height = 27, Width = 360,
            AutoSize = false, AutoEllipsis = true,
            Margin = Padding.Empty,
            Tag = key, AccessibleName = title,
            AccessibleDescription = description
        };
        _tooltips.SetToolTip(check, description);
        return check;
    }

    // CC0 "treasure.wav" by Beatscribe. Verified and embedded at build time.
    private static void PlayTwitchSuccessCue()
    {
        System.Threading.Tasks.Task.Run(() =>
        {
            try
            {
                using (var source = typeof(ModsForm).Assembly.GetManifestResourceStream(
                    "KharvoxMods.TwitchUnlockCue"))
                {
                    if (source == null) return;
                    using (var player = new System.Media.SoundPlayer(source))
                        player.PlaySync();
                }
            }
            catch (Exception) { /* Audio must never block Twitch. */ }
        });
    }

    private void RefreshPendingIndicator()
    {
        if (_pendingIndicator.IsDisposed) return;
        var revision = _modPrefs.RuntimeRevision ?? "";
        var applied = "";
        if (revision.Length > 0)
        {
            try
            {
                if (File.Exists(StandaloneModPreferences.AppliedReceiptPath))
                    applied = File.ReadAllText(
                        StandaloneModPreferences.AppliedReceiptPath).Trim();
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
        var pending = revision.Length > 0 &&
            !string.Equals(revision, applied, StringComparison.Ordinal);
        _pendingIndicator.Text = pending ? "● Next DOOM launch" : "✓ Settings applied";
        _pendingIndicator.ForeColor = pending
            ? Color.FromArgb(225, 178, 87)
            : Color.FromArgb(132, 181, 146);
        _tooltips.SetToolTip(_pendingIndicator, pending
            ? "Changes are saved locally. The modified launcher reads them when DOOM starts."
            : "No new runtime changes awaiting a launch.");
    }

    private Control BuildOriginalFooter()
    {
        var footer = new Panel
        {
            Dock = DockStyle.Fill, Margin = Padding.Empty,
            BackColor = Color.Black
        };
        _status.Dock = DockStyle.Left;
        _status.Width = 194;
        _status.Font = new Font("Segoe UI", 8f);
        _status.ForeColor = Color.Gray;
        _status.BackColor = Color.Black;
        _status.TextAlign = ContentAlignment.MiddleLeft;
        _status.AutoEllipsis = true;
        _status.Text = "Connecting…";
        footer.Controls.Add(_status);

        _pendingIndicator.Dock = DockStyle.Left;
        _pendingIndicator.Width = 161;
        _pendingIndicator.BackColor = Color.Black;
        _pendingIndicator.Font = new Font("Segoe UI", 8f);
        _pendingIndicator.TextAlign = ContentAlignment.MiddleLeft;
        footer.Controls.Add(_pendingIndicator);
        RefreshPendingIndicator();

        var right = new FlowLayoutPanel
        {
            Dock = DockStyle.Right, Width = 146,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Padding = new Padding(0, 5, 0, 0),
            BackColor = Color.Black
        };
        var connection = MakeButton("Connection ▾", 132);
        connection.Margin = new Padding(3, 0, 3, 0);
        var connectionMenu = new ContextMenuStrip();
        connectionMenu.Items.Add("Open Custom KHARVOX", null,
            async (_, _) => await StartLauncherAsync());
        connectionMenu.Items.Add("Change Original KHARVOX Location…", null,
            async (_, _) =>
            {
                if (SelectLauncher()) await StartLauncherAsync();
            });
        connection.Click += (_, _) =>
            connectionMenu.Show(connection, new Point(0, connection.Height));
        _tooltips.SetToolTip(connection,
            "Opens our custom launcher. The original KHARVOX path can be changed here.");
        right.Controls.Add(connection);
        footer.Controls.Add(right);

        // Original MrEGS emblem from the uploaded Twitch screenshot.
        // Embedding the cropped original keeps branding portable and avoids
        // an external file dependency or Windows' blocked-download prompts.
        try
        {
            using (var stream = typeof(ModsForm).Assembly.GetManifestResourceStream(
                "KharvoxMods.OriginalMrEGSLogo"))
            {
                if (stream != null)
                {
                    var logo = new PictureBox
                    {
                        Size = new Size(34, 34),
                        BackColor = Color.Black,
                        SizeMode = PictureBoxSizeMode.Zoom,
                        Image = new Bitmap(stream),
                        AccessibleName = "Original MrEGS Twitch watermark",
                        Cursor = Cursors.Hand
                    };
                    void CenterLogo() =>
                        logo.Location = new Point(
                            Math.Max(0, (footer.ClientSize.Width - logo.Width) / 2),
                            Math.Max(0, (footer.ClientSize.Height - logo.Height) / 2));
                    var normal = logo.Image;
                    var inverted = new Bitmap(normal.Width, normal.Height);
                    for (var y = 0; y < normal.Height; y++)
                        for (var x = 0; x < normal.Width; x++)
                        {
                            var pixel = ((Bitmap)normal).GetPixel(x, y);
                            inverted.SetPixel(x, y, Color.FromArgb(pixel.A,
                                255 - pixel.R, 255 - pixel.G, 255 - pixel.B));
                        }
                    var flash = new System.Windows.Forms.Timer { Interval = 190 };
                    flash.Tick += (_, _) =>
                    {
                        flash.Stop();
                        if (!logo.IsDisposed) logo.Image = normal;
                    };
                    int logoClicks = 0;
                    logo.Click += (_, _) =>
                    {
                        logo.Image = inverted;
                        flash.Stop();
                        flash.Start();
                        if (++logoClicks == 3)
                        {
                            try
                            {
                                Process.Start(new ProcessStartInfo
                                {
                                    FileName = "https://www.twitch.tv/MrEGS_",
                                    UseShellExecute = true
                                })?.Dispose();
                                PlayTwitchSuccessCue();
                            }
                            catch (System.ComponentModel.Win32Exception) { }
                        }
                    };
                    _tooltips.SetToolTip(logo, "MrEGS");
                    footer.Disposed += (_, _) =>
                    {
                        flash.Stop();
                        flash.Dispose();
                        logo.Image = null;
                        normal.Dispose();
                        inverted.Dispose();
                    };
                    footer.Controls.Add(logo);
                    footer.SizeChanged += (_, _) => CenterLogo();
                    CenterLogo();
                    logo.BringToFront();
                }
            }
        }
        catch (Exception e) when (e is ArgumentException || e is IOException)
        {
            // Branding must never prevent the launcher from opening.
        }

        return footer;
    }

    internal static bool VerifyOriginalLayoutMetadata()
    {
        var count = OriginalModGroups.Sum(group => group.Mods.Length);
        var titles = OriginalModGroups.SelectMany(group => group.Mods)
            .Select(mod => mod.Title).ToArray();
        return count == LegacyVrModCount && titles.Distinct(StringComparer.Ordinal).Count() == count
            && OriginalModGroups.Length == 6 && DevOverrides.Length == 7
            && titles.Contains("PAINSAW: RIPSHIFT OVERDRIVE");
    }

    internal static void RunUiSelfTest()
    {
        if (!VerifyOriginalLayoutMetadata() || ModNumber(GloryModTitle) != 23)
            throw new InvalidDataException("Legacy KHARVOX Mods inventory differs from the approved 22 mod rows.");
        ValidateActiveVrPolicies();
        // The DOOM resource-mod panel and its standalone companion sources
        // must not occur in the compiled application, even as hidden UI.
        var assembly = typeof(ModsForm).Assembly;
        var forbidden = new[] { "NexusModsPopup", "ModDropWindow",
            "PortableDoomUserMods" };
        if (assembly.GetTypes().Any(type => forbidden.Contains(type.Name)) ||
            typeof(ModsForm).GetMethod("BuildOriginalDoomPanel",
                System.Reflection.BindingFlags.NonPublic |
                System.Reflection.BindingFlags.Instance) != null ||
            typeof(ModsForm).GetMethod("ScanLocalMods",
                System.Reflection.BindingFlags.NonPublic |
                System.Reflection.BindingFlags.Instance) != null)
            throw new InvalidDataException("DOOM resource-mod code is present in VR-only executable.");
        using (var form = new ModsForm())
        {
            var groups = form.Controls.Descendants().OfType<GroupBox>().ToArray();
            if (groups.Length != 1 || groups[0].Text != "VR MODS")
                throw new InvalidDataException("VR-only build contains a non-VR groupbox.");
            // Same KHARVOX MainForm palette: outer black, panels 30/30/33,
            // light-gray captions. Native DWM frame is matched separately.
            if (form.BackColor.ToArgb() != Color.Black.ToArgb() ||
                groups[0].BackColor.ToArgb() != Color.FromArgb(30, 30, 33).ToArgb() ||
                groups[0].ForeColor.ToArgb() != Color.Gainsboro.ToArgb() ||
                groups[0].Padding != new Padding(8))
                throw new InvalidDataException("KHARVOX desktop window palette/group-box mismatch.");
            if (form.Controls.Descendants().OfType<TabControl>().Any())
                throw new InvalidDataException("Tabbed placeholder UI must not return.");
            var vr = form.Controls.Descendants().OfType<CheckBox>()
                .Where(check => string.Equals(check.Tag as string, "original-vr-mod",
                    StringComparison.Ordinal)).ToArray();
            if (vr.Length != LegacyVrModCount ||
                vr.Count(check => check.Enabled) != 4 ||
                vr.Any(check => check.Enabled && !IsConfirmedMod(check.Text)))
                throw new InvalidDataException("Only four confirmed numbered mod rows may be selectable.");
            if (!form.Controls.Descendants().OfType<Label>()
                .Any(label => label.Text.StartsWith("ACTIVE MODS", StringComparison.Ordinal)))
                throw new InvalidDataException("Active Mods heading is missing.");
            if (!form.Controls.Descendants().OfType<TrackBar>().All(slider => !slider.Visible))
                throw new InvalidDataException("Glory Kill slider is visible while override disabled.");
            var dev = form.Controls.Descendants().OfType<CheckBox>()
                .Count(check => string.Equals(check.Tag as string, "dev-override",
                    StringComparison.Ordinal));
            if (dev != DevOverrides.Length)
                throw new InvalidDataException("Developer overrides missing.");
            if (form.Controls.Descendants().OfType<Button>()
                .Any(button => string.Equals(button.Tag as string,
                    "native-bridge-install", StringComparison.Ordinal)))
                throw new InvalidDataException("Legacy Install Bridge button must not appear.");
            if (NativeDock.RequestClose(IntPtr.Zero, 1) ||
                NativeDock.RequestClose(form.Handle, -1))
                throw new InvalidDataException("Unrelated windows must never be closed.");
        }
    }
}

// Test-only recursive control enumeration; no native gameplay dependency.
internal static class LayoutControlTraversal
{
    internal static IEnumerable<Control> Descendants(this Control.ControlCollection controls)
    {
        foreach (Control item in controls)
        {
            yield return item;
            foreach (var descendant in item.Controls.Descendants())
                yield return descendant;
        }
    }
}
