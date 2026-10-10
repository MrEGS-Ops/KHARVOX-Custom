using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace KharvoxLauncher;

public sealed partial class MainForm
{
    private bool customModsDocked;
    private bool syncingCustomModsDock;
    private bool raisingCustomModsDock;

    // Both windows stay normal (never TopMost). Clicking either raises the
    // OTHER without stealing keyboard focus from the window the user clicked.
    // Windows only has one truly active window at a time.
    private const uint SwpNoMove = 0x0002;
    private const uint SwpNoSize = 0x0001;
    private const uint SwpNoActivate = 0x0010;
    private static readonly IntPtr HwndTop = IntPtr.Zero;

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    // Use actual outer window bounds, not ClientSize, to line up title bars,
    // bottom edges and the bordering frames with NO horizontal gap.
    internal static (Rectangle Launcher, Rectangle Mods) CalculateCustomModsDock(
        Rectangle launcher, int requestedModsWidth, Rectangle workingArea)
    {
        if (launcher.Width < 1 || launcher.Height < 1
            || requestedModsWidth < 1 || workingArea.Width < 1
            || workingArea.Height < 1)
            throw new ArgumentOutOfRangeException(nameof(launcher));

        // On ordinary desktop monitors, preserve both window widths. If the
        // desktop is too narrow for the pair, shrink the launcher down to its
        // supported minimum first, then the mods sidecar as a last resort.
        const int minimumLauncherWidth = 280;
        const int minimumModsWidth = 540;
        var launcherWidth = Math.Min(launcher.Width, workingArea.Width);
        var modsWidth = requestedModsWidth;
        if (launcherWidth + modsWidth > workingArea.Width)
        {
            launcherWidth = Math.Max(Math.Min(minimumLauncherWidth, launcherWidth),
                Math.Min(launcherWidth, workingArea.Width - modsWidth));
            if (launcherWidth + modsWidth > workingArea.Width)
                modsWidth = Math.Max(Math.Min(minimumModsWidth, modsWidth),
                    workingArea.Width - launcherWidth);
            if (launcherWidth + modsWidth > workingArea.Width)
            {
                // Very small displays cannot accommodate two useful panels;
                // keep both visible even if the mod panel becomes narrow.
                modsWidth = Math.Max(1, workingArea.Width - launcherWidth);
            }
        }

        var height = Math.Min(launcher.Height, workingArea.Height);
        var top = Math.Max(workingArea.Top,
            Math.Min(launcher.Top, workingArea.Bottom - height));
        var left = Math.Max(workingArea.Left + modsWidth,
            Math.Min(launcher.Left, workingArea.Right - launcherWidth));
        var right = new Rectangle(left, top, launcherWidth, height);
        var leftPanel = new Rectangle(left - modsWidth, top, modsWidth, height);
        return (right, leftPanel);
    }

    private void ConnectCustomModsDocking(Form mods)
    {
        mods.LocationChanged += (_, _) => FollowCustomModsMovement();
        mods.SizeChanged += (_, _) => FollowCustomModsMovement();
        mods.Activated += (_, _) => RaiseDockCompanion(this);
        mods.VisibleChanged += (_, _) =>
        {
            if (!mods.Visible) customModsDocked = false;
        };
    }

    private void RaiseDockCompanion(Form companion)
    {
        var mods = customOptionsForm;
        if (!customModsDocked || raisingCustomModsDock || mods?.Visible != true
            || !Visible || WindowState == FormWindowState.Minimized
            || companion.IsDisposed || !companion.IsHandleCreated) return;
        raisingCustomModsDock = true;
        try
        {
            // Place the partner directly above unrelated windows but do NOT
            // activate it, take focus from a textbox, or pin it TopMost.
            SetWindowPos(companion.Handle, HwndTop, 0, 0, 0, 0,
                SwpNoMove | SwpNoSize | SwpNoActivate);
        }
        finally { raisingCustomModsDock = false; }
    }

    private void PlaceCustomModsBesideLauncher()
    {
        var mods = customOptionsForm;
        if (mods is null || mods.IsDisposed || syncingCustomModsDock
            || WindowState == FormWindowState.Minimized) return;

        var workingArea = Screen.FromControl(this).WorkingArea;
        var target = CalculateCustomModsDock(Bounds, mods.Width, workingArea);
        syncingCustomModsDock = true;
        try
        {
            // First reposition the launcher if the sidecar would be offscreen,
            // then attach the sidecar to its actual left border.
            if (Bounds != target.Launcher) Bounds = target.Launcher;
            mods.MinimumSize = new Size(
                Math.Min(540, target.Mods.Width), Math.Min(400, target.Mods.Height));
            mods.MaximumSize = Size.Empty;
            if (mods.Bounds != target.Mods) mods.Bounds = target.Mods;
        }
        finally { syncingCustomModsDock = false; }
    }

    private void FollowLauncherMovement()
    {
        if (!customModsDocked || syncingCustomModsDock
            || customOptionsForm?.Visible != true) return;
        PlaceCustomModsBesideLauncher();
    }

    private void FollowLauncherMovementAndSize()
    {
        FollowLauncherMovement();
    }

    private void FollowCustomModsMovement()
    {
        var mods = customOptionsForm;
        if (!customModsDocked || syncingCustomModsDock || mods is null
            || !mods.Visible || WindowState == FormWindowState.Minimized
            || mods.WindowState != FormWindowState.Normal) return;

        syncingCustomModsDock = true;
        try
        {
            // Dragging the left window also moves the launcher, as though
            // they were one compound window. Resizing it keeps both heights
            // equal and the shared boundary touching.
            var desiredLauncher = new Rectangle(
                mods.Right, mods.Top, Width, mods.Height);
            var area = Screen.FromRectangle(mods.Bounds).WorkingArea;
            var layout = CalculateCustomModsDock(desiredLauncher, mods.Width, area);
            if (Bounds != layout.Launcher) Bounds = layout.Launcher;
            if (mods.Bounds != layout.Mods) mods.Bounds = layout.Mods;
        }
        finally { syncingCustomModsDock = false; }
    }
}
