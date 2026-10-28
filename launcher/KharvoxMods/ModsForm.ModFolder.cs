using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace KharvoxMods;

// Standalone adaptation of the original NexusFolderCompanion's custom window.
// Reuses the original ModDropWindow control and its ZIP operations unchanged.
// The companion docks relative to the linked launcher/Mods pair.
internal sealed partial class ModsForm
{
    private ModDropWindow? _modFolderWindow;
    private Rectangle _lastFolderBounds;
    private Point _folderOffset;
    private Point _lastFolderAnchor;

    // Original folder location policy: dock a ~515px window on the side
    // with more free space beyond the two launcher windows.
    internal static Rectangle CalculateOriginalFolderDock(
        Rectangle launcherBounds, Rectangle modsBounds, Rectangle workArea)
    {
        if (workArea.Width <= 0 || workArea.Height <= 0 ||
            launcherBounds.Width <= 0 || launcherBounds.Height <= 0 ||
            modsBounds.Width <= 0)
            throw new ArgumentOutOfRangeException(nameof(workArea));

        var leftEdge = modsBounds.Left;
        var roomLeft = Math.Max(0, leftEdge - workArea.Left);
        var roomRight = Math.Max(0, workArea.Right - launcherBounds.Right);
        var putLeft = roomLeft >= roomRight;
        var available = putLeft ? roomLeft : roomRight;
        var width = Math.Min(515, Math.Max(320, available));
        if (available < 320) width = Math.Min(515, workArea.Width);
        var top = Math.Max(workArea.Top, Math.Min(launcherBounds.Top,
            workArea.Bottom - Math.Min(launcherBounds.Height, workArea.Height)));
        var height = Math.Min(launcherBounds.Height, workArea.Bottom - top);
        var x = putLeft ? leftEdge - width : launcherBounds.Right;
        x = Math.Max(workArea.Left, Math.Min(x, workArea.Right - width));
        return new Rectangle(x, top, width, height);
    }

    private void ShowOriginalModFolder()
    {
        try
        {
            Directory.CreateDirectory(AppState.ModsDirectory);
            if (_modFolderWindow is { IsDisposed: false } existing)
            {
                existing.RefreshItems();
                if (existing.WindowState == FormWindowState.Minimized)
                    existing.WindowState = FormWindowState.Normal;
                existing.BringToFront();
                existing.Activate();
                return;
            }

            var launcherBounds = _launcherWindow != IntPtr.Zero
                ? NativeDock.Bounds(_launcherWindow) : null;
            var anchor = launcherBounds ?? Bounds;
            var workArea = Screen.FromControl(this).WorkingArea;
            var position = CalculateOriginalFolderDock(anchor, Bounds, workArea);
            var viewer = new ModDropWindow(AppState.ModsDirectory, position);
            _modFolderWindow = viewer;
            _folderOffset = new Point(position.Left - Left, position.Top - Top);
            _lastFolderAnchor = Location;
            _lastFolderBounds = position;
            viewer.FormClosed += (_, _) =>
            {
                if (ReferenceEquals(_modFolderWindow, viewer))
                {
                    _modFolderWindow = null;
                    _lastFolderBounds = Rectangle.Empty;
                }
                if (!IsDisposed && !Disposing) ScanLocalMods();
            };
            viewer.Show(this); // linked owner keeps the folder above its group
            viewer.Activate();
            ScanLocalMods();
        }
        catch (Exception error) when (error is IOException ||
            error is UnauthorizedAccessException ||
            error is System.ComponentModel.Win32Exception)
        {
            MessageBox.Show(this, "Could not open Mods Folder: " + error.Message,
                "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void SyncOriginalModFolder()
    {
        var viewer = _modFolderWindow;
        if (viewer is null || viewer.IsDisposed ||
            viewer.WindowState != FormWindowState.Normal ||
            WindowState != FormWindowState.Normal) return;

        if (Location != _lastFolderAnchor)
        {
            // Moving either of the docked main windows moves this companion.
            var desired = new Point(Left + _folderOffset.X, Top + _folderOffset.Y);
            if (viewer.Location != desired) viewer.Location = desired;
        }
        else if (_lastFolderBounds != Rectangle.Empty &&
            viewer.Location != _lastFolderBounds.Location)
        {
            // Original KHARVOX behavior: dragging the Mods Folder also moves
            // its linked launcher/Mods pair by the same amount.
            var dx = viewer.Left - _lastFolderBounds.Left;
            var dy = viewer.Top - _lastFolderBounds.Top;
            if (_state.WindowAttachmentApproved && _launcherWindow != IntPtr.Zero &&
                NativeDock.Bounds(_launcherWindow) is Rectangle launcher)
            {
                NativeDock.Move(_launcherWindow,
                    new Rectangle(launcher.X + dx, launcher.Y + dy,
                        launcher.Width, launcher.Height));
            }
            Location = new Point(Left + dx, Top + dy);
        }

        _lastFolderBounds = viewer.Bounds;
        _lastFolderAnchor = Location;
    }

    internal static void VerifyOriginalFolderPositioning()
    {
        var launcher = new Rectangle(2200, 70, 515, 900);
        var mods = new Rectangle(1300, 70, 900, 900);
        var area = new Rectangle(0, 0, 3200, 1080);
        var dock = CalculateOriginalFolderDock(launcher, mods, area);
        if (dock.X != launcher.Right || dock.Y != launcher.Top ||
            dock.Width != 485 || dock.Height != launcher.Height)
            throw new InvalidDataException("Original right-side folder positioning regressed.");

        launcher = new Rectangle(1405, 70, 515, 900);
        mods = new Rectangle(515, 70, 890, 900);
        area = new Rectangle(0, 0, 1920, 1080);
        dock = CalculateOriginalFolderDock(launcher, mods, area);
        if (dock.Right != mods.Left || dock.Y != launcher.Top ||
            dock.Width != 515)
            throw new InvalidDataException("Original left-side folder positioning regressed.");
    }
}
