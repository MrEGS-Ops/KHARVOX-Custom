namespace KharvoxLauncher;

public sealed partial class MainForm
{
    // A mod checkbox alone is not a safe save-mode switch. On the first
    // normal launch after a modded session, explicitly offer the clean baseline.
    private bool ConfirmReturnToCleanSaves()
    {
        try
        {
            if (DoomSaveProtection.SelectedResourceMods(doomPath.Text))
                return true;
            if (!DoomSaveProtection.ReturningFromMods()) return true;
            var clean = DoomSaveProtection.CleanBaseline();
            if (clean is null) return true;
            var choice = MessageBox.Show(this,
                "You disabled all DOOM resource mods, but the game may still "
                + "contain progress written in developer/modded mode."
                + Environment.NewLine + Environment.NewLine
                + "Restore the protected clean save from " + clean.CreatedUtc
                + " before starting normal DOOM?"
                + Environment.NewLine + Environment.NewLine
                + "YES: Back up the current modded save and restore clean saves."
                + Environment.NewLine
                + "NO: Keep current saves (may still be affected by mods)."
                + Environment.NewLine
                + "CANCEL: Do not start DOOM."
                + Environment.NewLine + Environment.NewLine
                + "Steam Cloud may replace local restored saves. Ideally "
                + "temporarily disable Steam Cloud for DOOM, then keep LOCAL "
                + "files if Steam reports a cloud conflict.",
                "KHARVOX — Return to clean saves",
                MessageBoxButtons.YesNoCancel, MessageBoxIcon.Warning);
            if (choice == DialogResult.Cancel) return false;
            if (choice == DialogResult.No) return true;
            var rescue = DoomSaveProtection.Restore(clean.Id);
            MessageBox.Show(this,
                "Clean saves restored. Previous modded saves were preserved in backup "
                + rescue.Id + "." + Environment.NewLine
                + "Keep the LOCAL files if Steam asks which saves to synchronise.",
                "KHARVOX — Clean saves restored",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
            return true;
        }
        catch (Exception error)
        {
            MessageBox.Show(this, error.Message,
                "KHARVOX — Save protection stopped launch",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
    }

    private void OpenSaveProtectionManager()
    {
        if (DoomSaveProtection.IsGameRunning())
        {
            MessageBox.Show(customOptionsForm ?? this,
                "Close DOOM before managing saves.", "KHARVOX",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        using var window = new Form {
            Text = "KHARVOX — DOOM Save Protection",
            StartPosition = FormStartPosition.CenterParent,
            MinimumSize = new Size(560, 410), Size = new Size(660, 480),
            BackColor = Color.FromArgb(31,31,34),
            ForeColor = Color.Gainsboro, Font = Font,
            ShowInTaskbar = false
        };
        var grid = new TableLayoutPanel {
            Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5,
            Padding = new Padding(14), BackColor = window.BackColor
        };
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
        grid.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 43));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 47));
        var intro = new Label {
            Text = "Protected snapshots of BOTH savegame and savegame.user. "
                + "Backups are outside DOOM and KHARVOX release folders.",
            Dock = DockStyle.Fill, AutoEllipsis = true,
            ForeColor = Color.Gainsboro
        };
        grid.Controls.Add(intro, 0, 0);
        var sourceLabel = new Label {
            Text = DoomSaveProtection.SaveRoot(), Dock = DockStyle.Fill,
            AutoEllipsis = true, ForeColor = Color.Silver
        };
        grid.Controls.Add(sourceLabel, 0, 1);
        var list = new ListBox {
            Dock = DockStyle.Fill, IntegralHeight = false,
            BackColor = Color.FromArgb(40,40,43),
            ForeColor = Color.WhiteSmoke
        };
        var snapshots = Array.Empty<DoomSaveProtection.Snapshot>();
        void RefreshList()
        {
            snapshots = DoomSaveProtection.ListSnapshots().ToArray();
            list.Items.Clear();
            var clean = DoomSaveProtection.CleanBaseline();
            foreach (var item in snapshots)
                list.Items.Add((clean?.Id == item.Id ? "[CLEAN BASELINE] " : "")
                    + item.ToString());
            if (list.Items.Count != 0) list.SelectedIndex = 0;
            sourceLabel.Text = DoomSaveProtection.SaveRoot();
        }
        grid.Controls.Add(list, 0, 2);
        var actions = new TableLayoutPanel {
            Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty
        };
        actions.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.3f));
        actions.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.3f));
        actions.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.4f));
        actions.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Button MakeSaveButton(string label) => new() {
            Text=label, Dock=DockStyle.Fill,
            FlatStyle=FlatStyle.Flat, ForeColor=Color.WhiteSmoke,
            BackColor=Color.FromArgb(49,49,54), Margin=new Padding(2)
        };
        var backup = MakeSaveButton("Backup now");
        var restore = MakeSaveButton("Restore selected…");
        var openFolder = MakeSaveButton("Open backups");
        actions.Controls.Add(backup,0,0);
        actions.Controls.Add(restore,1,0);
        actions.Controls.Add(openFolder,2,0);
        grid.Controls.Add(actions,0,3);
        var bottom = new TableLayoutPanel {
            Dock=DockStyle.Fill, ColumnCount=2, RowCount=1
        };
        bottom.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        bottom.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 135));
        bottom.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        var locate = MakeSaveButton("Choose save location…");
        var close = MakeSaveButton("Close");
        bottom.Controls.Add(locate,0,0);
        bottom.Controls.Add(close,1,0);
        grid.Controls.Add(bottom,0,4);
        window.Controls.Add(grid);
        void SafeRun(Action action)
        {
            try { DoomSaveProtection.EnsureGameClosed(); action(); RefreshList(); }
            catch (Exception error) {
                MessageBox.Show(window, error.Message,
                    "KHARVOX — Save protection", MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
            }
        }
        backup.Click += (_, _) => SafeRun(() => {
            var mode = DoomSaveProtection.SelectedResourceMods(doomPath.Text)
                ? "modded" : "normal";
            var item = DoomSaveProtection.Capture("Manual backup", mode);
            MessageBox.Show(window, "Verified backup created: " + item.Id,
                "KHARVOX", MessageBoxButtons.OK, MessageBoxIcon.Information);
        });
        restore.Click += (_, _) => SafeRun(() => {
            if (list.SelectedIndex < 0) return;
            var chosen = snapshots[list.SelectedIndex];
            var answer = MessageBox.Show(window,
                "Restore the selected " + chosen.Label + " snapshot?"
                + Environment.NewLine + "Created: " + chosen.CreatedUtc
                + Environment.NewLine + Environment.NewLine
                + "KHARVOX will FIRST back up the CURRENT saves. "
                + "The restore replaces the active game save folders."
                + Environment.NewLine + Environment.NewLine
                + "Steam Cloud warning: temporarily disable sync for DOOM "
                + "or select LOCAL files in any Steam Cloud conflict.",
                "KHARVOX — Confirm save restore",
                MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (answer != DialogResult.Yes) return;
            var rescue = DoomSaveProtection.Restore(chosen.Id);
            MessageBox.Show(window,
                "Restore complete. Previous save snapshot: " + rescue.Id,
                "KHARVOX", MessageBoxButtons.OK, MessageBoxIcon.Information);
        });
        openFolder.Click += (_, _) => SafeRun(() => {
            Directory.CreateDirectory(DoomSaveProtection.StorageRoot);
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(
                DoomSaveProtection.StorageRoot) { UseShellExecute = true })?.Dispose();
        });
        locate.Click += (_, _) => SafeRun(() => {
            using var picker = new FolderBrowserDialog {
                Description = "Select DOOM 2016 base folder containing savegame / savegame.user",
                ShowNewFolderButton = false
            };
            if (picker.ShowDialog(window) == DialogResult.OK)
                DoomSaveProtection.SetSaveRoot(picker.SelectedPath);
        });
        close.Click += (_, _) => window.Close();
        try { RefreshList(); }
        catch (Exception error)
        {
            MessageBox.Show(window, error.Message,
                "KHARVOX — Save backup location", MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
        }
        window.ShowDialog(customOptionsForm ?? this);
    }
}
