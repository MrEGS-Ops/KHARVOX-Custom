using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxMods;

// Mirrors the original KHARVOX Custom Mods form's groupboxes, 44/56 split,
// fixed 28px numbered rows, 6 categories, Glory Kill speed header and footer.
// Unlike the original controls, gameplay settings are read-only until the
// native bridge can actually apply them. Do not create fake saved selections.
internal sealed partial class ModsForm
{
    private readonly Label _detectedModCount = new Label();
    private readonly FlowLayoutPanel _detectedModRows = new FlowLayoutPanel();
    private readonly Label _localModsMessage = new Label();
    private const int LegacyVrModCount = 22;

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

    private static GroupBox MakeOriginalGroup(string title) => new GroupBox
    {
        Text = title,
        Dock = DockStyle.Fill,
        ForeColor = Color.White,
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
        // Use the original two rows: two GroupBoxes and a slim 38px footer.
        // No launcher-shaped header and no tabs in the VR column.
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(12),
            BackColor = Color.Black,
            RowCount = 2,
            ColumnCount = 1,
            Margin = Padding.Empty
        };
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));

        var columns = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, RowCount = 1, ColumnCount = 2,
            BackColor = Color.Black, Margin = Padding.Empty, Padding = Padding.Empty
        };
        columns.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 44));
        columns.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 56));
        columns.Controls.Add(BuildOriginalDoomPanel(), 0, 0);
        columns.Controls.Add(BuildOriginalVrPanel(), 1, 0);
        root.Controls.Add(columns, 0, 0);
        root.Controls.Add(BuildOriginalFooter(), 0, 1);
        Controls.Add(root);
    }

    private GroupBox BuildOriginalDoomPanel()
    {
        var group = MakeOriginalGroup("DOOM MODS");
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, RowCount = 6, ColumnCount = 1,
            Padding = new Padding(12, 2, 12, 2),
            Margin = Padding.Empty, BackColor = DarkPanel
        };
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 102));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        grid.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 27));

        var loaderRow = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1,
            Padding = Padding.Empty, Margin = Padding.Empty, BackColor = DarkPanel
        };
        loaderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        loaderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        var loaderStatus = MakeLabel("DOOMModLoader — managed by KHARVOX");
        loaderStatus.Font = new Font(Font.FontFamily, 8.5f);
        loaderStatus.ForeColor = Color.Gainsboro;
        loaderRow.Controls.Add(loaderStatus, 0, 0);
        var loaderButton = MakeOriginalButton("Install…", 105);
        loaderButton.Dock = DockStyle.Fill;
        loaderButton.Enabled = false;
        _tooltips.SetToolTip(loaderButton,
            "The original KHARVOX launcher owns the DOOMModLoader installer. Not connected here yet.");
        loaderRow.Controls.Add(loaderButton, 1, 0);
        grid.Controls.Add(loaderRow, 0, 0);

        var packaged = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, RowCount = 3, ColumnCount = 1,
            Padding = Padding.Empty, Margin = Padding.Empty,
            BackColor = DarkPanel
        };
        packaged.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        packaged.RowStyles.Add(new RowStyle(SizeType.Absolute, 27));
        packaged.RowStyles.Add(new RowStyle(SizeType.Absolute, 27));
        var packagedHeader = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = DarkPanel
        };
        packagedHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        packagedHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        var packagedTitle = MakeHeading("KHARVOX MODS");
        packagedTitle.Dock = DockStyle.Fill;
        packagedHeader.Controls.Add(packagedTitle, 0, 0);
        var generate = MakeOriginalButton("Generate", 105);
        generate.Dock = DockStyle.Fill;
        generate.Enabled = false;
        _tooltips.SetToolTip(generate,
            "Native resource generation is still managed by the original KHARVOX launcher.");
        packagedHeader.Controls.Add(generate, 1, 0);
        packaged.Controls.Add(packagedHeader, 0, 0);
        for (var index = 0; index < 2; index++)
        {
            var placeholder = MakePreviewCheck(
                index == 0 ? "Aggressive Demons — generate to enable"
                           : "Enhanced Gibs — generate to enable",
                "Original KHARVOX resource-mod generator is not connected to this application yet.",
                "resource-preview");
            placeholder.Dock = DockStyle.Fill;
            placeholder.Margin = new Padding(3, 0, 3, 0);
            packaged.Controls.Add(placeholder, 0, index + 1);
        }
        grid.Controls.Add(packaged, 0, 1);

        var userHeader = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 4, RowCount = 1,
            Margin = Padding.Empty, BackColor = DarkPanel
        };
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94));
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        var userTitle = MakeHeading("USER MODS", 94);
        userTitle.Dock = DockStyle.Fill;
        userHeader.Controls.Add(userTitle, 0, 0);
        _detectedModCount.Text = "0 detected";
        _detectedModCount.Dock = DockStyle.Fill;
        _detectedModCount.ForeColor = Color.Silver;
        _detectedModCount.BackColor = DarkPanel;
        _detectedModCount.TextAlign = ContentAlignment.MiddleCenter;
        userHeader.Controls.Add(_detectedModCount, 1, 0);
        var refresh = MakeOriginalButton("↻", 34);
        refresh.AccessibleName = "Rescan mods";
        refresh.Dock = DockStyle.Fill;
        refresh.Click += (_, _) => ScanLocalMods();
        _tooltips.SetToolTip(refresh, "Rescan the standalone Mods folder. Detection only.");
        userHeader.Controls.Add(refresh, 2, 0);
        var openFolder = MakeOriginalButton("Mod Folder", 105);
        openFolder.Dock = DockStyle.Fill;
        openFolder.Click += (_, _) => OpenLocalModsFolder();
        userHeader.Controls.Add(openFolder, 3, 0);
        grid.Controls.Add(userHeader, 0, 2);

        var modScroll = new Panel
        {
            Dock = DockStyle.Fill, AutoScroll = true, BackColor = DarkPanel,
            Padding = new Padding(2, 8, 2, 0)
        };
        _detectedModRows.AutoSize = true;
        _detectedModRows.AutoSizeMode = AutoSizeMode.GrowAndShrink;
        _detectedModRows.Dock = DockStyle.Top;
        _detectedModRows.FlowDirection = FlowDirection.TopDown;
        _detectedModRows.WrapContents = false;
        _detectedModRows.BackColor = DarkPanel;
        _detectedModRows.Margin = Padding.Empty;
        _detectedModRows.Padding = Padding.Empty;
        _localModsMessage.Text = "Drop ZIPs or unpacked mod folders here.";
        _localModsMessage.ForeColor = Color.Silver;
        _localModsMessage.AutoSize = true;
        _localModsMessage.MaximumSize = new Size(400, 80);
        _localModsMessage.Margin = new Padding(3, 1, 3, 3);
        _detectedModRows.Controls.Add(_localModsMessage);
        modScroll.Controls.Add(_detectedModRows);
        grid.Controls.Add(modScroll, 0, 3);

        var saveButtons = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1,
            BackColor = DarkPanel, Margin = Padding.Empty
        };
        saveButtons.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        saveButtons.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        var backup = MakeOriginalButton("Backup saves", 120);
        backup.Dock = DockStyle.Fill;
        backup.Enabled = false;
        _tooltips.SetToolTip(backup,
            "DOOM save backups are not connected to the standalone app yet.");
        var manage = MakeOriginalButton("Manage / Restore saves…", 180);
        manage.Dock = DockStyle.Fill;
        manage.Enabled = false;
        _tooltips.SetToolTip(manage,
            "Use the original KHARVOX launcher for save management.");
        saveButtons.Controls.Add(backup, 0, 0);
        saveButtons.Controls.Add(manage, 1, 0);
        grid.Controls.Add(saveButtons, 0, 4);

        var nexus = new LinkLabel
        {
            Text = "Browse DOOM mods on Nexus Mods",
            Dock = DockStyle.Fill,
            ForeColor = Color.Gray,
            LinkColor = Color.Gray,
            ActiveLinkColor = Color.Gainsboro,
            VisitedLinkColor = Color.Gray,
            BackColor = DarkPanel,
            TextAlign = ContentAlignment.MiddleLeft,
            Font = new Font(Font.FontFamily, 8.5f),
            Margin = new Padding(3, 1, 3, 2)
        };
        nexus.LinkClicked += async (_, _) =>
        {
            try
            {
                // Original KHARVOX behaviour: new window in the user's
                // default browser, reused when the link is clicked again.
                await NexusModsPopup.OpenTrackedAsync(Bounds);
                // Keep the local mod folder available beside the browser.
                OpenLocalModsFolder();
            }
            catch (Exception error)
            {
                MessageBox.Show(this, error.Message, "KHARVOX Mods",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        };
        grid.Controls.Add(nexus, 0, 5);
        group.Controls.Add(grid);
        ScanLocalMods();
        return group;
    }

    private void ScanLocalMods()
    {
        try
        {
            // UI is constructed before first-install consent. Do not create
            // any folders if the user presses NO and the app exits.
            var mods = (Directory.Exists(AppState.ModsDirectory)
                    ? Directory.GetFileSystemEntries(AppState.ModsDirectory)
                    : Array.Empty<string>())
                .Where(path => Directory.Exists(path) ||
                    (File.Exists(path) && new[] { ".zip", ".pk3", ".wad" }
                        .Contains(Path.GetExtension(path), StringComparer.OrdinalIgnoreCase)))
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
                .ToArray();
            _detectedModCount.Text = mods.Length + " detected";
            _detectedModRows.SuspendLayout();
            try
            {
                _detectedModRows.Controls.Clear();
                _localModsMessage.Text = mods.Length == 0
                    ? "Drop ZIPs or unpacked mod folders here."
                    : "Local files detected (not installed):";
                _detectedModRows.Controls.Add(_localModsMessage);
                foreach (var mod in mods)
                {
                    var check = MakePreviewCheck(Path.GetFileName(mod),
                        "Detected in the standalone Mods folder. Not installed or enabled.",
                        "detected-local-mod");
                    check.Width = 365;
                    check.Margin = new Padding(3, 1, 3, 1);
                    _detectedModRows.Controls.Add(check);
                }
            }
            finally { _detectedModRows.ResumeLayout(); }
        }
        catch (Exception error) when (error is IOException ||
            error is UnauthorizedAccessException)
        {
            _detectedModCount.Text = "unavailable";
            _localModsMessage.Text = "Cannot read Mods folder: " + error.Message;
        }
    }

    private void OpenLocalModsFolder()
    {
        try
        {
            Directory.CreateDirectory(AppState.ModsDirectory);
            Process.Start(new ProcessStartInfo
            {
                FileName = AppState.ModsDirectory,
                UseShellExecute = true
            });
        }
        catch (Exception error) when (error is IOException ||
            error is UnauthorizedAccessException ||
            error is System.ComponentModel.Win32Exception)
        {
            MessageBox.Show(this, error.Message, "KHARVOX Mods",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private GroupBox BuildOriginalVrPanel()
    {
        var group = MakeOriginalGroup("VR MODS");
        var scroll = new Panel
        {
            Dock = DockStyle.Fill, AutoScroll = true,
            BackColor = DarkPanel, Margin = Padding.Empty
        };
        var list = new TableLayoutPanel
        {
            Dock = DockStyle.Top, AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = DarkPanel, ForeColor = Color.White,
            Padding = new Padding(12, 8, 10, 6),
            Margin = Padding.Empty,
            ColumnCount = 1
        };
        list.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        AddGlorySpeedHeader(list);
        var number = 1;
        foreach (var groupRow in OriginalModGroups)
        {
            AppendSection(list, groupRow.Title);
            foreach (var mod in groupRow.Mods)
                AppendNumberedMod(list, number++, mod);
        }
        AppendSection(list, "⚙  Dev — Gameplay Overrides");
        foreach (var item in DevOverrides)
            AppendDeveloperMod(list, item);
        scroll.Controls.Add(list);
        group.Controls.Add(scroll);
        return group;
    }

    private void AddGlorySpeedHeader(TableLayoutPanel list)
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Top, Height = 78, RowCount = 2, ColumnCount = 3,
            BackColor = DarkPanel, Margin = new Padding(4, 0, 4, 0),
            Padding = new Padding(4, 2, 4, 2)
        };
        panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
        panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 154));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 80));
        var title = MakeHeading("GLORY KILL SPEED");
        title.Dock = DockStyle.Fill;
        panel.Controls.Add(title, 0, 0);
        panel.SetColumnSpan(title, 3);
        var check = MakePreviewCheck("Override",
            "Standalone 30–240 Hz Glory Kill speed control. Not connected to the native bridge.",
            "glory-kill-speed-preview");
        check.Dock = DockStyle.Fill;
        panel.Controls.Add(check, 0, 1);
        var slider = new TrackBar
        {
            Minimum = 30, Maximum = 240, Value = 200,
            Dock = DockStyle.Fill,
            TickStyle = TickStyle.None,
            Enabled = false,
            Margin = new Padding(5, 3, 5, 3)
        };
        _tooltips.SetToolTip(slider,
            "In original KHARVOX, right is faster (30 Hz) and left is slower (240 Hz). Not connected here yet.");
        panel.Controls.Add(slider, 1, 1);
        var speedValue = MakeLabel("Native");
        speedValue.ForeColor = Color.Gainsboro;
        panel.Controls.Add(speedValue, 2, 1);
        AppendFixedRow(list, panel, 78);
    }

    private void AppendSection(TableLayoutPanel list, string caption)
    {
        var label = new Label
        {
            Text = caption,
            Dock = DockStyle.Top,
            Height = 20,
            ForeColor = Color.FromArgb(157, 157, 162),
            BackColor = DarkPanel,
            Font = new Font("Segoe UI", 9f, FontStyle.Bold),
            TextAlign = ContentAlignment.BottomLeft,
            Margin = new Padding(2, 0, 0, 0)
        };
        AppendFixedRow(list, label, 20);
    }

    private void AppendNumberedMod(TableLayoutPanel list, int number, PreviewMod mod)
    {
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Top, Height = 28, ColumnCount = 3, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty, BackColor = DarkPanel
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 32));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 20));
        var index = new Label
        {
            Text = number.ToString("00") + ".",
            ForeColor = Color.White,
            BackColor = DarkPanel,
            Dock = DockStyle.Fill,
            Font = new Font("Segoe UI", 9f),
            TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(2, 0, 0, 0)
        };
        var check = MakePreviewCheck(mod.Title,
            mod.Help + Environment.NewLine +
            (mod.WasAvailable ? "Previously available in the original launcher."
                              : "Previously paused in the original launcher.") +
            Environment.NewLine + "Native bridge not connected; not enabled.",
            "original-vr-mod");
        check.Dock = DockStyle.Fill;
        check.Margin = new Padding(2, 0, 0, 0);
        var info = MakeHelpLabel(mod.Help);
        row.Controls.Add(index, 0, 0);
        row.Controls.Add(check, 1, 0);
        row.Controls.Add(info, 2, 0);
        AppendFixedRow(list, row, 28);
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
        var check = new CheckBox
        {
            Text = title, Checked = false, Enabled = false,
            FlatStyle = FlatStyle.Flat,
            ForeColor = Color.FromArgb(194, 194, 200),
            BackColor = DarkPanel,
            Height = 27, Width = 405,
            AutoSize = false, AutoEllipsis = true,
            Margin = Padding.Empty,
            Tag = key,
            AccessibleName = title,
            AccessibleDescription = description
        };
        // The original KHARVOX disabled-checkbox paint fix keeps the text
        // readable while the checkbox itself remains inoperable.
        check.Paint += (_, e) =>
        {
            if (check.Enabled) return;
            var region = new Rectangle(20, 0, Math.Max(0, check.Width - 20), check.Height);
            using (var background = new SolidBrush(DarkPanel))
                e.Graphics.FillRectangle(background, region);
            TextRenderer.DrawText(e.Graphics, title, check.Font, region,
                Color.FromArgb(194, 194, 200),
                TextFormatFlags.Left | TextFormatFlags.VerticalCenter |
                TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
        };
        _tooltips.SetToolTip(check, description);
        return check;
    }

    private Control BuildOriginalFooter()
    {
        var footer = new Panel
        {
            Dock = DockStyle.Fill, Margin = Padding.Empty,
            BackColor = Color.Black
        };
        _status.Dock = DockStyle.Left;
        _status.Width = 500;
        _status.Font = new Font("Segoe UI", 8f);
        _status.ForeColor = Color.Gray;
        _status.BackColor = Color.Black;
        _status.TextAlign = ContentAlignment.MiddleLeft;
        _status.AutoEllipsis = true;
        _status.Text = "Standalone UI — native bridge not connected.";
        footer.Controls.Add(_status);

        var right = new FlowLayoutPanel
        {
            Dock = DockStyle.Right, Width = 374,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Padding = new Padding(0, 5, 0, 0),
            BackColor = Color.Black
        };
        var close = MakeButton("Close", 92);
        close.Margin = new Padding(3, 0, 3, 0);
        close.Click += (_, _) => Close();
        right.Controls.Add(close);
        var credit = new Label
        {
            Text = "Made by MrEGS", Size = new Size(108, 28),
            ForeColor = Color.Gray, BackColor = Color.Black,
            Font = new Font("Segoe UI", 8.5f),
            TextAlign = ContentAlignment.MiddleRight,
            Margin = new Padding(0, 0, 12, 0)
        };
        right.Controls.Add(credit);

        var connection = MakeButton("Connection ▾", 138);
        connection.Margin = new Padding(3, 0, 3, 0);
        var connectionMenu = new ContextMenuStrip();
        connectionMenu.Items.Add("Start / Find KHARVOX", null, (_, _) => StartLauncher());
        connectionMenu.Items.Add("Select KHARVOX…", null, (_, _) => SelectLauncher());
        connectionMenu.Items.Add("Attach / Detach Window", null, (_, _) => ToggleAttachment());
        connection.Click += (_, _) =>
            connectionMenu.Show(connection, new Point(0, connection.Height));
        _tooltips.SetToolTip(connection,
            "Original launcher connection, window attachment and KHARVOX selection.");
        right.Controls.Add(connection);
        footer.Controls.Add(right);
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
        if (!VerifyOriginalLayoutMetadata())
            throw new InvalidDataException("Legacy KHARVOX Mods inventory differs from the approved 22 mod rows.");
        using (var form = new ModsForm())
        {
            var groups = form.Controls.Descendants().OfType<GroupBox>().ToArray();
            if (groups.Length != 2 ||
                !groups.Any(group => group.Text == "DOOM MODS") ||
                !groups.Any(group => group.Text == "VR MODS"))
                throw new InvalidDataException("Original two-column DOOM/VR groupboxes missing.");
            if (form.Controls.Descendants().OfType<TabControl>().Any())
                throw new InvalidDataException("Tabbed placeholder UI must not return.");
            var vr = form.Controls.Descendants().OfType<CheckBox>()
                .Where(check => string.Equals(check.Tag as string, "original-vr-mod",
                    StringComparison.Ordinal)).ToArray();
            if (vr.Length != LegacyVrModCount || vr.Any(check => check.Enabled))
                throw new InvalidDataException("Legacy mod row count or safe disabled state mismatch.");
            var dev = form.Controls.Descendants().OfType<CheckBox>()
                .Count(check => string.Equals(check.Tag as string, "dev-override",
                    StringComparison.Ordinal));
            if (dev != DevOverrides.Length)
                throw new InvalidDataException("Developer overrides missing.");
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
