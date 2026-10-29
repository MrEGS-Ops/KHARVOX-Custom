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
        var saved = _modPrefs.ActiveOrder ?? Array.Empty<string>();
        var ordered = saved.Where(selected.Contains)
            .Distinct(StringComparer.Ordinal)
            .Concat(selected.Where(name => !saved.Contains(name))).ToArray();
        // Categories are atomic blocks in Active Mods. Preserve block order
        // based on each category's first appearance in saved preferences.
        return ordered.GroupBy(CategoryFor).SelectMany(group => group).ToArray();
    }

    private void StoreActiveOrder(string[] next, bool saveUndo = true)
    {
        var current = ActiveVrOrder();
        var selected = VerifiedVrMods().Where(IsVrActive).ToArray();
        var normalized = next.Where(selected.Contains)
            .Distinct(StringComparer.Ordinal)
            .Concat(selected.Where(item => !next.Contains(item)))
            .GroupBy(CategoryFor).SelectMany(group => group).ToArray();
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

    private static string CategoryFor(string title)
    {
        if (title == GloryModTitle) return "✦  Weapons & Combat";
        if (DevOverrides.Any(dev => dev.Title == title))
            return "⚙  Dev — Gameplay Overrides";
        return OriginalModGroups.First(group =>
            group.Mods.Any(mod => mod.Title == title)).Title;
    }

    private static int ModNumber(string title) =>
        Array.IndexOf(AllDisplayMods(), title) + 1;

    private string[] AvailableVrOrder()
    {
        var inactive = AllDisplayMods().Where(title => !IsVrActive(title)).ToArray();
        var saved = _modPrefs.AvailableOrder ?? Array.Empty<string>();
        // Independently preserve the order WITHIN each category. Never allow
        // a disabled or unverified mod to drift into another category.
        return inactive.GroupBy(CategoryFor).SelectMany(category =>
            saved.Where(name => category.Contains(name))
                .Distinct(StringComparer.Ordinal)
                .Concat(category.Where(name => !saved.Contains(name))))
            .ToArray();
    }

    private void MoveAvailable(string source, string? target, bool after)
    {
        if (target != null && CategoryFor(source) != CategoryFor(target)) return;
        var items = AvailableVrOrder().ToList();
        if (!items.Remove(source) || items.Count == 0) return;
        int index;
        if (target == null)
            index = items.FindLastIndex(name => CategoryFor(name) ==
                CategoryFor(source)) + 1;
        else
        {
            index = items.IndexOf(target);
            if (index < 0) return;
            if (after) index++;
        }
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
        if (!items.Contains(source) || items.Count < 2) return;
        var category = CategoryFor(source);
        if (target != null && CategoryFor(target) != category)
        {
            // A cross-category drag moves ALL active mods in this category
            // together, without mixing them into the destination category.
            var groups = items.Select(CategoryFor).Distinct().ToList();
            groups.Remove(category);
            var other = CategoryFor(target);
            var index = groups.IndexOf(other);
            if (index < 0) return;
            if (operation == "after") index++;
            groups.Insert(Math.Min(index, groups.Count), category);
            StoreActiveOrder(groups.SelectMany(group =>
                items.Where(name => CategoryFor(name) == group)).ToArray());
            return;
        }
        var own = items.Where(name => CategoryFor(name) == category).ToList();
        var from = own.IndexOf(source);
        if (from < 0 || own.Count < 2) return;
        own.RemoveAt(from);
        int to;
        if (operation == "top") to = 0;
        else if (operation == "bottom") to = own.Count;
        else if (operation == "up") to = Math.Max(0, from - 1);
        else if (operation == "down") to = Math.Min(own.Count, from + 1);
        else to = target == null ? own.Count : own.IndexOf(target);
        if (to < 0) to = own.Count;
        else if (operation == "after") to++;
        own.Insert(Math.Min(to, own.Count), source);
        var result = items.SelectMany(name => new[] { CategoryFor(name) })
            .Distinct().SelectMany(group => group == category
                ? own : items.Where(name => CategoryFor(name) == group));
        StoreActiveOrder(result.ToArray());
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

    private static IEnumerable<Control> EnumerateRowControls(Control root)
    {
        yield return root;
        foreach (Control child in root.Controls)
            foreach (var descendant in EnumerateRowControls(child))
                yield return descendant;
    }

    private void RepaintDependencyRows()
    {
        foreach (var pair in _visibleModRows)
        {
            var tint = _dependencyHighlights.TryGetValue(pair.Key, out var indicator)
                ? (indicator.G > indicator.R
                    ? Color.FromArgb(37, 100, 65)
                    : Color.FromArgb(112, 42, 45))
                : (pair.Value.Tag as string == "active-vr-mod"
                    ? Color.FromArgb(37, 38, 42) : DarkPanel);
            pair.Value.BackColor = tint;
            foreach (var child in EnumerateRowControls(pair.Value).Skip(1))
                if (!(child is TrackBar)) child.BackColor = tint;
            pair.Value.Invalidate(true);
        }
    }

    private void SetDependencyHover(string name)
    {
        _dependencyHighlights.Clear();
        foreach (var link in DependencyLinks(name))
            _dependencyHighlights[link.Name] = link.Required
                ? Color.FromArgb(65, 212, 123) : Color.FromArgb(240, 84, 80);
        RepaintDependencyRows();
    }

    private void ClearDependencyHover()
    {
        if (_dependencyHighlights.Count == 0) return;
        _dependencyHighlights.Clear();
        RepaintDependencyRows();
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
        var grip = EnumerateRowControls(row).OfType<Label>()
            .FirstOrDefault(label => string.Equals(label.Tag as string,
                "vr-drag-grip", StringComparison.Ordinal));
        if (grip != null) grip.Text = "";
        void Reveal(object? sender, EventArgs e)
        {
            // The drag target must remain Visible for the mouse enter and
            // mouse move events to fire correctly. Only toggle its glyph.
            if (grip != null) grip.Text = "⋮⋮";
            Enter(sender, e);
        }
        void Conceal(object? sender, EventArgs e)
        {
            if (grip != null && !row.RectangleToScreen(row.ClientRectangle)
                    .Contains(Cursor.Position))
                grip.Text = "";
            Leave(sender, e);
        }
        foreach (var control in EnumerateRowControls(row).ToArray())
        {
            control.MouseEnter += Reveal;
            control.MouseLeave += Conceal;
            if (control != row)
                control.Paint += (_, e) =>
                {
                    if (_dropLineRow != row) return;
                    using (var pen = new Pen(Color.FromArgb(70, 180, 255), 3))
                    {
                        var y = _dropLineAfter ? control.Height - 2 : 1;
                        e.Graphics.DrawLine(pen, 0, y,
                            Math.Max(1, control.Width), y);
                    }
                };
        }
    }

    private void MarkDropLine(Control? row, bool after = false)
    {
        if (_dropLineRow == row && _dropLineAfter == after) return;
        var previous = _dropLineRow;
        _dropLineRow = row;
        _dropLineAfter = after;
        previous?.Invalidate(true);
        row?.Invalidate(true);
    }

    private void ShowModDependencies(string name, string description)
    {
        var dependencies = DependenciesFor(name);
        var number = ModNumber(name);
        var heading = number > 0 ? "#" + number.ToString("00") + " " + name : name;
        var howToUse = name switch
        {
            "Weapon Wheel Button Remap" =>
                "In DOOM: tap A to quick-switch; hold A for the wheel and use the left stick.",
            "Hand Focus" =>
                "In DOOM: activate with right-stick click, then aim or select with your hand.",
            "Physical Crouch" =>
                "In DOOM: physically lower your headset to crouch; stand to rise.",
            "PAINSAW: RIPSHIFT OVERDRIVE" =>
                "During a chainsaw execution: release and repull the trigger to rev in short bursts. Holding the trigger does not continuously rev.",
            "Glory Kill Speed Override" =>
                "Enable this mod, then move its slider. Right is faster (30 Hz); left is slower (240 Hz). Changes apply on the next DOOM launch.",
            _ => "This mod is currently paused. You can organize its row, but it cannot be enabled until its runtime support is verified."
        };
        var info = "How to use:\n" + howToUse + "\n\n" +
            "Category: " + CategoryFor(name).Trim();
        if (dependencies.Length > 0)
            info += "\n\nDependencies:\n" + string.Join("\n", dependencies);
        MessageBox.Show(this, info, heading + " — Mod Info",
            MessageBoxButtons.OK, MessageBoxIcon.Information);
    }

    private GroupBox BuildOriginalVrPanel()
    {
        var group = MakeOriginalGroup("VR MODS");
        _vrModScroll.Dock = DockStyle.Fill;
        _vrModScroll.AutoScroll = true;
        _vrModScroll.BackColor = DarkPanel;

        var content = new TableLayoutPanel
        {
            Dock = DockStyle.Top, ColumnCount = 1, RowCount = 1,
            AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = DarkPanel, Padding = new Padding(6, 2, 5, 6),
            Margin = Padding.Empty
        };
        content.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        content.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        _vrModList.Dock = DockStyle.Top;
        _vrModList.AutoSize = true;
        _vrModList.AutoSizeMode = AutoSizeMode.GrowAndShrink;
        _vrModList.BackColor = DarkPanel;
        _vrModList.Padding = new Padding(0, 4, 0, 5);
        _vrModList.Margin = Padding.Empty;
        _vrModList.ColumnCount = 1;
        _vrModList.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        content.Controls.Add(_vrModList, 0, 0);
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

            var activeList = new TableLayoutPanel
            {
                Dock = DockStyle.Top, AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                ColumnCount = 1, RowCount = 0,
                BackColor = Color.FromArgb(37, 38, 42),
                Margin = Padding.Empty, Padding = new Padding(5, 3, 5, 6)
            };
            activeList.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
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
            AppendFixedRow(activeList, header, 30);
            if (active.Length == 0)
            {
                AppendFixedRow(activeList, new Label
                {
                    Text = "Check a VR mod below to add it here.",
                    ForeColor = Color.Silver, BackColor = DarkPanel,
                    Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft
                }, 28);
            }
            else
            {
                string activeCategory = "";
                foreach (var name in active)
                {
                    var category = CategoryFor(name);
                    if (category != activeCategory)
                    {
                        AppendSection(activeList, category);
                        activeCategory = category;
                    }
                    if (name == GloryModTitle) AddGlorySpeedRow(activeList, true);
                    else
                    {
                        var mod = OriginalModGroups.SelectMany(g => g.Mods)
                            .First(m => m.Title == name);
                        AppendVrMod(activeList, mod, true);
                    }
                }
            }


            var activeCard = new Panel
            {
                Dock = DockStyle.Top, AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                BorderStyle = BorderStyle.FixedSingle,
                BackColor = Color.FromArgb(37, 38, 42),
                Margin = new Padding(0, 1, 3, 0),
                Padding = new Padding(4)
            };
            activeCard.Controls.Add(activeList);
            var cardRow = _vrModList.RowCount++;
            _vrModList.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            _vrModList.Controls.Add(activeCard, 0, cardRow);

            // The inactive list remains separate from the active card.
            AppendFixedRow(_vrModList, new Panel
            {
                Dock = DockStyle.Fill, BackColor = DarkPanel,
                Margin = Padding.Empty
            }, 15);
            AppendSection(_vrModList, "AVAILABLE VR MODS");
            string lastGroup = "";
            foreach (var name in AvailableVrOrder())
            {
                var category = CategoryFor(name);
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

    // Every row is a grid: number | name | optional speed | Hz | info | grip.
    // The information and drag columns are anchored to the right edge,
    // not appended immediately after the checkbox caption.
    private void AddGlorySpeedRow(TableLayoutPanel list, bool active)
    {
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Height = 30,
            ColumnCount = 6, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = active ? Color.FromArgb(37, 38, 42) : DarkPanel,
            AllowDrop = true,
            Tag = active ? "active-vr-mod" : "available-vr-mod"
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 30));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 112));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 50));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 28));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 22));

        var number = new Label
        {
            Text = ModNumber(GloryModTitle).ToString("00") + ".",
            Dock = DockStyle.Fill, Margin = new Padding(2, 0, 0, 0),
            BackColor = row.BackColor, ForeColor = Color.WhiteSmoke,
            TextAlign = ContentAlignment.MiddleLeft
        };
        var check = MakePreviewCheck(GloryModTitle,
            "Control only Glory Kill speed (30–240 Hz).",
            "glory-kill-speed-preview");
        check.Enabled = true;
        check.Checked = active;
        check.Dock = DockStyle.Fill;
        check.AutoSize = false;
        check.AutoEllipsis = true;
        check.TextAlign = ContentAlignment.MiddleLeft;
        check.CheckAlign = ContentAlignment.MiddleLeft;
        check.Margin = Padding.Empty;
        check.BackColor = row.BackColor;

        var slider = new TrackBar
        {
            Minimum = 30, Maximum = 240,
            Value = 270 - Math.Max(30, Math.Min(240, _modPrefs.GloryKillHz)),
            AutoSize = false, TickStyle = TickStyle.None,
            Dock = DockStyle.Fill, Height = 26,
            Visible = active, Enabled = active,
            Margin = new Padding(0, 3, 2, 3),
            Tag = "glory-speed-slider"
        };
        var speed = new Label
        {
            Text = _modPrefs.GloryKillHz + " Hz",
            Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft,
            Margin = Padding.Empty, BackColor = row.BackColor,
            ForeColor = Color.Gainsboro, Visible = active
        };
        _tooltips.SetToolTip(slider,
            "Right is faster (30 Hz); left is slower (240 Hz).");
        check.CheckedChanged += (_, _) =>
        {
            _modPrefs.GloryKillSpeedOverride = check.Checked;
            slider.Visible = check.Checked;
            speed.Visible = check.Checked;
            OnVrSelectionChanged(GloryModTitle, check.Checked);
        };
        slider.ValueChanged += (_, _) =>
        {
            _modPrefs.GloryKillHz = 270 - slider.Value;
            speed.Text = _modPrefs.GloryKillHz + " Hz";
        };
        slider.MouseUp += (_, _) => SaveModSelections();
        slider.KeyUp += (_, _) => SaveModSelections();
        slider.Leave += (_, _) => SaveModSelections();

        var info = MakeRowInfo("Glory Kill speed: 30–240 Hz. Click for usage instructions.",
            GloryModTitle, "Only Glory Kill animations are affected.");
        info.BackColor = row.BackColor;
        var grip = MakeRowGrip(row.BackColor);

        row.Controls.Add(number, 0, 0);
        row.Controls.Add(check, 1, 0);
        row.Controls.Add(slider, 2, 0);
        row.Controls.Add(speed, 3, 0);
        row.Controls.Add(info, 4, 0);
        row.Controls.Add(grip, 5, 0);
        if (active)
            AttachActiveRow(row, check, grip, GloryModTitle, "Glory Kill speed override");
        else
            AttachAvailableRow(row, grip, GloryModTitle);
        AttachDependencyHover(row, GloryModTitle);
        _visibleModRows[GloryModTitle] = row;
        AppendFixedRow(list, row, 30);
    }

    private LinkLabel MakeRowInfo(string help, string mod, string description)
    {
        var info = new LinkLabel
        {
            Text = "ⓘ",
            Dock = DockStyle.Fill,
            Margin = Padding.Empty, Padding = Padding.Empty,
            ForeColor = Color.Silver, LinkColor = Color.Silver,
            ActiveLinkColor = Color.White,
            TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 9f),
            Tag = "vr-mod-info"
        };
        _tooltips.SetToolTip(info, help);
        info.LinkClicked += (_, _) => ShowModDependencies(mod, description);
        return info;
    }

    private static Label MakeRowGrip(Color backColor) => new Label
    {
        // Leave the control visible for WinForms hit testing. Change text,
        // not Visible, on hover so the drag handle cannot disappear.
        Text = "",
        Tag = "vr-drag-grip",
        Dock = DockStyle.Fill, Margin = Padding.Empty,
        BackColor = backColor,
        ForeColor = Color.FromArgb(178, 178, 185),
        TextAlign = ContentAlignment.MiddleCenter,
        Cursor = Cursors.SizeNS,
        Font = new Font("Segoe UI", 9.5f, FontStyle.Bold)
    };

    private void AppendVrMod(TableLayoutPanel list, PreviewMod mod, bool active, bool isDev = false)
    {
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Height = 29,
            ColumnCount = 4, RowCount = 1,
            BackColor = active ? Color.FromArgb(37, 38, 42) : DarkPanel,
            Margin = Padding.Empty, Padding = Padding.Empty,
            AllowDrop = true,
            Tag = active ? "active-vr-mod" : "available-vr-mod"
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 30));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 28));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 22));

        var number = new Label
        {
            Text = ModNumber(mod.Title).ToString("00") + ".",
            Dock = DockStyle.Fill, BackColor = row.BackColor,
            ForeColor = Color.WhiteSmoke, TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(2, 0, 0, 0)
        };
        var description = mod.Help;
        var dependencies = DependenciesFor(mod.Title);
        if (dependencies.Length > 0)
            description += Environment.NewLine + string.Join(Environment.NewLine, dependencies);
        var check = MakePreviewCheck(mod.Title,
            description + Environment.NewLine +
            (IsConfirmedMod(mod.Title)
                ? "Verified core feature. Changes apply on next DOOM launch."
                : "Paused; not available in this build."),
            isDev ? "dev-override" : "original-vr-mod");
        check.Enabled = IsConfirmedMod(mod.Title);
        check.Checked = active;
        check.Dock = DockStyle.Fill;
        check.AutoSize = false;
        check.AutoEllipsis = true;
        check.TextAlign = ContentAlignment.MiddleLeft;
        check.CheckAlign = ContentAlignment.MiddleLeft;
        check.Margin = Padding.Empty;
        check.BackColor = row.BackColor;
        if (check.Enabled)
            check.CheckedChanged += (_, _) => ApplySelection(mod.Title, check.Checked);

        var info = MakeRowInfo("How to use this mod and its dependencies",
            mod.Title, mod.Help);
        info.BackColor = row.BackColor;
        var grip = MakeRowGrip(row.BackColor);

        row.Controls.Add(number, 0, 0);
        row.Controls.Add(check, 1, 0);
        row.Controls.Add(info, 2, 0);
        row.Controls.Add(grip, 3, 0);
        if (active) AttachActiveRow(row, check, grip, mod.Title, description);
        else AttachAvailableRow(row, grip, mod.Title);
        AttachDependencyHover(row, mod.Title);
        _visibleModRows[mod.Title] = row;
        AppendFixedRow(list, row, 29);
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
            MoveAvailable(title, AvailableVrOrder()
                .FirstOrDefault(name => CategoryFor(name) == CategoryFor(title)), false));
        menu.Items.Add("Move to Bottom", null, (_, _) => MoveAvailable(title, null, false));
        menu.Items.Add("Dependencies…", null, (_, _) =>
            ShowModDependencies(title, "Mod settings and ordering information."));
        row.ContextMenuStrip = menu;
        grip.ContextMenuStrip = menu;
        StartGripDrag(grip, title);
        AttachAvailableDrop(row, title);
        AttachAvailableDrop(grip, title);
        foreach (var child in EnumerateRowControls(row).Skip(1))
            if (child != grip && child.Enabled && child is not LinkLabel)
                AttachAvailableDrop(child, title);
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
                    AvailableVrOrder().Contains(source) &&
                    (targetTitle == null || CategoryFor(source) == CategoryFor(targetTitle)));
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
            DependenciesFor("No Weapon Wheel").Length != 2 ||
            DependenciesFor("PAINSAW: RIPSHIFT OVERDRIVE").Length != 0)
            throw new InvalidDataException("VR active-mod inventory/dependency policy changed.");

        // Only enabled confirmed mods can appear in the Active Mods area.
        using (var form = new ModsForm())
        {
            if (form.ActiveVrOrder().Any(name => !verified.Contains(name)))
                throw new InvalidDataException("Unsupported mod in Active Mods.");
            if (form.ActiveVrOrder().Count(name => name == GloryModTitle) > 1)
                throw new InvalidDataException("Glory speed appears twice.");
            var glory = form._visibleModRows[GloryModTitle];
            var controls = EnumerateRowControls(glory).ToArray();
            var info = controls.OfType<LinkLabel>()
                .SingleOrDefault(x => (x.Tag as string) == "vr-mod-info");
            var grip = controls.OfType<Label>()
                .SingleOrDefault(x => (x.Tag as string) == "vr-drag-grip");
            var slider = controls.OfType<TrackBar>().SingleOrDefault();
            if (info == null || grip == null || slider == null ||
                grip.Text != "" ||
                glory is not TableLayoutPanel grid ||
                grid.GetColumn(info) != 4 ||
                grid.GetColumn(grip) != 5 ||
                grid.GetColumn(slider) != 2)
                throw new InvalidDataException(
                    "Glory row must center slider, info and hover grip in fixed columns.");

            // Standard rows must keep their info/drag columns anchored right.
            var other = form._visibleModRows["Hand Focus"];
            var normal = other as TableLayoutPanel;
            var normalInfo = EnumerateRowControls(other).OfType<LinkLabel>()
                .SingleOrDefault(x => (x.Tag as string) == "vr-mod-info");
            var normalGrip = EnumerateRowControls(other).OfType<Label>()
                .SingleOrDefault(x => (x.Tag as string) == "vr-drag-grip");
            if (normal == null || normalInfo == null || normalGrip == null ||
                normal.GetColumn(normalInfo) != 2 ||
                normal.GetColumn(normalGrip) != 3 ||
                normalGrip.Text != "")
                throw new InvalidDataException("Standard row info or drag handle regression.");
        }
    }
}
