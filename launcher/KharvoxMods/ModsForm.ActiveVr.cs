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
    private Control? _selectedDependencyRow;

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

    private void OnVrSelectionChanged(string title, bool enabled)
    {
        var current = (_modPrefs.ActiveOrder ?? Array.Empty<string>())
            .Where(name => name != title).ToList();
        if (enabled && VerifiedVrMods().Contains(title)) current.Add(title);
        _modPrefs.ActiveOrder = current.ToArray();
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
    private static string[] DependenciesFor(string mod) => mod switch
    {
        "Directional Dash" => new[] {
            "Requires: Weapon Wheel Button Remap, Behind-the-Head Weapon Wheel",
            "Conflicts with: No Weapon Wheel" },
        "Behind-the-Head Weapon Wheel" => new[] { "Conflicts with: No Weapon Wheel" },
        "No Weapon Wheel" => new[] {
            "Conflicts with: Behind-the-Head Weapon Wheel, Directional Dash" },
        "Dynamic Quick Holsters" => new[] { "Requires: Hands enabled in main KHARVOX launcher" },
        _ => Array.Empty<string>()
    };

    private void ShowModDependencies(string name, string description)
    {
        var dependencies = DependenciesFor(name);
        if (dependencies.Length == 0)
        {
            MessageBox.Show(this,
                description + "\n\nNo verified inter-mod dependencies for this feature.",
                name, MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        var info = description + "\n\n" + string.Join("\n", dependencies);
        MessageBox.Show(this, info + "\n\nUnsupported dependent mods remain disabled.",
            name + " — Dependencies", MessageBoxButtons.OK,
            MessageBoxIcon.Information);
        var firstDependency = name switch
        {
            "Directional Dash" => "Weapon Wheel Button Remap",
            "Behind-the-Head Weapon Wheel" => "No Weapon Wheel",
            "No Weapon Wheel" => "Behind-the-Head Weapon Wheel",
            _ => ""
        };
        if (_visibleModRows.TryGetValue(firstDependency, out var row) &&
            !row.IsDisposed)
        {
            if (_selectedDependencyRow != null && !_selectedDependencyRow.IsDisposed)
                _selectedDependencyRow.BackColor = DarkPanel;
            _selectedDependencyRow = row;
            row.BackColor = Color.FromArgb(65, 58, 35);
            _vrModScroll.ScrollControlIntoView(row);
        }
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
            _selectedDependencyRow = null;
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
                    Text = "⋮⋮  Drag active mods to reorder  •  Right-click for Undo / Reset",
                    ForeColor = Color.FromArgb(145, 145, 150),
                    BackColor = DarkPanel, Dock = DockStyle.Fill,
                    AllowDrop = true, TextAlign = ContentAlignment.MiddleLeft,
                    Font = new Font("Segoe UI", 8f),
                    Margin = new Padding(2, 0, 0, 2)
                };
                AttachActiveDrop(dropEnd, null);
                AppendFixedRow(_vrModList, dropEnd, 24);
            }

            AppendSection(_vrModList, "AVAILABLE VR MODS");
            if (!IsVrActive(GloryModTitle))
                AddGlorySpeedRow(_vrModList, false);
            foreach (var group in OriginalModGroups)
            {
                var remaining = group.Mods.Where(mod => !IsVrActive(mod.Title)).ToArray();
                if (remaining.Length == 0) continue;
                AppendSection(_vrModList, group.Title);
                foreach (var mod in remaining)
                    AppendVrMod(_vrModList, mod, false);
            }
            AppendSection(_vrModList, "⚙  Dev — Gameplay Overrides");
            foreach (var dev in DevOverrides)
                AppendDeveloperMod(_vrModList, dev);
        }
        finally { _vrModList.ResumeLayout(true); }
    }

    private void AddGlorySpeedRow(TableLayoutPanel list, bool active)
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Height = 34,
            ColumnCount = 4, RowCount = 1,
            BackColor = DarkPanel, Padding = new Padding(2, 0, 0, 0),
            Margin = Padding.Empty
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 174));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 58));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 24));
        var check = MakePreviewCheck(GloryModTitle,
            "Override just Glory Kill speed (30–240 Hz). No other animations affected.",
            "glory-kill-speed-preview");
        check.Enabled = true;
        check.Checked = active;
        check.Dock = DockStyle.Fill;
        panel.Controls.Add(check, 0, 0);

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
        panel.Controls.Add(slider, 1, 0);
        var speedLabel = new Label
        {
            Text = _modPrefs.GloryKillHz + " Hz",
            BackColor = DarkPanel, ForeColor = Color.Gainsboro,
            Visible = check.Checked, Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft
        };
        panel.Controls.Add(speedLabel, 2, 0);
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
        Label? grip = null;
        if (active)
        {
            grip = new Label
            {
                Text = "⋮⋮", Dock = DockStyle.Fill,
                ForeColor = Color.FromArgb(178, 178, 185),
                TextAlign = ContentAlignment.MiddleCenter,
                Cursor = Cursors.SizeNS,
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold)
            };
            panel.Controls.Add(grip, 3, 0);
            AttachActiveRow(panel, check, grip, GloryModTitle,
                "Glory Kill speed override has no other VR-mod prerequisites.");
        }
        _visibleModRows[GloryModTitle] = panel;
        AppendFixedRow(list, panel, 36);
    }

    private void AppendVrMod(TableLayoutPanel list, PreviewMod mod, bool active)
    {
        var number = Array.FindIndex(OriginalModGroups.SelectMany(x => x.Mods)
            .ToArray(), x => x.Title == mod.Title) + 1;
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Height = 30,
            ColumnCount = 5, RowCount = 1,
            BackColor = DarkPanel, Margin = Padding.Empty,
            Padding = Padding.Empty,
            AllowDrop = active,
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
            "original-vr-mod");
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
            Text = active ? "⋮⋮" : "", Dock = DockStyle.Fill,
            ForeColor = Color.FromArgb(178,178,185),
            TextAlign = ContentAlignment.MiddleCenter,
            Cursor = active ? Cursors.SizeNS : Cursors.Default,
            Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
            AllowDrop = active
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
            var startedAt = Point.Empty;
            grip.MouseDown += (_, e) =>
            {
                if (e.Button == MouseButtons.Left) startedAt = e.Location;
            };
            grip.MouseMove += (_, e) =>
            {
                if (e.Button != MouseButtons.Left ||
                    (Math.Abs(e.X - startedAt.X) < 4 &&
                     Math.Abs(e.Y - startedAt.Y) < 4)) return;
                var drag = new DataObject();
                drag.SetData(ActiveDragFormat, title);
                try { grip.DoDragDrop(drag, DragDropEffects.Move); }
                finally { startedAt = Point.Empty; }
            };
            AttachActiveDrop(grip, title);
        }
        AttachActiveDrop(row, title);
        AttachActiveDrop(check, title);
    }

    private void AttachActiveDrop(Control target, string? targetTitle)
    {
        target.AllowDrop = true;
        void Check(DragEventArgs e)
        {
            var source = e.Data?.GetDataPresent(ActiveDragFormat) == true
                ? e.Data.GetData(ActiveDragFormat) as string : null;
            e.Effect = source != null && ActiveVrOrder().Contains(source) &&
                source != targetTitle
                ? DragDropEffects.Move : DragDropEffects.None;
        }
        target.DragEnter += (_, e) => Check(e);
        target.DragOver += (_, e) => Check(e);
        target.DragDrop += (_, e) =>
        {
            Check(e);
            if (e.Effect == DragDropEffects.Move &&
                e.Data?.GetData(ActiveDragFormat) is string source)
                MoveActive(source, targetTitle, "before");
        };
    }

    internal static void ValidateActiveVrPolicies()
    {
        var verified = VerifiedVrMods();
        if (verified.Length != 5 || verified.Distinct().Count() != 5 ||
            DependenciesFor("Directional Dash").Length != 2 ||
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
