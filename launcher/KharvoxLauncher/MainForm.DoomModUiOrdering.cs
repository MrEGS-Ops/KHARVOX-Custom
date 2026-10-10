using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// The DOOM MODS list can be reordered without touching mods, selections,
// game files or DOOMModLoader's currently blocked resource conflicts.
// Unlike the separate VR mod order, this is a presentation preference only.
public sealed partial class MainForm
{
    private Panel? activeDoomDragRow;
    private Panel? highlightedDoomDropRow;

    private static string DoomDisplayOrderPath =>
        Path.Combine(AppContext.BaseDirectory, "mods", "doom", "display-order.json");

    private static string[] ReadDoomDisplayOrder()
    {
        try
        {
            if (!File.Exists(DoomDisplayOrderPath)) return Array.Empty<string>();
            return new JavaScriptSerializer().Deserialize<string[]>(
                File.ReadAllText(DoomDisplayOrderPath)) ?? Array.Empty<string>();
        }
        catch (Exception e) when (e is IOException || e is UnauthorizedAccessException
            || e is ArgumentException || e is InvalidOperationException)
        {
            // Preferences may be regenerated; never damage existing mod choices.
            return Array.Empty<string>();
        }
    }

    private static DoomUserMods.ModEntry[] OrderDoomModRows(
        IEnumerable<DoomUserMods.ModEntry> mods, IEnumerable<string> preference)
    {
        var saved = preference.Distinct(StringComparer.OrdinalIgnoreCase)
            .Select((id, i) => (id, i)).ToDictionary(x => x.id, x => x.i,
                StringComparer.OrdinalIgnoreCase);
        return mods.OrderByDescending(mod => mod.IsPackaged)
            .ThenBy(mod => saved.TryGetValue(mod.Id, out var index)
                ? index : int.MaxValue)
            .ThenBy(mod => mod.IsFromDoom)
            .ThenBy(mod => mod.Name, StringComparer.OrdinalIgnoreCase)
            .ThenBy(mod => mod.Id, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private static bool MoveDoomIdWithinGroup(
        List<(string Id, bool Packaged)> rows, int from, int target)
    {
        if (from < 0 || target < 0 || from >= rows.Count || target >= rows.Count
            || from == target || rows[from].Packaged != rows[target].Packaged)
            return false;
        var moving = rows[from];
        rows.RemoveAt(from);
        rows.Insert(target, moving);
        return true;
    }

    private void PersistDoomDisplayOrder()
    {
        var ids = new List<string>();
        foreach (var list in new[] { packagedDoomModChecks, userDoomModChecks })
        {
            if (list is null) continue;
            ids.AddRange(list.Controls.OfType<Panel>()
                .Select(row => row.Tag as string)
                .Where(id => !string.IsNullOrWhiteSpace(id))!);
        }
        var file = DoomDisplayOrderPath;
        var temp = file + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(file)!);
            File.WriteAllText(temp, new JavaScriptSerializer().Serialize(ids));
            if (File.Exists(file)) File.Replace(temp, file, null);
            else File.Move(temp, file);
        }
        finally
        {
            if (File.Exists(temp)) File.Delete(temp);
        }
    }

    private void RenumberDoomDisplayRows()
    {
        var number = 0;
        foreach (var list in new[] { packagedDoomModChecks, userDoomModChecks })
        {
            if (list is null) continue;
            foreach (Control row in list.Controls)
            {
                if (row is Panel panel
                    && panel.Controls.OfType<CheckBox>().FirstOrDefault() is { } option)
                {
                    number++;
                    option.Text = number.ToString("00") + ". "
                        + panel.AccessibleName;
                }
                else if (row is CheckBox missing)
                {
                    number++;
                    var label = missing.Text;
                    var separator = label.IndexOf(". ", StringComparison.Ordinal);
                    if (separator >= 0 && separator <= 3)
                        missing.Text = number.ToString("00") + label.Substring(separator);
                }
            }
        }
    }

    private void SetDoomDragHighlight(Panel? target)
    {
        if (ReferenceEquals(highlightedDoomDropRow, target)) return;
        if (highlightedDoomDropRow is { IsDisposed: false } old)
            old.Invalidate();
        highlightedDoomDropRow = target;
        target?.Invalidate();
    }

    private void ScrollDoomModsDuringDrag(DragEventArgs e)
    {
        var scroll = doomModItemsPanel;
        if (scroll is null || scroll.IsDisposed || !scroll.VerticalScroll.Visible) return;
        var local = scroll.PointToClient(new Point(e.X, e.Y));
        var step = local.Y < 28 ? -24
            : local.Y > scroll.ClientSize.Height - 28 ? 24 : 0;
        if (step == 0) return;
        var max = Math.Max(0, scroll.VerticalScroll.Maximum
            - scroll.VerticalScroll.LargeChange + 1);
        scroll.AutoScrollPosition = new Point(0, Math.Max(0, Math.Min(max,
            scroll.VerticalScroll.Value + step)));
    }

    private Panel CreateDoomDraggableRow(CheckBox option, DoomUserMods.ModEntry mod)
    {
        var row = new Panel
        {
            Width = 337, Height = 26,
            Margin = new Padding(0, 0, 0, 0),
            Padding = Padding.Empty,
            BackColor = PanelColor,
            AllowDrop = true,
            Tag = mod.Id,
            AccessibleName = mod.Name + (mod.IsFromDoom ? " [DOOM Mods]" : "")
        };
        option.Text = row.AccessibleName;
        option.AutoSize = false;
        option.Size = new Size(311, 26);
        option.Location = new Point(2, 0);
        option.AutoEllipsis = true;
        option.Margin = Padding.Empty;
        option.AllowDrop = true;
        row.Controls.Add(option);

        var grip = new Label
        {
            Text = "⋮⋮", Width = 22, Height = 26,
            Left = 314, Top = 0,
            BackColor = PanelColor, ForeColor = Color.LightSteelBlue,
            Font = new Font("Segoe UI", 9f, FontStyle.Bold),
            TextAlign = ContentAlignment.MiddleCenter,
            Cursor = Cursors.SizeNS, Visible = false,
            AllowDrop = true,
            AccessibleName = "Reorder DOOM mod " + mod.Name
        };
        row.Controls.Add(grip);
        var orderTip = "Drag to change the DOOM MODS display order. "
            + "DOOMModLoader installation precedence is NOT changed; "
            + "KHARVOX still blocks overlapping resource mods.";
        statusToolTip.SetToolTip(grip, orderTip);

        void ShowGrip(object? sender, EventArgs e) => grip.Visible = true;
        void HideGrip(object? sender, EventArgs e)
        {
            if (activeDoomDragRow is null
                && !row.ClientRectangle.Contains(row.PointToClient(Cursor.Position)))
                grip.Visible = false;
        }
        foreach (Control child in new Control[] { row, option, grip })
        {
            child.MouseEnter += ShowGrip;
            child.MouseLeave += HideGrip;
        }

        Point down = Point.Empty;
        grip.MouseDown += (_, e) => { if (e.Button == MouseButtons.Left) down = e.Location; };
        grip.MouseMove += (_, e) =>
        {
            if (e.Button != MouseButtons.Left || activeDoomDragRow is not null
                || (Math.Abs(e.X - down.X) < 5 && Math.Abs(e.Y - down.Y) < 5))
                return;
            activeDoomDragRow = row;
            try { grip.DoDragDrop(mod.Id, DragDropEffects.Move); }
            finally
            {
                activeDoomDragRow = null;
                SetDoomDragHighlight(null);
                grip.Visible = row.ClientRectangle.Contains(row.PointToClient(Cursor.Position));
            }
        };

        void Accept(object? sender, DragEventArgs e)
        {
            var valid = activeDoomDragRow is { IsDisposed: false } source
                && row.Parent is FlowLayoutPanel targetList
                && ReferenceEquals(source.Parent, targetList)
                && !ReferenceEquals(source, row);
            e.Effect = valid ? DragDropEffects.Move : DragDropEffects.None;
            SetDoomDragHighlight(valid ? row : null);
            if (valid) ScrollDoomModsDuringDrag(e);
        }
        void Drop(object? sender, DragEventArgs e)
        {
            SetDoomDragHighlight(null);
            var source = activeDoomDragRow;
            if (source is null || ReferenceEquals(source, row)
                || row.Parent is not FlowLayoutPanel list
                || !ReferenceEquals(source.Parent, list)) return;
            var before = list.Controls.GetChildIndex(source);
            var target = list.Controls.GetChildIndex(row);
            if (before == target) return;
            list.SuspendLayout();
            try { list.Controls.SetChildIndex(source, target); }
            finally { list.ResumeLayout(true); }
            RenumberDoomDisplayRows();
            try { PersistDoomDisplayOrder(); }
            catch (Exception error) when (error is IOException
                || error is UnauthorizedAccessException)
            {
                MessageBox.Show(customOptionsForm,
                    "Mod display order changed but could not be saved: "
                    + error.Message, "KHARVOX", MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
            }
        }
        foreach (Control child in new Control[] { row, option, grip })
        {
            child.DragEnter += Accept;
            child.DragOver += Accept;
            child.DragLeave += (_, _) => SetDoomDragHighlight(null);
            child.DragDrop += Drop;
        }
        row.Paint += (_, e) =>
        {
            if (!ReferenceEquals(highlightedDoomDropRow, row)
                || activeDoomDragRow is null || row.Parent is not FlowLayoutPanel list)
                return;
            var downwards = list.Controls.GetChildIndex(activeDoomDragRow)
                < list.Controls.GetChildIndex(row);
            using var stroke = new Pen(Color.LightSkyBlue, 2f);
            var y = downwards ? row.Height - 2 : 1;
            e.Graphics.DrawLine(stroke, 1, y, row.Width - 2, y);
        };

        var menu = new ContextMenuStrip();
        var up = menu.Items.Add("Move up");
        var down = menu.Items.Add("Move down");
        menu.Opening += (_, _) =>
        {
            if (row.Parent is not FlowLayoutPanel list) return;
            var index = list.Controls.GetChildIndex(row);
            up.Enabled = index > 0;
            down.Enabled = index < list.Controls.Count - 1
                && list.Controls[index + 1] is Panel;
        };
        void Move(int delta)
        {
            if (row.Parent is not FlowLayoutPanel list) return;
            var index = list.Controls.GetChildIndex(row);
            var target = index + delta;
            if (target < 0 || target >= list.Controls.Count
                || list.Controls[target] is not Panel) return;
            list.Controls.SetChildIndex(row, target);
            RenumberDoomDisplayRows();
            try { PersistDoomDisplayOrder(); }
            catch (IOException) { /* Row stays visible; next successful reorder saves it. */ }
            catch (UnauthorizedAccessException) { /* Same as above. */ }
        }
        up.Click += (_, _) => Move(-1);
        down.Click += (_, _) => Move(1);
        foreach (Control child in new Control[] { row, option, grip })
            child.ContextMenuStrip = menu;
        return row;
    }

    internal static bool RunDoomDisplayOrderPolicySelfTest()
    {
        var first = new DoomUserMods.ModEntry("user:a", "A", "a", false);
        var second = new DoomUserMods.ModEntry("user:b", "B", "b", false);
        var packaged = new DoomUserMods.ModEntry("kharvox:x", "X", "x", false, true);
        var sorted = OrderDoomModRows(new[] { first, second, packaged },
            new[] { "user:b", "user:a", "kharvox:x", "missing" });
        if (!sorted.Select(x => x.Id).SequenceEqual(
            new[] { "kharvox:x", "user:b", "user:a" })) return false;
        var rows = new List<(string Id, bool Packaged)>
        {
            ("x", true), ("a", false), ("b", false)
        };
        if (MoveDoomIdWithinGroup(rows, 0, 1)) return false;
        if (!MoveDoomIdWithinGroup(rows, 1, 2)) return false;
        if (!rows.Select(x => x.Id).SequenceEqual(new[] { "x", "b", "a" }))
            return false;
        return true;
    }
}
