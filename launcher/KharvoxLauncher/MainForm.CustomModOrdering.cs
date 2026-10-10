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
        internal TableLayoutPanel? Panel;
        internal Label? Grip;
    }

    private static readonly (string Title, string Symbol)[] ModGroupHeaders =
    {
        ("HANDS & ARMS", "✋"),
        ("LEGS & MOVEMENT", "👣"),
        ("WEAPONS & COMBAT", "⌖"),
        ("HUD & IMMERSION", "◉"),
        ("DEMONS & AI", "☠")
    };

    private readonly List<ModOrderRow> modOrderRows = new();
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
        modOrderRows.Clear();
        modOrderRows.AddRange(NormalizeModOrder(defaults, ReadModOrder()));

        grid.ColumnStyles.Clear();
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 40));

        foreach (var (title, symbol) in ModGroupHeaders)
        {
            var header = new Label
            {
                Text = symbol + "   " + title,
                AccessibleName = title + " mod group",
                Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.BottomLeft,
                ForeColor = Color.LightSteelBlue,
                BackColor = PanelColor,
                Font = new Font("Segoe UI", 8.5f, FontStyle.Bold),
                Margin = new Padding(2, 2, 0, 0)
            };
            modOrderHeaders.Add(header);
        }

        foreach (var item in modOrderRows)
        {
            var row = new TableLayoutPanel
            {
                ColumnCount = 2, RowCount = 1,
                Dock = DockStyle.Fill,
                Margin = Padding.Empty, Padding = Padding.Empty,
                BackColor = PanelColor,
                AllowDrop = true
            };
            row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 25));
            row.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

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
            row.Controls.Add(check, 0, 0);

            var grip = new Label
            {
                Text = "⋮⋮", AccessibleName = "Drag to reorder " + item.Caption,
                Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleCenter,
                ForeColor = Color.LightSteelBlue, BackColor = PanelColor,
                Cursor = Cursors.SizeNS,
                Font = new Font("Segoe UI", 11f, FontStyle.Bold),
                Margin = Padding.Empty, Visible = false,
                AllowDrop = true
            };
            row.Controls.Add(grip, 1, 0);
            item.Panel = row;
            item.Grip = grip;
            statusToolTip.SetToolTip(grip,
                "Drag to change order within " + ModGroupHeaders[item.Group].Title);

            // Child controls receive their own mouse enter events in WinForms.
            // Keep the handle available when crossing from the checkbox.
            void ShowGrip(object? sender, EventArgs e)
            {
                if (modOrderRows.Count(x => x.Group == item.Group) > 1)
                    grip.Visible = true;
            }
            void HideGrip(object? sender, EventArgs e)
            {
                if (activeModDrag is null
                    && !row.ClientRectangle.Contains(row.PointToClient(Cursor.Position)))
                    grip.Visible = false;
            }
            row.MouseEnter += ShowGrip;
            check.MouseEnter += ShowGrip;
            grip.MouseEnter += ShowGrip;
            row.MouseLeave += HideGrip;
            check.MouseLeave += HideGrip;
            grip.MouseLeave += HideGrip;

            Point mouseDown = Point.Empty;
            grip.MouseDown += (_, e) =>
            {
                if (e.Button == MouseButtons.Left) mouseDown = e.Location;
            };
            grip.MouseMove += (_, e) =>
            {
                if (e.Button != MouseButtons.Left || activeModDrag is not null
                    || Math.Abs(e.X - mouseDown.X) < 5
                    && Math.Abs(e.Y - mouseDown.Y) < 5) return;
                activeModDrag = item.Id;
                try { grip.DoDragDrop(item.Id, DragDropEffects.Move); }
                finally
                {
                    activeModDrag = null;
                    grip.Visible = row.ClientRectangle.Contains(row.PointToClient(Cursor.Position));
                }
            };

            void AcceptDrag(object? sender, DragEventArgs e)
            {
                var source = modOrderRows.FirstOrDefault(x => x.Id == activeModDrag);
                e.Effect = source is not null && source != item
                    && source.Group == item.Group
                    ? DragDropEffects.Move : DragDropEffects.None;
            }
            void Drop(object? sender, DragEventArgs e)
            {
                var fromId = activeModDrag;
                if (fromId is null || !MoveModWithinGroup(modOrderRows, fromId, item.Id))
                {
                    e.Effect = DragDropEffects.None;
                    return;
                }
                RefreshGroupedModRows();
                try { SaveModOrder(); }
                catch (Exception error) when (error is IOException
                    || error is UnauthorizedAccessException)
                {
                    MessageBox.Show(customOptionsForm,
                        "The new mod order is shown, but could not be saved: "
                        + error.Message, "KHARVOX", MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                }
            }

            foreach (Control target in new Control[] { row, check, grip })
            {
                target.AllowDrop = true;
                target.DragEnter += AcceptDrag;
                target.DragOver += AcceptDrag;
                target.DragDrop += Drop;
            }
        }
        RefreshGroupedModRows();
    }

    private void RefreshGroupedModRows()
    {
        var grid = modOrderGrid;
        if (grid is null) return;
        grid.SuspendLayout();
        try
        {
            foreach (var row in modOrderRows)
                if (row.Panel is not null) grid.Controls.Remove(row.Panel);
            foreach (var header in modOrderHeaders) grid.Controls.Remove(header);
            grid.RowStyles.Clear();
            grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
            var numbers = modOrderRows.Select((row, i) => (row.Box, Index: i + 1))
                .ToDictionary(x => x.Box, x => x.Index);
            string Number(CheckBox check) => "#" + numbers[check];
            var currentGroup = -1;
            var tableRow = 1;
            for (var i = 0; i < modOrderRows.Count; i++)
            {
                var item = modOrderRows[i];
                if (item.Group != currentGroup)
                {
                    currentGroup = item.Group;
                    grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 24));
                    var header = modOrderHeaders[currentGroup];
                    grid.Controls.Add(header, 0, tableRow++);
                    grid.SetColumnSpan(header, 2);
                }

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
                item.Box.Text = (i + 1).ToString("00") + ". " + item.Caption + dependencies;
                statusToolTip.SetToolTip(item.Box, item.Tip
                    + (dependencies.Length == 0 ? "" : Environment.NewLine
                        + dependencies.TrimStart(' ', '—')));
                grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
                grid.Controls.Add(item.Panel!, 0, tableRow++);
                grid.SetColumnSpan(item.Panel!, 2);
            }
            grid.RowCount = tableRow;
            grid.Height = 32 + ModGroupHeaders.Length * 24 + modOrderRows.Count * 28 + 14;
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
