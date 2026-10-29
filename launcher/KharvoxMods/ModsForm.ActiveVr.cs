using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxMods;

// VR-only version of the original detached mod ordering UI. Native settings
// are still applied by the verified bridge at the NEXT DOOM launch. The order
// controls presentation only; it cannot change hook execution/dependencies.
internal sealed partial class ModsForm
{
    private const string GloryModTitle = "Glory Kill Speed Override";
    private const string ActiveDragFormat = "KHARVOX.ActiveVRMod";
    private readonly TableLayoutPanel _vrModList = new TableLayoutPanel();
    private readonly Panel _vrModScroll = new Panel();
    private readonly Stack<string[]> _activeUndo = new Stack<string[]>();
    private readonly Dictionary<string, Control> _visibleModRows =
        new Dictionary<string, Control>(StringComparer.Ordinal);
    private readonly Dictionary<string, Color> _dependencyHighlights =
        new Dictionary<string, Color>(StringComparer.Ordinal);
    private Control? _dropLineRow;
    private bool _dropLineAfter;

    private static string[] VerifiedVrMods() => OriginalModGroups
        .SelectMany(group => group.Mods)
        .Where(mod => IsConfirmedMod(mod.Title))
        .Select(mod => mod.Title).Concat(new[] { GloryModTitle }).ToArray();

    private bool IsVrActive(string title) => title == GloryModTitle
        ? _modPrefs.GloryKillSpeedOverride : IsConfirmedMod(title) &&
          CurrentSelection(title);

    private string[] ActiveVrOrder()
    {
        var selected = VerifiedVrMods().Where(IsVrActive).ToArray();
        return (_modPrefs.ActiveOrder ?? Array.Empty<string>())
            .Where(selected.Contains).Distinct(StringComparer.Ordinal)
            .Concat(selected.Where(item => !(_modPrefs.ActiveOrder ??
                Array.Empty<string>()).Contains(item)))
            .Distinct(StringComparer.Ordinal).ToArray();
    }

    private void StoreActiveOrder(string[] next, bool saveUndo = true)
    {
        var current = ActiveVrOrder();
        var selected = VerifiedVrMods().Where(IsVrActive).ToArray();
        var normalized = next.Where(selected.Contains)
            .Distinct(StringComparer.Ordinal)
            .Concat(selected.Where(item => !next.Contains(item)))
            .ToArray();
        if (current.SequenceEqual(normalized)) return;
        if (saveUndo) _activeUndo.Push(current);
        _modPrefs.ActiveOrder = normalized;
        SaveModSelections();
        QueueVrRebuild();
    }

    private static string[] AllDisplayMods() => OriginalModGroups
        .SelectMany(group => group.Mods).Select(mod => mod.Title)
        .Concat(new[] { GloryModTitle })
        .Concat(DevOverrides.Select(dev => dev.Title)).ToArray();

    private static int ModNumber(string title) =>
        Array.IndexOf(AllDisplayMods(), title) + 1;

    private string[] AvailableVrOrder()
    {
        var inactive = AllDisplayMods().Where(mod => !IsVrActive(mod)).ToArray();
        return (_modPrefs.AvailableOrder ?? Array.Empty<string>())
            .Where(inactive.Contains).Distinct(StringComparer.Ordinal)
            .Concat(inactive.Where(name => !(_modPrefs.AvailableOrder ??
                Array.Empty<string>()).Contains(name)))
            .Distinct(StringComparer.Ordinal).ToArray();
    }

    private void MoveAvailable(string source, string? target, bool after)
    {
        var items = AvailableVrOrder().ToList();
        if (!items.Remove(source) || items.Count == 0) return;
        var index = target == null ? items.Count : items.IndexOf(target);
        if (index < 0) index = items.Count;
        else if (after) index++;
        items.Insert(Math.Min(index, items.Count), source);
        _modPrefs.AvailableOrder = items.ToArray();
        SaveModSelections();
        QueueVrRebuild();
    }

    private void OnVrSelectionChanged(string title, bool enabled)
    {
        var current = (_modPrefs.ActiveOrder ?? Array.Empty<string>())
            .Where(name => name != title).ToList();
        if (enabled && VerifiedVrMods().Contains(title)) current.Add(title);
        _modPrefs.ActiveOrder = current.ToArray();
        // Keep inactive row order even when a mod is toggled on and off.
        _modPrefs.AvailableOrder = AvailableVrOrder();
        SaveModSelections();
        QueueVrRebuild();
    }

    private void QueueVrRebuild()
    {
        if (IsHandleCreated && !IsDisposed)
            BeginInvoke(new Action(RenderVrRows));
        else RenderVrRows();
    }

    private void MoveActive(string source, string? target, string operation)
    {
        var items = ActiveVrOrder().ToList();
        var from = items.IndexOf(source);
        if (from < 0 || items.Count < 2) return;
        items.RemoveAt(from);
        int to;
        if (operation == "top") to = 0;
        else if (operation == "bottom") to = items.Count;
        else if (operation == "up") to = Math.Max(0, from - 1);
        else if (operation == "down") to = Math.Min(items.Count, from + 1);
        else to = target == null ? items.Count : items.IndexOf(target);
        if (to < 0) to = items.Count;
        else if (operation == "after") to++;
        items.Insert(Math.Min(to, items.Count), source);
        StoreActiveOrder(items.ToArray());
    }

    private void UndoActiveOrder()
    {
        if (_activeUndo.Count == 0) return;
        StoreActiveOrder(_activeUndo.Pop(), false);
    }

    private void ResetActiveOrder()
    {
        StoreActiveOrder(VerifiedVrMods().Where(IsVrActive).ToArray());
    }

    // These are the dependencies recorded by the original KHARVOX ordering
    // panel. All currently unverified dependent mods remain DISABLED; never
    // silently enable an unsupported gameplay feature.
    private static (string Name, bool Required)[] DependencyLinks(string mod) => mod switch
    {
        "Directional Dash" => new[] {
            ("Weapon Wheel Button Remap", true),
            ("Behind-the-Head Weapon Wheel", true),
            ("No Weapon Wheel", false)
        },
        "Behind-the-Head Weapon Wheel" => new[] { ("No Weapon Wheel", false) },
        "No Weapon Wheel" => new[] {
            ("Behind-the-Head Weapon Wheel", false),
            ("Directional Dash", false)
        },
        _ => Array.Empty<(string, bool)>()
    };

    private static string[] DependenciesFor(string mod)
    {
        var links = DependencyLinks(mod);
        var lines = links.Select(x =>
            (x.Required ? "Requires: " : "Conflicts with: ") +
            "#" + ModNumber(x.Name).ToString("00") + " " + x.Name).ToList();
        if (mod == "Dynamic Quick Holsters")
            lines.Add("Requires: Hands enabled in the original KHARVOX launcher");
        return lines.ToArray();
    }

    private void SetDependencyHover(string name)
    {
        _dependencyHighlights.Clear();
        foreach (var link in DependencyLinks(name))
            _dependencyHighlights[link.Name] = link.Required
                ? Color.FromArgb(65, 212, 123) : Color.FromArgb(240, 84, 80);
        foreach (var row in _visibleModRows.Values) row.Invalidate();
    }

    private void ClearDependencyHover()
    {
        if (_dependencyHighlights.Count == 0) return;
        _dependencyHighlights.Clear();
        foreach (var row in _visibleModRows.Values) row.Invalidate();
    }

    private void AttachDependencyHover(Control row, string title)
    {
        row.Paint += (_, e) =>
        {
            if (_dependencyHighlights.TryGetValue(title, out var color))
            {
                using (var pen = new Pen(color, 2))
                    e.Graphics.DrawRectangle(pen, 1, 1,
                        Math.Max(0, row.Width - 3), Math.Max(0, row.Height - 3));
            }
            if (_dropLineRow == row)
            {
                using (var pen = new Pen(Color.FromArgb(70, 180, 255), 3))
                {
                    var y = _dropLineAfter ? row.Height - 2 : 1;
                    e.Graphics.DrawLine(pen, 1, y, Math.Max(1, row.Width - 2), y);
                }
            }
        };
        void Enter(object? sender, EventArgs e) => SetDependencyHover(title);
        void Leave(object? sender, EventArgs e)
        {
            if (!row.RectangleToScreen(row.ClientRectangle).Contains(Cursor.Position))
                ClearDependencyHover();
        }
        foreach (var control in new[] { row }.Concat(row.Controls.Cast<Control>()))
        {
            control.MouseEnter += Enter;
            control.MouseLeave += Leave;
        }
    }

    private void MarkDropLine(Control? row, bool after = false)
    {
        if (_dropLineRow == row && _dropLineAfter == after) return;
        var previous = _dropLineRow;
        _dropLineRow = row;
        _dropLineAfter = after;
        previous?.Invalidate();
        row?.Invalidate();
    }

    private void ShowModDependencies(string name, string description)
    {
        var dependencies = DependenciesFor(name);
        var number = ModNumber(name);
        var heading = number > 0 ? "#" + number.ToString("00") + " " + name : name;
        var details = dependencies.Length == 0
            ? "No recorded mod dependencies."
            : string.Join(Environment.NewLine, dependencies);
        MessageBox.Show(this, description + Environment.NewLine + Environment.NewLine +
            details + Environment.NewLine + Environment.NewLine +
            "Green = required. Red = conflicting. Paused mods remain disabled.",
            heading + " — Dependencies", MessageBoxButtons.OK, MessageBoxIcon.Information);
    }

    private GroupBox BuildOriginalVrPanel()
    {
        var group = MakeOriginalGroup("VR MODS");
        _vrModScroll.Dock = DockStyle.Fill;
        _vrModScroll.AutoScroll = true;
        _vrModScroll.BackColor = DarkPanel;

        var content = new TableLayoutPanel
        {
            Dock = DockStyle.Top, ColumnCount = 1, RowCount = 2,
            AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = DarkPanel, Padding = new Padding(12, 8, 10, 6),
            Margin = Padding.Empty
        };
        content.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        content.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
        content.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        _bridgeState.Dock = DockStyle.Fill;
        _bridgeState.BackColor = DarkPanel;
        _bridgeState.Font = new Font("Segoe UI", 8f);
        _bridgeState.TextAlign = ContentAlignment.MiddleLeft;
        _bridgeState.AutoEllipsis = true;
        _tooltips.SetToolTip(_bridgeState,
            "Our verified custom KHARVOX starts automatically; your original installation is untouched.");
        RefreshBridgeState();
        content.Controls.Add(_bridgeState, 0, 0);

        _vrModList.Dock = DockStyle.Top;
        _vrModList.AutoSize = true;
        _vrModList.AutoSizeMode = AutoSizeMode.GrowAndShrink;
        _vrModList.BackColor = DarkPanel;
        _vrModList.Padding = new Padding(0, 4, 0, 5);
        _vrModList.Margin = Padding.Empty;
        _vrModList.ColumnCount = 1;
        _vrModList.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        content.Controls.Add(_vrModList, 0, 1);
        _vrModScroll.Controls.Add(content);
        group.Controls.Add(_vrModScroll);
        RenderVrRows();
        return group;
    }

    private void RenderVrRows()
    {
        if (_vrModList.IsDisposed) return;
        _vrModList.SuspendLayout();
        try
        {
            _visibleModRows.Clear();
            _dependencyHighlights.Clear();
            _dropLineRow = null;
            var previous = _vrModList.Controls.Cast<Control>().ToArray();
            _vrModList.Controls.Clear();
            foreach (var control in previous) control.Dispose();
            _vrModList.RowStyles.Clear();
            _vrModList.RowCount = 0;

            var active = ActiveVrOrder();
            var header = new Label
            {
                Text = "ACTIVE MODS  (" + active.Length + ")",
                Tag = "active-mods-header",
                ForeColor = Color.WhiteSmoke, BackColor = DarkPanel,
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                TextAlign = ContentAlignment.MiddleLeft,
                Dock = DockStyle.Fill, Margin = new Padding(2, 2, 0, 2)
            };
            AppendFixedRow(_vrModList, header, 30);
            if (active.Length == 0)
            {
                AppendFixedRow(_vrModList, new Label
                {
                    Text = "Check a VR mod below to add it here.",
                    ForeColor = Color.Silver, BackColor = DarkPanel,
                    Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft
                }, 28);
            }
            else
            {
                foreach (var name in active)
                {
                    if (name == GloryModTitle) AddGlorySpeedRow(_vrModList, true);
                    else
                    {
                        var mod = OriginalModGroups.SelectMany(g => g.Mods)
                            .First(m => m.Title == name);
                        AppendVrMod(_vrModList, mod, true);
                    }
                }
            }

            // A drop target at the end allows a drag to append the item.
            if (active.Length > 1)
            {
                var dropEnd = new Label
                {
                    Text = "⋮⋮  Drag to reorder  •  Right-click for Undo / Reset",
                    ForeColor = Color.FromArgb(145, 145, 150),
                    BackColor = DarkPanel, Dock = DockStyle.Fill,
                    AllowDrop = true, TextAlign = ContentAlignment.MiddleLeft,
                    Font = new Font("Segoe UI", 8f),
                    Margin = new Padding(2, 0, 0, 2)
                };
                AttachActiveDrop(dropEnd, null);
                AppendFixedRow(_vrModList, dropEnd, 24);
            }

            AppendSection(_vrModList, "AVAILABLE VR MODS  ·  DRAG TO REORDER");
            string lastGroup = "";
            foreach (var name in AvailableVrOrder())
            {
                var category = name == GloryModTitle ? "✦  Weapons & Combat" :
                    DevOverrides.Any(d => d.Title == name) ? "⚙  Dev — Gameplay Overrides" :
                    OriginalModGroups.First(g => g.Mods.Any(m => m.Title == name)).Title;
                if (category != lastGroup)
                {
                    AppendSection(_vrModList, category);
                    lastGroup = category;
                }
                if (name == GloryModTitle) AddGlorySpeedRow(_vrModList, false);
                else
                {
                    var mod = OriginalModGroups.SelectMany(g => g.Mods)
                        .Concat(DevOverrides).First(m => m.Title == name);
                    AppendVrMod(_vrModList, mod, false,
                        DevOverrides.Any(d => d.Title == name));
                }
            }
        }
        finally { _vrModList.ResumeLayout(true); }
    }

    private void AddGlorySpeedRow(TableLayoutPanel list, bool active)
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Height = 34,
            AllowDrop = true,
            Tag = active ? "active-vr-mod" : "available-vr-mod",
            ColumnCount = 5, RowCount = 1,
            BackColor = DarkPanel, Padding = new Padding(2, 0, 0, 0),
            Margin = Padding.Empty
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 146));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 58));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 24));
        var numberLabel = new Label
        {
            Text = ModNumber(GloryModTitle).ToString("00") + ".",
            Dock = DockStyle.Fill, ForeColor = Color.WhiteSmoke,
            BackColor = DarkPanel, TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(2, 0, 0, 0)
        };
        panel.Controls.Add(numberLabel, 0, 0);
        var check = MakePreviewCheck(GloryModTitle,
            "Override just Glory Kill speed (30–240 Hz). No other animations affected.",
            "glory-kill-speed-preview");
        check.Enabled = true;
        check.Checked = active;
        check.Dock = DockStyle.Fill;
        panel.Controls.Add(check, 1, 0);

        var slider = new TrackBar
        {
            Minimum = 30, Maximum = 240,
            Value = 270 - Math.Max(30, Math.Min(240, _modPrefs.GloryKillHz)),
            Width = 165, Height = 31,
            TickStyle = TickStyle.None,
            Visible = check.Checked, Enabled = check.Checked,
            Margin = new Padding(2, 0, 1, 0),
            Tag = "glory-speed-slider"
        };
        // Intentionally short and completely hidden while unchecked.
        panel.Controls.Add(slider, 2, 0);
        var speedLabel = new Label
        {
            Text = _modPrefs.GloryKillHz + " Hz",
            BackColor = DarkPanel, ForeColor = Color.Gainsboro,
            Visible = check.Checked, Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft
        };
        panel.Controls.Add(speedLabel, 3, 0);
        _tooltips.SetToolTip(slider,
            "Right is faster (30 Hz); left is slower (240 Hz).");
        check.CheckedChanged += (_, _) =>
        {
            _modPrefs.GloryKillSpeedOverride = check.Checked;
            slider.Visible = check.Checked;
            speedLabel.Visible = check.Checked;
            OnVrSelectionChanged(GloryModTitle, check.Checked);
        };
        slider.ValueChanged += (_, _) =>
        {
            _modPrefs.GloryKillHz = 270 - slider.Value;
            speedLabel.Text = _modPrefs.GloryKillHz + " Hz";
        };
        slider.MouseUp += (_, _) => SaveModSelections();
        slider.KeyUp += (_, _) => SaveModSelections();
        slider.Leave += (_, _) => SaveModSelections();
        var grip = new Label
        {
            Text = "⋮⋮", Dock = DockStyle.Fill,
            ForeColor = Color.FromArgb(178, 178, 185),
            TextAlign = ContentAlignment.MiddleCenter,
            Cursor = Cursors.SizeNS,
            Font = new Font("Segoe UI", 9.5f, FontStyle.Bold)
        };
        panel.Controls.Add(grip, 4, 0);
        if (active) AttachActiveRow(panel, check, grip, GloryModTitle,
            "Override Glory Kill speed (30–240 Hz).");
        else AttachAvailableRow(panel, grip, GloryModTitle);
        AttachDependencyHover(panel, GloryModTitle);
        _visibleModRows[GloryModTitle] = panel;
        AppendFixedRow(list, panel, 36);
    }

    private void AppendVrMod(TableLayoutPanel list, PreviewMod mod, bool active, bool isDev = false)
    {
        var number = ModNumber(mod.Title);
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Height = 30,
            ColumnCount = 5, RowCount = 1,
            BackColor = DarkPanel, Margin = Padding.Empty,
            Padding = Padding.Empty,
            AllowDrop = true,
            Tag = active ? "active-vr-mod" : "available-vr-mod"
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        var dependent = DependenciesFor(mod.Title);
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,
            dependent.Length > 0 ? 195 : 0));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 26));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 24));
        var numberLabel = new Label
        {
            Text = number.ToString("00") + ".",
            Dock = DockStyle.Fill, BackColor = DarkPanel,
            ForeColor = Color.White, TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(2, 0, 0, 0), AllowDrop = active
        };
        var description = mod.Help;
        var dependencies = DependenciesFor(mod.Title);
        if (dependencies.Length > 0)
            description += Environment.NewLine + string.Join(Environment.NewLine, dependencies);
        var check = MakePreviewCheck(mod.Title,
            description + Environment.NewLine +
            (IsConfirmedMod(mod.Title)
                ? "Verified core feature. Uses installed Native Bridge on next DOOM launch."
                : "Paused; not available in this build."),
            isDev ? "dev-override" : "original-vr-mod");
        check.Enabled = IsConfirmedMod(mod.Title);
        check.Checked = active;
        check.Dock = DockStyle.Fill;
        check.Margin = Padding.Empty;
        if (check.Enabled)
            check.CheckedChanged += (_, _) =>
                ApplySelection(mod.Title, check.Checked);
        var info = new LinkLabel
        {
            Text = "ⓘ", Dock = DockStyle.Fill,
            BackColor = DarkPanel,
            ForeColor = Color.Silver, LinkColor = Color.Silver,
            ActiveLinkColor = Color.White,
            TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 9f)
        };
        _tooltips.SetToolTip(info, description +
            Environment.NewLine + "Click for original dependency information.");
        info.LinkClicked += (_, _) => ShowModDependencies(mod.Title, mod.Help);
        var grip = new Label
        {
            Text = "⋮⋮", Dock = DockStyle.Fill,
            ForeColor = Color.FromArgb(178,178,185),
            TextAlign = ContentAlignment.MiddleCenter,
            Cursor = Cursors.SizeNS,
            Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
            AllowDrop = true
        };
        row.Controls.Add(numberLabel, 0, 0);
        row.Controls.Add(check, 1, 0);
        if (dependent.Length > 0)
        {
            var link = new LinkLabel
            {
                Text = dependent[0],
                Dock = DockStyle.Fill, BackColor = DarkPanel,
                LinkColor = Color.FromArgb(176,176,184),
                ActiveLinkColor = Color.White,
                Font = new Font("Segoe UI", 7.5f),
                TextAlign = ContentAlignment.MiddleLeft,
                AutoEllipsis = true,
                Tag = "vr-mod-dependency-link",
                Margin = Padding.Empty
            };
            _tooltips.SetToolTip(link, string.Join(Environment.NewLine, dependent));
            link.LinkClicked += (_, _) => ShowModDependencies(mod.Title, mod.Help);
            row.Controls.Add(link, 2, 0);
        }
        row.Controls.Add(info, 3, 0);
        row.Controls.Add(grip, 4, 0);
        _visibleModRows[mod.Title] = row;
        if (active) AttachActiveRow(row, check, grip, mod.Title, description);
        else AttachAvailableRow(row, grip, mod.Title);
        AttachDependencyHover(row, mod.Title);
        AppendFixedRow(list, row, 30);
    }

    private void AttachActiveRow(Control row, CheckBox check, Label? grip,
        string title, string description)
    {
        var menu = new ContextMenuStrip();
        menu.Items.Add("Move to Top", null, (_, _) => MoveActive(title, null, "top"));
        menu.Items.Add("Move Up", null, (_, _) => MoveActive(title, null, "up"));
        menu.Items.Add("Move Down", null, (_, _) => MoveActive(title, null, "down"));
        menu.Items.Add("Move to Bottom", null, (_, _) => MoveActive(title, null, "bottom"));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Undo Order Change", null, (_, _) => UndoActiveOrder())
            .Enabled = _activeUndo.Count > 0;
        menu.Items.Add("Reset Default Order", null, (_, _) => ResetActiveOrder());
        menu.Items.Add("Dependencies…", null,
            (_, _) => ShowModDependencies(title, description));
        row.ContextMenuStrip = menu;
        check.ContextMenuStrip = menu;
        if (grip != null)
        {
            grip.ContextMenuStrip = menu;
            StartGripDrag(grip, title);
            AttachActiveDrop(grip, title);
        }
        AttachActiveDrop(row, title);
        AttachActiveDrop(check, title);
    }

    private static void StartGripDrag(Label grip, string title)
    {
        var starting = Point.Empty;
        grip.MouseDown += (_, e) =>
        {
            if (e.Button == MouseButtons.Left) starting = e.Location;
        };
        grip.MouseMove += (_, e) =>
        {
            if (e.Button != MouseButtons.Left ||
                Math.Abs(e.X - starting.X) < 4 &&
                Math.Abs(e.Y - starting.Y) < 4) return;
            var payload = new DataObject();
            payload.SetData(ActiveDragFormat, title);
            try { grip.DoDragDrop(payload, DragDropEffects.Move); }
            finally { starting = Point.Empty; }
        };
    }

    private void AttachAvailableRow(Control row, Label grip, string title)
    {
        var menu = new ContextMenuStrip();
        menu.Items.Add("Move to Top", null, (_, _) =>
            MoveAvailable(title, AvailableVrOrder().FirstOrDefault(), false));
        menu.Items.Add("Move to Bottom", null, (_, _) => MoveAvailable(title, null, false));
        menu.Items.Add("Dependencies…", null, (_, _) =>
            ShowModDependencies(title, "Mod settings and ordering information."));
        row.ContextMenuStrip = menu;
        grip.ContextMenuStrip = menu;
        StartGripDrag(grip, title);
        AttachAvailableDrop(row, title);
        AttachAvailableDrop(grip, title);
        foreach (Control child in row.Controls)
            if (child != grip && child.Enabled) AttachAvailableDrop(child, title);
    }

    private Control DropHighlightSurface(Control target)
    {
        return target.Parent is TableLayoutPanel parent && parent.Tag is string tag &&
            (tag == "active-vr-mod" || tag == "available-vr-mod")
            ? parent : target;
    }

    private void AttachDrop(Control target, string? targetTitle, bool activeSection)
    {
        target.AllowDrop = true;
        var surface = DropHighlightSurface(target);
        void Check(DragEventArgs e)
        {
            var source = e.Data?.GetDataPresent(ActiveDragFormat) == true
                ? e.Data.GetData(ActiveDragFormat) as string : null;
            var valid = source != null && source != targetTitle &&
                (activeSection ? ActiveVrOrder().Contains(source) :
                    AvailableVrOrder().Contains(source));
            e.Effect = valid ? DragDropEffects.Move : DragDropEffects.None;
            if (valid)
            {
                var point = surface.PointToClient(new Point(e.X, e.Y));
                MarkDropLine(surface, targetTitle != null &&
                    point.Y > surface.Height / 2);
            }
            else MarkDropLine(null);
        }
        target.DragEnter += (_, e) => Check(e);
        target.DragOver += (_, e) => Check(e);
        target.DragLeave += (_, _) => MarkDropLine(null);
        target.DragDrop += (_, e) =>
        {
            Check(e);
            var after = _dropLineAfter;
            MarkDropLine(null);
            if (e.Effect != DragDropEffects.Move ||
                e.Data?.GetData(ActiveDragFormat) is not string source) return;
            if (activeSection)
                MoveActive(source, targetTitle, after ? "after" : "before");
            else
                MoveAvailable(source, targetTitle, after);
        };
    }

    private void AttachActiveDrop(Control target, string? targetTitle) =>
        AttachDrop(target, targetTitle, true);

    private void AttachAvailableDrop(Control target, string? targetTitle) =>
        AttachDrop(target, targetTitle, false);

    internal static void ValidateActiveVrPolicies()
    {
        var verified = VerifiedVrMods();
        if (verified.Length != 5 || verified.Distinct().Count() != 5 ||
            DependenciesFor("Directional Dash").Length != 3 ||
            DependenciesFor("No Weapon Wheel").Length != 1 ||
            DependenciesFor("PAINSAW: RIPSHIFT OVERDRIVE").Length != 0)
            throw new InvalidDataException("VR active-mod inventory/dependency policy changed.");

        // Only enabled confirmed mods can appear in the Active Mods area.
        using (var form = new ModsForm())
        {
            if (form.ActiveVrOrder().Any(name => !verified.Contains(name)))
                throw new InvalidDataException("Unsupported mod in Active Mods.");
            if (form.ActiveVrOrder().Count(name => name == GloryModTitle) > 1)
                throw new InvalidDataException("Glory speed appears twice.");
        }
    }
}
