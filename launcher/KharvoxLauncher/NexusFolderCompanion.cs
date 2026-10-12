using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace KharvoxLauncher;

// One KHARVOX-styled viewer used by BOTH the Mod Folder button and the
// Nexus popup. It never opens, repositions or terminates Windows Explorer.
internal static class NexusFolderCompanion
{
    internal static Action<Rectangle>? TestModsPanelOpenRequested;
    private static ModDropWindow? activeWindow;
    private static Form? launcher;
    private static Func<Rectangle?>? modsBoundsProvider;
    private static IntPtr trackedBrowser;
    private static System.Windows.Forms.Timer? dockTimer;

    // The folder and Nexus share one dock controller and one folder instance.
    internal static ModDropWindow? ActiveWindow =>
        activeWindow is { IsDisposed: false } ? activeWindow : null;
    internal static int ActiveWindowCount => ActiveWindow is null ? 0 : 1;


    internal static Task OpenForPopupAsync(string folder,
        Rectangle launcherBounds, IntPtr browserPopup,
        Form? anchor = null, Func<Rectangle?>? modsBounds = null)
    {
        if (TestModsPanelOpenRequested is { } test)
        {
            test(launcherBounds);
            return Task.CompletedTask;
        }
        var viewer = CreateOrReuse(folder, launcherBounds, anchor, modsBounds);
        trackedBrowser = browserPopup != IntPtr.Zero && IsWindow(browserPopup)
            ? browserPopup : IntPtr.Zero;
        ShowOrFocus(viewer, focus: false);
        UpdateDock();
        if (trackedBrowser != IntPtr.Zero) SetForegroundWindow(trackedBrowser);
        return Task.CompletedTask;
    }

    internal static void OpenStandalone(string folder, Rectangle launcherBounds,
        Form? anchor = null, Func<Rectangle?>? modsBounds = null)
    {
        if (TestModsPanelOpenRequested is { } test)
        {
            test(launcherBounds);
            return;
        }
        var viewer = CreateOrReuse(folder, launcherBounds, anchor, modsBounds);
        ShowOrFocus(viewer, focus: true);
        UpdateDock();
    }

    private static void ShowOrFocus(ModDropWindow viewer, bool focus)
    {
        if (!viewer.Visible) viewer.Show();
        viewer.RefreshItems();
        if (viewer.WindowState == FormWindowState.Minimized)
            viewer.WindowState = FormWindowState.Normal;
        if (focus) { viewer.BringToFront(); viewer.Activate(); }
    }

    private static ModDropWindow CreateOrReuse(string folder,
        Rectangle launcherBounds, Form? anchor, Func<Rectangle?>? modsBounds)
    {
        Directory.CreateDirectory(folder);
        launcher = anchor ?? launcher;
        modsBoundsProvider = modsBounds ?? modsBoundsProvider;
        if (ActiveWindow is { } existing)
        {
            if (string.Equals(existing.FolderPath,
                Path.GetFullPath(folder), StringComparison.OrdinalIgnoreCase))
            {
                EnsureDockTimer();
                return existing;
            }
            existing.Close();
        }
        var viewer = new ModDropWindow(folder, launcherBounds);
        activeWindow = viewer;
        viewer.FormClosed += (_, _) =>
        {
            if (ReferenceEquals(activeWindow, viewer))
            {
                activeWindow = null;
                StopDockTimer();
            }
        };
        EnsureDockTimer();
        return viewer;
    }

    private static void EnsureDockTimer()
    {
        if (dockTimer is not null) return;
        dockTimer = new System.Windows.Forms.Timer { Interval = 300 };
        dockTimer.Tick += (_, _) => UpdateDock();
        dockTimer.Start();
    }

    private static void StopDockTimer()
    {
        dockTimer?.Stop();
        dockTimer?.Dispose();
        dockTimer = null;
        trackedBrowser = IntPtr.Zero;
        modsBoundsProvider = null;
        launcher = null;
    }

    // A screen may not accommodate launcher + 890px VR MODS + folder + a
    // full-width browser in one row. In that case share the free side column
    // vertically, keeping both folder and browser visible and draggable.
    internal static (Rectangle Folder, Rectangle Browser) CalculateDock(
        Rectangle launcherBounds, Rectangle? visibleMods, Rectangle workArea,
        bool browserOpen)
    {
        var leftEdge = visibleMods?.Left ?? launcherBounds.Left;
        var roomLeft = Math.Max(0, leftEdge - workArea.Left);
        var roomRight = Math.Max(0, workArea.Right - launcherBounds.Right);
        var putLeft = roomLeft >= roomRight;
        var available = putLeft ? roomLeft : roomRight;
        var width = Math.Min(515, Math.Max(320, available));
        if (available < 320)
        {
            // Avoid offscreen windows on monitors too narrow for four panels.
            width = Math.Min(515, workArea.Width);
        }
        var top = Math.Max(workArea.Top, Math.Min(launcherBounds.Top,
            workArea.Bottom - Math.Min(launcherBounds.Height, workArea.Height)));
        var height = Math.Min(launcherBounds.Height, workArea.Bottom - top);
        var x = putLeft ? leftEdge - width : launcherBounds.Right;
        x = Math.Max(workArea.Left, Math.Min(x, workArea.Right - width));
        var folder = new Rectangle(x, top, width, height);
        var browser = Rectangle.Empty;
        if (browserOpen)
        {
            // Only the side AWAY from launcher/Custom Mods is free.
            // The apparent room across the launcher is occupied space.
            var extraLeft = putLeft ? x - workArea.Left : 0;
            var extraRight = putLeft ? 0 : workArea.Right - folder.Right;
            var canUseLeft = extraLeft >= 420;
            var canUseRight = extraRight >= 420;
            if (canUseLeft || canUseRight)
            {
                var browserWidth = Math.Min(760,
                    canUseLeft && (!canUseRight || extraLeft >= extraRight)
                        ? extraLeft : extraRight);
                var bx = canUseLeft && (!canUseRight || extraLeft >= extraRight)
                    ? folder.Left - browserWidth : folder.Right;
                browser = new Rectangle(bx, top, browserWidth, height);
            }
            else
            {
                // One side column: ZIP list on top, Nexus beneath it.
                var folderHeight = Math.Max(200, Math.Min(height - 300,
                    (int)(height * 0.42)));
                if (height < 520) folderHeight = Math.Max(200, height / 2);
                folder = new Rectangle(x, top, width, folderHeight);
                browser = new Rectangle(x, top + folderHeight,
                    width, height - folderHeight);
            }
        }
        return (folder, browser);
    }

    internal static Rectangle BrowserDockBounds(Rectangle fallback)
    {
        if (launcher is null || launcher.IsDisposed) return fallback;
        var bounds = launcher.WindowState == FormWindowState.Normal
            ? launcher.Bounds : launcher.RestoreBounds;
        var mods = modsBoundsProvider?.Invoke();
        return CalculateDock(bounds, mods, Screen.FromControl(launcher).WorkingArea,
            browserOpen: true).Browser;
    }

    internal static void UpdateDock()
    {
        var viewer = ActiveWindow;
        if (viewer is null || !viewer.Visible || launcher is null
            || launcher.IsDisposed || !launcher.Visible
            || launcher.WindowState == FormWindowState.Minimized) return;
        if (trackedBrowser != IntPtr.Zero && !IsWindow(trackedBrowser))
            trackedBrowser = IntPtr.Zero;
        var bounds = launcher.WindowState == FormWindowState.Normal
            ? launcher.Bounds : launcher.RestoreBounds;
        var layout = CalculateDock(bounds, modsBoundsProvider?.Invoke(),
            Screen.FromControl(launcher).WorkingArea, trackedBrowser != IntPtr.Zero);
        if (viewer.WindowState == FormWindowState.Normal
            && viewer.Bounds != layout.Folder) viewer.Bounds = layout.Folder;
        if (trackedBrowser != IntPtr.Zero && !layout.Browser.IsEmpty)
            NexusModsPopup.PositionTrackedBrowser(trackedBrowser, layout.Browser);
    }


    internal static bool RunFileDropPolicySelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-drop-" + Guid.NewGuid().ToString("N"));
        try
        {
            var user = Path.Combine(root, "user");
            Directory.CreateDirectory(user);
            var zipped = Path.Combine(root, "sample.zip");
            File.WriteAllText(zipped, "test-zip");
            var result = ModDropWindow.CopyZipFiles(user,
                new[] { zipped, Path.Combine(root, "installer.exe") });
            if (result.Copied != 1 || result.Skipped != 1
                || !File.Exists(Path.Combine(user, "sample.zip")))
                return false;
            result = ModDropWindow.CopyZipFiles(user, new[] { zipped });
            var compact = CalculateDock(new Rectangle(1405, 70, 515, 900),
                new Rectangle(515, 70, 890, 900),
                new Rectangle(0, 0, 1920, 1080), browserOpen: true);
            var roomy = CalculateDock(new Rectangle(2200, 70, 515, 900),
                new Rectangle(1310, 70, 890, 900),
                new Rectangle(0, 0, 3200, 1080), browserOpen: true);
            if (compact.Folder.IntersectsWith(compact.Browser)
                || compact.Folder.Right > 515
                || compact.Browser.Right > 515
                || compact.Folder.Bottom != compact.Browser.Top
                || roomy.Browser.Right != roomy.Folder.Left
                || roomy.Browser.IntersectsWith(roomy.Folder))
                return false;
            return result.Copied == 1
                && ModDropWindow.IsDirectChild(user, Path.Combine(user, "sample (2).zip"))
                && !ModDropWindow.IsDirectChild(user, zipped)
                && !ModDropWindow.IsDirectChild(user, Path.Combine(user, "nested", "sample.zip"))
                && File.Exists(Path.Combine(user, "sample (2).zip"))
                && File.ReadAllText(zipped) == "test-zip"
                && ModDropWindow.AcceptsFile(zipped)
                && !ModDropWindow.AcceptsFile(Path.Combine(root, "sample.zip.part"));
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }

    [DllImport("user32.dll")]
    private static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr window);
}

internal sealed class ModDropWindow : Form
{
    private readonly string folder;
    private readonly ListView files;
    private readonly Label status;
     private readonly FileSystemWatcher folderWatch;
 
    internal ModDropWindow(string directory, Rectangle bounds)
    {
        folder = directory;
        Text = "KHARVOX // MODS FOLDER";
        Bounds = bounds;
        // Use the exact icon resource embedded in KharvoxLauncher.exe.
        try { Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath); }
        catch (ArgumentException) { Icon = SystemIcons.Application; }
        ShowIcon = true;
        KeyPreview = true;
        ApplyDarkTitleBar();
        MinimumSize = new Size(300, 200);
        StartPosition = FormStartPosition.Manual;
        BackColor = Color.Black;
        ForeColor = Color.Gainsboro;
        Font = new Font("Segoe UI", 9F);
        ShowInTaskbar = true;
        AllowDrop = true;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, RowCount = 4, ColumnCount = 1,
            Padding = new Padding(12), BackColor = BackColor
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));

        var title = new Label
        {
            Dock = DockStyle.Fill, ForeColor = Color.White,
            Text = "DOOM // USER MODS", Font = new Font("Segoe UI", 12f, FontStyle.Bold),
            TextAlign = ContentAlignment.MiddleLeft
        };
        root.Controls.Add(title, 0, 0);
        var instructions = new Label
        {
            Dock = DockStyle.Fill, ForeColor = Color.Silver,
            Text = "Drag or paste downloaded .ZIP file here.",
            TextAlign = ContentAlignment.MiddleLeft,
            AutoEllipsis = true
        };
        root.Controls.Add(instructions, 0, 1);
        files = new ListView
        {
            Dock = DockStyle.Fill, View = View.Details,
            FullRowSelect = true, HideSelection = false, MultiSelect = true,
            BackColor = Color.FromArgb(19, 19, 22),
            ForeColor = Color.Gainsboro, BorderStyle = BorderStyle.FixedSingle,
            HeaderStyle = ColumnHeaderStyle.Nonclickable,
            AllowDrop = true
        };
        files.Columns.Add("Filename", 320);
        files.Columns.Add("Type", 82);
        files.Columns.Add("Size", 95);
        InstallFileCommands();
        files.Resize += (_, _) =>
        {
            files.Columns[0].Width = Math.Max(160, files.ClientSize.Width - 200);
        };
        root.Controls.Add(files, 0, 2);
        var footer = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1,
            BackColor = BackColor
        };
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 84));
        status = new Label
        {
            Dock = DockStyle.Fill, ForeColor = Color.Silver,
            TextAlign = ContentAlignment.MiddleLeft
        };
        footer.Controls.Add(status, 0, 0);
        var refresh = CreateButton("Refresh");
        refresh.Click += (_, _) => RefreshFiles();
        footer.Controls.Add(refresh, 1, 0);
        var close = CreateButton("Close");
        close.Click += (_, _) => Close();
        footer.Controls.Add(close, 2, 0);
        root.Controls.Add(footer, 0, 3);
        Controls.Add(root);

        DragEnter += AcceptDrag;
        DragDrop += DropFiles;
        files.DragEnter += AcceptDrag;
        files.DragDrop += DropFiles;
        root.DragEnter += AcceptDrag;
        root.DragDrop += DropFiles;
        instructions.AllowDrop = true;
        instructions.DragEnter += AcceptDrag;
        instructions.DragDrop += DropFiles;

        folderWatch = new FileSystemWatcher(folder)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName,
            IncludeSubdirectories = false, EnableRaisingEvents = true
        };
        folderWatch.Created += FilesChanged;
        folderWatch.Deleted += FilesChanged;
        folderWatch.Renamed += (_, _) => ScheduleRefresh();
        RefreshFiles();
    }

    private void InstallFileCommands()
    {
        var menu = new ContextMenuStrip { BackColor = Color.FromArgb(30, 30, 33),
            ForeColor = Color.WhiteSmoke, ShowImageMargin = false };
        var copy = menu.Items.Add("Copy    Ctrl+C");
        copy.Click += (_, _) => CopySelected();
        var paste = menu.Items.Add("Paste ZIPs    Ctrl+V");
        paste.Click += async (_, _) => await PasteZipsAsync();
        var delete = menu.Items.Add("Delete    Del");
        delete.Click += (_, _) => DeleteSelected();
        menu.Items.Add(new ToolStripSeparator());
        var all = menu.Items.Add("Select All    Ctrl+A");
        all.Click += (_, _) => SelectAllFiles();
        menu.Opening += (_, _) =>
        {
            copy.Enabled = files.SelectedItems.Count > 0;
            delete.Enabled = files.SelectedItems.Count > 0;
            paste.Enabled = ClipboardHasZip();
            all.Enabled = files.Items.Count > 0;
        };
        files.ContextMenuStrip = menu;
        ContextMenuStrip = menu;
    }

    private static bool ClipboardHasZip()
    {
        try
        {
            return Clipboard.ContainsFileDropList()
                && Clipboard.GetFileDropList().Cast<string>().Any(AcceptsFile);
        }
        catch (ExternalException) { return false; }
    }

    // Resolve selected paths from our list, not arbitrary clipboard/menu text.
    // Only direct children of this dedicated user-mod directory may be deleted.
    internal static bool IsDirectChild(string root, string candidate)
    {
        try
        {
            var source = Path.GetFullPath(candidate).TrimEnd(Path.DirectorySeparatorChar);
            var parent = Path.GetDirectoryName(source);
            return parent is not null
                && string.Equals(parent,
                    Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar),
                    StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException
            or PathTooLongException) { return false; }
    }

    private string[] SelectedPaths() => files.SelectedItems
        .Cast<ListViewItem>().Select(item => item.Tag as string)
        .Where(value => value is not null)
        .Select(value => value!)
        .Where(path => IsDirectChild(folder, path)
            && (File.Exists(path) || Directory.Exists(path)))
        .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();

    private void CopySelected()
    {
        var paths = SelectedPaths();
        if (paths.Length == 0) return;
        var entries = new StringCollection();
        entries.AddRange(paths);
        try
        {
            Clipboard.SetFileDropList(entries); // Explorer-compatible copy
            status.Text = paths.Length + " item(s) copied to clipboard.";
        }
        catch (ExternalException ex)
        {
            MessageBox.Show(this, ex.Message, "KHARVOX — Copy",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private async Task PasteZipsAsync()
    {
        try
        {
            if (!Clipboard.ContainsFileDropList())
            {
                status.Text = "Clipboard contains no ZIP files.";
                return;
            }
            var paths = Clipboard.GetFileDropList().Cast<string>().ToArray();
            status.Text = "Pasting ZIP archives…";
            var result = await Task.Run(() => CopyZipFiles(folder, paths));
            if (IsDisposed || Disposing) return;
            RefreshFiles();
            status.Text = result.Copied + " ZIP(s) imported, " + result.Skipped
                + " skipped; existing files and originals retained.";
        }
        catch (Exception ex) when (ex is ExternalException or IOException
            or UnauthorizedAccessException or ArgumentException)
        {
            if (!IsDisposed)
                MessageBox.Show(this, ex.Message, "KHARVOX — Paste ZIP",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void DeleteSelected()
    {
        var entries = SelectedPaths();
        if (entries.Length == 0) return;
        if (MessageBox.Show(this, "Permanently delete " + entries.Length
                + " selected item(s) from the KHARVOX mods folder?\n\n"
                + "This does not delete their original downloaded ZIPs.",
                "KHARVOX — Confirm delete",
                MessageBoxButtons.YesNo, MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2) != DialogResult.Yes) return;
        try
        {
            foreach (var entry in entries)
            {
                if (!IsDirectChild(folder, entry)) continue;
                // Never follow a junction/symlink while removing a mod entry.
                if ((File.GetAttributes(entry) & FileAttributes.ReparsePoint) != 0)
                    continue;
                if (File.Exists(entry)) File.Delete(entry);
                else if (Directory.Exists(entry)) Directory.Delete(entry, true);
            }
            RefreshFiles();
            status.Text = entries.Length + " selected item(s) deleted.";
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            MessageBox.Show(this, ex.Message, "KHARVOX — Delete",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
            RefreshFiles();
        }
    }

    private void SelectAllFiles()
    {
        foreach (ListViewItem item in files.Items) item.Selected = true;
        files.Focus();
    }

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        switch (keyData)
        {
            case Keys.Control | Keys.C: CopySelected(); return true;
            case Keys.Control | Keys.V: _ = PasteZipsAsync(); return true;
            case Keys.Delete: DeleteSelected(); return true;
            case Keys.Control | Keys.A: SelectAllFiles(); return true;
            case Keys.F5: RefreshFiles(); return true;
        }
        return base.ProcessCmdKey(ref msg, keyData);
    }

    private static Button CreateButton(string text)
    {
        var button = new Button
        {
            Text = text, Dock = DockStyle.Fill,
            Margin = new Padding(3, 2, 3, 2),
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(43, 43, 47),
            ForeColor = Color.WhiteSmoke
        };
        button.FlatAppearance.BorderColor = Color.DimGray;
        return button;
    }

    private void FilesChanged(object? sender, FileSystemEventArgs e) => ScheduleRefresh();
    private void ScheduleRefresh()
    {
        if (!IsHandleCreated || IsDisposed || Disposing) return;
        try { BeginInvoke((Action)RefreshFiles); }
        catch (InvalidOperationException) { }
    }
    internal string FolderPath => Path.GetFullPath(folder);
    internal void RefreshItems() => RefreshFiles();

    private void RefreshFiles()
    {
        if (IsDisposed) return;
        files.BeginUpdate();
        try
        {
            files.Items.Clear();
            foreach (var path in Directory.EnumerateFileSystemEntries(folder)
                .OrderBy(p => Path.GetFileName(p), StringComparer.OrdinalIgnoreCase))
            {
                var name = Path.GetFileName(path);
                if (name.StartsWith(".", StringComparison.Ordinal)) continue;
                var isDir = Directory.Exists(path);
                var item = new ListViewItem(name);
                item.SubItems.Add(isDir ? "Folder" : Path.GetExtension(path).ToLowerInvariant());
                item.SubItems.Add(isDir ? "" : new FileInfo(path).Length.ToString("N0") + " B");
                item.Tag = path;
                files.Items.Add(item);
            }
            status.Text = files.Items.Count + " items  •  " + folder;
        }
        catch (IOException ex) { status.Text = ex.Message; }
        catch (UnauthorizedAccessException ex) { status.Text = ex.Message; }
        finally { files.EndUpdate(); }
    }

    internal static bool AcceptsFile(string path) =>
        Path.GetExtension(path).Equals(".zip", StringComparison.OrdinalIgnoreCase)
        && File.Exists(path)
        && (File.GetAttributes(path) & FileAttributes.ReparsePoint) == 0;

    internal static (int Copied, int Skipped) CopyZipFiles(
        string target, IEnumerable<string> paths)
    {
        Directory.CreateDirectory(target);
        var copied = 0;
        var skipped = 0;
        foreach (var source in paths.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (!AcceptsFile(source)) { skipped++; continue; }
            var baseName = Path.GetFileNameWithoutExtension(source);
            var extension = ".zip";
            var dest = Path.Combine(target, baseName + extension);
            for (var suffix = 2; File.Exists(dest); suffix++)
                dest = Path.Combine(target, baseName + " (" + suffix + ")" + extension);
            var temporary = Path.Combine(target, "." + Guid.NewGuid().ToString("N") + ".part");
            try
            {
                File.Copy(source, temporary, overwrite: false);
                File.Move(temporary, dest); // never replace user ZIPs
                copied++;
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        return (copied, skipped);
    }

    private void AcceptDrag(object? sender, DragEventArgs e)
    {
        e.Effect = e.Data?.GetDataPresent(DataFormats.FileDrop) == true
            && (e.Data.GetData(DataFormats.FileDrop) as string[])?.Any(path =>
                File.Exists(path)
                && Path.GetExtension(path).Equals(".zip",
                    StringComparison.OrdinalIgnoreCase)) == true
            ? DragDropEffects.Copy : DragDropEffects.None;
    }

    private async void DropFiles(object? sender, DragEventArgs e)
    {
        if (e.Data?.GetData(DataFormats.FileDrop) is not string[] paths) return;
        status.Text = "Copying mod archives…";
        try
        {
            var selected = paths.ToArray();
            var result = await Task.Run(() => CopyZipFiles(folder, selected));
            if (IsDisposed || Disposing) return;
            RefreshFiles();
            status.Text = result.Copied + " ZIPs copied, " + result.Skipped
                + " unsupported or unavailable. Original files retained.";
        }
        catch (Exception error) when (error is IOException
            or UnauthorizedAccessException or ArgumentException)
        {
            if (!IsDisposed && !Disposing)
                MessageBox.Show(this, "Could not copy ZIP: " + error.Message,
                    "KHARVOX — Mod import", MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
        }
    }

    private void ApplyDarkTitleBar()
    {
        if (!IsHandleCreated) return;
        var enabled = 1;
        try
        {
            if (DwmSetWindowAttribute(Handle, 20, ref enabled, sizeof(int)) != 0)
                DwmSetWindowAttribute(Handle, 19, ref enabled, sizeof(int));
        }
        catch (DllNotFoundException) { }
        catch (EntryPointNotFoundException) { }
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        ApplyDarkTitleBar();
    }

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute,
        ref int value, int valueSize);

    protected override void Dispose(bool disposing)
    {
        if (disposing) folderWatch.Dispose();
        base.Dispose(disposing);
    }
}
