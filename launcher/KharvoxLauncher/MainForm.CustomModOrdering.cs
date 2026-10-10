using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Launcher-only presentation order: it never changes mod flags or input mappings.
public sealed partial class MainForm
{
    private sealed class ModOrderRow
    {
        internal CheckBox Box = null!;
        internal string Id = "";
        internal string Caption = "";
        internal string Tip = "";
        internal int Group;
    }

    // Number labels and drag handles belong to the POSITION, not the mod.
    // These 15 physical slots remain anchored while their checkboxes move.
    private sealed class ModOrderSlot
    {
        internal int Index;
        internal int Group;
        internal TableLayoutPanel Panel = null!;
        internal Label Number = null!;
        internal Label Grip = null!;
        internal LinkLabel Dependency = null!;
    }

    private static readonly (string Title, string Symbol)[] ModGroupHeaders =
    {
        ("HANDS & ARMS", "◈"),
        ("LEGS & MOVEMENT", "↔"),
        ("WEAPONS & COMBAT", "✣"),
        ("HUD & IMMERSION", "▣"),
        ("DEMONS & AI", "◆")
    };

    private readonly List<ModOrderRow> modOrderRows = new();
    private ModOrderRow[] modOrderDefaultRows = Array.Empty<ModOrderRow>();
    private string[] defaultModIds = Array.Empty<string>();
    private readonly Stack<string[]> modOrderUndo = new();
    private Panel? modOrderScroll;
    private int highlightedDropSlot = -1;
    private readonly List<ModOrderSlot> modOrderSlots = new();
    private readonly List<Label> modOrderHeaders = new();
    private TableLayoutPanel? modOrderGrid;
    private string? activeModDrag;

    private static string ModOrderSettingsPath =>
        Path.Combine(AppContext.BaseDirectory, "kharvox-custom-mod-order.json");

    private static string[] ReadModOrder()
    {
        try
        {
            if (!File.Exists(ModOrderSettingsPath)) return Array.Empty<string>();
            return new JavaScriptSerializer().Deserialize<string[]>(
                File.ReadAllText(ModOrderSettingsPath)) ?? Array.Empty<string>();
        }
        catch (Exception e) when (e is IOException || e is UnauthorizedAccessException
            || e is ArgumentException || e is InvalidOperationException)
        {
            // A broken optional layout preference must never block the launcher.
            return Array.Empty<string>();
        }
    }

    private void SaveModOrder()
    {
        var path = ModOrderSettingsPath;
        var temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
            var json = new JavaScriptSerializer().Serialize(
                modOrderRows.Select(row => row.Id).ToArray());
            File.WriteAllText(temp, json);
            if (File.Exists(path)) File.Replace(temp, path, null);
            else File.Move(temp, path);
        }
        finally
        {
            if (File.Exists(temp)) File.Delete(temp);
        }
    }

    // Saved ordering can only rearrange IDs inside their original category.
    // Missing, duplicate, obsolete or cross-group IDs cannot move group borders.
    private static List<ModOrderRow> NormalizeModOrder(
        IReadOnlyList<ModOrderRow> defaults, IEnumerable<string> saved)
    {
        var savedIds = saved.ToArray();
        var result = new List<ModOrderRow>(defaults.Count);
        for (var group = 0; group < ModGroupHeaders.Length; group++)
        {
            var originals = defaults.Where(row => row.Group == group).ToArray();
            var names = new HashSet<string>(originals.Select(row => row.Id),
                StringComparer.Ordinal);
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var id in savedIds)
            {
                if (!names.Contains(id) || !seen.Add(id)) continue;
                result.Add(originals.Single(row => row.Id == id));
            }
            result.AddRange(originals.Where(row => !seen.Contains(row.Id)));
        }
        return result;
    }

    private static bool MoveModWithinGroup(List<ModOrderRow> rows,
        string sourceId, string targetId)
    {
        var from = rows.FindIndex(row => row.Id == sourceId);
        var to = rows.FindIndex(row => row.Id == targetId);
        if (from < 0 || to < 0 || from == to
            || rows[from].Group != rows[to].Group) return false;
        var row = rows[from];
        rows.RemoveAt(from);
        // Moving down drops AFTER the target; moving up drops BEFORE it.
        var position = rows.FindIndex(item => item.Id == targetId);
        rows.Insert(from < to ? position + 1 : position, row);
        return true;
    }

    private void InitializeGroupedCustomMods(TableLayoutPanel grid,
        (CheckBox Box, string Tip)[] available)
    {
        modOrderGrid = grid;
        var tips = available.ToDictionary(item => item.Box, item => item.Tip);
        ModOrderRow Define(CheckBox check, string id, int group) => new()
        {
            Box = check, Id = id, Group = group,
            Caption = check.Text, Tip = tips[check]
        };
        var defaults = new[]
        {
            Define(weaponWheelRemap, "weapon-wheel-remap", 0),
            Define(customHandFocusedRs, "hand-focus", 0),
            Define(customBackOfHandHud, "back-of-hand-hud", 0),
            Define(customBehindHeadWeaponWheel, "behind-head-wheel", 0),
            Define(customBehindHeadWheelHandSelection, "behind-head-selection", 0),
            Define(customDynamicShoulderHolster, "dynamic-shoulder-holster", 0),
            Define(customPhysicalCrouch, "physical-crouch", 1),
            Define(customDirectionalDash, "directional-dash", 1),
            Define(customPhysicalGrenadeThrow, "physical-grenade", 2),
            Define(customGaussChargeSlowMovement, "gauss-slow-movement", 2),
            Define(customMotionGloryKillSpeed, "punch-glory-kill", 2),
            Define(customPhysicalChainsawGestures, "physical-chainsaw", 2),
            Define(customDisableHud, "no-hud", 3),
            Define(customDisableWeaponWheel, "no-weapon-wheel", 3),
            Define(customRevengeDemon, "revenge-demon", 4)
        };
        if (tips.Count != defaults.Length || defaults.Select(row => row.Box).Distinct().Count()
                != defaults.Length)
            throw new InvalidOperationException("VR mod order definitions are incomplete.");
        modOrderDefaultRows = defaults;
        defaultModIds = defaults.Select(item => item.Id).ToArray();
        modOrderRows.Clear();
        modOrderRows.AddRange(NormalizeModOrder(defaults, ReadModOrder()));

        grid.ColumnStyles.Clear();
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 40));

        foreach (var (title, symbol) in ModGroupHeaders)
        {
            var header = new Label
            {
                Text = symbol + "  " + title,
                AccessibleName = title + " mod group",
                Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.BottomLeft,
                ForeColor = Color.LightSteelBlue,
                BackColor = PanelColor,
                Font = new Font("Segoe UI Symbol", 8.2f, FontStyle.Bold),
                Margin = new Padding(2, 0, 0, 0)
            };
            modOrderHeaders.Add(header);
        }

        // Checkbox identity and event subscriptions never change when moved.
        foreach (var item in modOrderRows)
        {
            var check = item.Box;
            check.Dock = DockStyle.Fill;
            check.AutoSize = false;
            check.AutoEllipsis = true;
            check.ForeColor = Color.White;
            check.Margin = new Padding(2, 0, 0, 0);
            check.AllowDrop = true;
            if (check == weaponWheelRemap)
                check.CheckedChanged += WeaponWheelRemapChanged;
            else
                check.CheckedChanged += CustomModChanged;
            check.MouseEnter += (_, _) =>
            {
                var slot = modOrderSlots.FirstOrDefault(x =>
                    ReferenceEquals(x.Panel, check.Parent));
                if (slot is not null && modOrderRows.Count(x => x.Group == slot.Group) > 1)
                    slot.Grip.Visible = true;
            };
            check.MouseLeave += (_, _) =>
            {
                var slot = modOrderSlots.FirstOrDefault(x =>
                    ReferenceEquals(x.Panel, check.Parent));
                if (slot is not null && activeModDrag is null
                    && !slot.Panel.ClientRectangle.Contains(
                        slot.Panel.PointToClient(Cursor.Position)))
                    slot.Grip.Visible = false;
            };
        }

        // Drop targets are always the numbered slots; each target resolves its
        // CURRENT occupant before applying within-group ordering.
        void AcceptDrag(int targetIndex, DragEventArgs e)
        {
            var source = modOrderRows.FirstOrDefault(x => x.Id == activeModDrag);
            var target = modOrderRows[targetIndex];
            var allowed = source is not null && !ReferenceEquals(source, target)
                && source.Group == target.Group;
            e.Effect = allowed ? DragDropEffects.Move : DragDropEffects.None;
            SetDropIndicator(allowed ? targetIndex : -1);
            if (allowed) ScrollModOrderDuringDrag(e);
        }
        void Drop(int targetIndex, DragEventArgs e)
        {
            SetDropIndicator(-1);
            var sourceId = activeModDrag;
            var before = modOrderRows.Select(row => row.Id).ToArray();
            var targetId = modOrderRows[targetIndex].Id;
            if (sourceId is null || !MoveModWithinGroup(
                modOrderRows, sourceId, targetId))
            {
                e.Effect = DragDropEffects.None;
                return;
            }
            FinishModOrderChange(before);
        }

        modOrderSlots.Clear();
        grid.RowStyles.Clear();
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        var currentGroup = -1;
        var tableRow = 1;
        for (var i = 0; i < modOrderRows.Count; i++)
        {
            var slotIndex = i;
            var group = modOrderRows[i].Group;
            if (group != currentGroup)
            {
                currentGroup = group;
                grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 20));
                var header = modOrderHeaders[currentGroup];
                grid.Controls.Add(header, 0, tableRow++);
                grid.SetColumnSpan(header, 2);
            }
            var panel = new TableLayoutPanel
            {
                ColumnCount = 4, RowCount = 1,
                Dock = DockStyle.Fill,
                Margin = Padding.Empty, Padding = Padding.Empty,
                BackColor = PanelColor, AllowDrop = true
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 32));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 0)); // dependency width per mod
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 19));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var number = new Label
            {
                Text = (i + 1).ToString("00") + ".",
                AccessibleName = "Mod position " + (i + 1),
                ForeColor = Color.White, BackColor = PanelColor,
                Font = new Font("Segoe UI", 9F, FontStyle.Regular),
                Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.MiddleLeft,
                Margin = new Padding(2, 0, 0, 0),
                AllowDrop = true
            };
            var grip = new Label
            {
                Text = "⋮⋮", AccessibleName = "Drag mod in position " + (i + 1),
                Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.MiddleCenter,
                ForeColor = Color.LightSteelBlue, BackColor = PanelColor,
                Cursor = Cursors.SizeNS,
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                Margin = Padding.Empty, Visible = false,
                AllowDrop = true
            };
            var dependency = new LinkLabel
            {
                Dock = DockStyle.Fill, Margin = Padding.Empty,
                Font = new Font("Segoe UI", 8f),
                LinkColor = Color.LightSkyBlue,
                ActiveLinkColor = Color.White,
                VisitedLinkColor = Color.LightSkyBlue,
                BackColor = PanelColor, ForeColor = Color.Gainsboro,
                TextAlign = ContentAlignment.MiddleLeft,
                LinkBehavior = LinkBehavior.HoverUnderline,
                AutoEllipsis = true
            };
            panel.Controls.Add(number, 0, 0);
            panel.Controls.Add(grip, 3, 0);
            panel.Controls.Add(dependency, 2, 0);
            var slot = new ModOrderSlot
            {
                Index = i, Group = group, Panel = panel,
                Number = number, Grip = grip,
                Dependency = dependency
            };
            dependency.LinkClicked += (_, e) => NavigateModDependency(e.Link.LinkData);
            dependency.MouseEnter += (_, _) => grip.Visible = true;
            dependency.MouseLeave += (_, _) =>
            {
                if (activeModDrag is null && !panel.ClientRectangle.Contains(
                    panel.PointToClient(Cursor.Position))) grip.Visible = false;
            };
            modOrderSlots.Add(slot);
            void ShowGrip(object? sender, EventArgs e)
            {
                if (modOrderRows.Count(row => row.Group == group) > 1)
                    grip.Visible = true;
            }
            void HideGrip(object? sender, EventArgs e)
            {
                if (activeModDrag is null
                    && !panel.ClientRectangle.Contains(
                        panel.PointToClient(Cursor.Position)))
                    grip.Visible = false;
            }
            panel.DragLeave += (_, _) => SetDropIndicator(-1);
            panel.MouseEnter += ShowGrip;
            number.MouseEnter += ShowGrip;
            grip.MouseEnter += ShowGrip;
            panel.MouseLeave += HideGrip;
            number.MouseLeave += HideGrip;
            grip.MouseLeave += HideGrip;

            Point mouseDown = Point.Empty;
            grip.MouseDown += (_, e) =>
            {
                if (e.Button == MouseButtons.Left) mouseDown = e.Location;
            };
            grip.MouseMove += (_, e) =>
            {
                if (e.Button != MouseButtons.Left || activeModDrag is not null
                    || (Math.Abs(e.X - mouseDown.X) < 5
                        && Math.Abs(e.Y - mouseDown.Y) < 5)) return;
                activeModDrag = modOrderRows[slotIndex].Id;
                try { grip.DoDragDrop(activeModDrag, DragDropEffects.Move); }
                finally
                {
                    activeModDrag = null;
                    SetDropIndicator(-1);
                    grip.Visible = panel.ClientRectangle.Contains(
                        panel.PointToClient(Cursor.Position));
                }
            };
            foreach (Control target in new Control[] {
                panel, number, grip, dependency })
            {
                target.DragEnter += (_, e) => AcceptDrag(slotIndex, e);
                target.DragOver += (_, e) => AcceptDrag(slotIndex, e);
                target.DragDrop += (_, e) => Drop(slotIndex, e);
            }
            AttachModOrderContext(slot);
            panel.Paint += (_, e) => DrawModDropIndicator(slot, e);
            grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
            grid.Controls.Add(panel, 0, tableRow++);
            grid.SetColumnSpan(panel, 2);
        }
        // Each checkbox is also a drop target. Look up its CURRENT slot.
        foreach (var item in modOrderRows)
        {
            var check = item.Box;
            void AcceptHere(object? sender, DragEventArgs e)
            {
                var targetIndex = modOrderSlots.FindIndex(x =>
                    ReferenceEquals(x.Panel, check.Parent));
                if (targetIndex >= 0) AcceptDrag(targetIndex, e);
                else e.Effect = DragDropEffects.None;
            }
            check.DragEnter += AcceptHere;
            check.DragOver += AcceptHere;
            check.DragDrop += (_, e) =>
            {
                var targetIndex = modOrderSlots.FindIndex(x =>
                    ReferenceEquals(x.Panel, check.Parent));
                if (targetIndex >= 0) Drop(targetIndex, e);
                else e.Effect = DragDropEffects.None;
            };
        }
        grid.RowCount = tableRow;
        grid.Height = 32 + ModGroupHeaders.Length * 20
            + modOrderRows.Count * 28 + 6;
        RefreshGroupedModRows();
    }

    private void RefreshGroupedModRows()
    {
        var grid = modOrderGrid;
        if (grid is null || modOrderSlots.Count != modOrderRows.Count) return;
        grid.SuspendLayout();
        try
        {
            // First detach every checkbox; the 15 numbered rows, category
            // headers and hover grips NEVER move or get renumbered.
            foreach (var slot in modOrderSlots)
                foreach (var check in slot.Panel.Controls.OfType<CheckBox>().ToArray())
                    slot.Panel.Controls.Remove(check);

            var numbers = modOrderRows.Select((row, i) => (row.Box, Index: i + 1))
                .ToDictionary(x => x.Box, x => x.Index);
            string Number(CheckBox check) => "#" + numbers[check];
            for (var i = 0; i < modOrderRows.Count; i++)
            {
                var item = modOrderRows[i];
                var slot = modOrderSlots[i];
                string dependencies = "";
                if (item.Box == customDirectionalDash)
                    dependencies = " — Requires " + Number(weaponWheelRemap)
                        + ", " + Number(customBehindHeadWeaponWheel)
                        + "; disables " + Number(customDisableWeaponWheel);
                else if (item.Box == customDisableWeaponWheel)
                    dependencies = " — Disables " + Number(customBehindHeadWeaponWheel)
                        + ", " + Number(customDirectionalDash);
                else if (item.Box == customDisableHud)
                    dependencies = " — Disables " + Number(customBackOfHandHud);
                else if (item.Box == customBackOfHandHud)
                    dependencies = " — Disables " + Number(customDisableHud);
                else if (item.Box == customBehindHeadWeaponWheel)
                    dependencies = " — Disables " + Number(customDisableWeaponWheel);
                else if (item.Box == customBehindHeadWheelHandSelection)
                    dependencies = " — Requires " + Number(customBehindHeadWeaponWheel);
                else if (item.Box == customDynamicShoulderHolster)
                    dependencies = " — Requires Enable Hands (main launcher)";
                // The mod's own caption and tooltip move to the new position,
                // while the number label stays attached to the fixed slot.
                item.Box.Text = item.Caption;
                slot.Panel.Controls.Add(item.Box, 1, 0);
                // The right-click actions follow the current occupant as
                // checkboxes move between numbered rows.
                item.Box.ContextMenuStrip = slot.Panel.ContextMenuStrip;
                PopulateDependencyLinks(slot, item, dependencies, numbers);
                // No on-screen status badge: it cluttered the narrow layout.
                // Development information remains accessible in the tooltip.
                var state = DevelopmentState(item.Id);
                statusToolTip.SetToolTip(item.Box, item.Tip
                    + Environment.NewLine + "Development: " + state.State
                    + " — " + state.Detail
                    + (dependencies.Length == 0 ? "" : Environment.NewLine
                        + dependencies.TrimStart(' ', '—')));
                slot.Grip.AccessibleName = "Drag to reorder " + item.Caption
                    + " in position " + (i + 1);
                statusToolTip.SetToolTip(slot.Grip,
                    "Drag " + item.Caption + " within "
                    + ModGroupHeaders[slot.Group].Title);
                slot.Grip.Visible = false;
            }
        }
        finally { grid.ResumeLayout(true); }
    }

    internal static bool RunModReorderPolicySelfTest()
    {
        // Pure ordering checks: same-group reorder, other-group rejection,
        // persistence normalization and duplicate/unknown IDs.
        var defaults = new List<ModOrderRow>
        {
            new() { Id = "a", Group = 0 },
            new() { Id = "b", Group = 0 },
            new() { Id = "c", Group = 1 },
            new() { Id = "d", Group = 1 },
            new() { Id = "e", Group = 4 }
        };
        var rows = NormalizeModOrder(defaults, new[] { "b", "d", "a", "x", "b" });
        if (string.Join(",", rows.Select(x => x.Id)) != "b,a,d,c,e") return false;
        if (MoveModWithinGroup(rows, "a", "c")) return false;
        if (!MoveModWithinGroup(rows, "a", "b")
            || string.Join(",", rows.Select(x => x.Id)) != "a,b,d,c,e")
            return false;
        if (MoveModWithinGroup(rows, "e", "e")) return false;
        var serialized = new JavaScriptSerializer().Serialize(
            rows.Select(row => row.Id).ToArray());
        var roundTrip = new JavaScriptSerializer().Deserialize<string[]>(serialized);
        return roundTrip is not null && string.Join(",", roundTrip) == "a,b,d,c,e";
    }
}
