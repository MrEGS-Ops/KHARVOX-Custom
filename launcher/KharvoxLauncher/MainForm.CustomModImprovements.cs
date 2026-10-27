using System.ComponentModel;
using System.Text.RegularExpressions;

namespace KharvoxLauncher;

// UI-only polish. These operations never change the enabled mods or gameplay flags.
public sealed partial class MainForm
{
    private System.Windows.Forms.Timer? modLinkHighlightTimer;
    private readonly List<ModOrderSlot> highlightedModLinkSlots = new();

    private void FinishModOrderChange(string[]? before)
    {
        if (before is not null)
            modOrderUndo.Push(before);
        RefreshGroupedModRows();
        try { SaveModOrder(); }
        catch (Exception error) when (error is IOException
            || error is UnauthorizedAccessException)
        {
            MessageBox.Show(customOptionsForm,
                "New mod order is displayed but could not be saved: "
                + error.Message, "KHARVOX", MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
        }
    }

    private bool MoveModAtSlot(int slotIndex, int offset)
    {
        var target = slotIndex + offset;
        if (slotIndex < 0 || target < 0 || target >= modOrderRows.Count
            || modOrderRows[slotIndex].Group != modOrderRows[target].Group)
            return false;
        var before = modOrderRows.Select(row => row.Id).ToArray();
        var movingId = modOrderRows[slotIndex].Id;
        if (!MoveModWithinGroup(modOrderRows, movingId, modOrderRows[target].Id))
            return false;
        FinishModOrderChange(before);
        return true;
    }

    private void UndoModOrder()
    {
        if (modOrderUndo.Count == 0) return;
        var previous = modOrderUndo.Pop();
        modOrderRows.Clear();
        modOrderRows.AddRange(NormalizeModOrder(
            modOrderDefaultRows, previous));
        FinishModOrderChange(null);
    }

    private void ResetModOrderGroup(int group)
    {
        var before = modOrderRows.Select(row => row.Id).ToArray();
        var preferred = new List<string>();
        foreach (var otherGroup in Enumerable.Range(0, ModGroupHeaders.Length))
        {
            preferred.AddRange(otherGroup == group
                ? defaultModIds.Where(id => modOrderDefaultRows.Any(row =>
                    row.Id == id && row.Group == group))
                : modOrderRows.Where(row => row.Group == otherGroup)
                    .Select(row => row.Id));
        }
        var reordered = NormalizeModOrder(modOrderDefaultRows, preferred);
        if (before.SequenceEqual(reordered.Select(row => row.Id))) return;
        modOrderRows.Clear();
        modOrderRows.AddRange(reordered);
        FinishModOrderChange(before);
    }

    private void ResetAllModOrder()
    {
        var before = modOrderRows.Select(row => row.Id).ToArray();
        if (before.SequenceEqual(defaultModIds)) return;
        modOrderRows.Clear();
        modOrderRows.AddRange(modOrderDefaultRows);
        FinishModOrderChange(before);
    }

    private void AttachModOrderContext(ModOrderSlot slot)
    {
        var menu = new ContextMenuStrip();
        var undo = menu.Items.Add("Undo last move");
        menu.Items.Add(new ToolStripSeparator());
        var up = menu.Items.Add("Move up");
        var down = menu.Items.Add("Move down");
        menu.Items.Add(new ToolStripSeparator());
        var resetGroup = menu.Items.Add("Reset this group");
        var resetAll = menu.Items.Add("Reset all groups");

        undo.Click += (_, _) => UndoModOrder();
        up.Click += (_, _) => MoveModAtSlot(slot.Index, -1);
        down.Click += (_, _) => MoveModAtSlot(slot.Index, 1);
        resetGroup.Click += (_, _) => ResetModOrderGroup(slot.Group);
        resetAll.Click += (_, _) => ResetAllModOrder();
        menu.Opening += (_, _) =>
        {
            undo.Enabled = modOrderUndo.Count != 0;
            up.Enabled = slot.Index > 0
                && modOrderRows[slot.Index - 1].Group == slot.Group;
            down.Enabled = slot.Index + 1 < modOrderRows.Count
                && modOrderRows[slot.Index + 1].Group == slot.Group;
            resetGroup.Enabled = modOrderRows.Where(x => x.Group == slot.Group)
                .Select(x => x.Id).SequenceEqual(modOrderDefaultRows
                    .Where(x => x.Group == slot.Group).Select(x => x.Id)) == false;
            resetAll.Enabled = !modOrderRows.Select(x => x.Id)
                .SequenceEqual(defaultModIds);
        };
        foreach (Control target in new Control[] {
            slot.Panel, slot.Number, slot.Grip, slot.Dependency })
            target.ContextMenuStrip = menu;
    }

    private static (string Symbol, string State, string Detail, Color Color)
        DevelopmentState(string id) => id switch
    {
        "weapon-wheel-remap" => ("✓", "Verified",
            "Previously reported working in-game; retest this build.", Color.LightGreen),
        "chainsaw-animation-investigator" => ("D", "Diagnostic only",
            "Read-only chainsaw timing and animation candidate log; no gameplay changes.",
            Color.FromArgb(194,194,200)),
        "revenge-demon" => ("D", "Diagnostic only",
            "Collects Supervisor traces; does not yet empower a demon.", Color.FromArgb(186, 186, 190)),
        "persistent-ssg-reload" => ("!", "Incomplete",
            "Now makes a guarded attempt to trigger DOOM's original SSG reload using native reload CVars when redrawn, but headset verification and a true animation completion hook are still missing. Never count the timer alone as success.", Color.Orange),
        "haptic-overhaul" => ("!", "Incomplete",
            "The damage pulse policy is implemented, but live DOOM health/armour capture is not yet connected. Enabling this does not generate damage feedback until verified vitals are available.", Color.Orange),
        "gauss-slow-movement" => ("!", "Incomplete",
            "Experimental work; movement behaviour still requires validation.", Color.Orange),
        _ => ("◇", "Experimental",
            "Not yet fully validated in the current game build.", Color.Khaki)
    };

    private void PopulateDependencyLinks(ModOrderSlot slot, ModOrderRow item,
        string dependencies, IReadOnlyDictionary<CheckBox, int> numbers)
    {
        var label = slot.Dependency;
        label.Links.Clear();
        // Single-line compact references preserve each clickable #number;
        // complete Requires/Disables wording stays available on hover.
        var compact = dependencies.TrimStart(' ', '—')
            .Replace("Requires ", "R ")
            .Replace("Disables ", "D ")
            .Replace("disables ", "D ")
            .Replace("; ", "  ")
            .Replace(", ", ",");
        label.Text = compact;
        label.AccessibleName = item.Caption + " — " + dependencies.TrimStart(' ', '—');
        label.Visible = label.Text.Length != 0;
        var measured = TextRenderer.MeasureText(label.Text, label.Font,
            Size.Empty, TextFormatFlags.SingleLine | TextFormatFlags.NoPadding).Width;
        // Keep the name readable: limit the occupied width and use
        // ellipsis + tooltip for unusually long dependency descriptions.
        slot.Panel.ColumnStyles[2].Width = label.Visible
            ? Math.Min(133, measured + 6) : 0;
        if (!label.Visible) return;
        foreach (Match match in Regex.Matches(label.Text, @"#(\d+)"))
        {
            if (!int.TryParse(match.Groups[1].Value, out var position)
                || position < 1 || position > modOrderRows.Count) continue;
            label.Links.Add(match.Index, match.Length, modOrderRows[position - 1].Box);
        }
        var handsIndex = label.Text.IndexOf("#Hands", StringComparison.Ordinal);
        if (handsIndex >= 0)
            label.Links.Add(handsIndex, "#Hands".Length, showHands);
        statusToolTip.SetToolTip(label,
            "R = Requires; D = Disables. Hover to highlight ALL linked mods."
            + Environment.NewLine
            + (item.Box == customDynamicShoulderHolster
                ? "Enable Hands in main launcher (VR Options) is checked and locked while this mod is active."
                : dependencies.TrimStart(' ', '—'))
            + Environment.NewLine + item.Tip);
    }

    // R = teal, D = muted red. Mixed dependency labels light up BOTH groups
    // together but with different colors; no confusing all-amber grouping.
    private IEnumerable<(ModOrderSlot Slot, bool Disables)> ReferencedSlots(
        ModOrderSlot source)
    {
        var display = source.Dependency.Text;
        var disableOffset = display.IndexOf("D ", StringComparison.Ordinal);
        foreach (LinkLabel.Link link in source.Dependency.Links)
        {
            if (link.LinkData is not CheckBox box || box == showHands) continue;
            var index = modOrderRows.FindIndex(row => row.Box == box);
            if (index < 0 || index >= modOrderSlots.Count) continue;
            yield return (modOrderSlots[index],
                disableOffset >= 0 && link.Start >= disableOffset);
        }
    }

    private void HighlightDependencyReferences(ModOrderSlot source, bool persist)
    {
        ClearDependencyHighlight();
        foreach (var (slot, disables) in ReferencedSlots(source))
        {
            if (highlightedModLinkSlots.Contains(slot)) continue;
            highlightedModLinkSlots.Add(slot);
            var background = disables
                ? Color.FromArgb(79, 42, 43) // muted red = disables
                : Color.FromArgb(35, 69, 66); // muted teal = requires
            var foreground = disables
                ? Color.FromArgb(233, 153, 150)
                : Color.FromArgb(157, 225, 213);
            slot.Number.BackColor = background;
            slot.Number.ForeColor = foreground;
            slot.Panel.BackColor = background;
            slot.Dependency.BackColor = background;
            if (slot.Panel.Controls.OfType<CheckBox>().FirstOrDefault() is { } check)
                check.BackColor = background;
            slot.Panel.Invalidate();
        }
        if (persist)
        {
            modLinkHighlightTimer = new System.Windows.Forms.Timer { Interval = 1800 };
            modLinkHighlightTimer.Tick += (_, _) => ClearDependencyHighlight();
            modLinkHighlightTimer.Start();
            if (source.Dependency.Links.Cast<LinkLabel.Link>()
                .Any(link => ReferenceEquals(link.LinkData, showHands)))
            {
                BringToFront();
                Activate();
                showHands.Focus();
            }
        }
    }

    private void ClearDependencyHighlight()
    {
        modLinkHighlightTimer?.Stop();
        modLinkHighlightTimer?.Dispose();
        modLinkHighlightTimer = null;
        foreach (var slot in highlightedModLinkSlots)
        {
            if (slot.Panel.IsDisposed) continue;
            slot.Number.BackColor = PanelColor;
            slot.Number.ForeColor = Color.White;
            slot.Panel.BackColor = PanelColor;
            slot.Dependency.BackColor = PanelColor;
            if (slot.Panel.Controls.OfType<CheckBox>().FirstOrDefault() is { } check)
                check.BackColor = PanelColor;
            slot.Panel.Invalidate();
        }
        highlightedModLinkSlots.Clear();
    }

    private void SetDropIndicator(int targetIndex)
    {
        if (highlightedDropSlot == targetIndex) return;
        var previous = highlightedDropSlot;
        highlightedDropSlot = targetIndex;
        void Redraw(int index)
        {
            if (index < 0 || index >= modOrderSlots.Count) return;
            var slot = modOrderSlots[index];
            var active = index == highlightedDropSlot;
            var background = active
                ? Color.FromArgb(40, 74, 106) : PanelColor;
            slot.Panel.BackColor = background;
            slot.Number.BackColor = background;
            slot.Dependency.BackColor = background;
            if (slot.Panel.Controls.OfType<CheckBox>().FirstOrDefault() is { } check)
                check.BackColor = background;
            slot.Panel.Invalidate();
        }
        Redraw(previous);
        Redraw(targetIndex);
    }

    private void DrawModDropIndicator(ModOrderSlot slot, PaintEventArgs e)
    {
        if (slot.Index != highlightedDropSlot || activeModDrag is null) return;
        var from = modOrderRows.FindIndex(row => row.Id == activeModDrag);
        if (from < 0 || from == slot.Index
            || modOrderRows[from].Group != slot.Group) return;
        using var pen = new Pen(Color.DeepSkyBlue, 3f);
        var y = from < slot.Index ? slot.Panel.Height - 2 : 2;
        e.Graphics.DrawLine(pen, 0, y, slot.Panel.Width, y);
    }

    private void ScrollModOrderDuringDrag(DragEventArgs e)
    {
        var scroll = modOrderScroll;
        if (scroll is null || scroll.IsDisposed || !scroll.VerticalScroll.Visible) return;
        var mouse = scroll.PointToClient(new Point(e.X, e.Y));
        var change = mouse.Y < 30 ? -21
            : mouse.Y > scroll.ClientSize.Height - 30 ? 21 : 0;
        if (change == 0) return;
        var max = Math.Max(0, scroll.VerticalScroll.Maximum
            - scroll.VerticalScroll.LargeChange + 1);
        var next = Math.Max(0, Math.Min(max,
            scroll.VerticalScroll.Value + change));
        scroll.AutoScrollPosition = new Point(0, next);
    }

    internal static bool RunModPolishPolicySelfTest()
    {
        if (DevelopmentState("revenge-demon").State != "Diagnostic only"
            || DevelopmentState("weapon-wheel-remap").State != "Verified"
            || DevelopmentState("gauss-slow-movement").State != "Incomplete"
            || DevelopmentState("physical-crouch").State != "Experimental")
            return false;
        var rows = new List<ModOrderRow> {
            new() { Id = "one", Group = 0 },
            new() { Id = "two", Group = 0 },
            new() { Id = "leg", Group = 1 } };
        if (!MoveModWithinGroup(rows, "one", "two")) return false;
        if (string.Join(",", rows.Select(x => x.Id)) != "two,one,leg") return false;
        if (MoveModWithinGroup(rows, "one", "leg")) return false;
        return true;
    }
}
