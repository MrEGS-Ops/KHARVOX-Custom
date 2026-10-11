using System;
using System.Collections.Generic;
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
    private static readonly List<ModDropWindow> windows = new();

    internal static Task OpenForPopupAsync(string folder,
        Rectangle launcherBounds, IntPtr browserPopup)
    {
        if (TestModsPanelOpenRequested is { } test)
        {
            test(launcherBounds);
            return Task.CompletedTask;
        }
        var viewer = Create(folder, launcherBounds);
        if (browserPopup != IntPtr.Zero && IsWindow(browserPopup))
            viewer.TrackBrowser(browserPopup);
        viewer.Show();
        if (browserPopup != IntPtr.Zero && IsWindow(browserPopup))
            SetForegroundWindow(browserPopup);
        return Task.CompletedTask;
    }

    internal static void OpenStandalone(string folder, Rectangle launcherBounds)
    {
        if (TestModsPanelOpenRequested is { } test)
        {
            test(launcherBounds);
            return;
        }
        var viewer = Create(folder, launcherBounds);
        viewer.Show();
        viewer.Activate();
    }

    private static ModDropWindow Create(string folder, Rectangle launcherBounds)
    {
        Directory.CreateDirectory(folder);
        var viewer = new ModDropWindow(folder, launcherBounds);
        windows.Add(viewer);
        viewer.FormClosed += (_, _) => windows.Remove(viewer);
        return viewer;
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
            return result.Copied == 1
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
    private readonly System.Windows.Forms.Timer? browserWatch;
    private readonly FileSystemWatcher folderWatch;
    private IntPtr linkedBrowser;

    internal ModDropWindow(string directory, Rectangle bounds)
    {
        folder = directory;
        Text = "KHARVOX // MODS FOLDER";
        Bounds = bounds;
        MinimumSize = new Size(300, 320);
        StartPosition = FormStartPosition.Manual;
        BackColor = Color.FromArgb(20, 20, 23);
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
            Text = "Drag downloaded .ZIP files from Firefox here. Originals stay in Downloads.",
            TextAlign = ContentAlignment.MiddleLeft,
            AutoEllipsis = true
        };
        root.Controls.Add(instructions, 0, 1);
        files = new ListView
        {
            Dock = DockStyle.Fill, View = View.Details,
            FullRowSelect = true, HideSelection = false, MultiSelect = true,
            BackColor = Color.FromArgb(31, 31, 35),
            ForeColor = Color.Gainsboro, BorderStyle = BorderStyle.FixedSingle,
            HeaderStyle = ColumnHeaderStyle.Nonclickable,
            AllowDrop = true
        };
        files.Columns.Add("Filename", 320);
        files.Columns.Add("Type", 82);
        files.Columns.Add("Size", 95);
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

    private void DropFiles(object? sender, DragEventArgs e)
    {
        if (e.Data?.GetData(DataFormats.FileDrop) is not string[] paths) return;
        try
        {
            var result = CopyZipFiles(folder, paths);
            RefreshFiles();
            status.Text = result.Copied + " ZIPs copied, " + result.Skipped
                + " unsupported or unavailable. Original files retained.";
        }
        catch (Exception error) when (error is IOException
            or UnauthorizedAccessException or ArgumentException)
        {
            MessageBox.Show(this, "Could not copy ZIP: " + error.Message,
                "KHARVOX — Mod import", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    internal void TrackBrowser(IntPtr browser)
    {
        linkedBrowser = browser;
        var poll = new System.Windows.Forms.Timer { Interval = 500 };
        poll.Tick += (_, _) =>
        {
            if (linkedBrowser != IntPtr.Zero && IsWindow(linkedBrowser)) return;
            poll.Stop();
            poll.Dispose();
            if (!IsDisposed && !Disposing) Close();
        };
        FormClosed += (_, _) => { poll.Stop(); poll.Dispose(); };
        poll.Start();
    }

    [DllImport("user32.dll")]
    private static extern bool IsWindow(IntPtr window);

    protected override void Dispose(bool disposing)
    {
        if (disposing) folderWatch.Dispose();
        base.Dispose(disposing);
    }
}
