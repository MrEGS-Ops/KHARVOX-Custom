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

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeRect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetWindowRect(IntPtr window, out NativeRect bounds);

    // Windows 10/11 have invisible resize borders on both windows. Two
    // adjacent WinForms Bounds therefore leave a visible desktop strip.
    private const int DwmwaExtendedFrameBounds = 9;

    [DllImport("dwmapi.dll")]
    private static extern int DwmGetWindowAttribute(IntPtr window,
        int attribute, out NativeRect bounds, int size);

    internal static int CalculateVisibleSeamOverlap(int modsRightInset,
        int launcherLeftInset) =>
        Math.Min(48, Math.Max(0, modsRightInset) + Math.Max(0, launcherLeftInset));

    private static int VisibleLeftInset(Form window)
    {
        if (!window.IsHandleCreated) return 0;
        try
        {
            return GetWindowRect(window.Handle, out var outer)
                && DwmGetWindowAttribute(window.Handle, DwmwaExtendedFrameBounds,
                    out var frame, Marshal.SizeOf<NativeRect>()) == 0
                ? Math.Max(0, Math.Min(24, frame.Left - outer.Left)) : 0;
        }
        catch (DllNotFoundException) { return 0; }
        catch (EntryPointNotFoundException) { return 0; }
    }

    private static int VisibleRightInset(Form window)
    {
        if (!window.IsHandleCreated) return 0;
        try
        {
            return GetWindowRect(window.Handle, out var outer)
                && DwmGetWindowAttribute(window.Handle, DwmwaExtendedFrameBounds,
                    out var frame, Marshal.SizeOf<NativeRect>()) == 0
                ? Math.Max(0, Math.Min(24, outer.Right - frame.Right)) : 0;
        }
        catch (DllNotFoundException) { return 0; }
        catch (EntryPointNotFoundException) { return 0; }
    }

    private int CurrentVisibleSeamOverlap() =>
        customOptionsForm is { } mods
            ? CalculateVisibleSeamOverlap(
                VisibleRightInset(mods), VisibleLeftInset(this)) : 0;

    // Use actual outer window bounds, not ClientSize, to line up title bars,
    // bottom edges and the bordering frames with NO horizontal gap.
    internal static (Rectangle Launcher, Rectangle Mods) CalculateCustomModsDock(
        Rectangle launcher, int requestedModsWidth, Rectangle workingArea,
        int visibleSeamOverlap = 0)
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
        var seam = Math.Max(0, Math.Min(48, visibleSeamOverlap));
        var launcherWidth = Math.Min(launcher.Width, workingArea.Width);
        var modsWidth = requestedModsWidth;
        if (launcherWidth + modsWidth - seam > workingArea.Width)
        {
            launcherWidth = Math.Max(Math.Min(minimumLauncherWidth, launcherWidth),
                Math.Min(launcherWidth, workingArea.Width - modsWidth + seam));
            if (launcherWidth + modsWidth - seam > workingArea.Width)
                modsWidth = Math.Max(Math.Min(minimumModsWidth, modsWidth),
                    workingArea.Width - launcherWidth + seam);
            if (launcherWidth + modsWidth - seam > workingArea.Width)
            {
                // Very small displays cannot accommodate two useful panels.
                modsWidth = Math.Max(1, workingArea.Width - launcherWidth + seam);
            }
        }

        var height = Math.Min(launcher.Height, workingArea.Height);
        var top = Math.Max(workingArea.Top,
            Math.Min(launcher.Top, workingArea.Bottom - height));
        var left = Math.Max(workingArea.Left + modsWidth - seam,
            Math.Min(launcher.Left, workingArea.Right - launcherWidth));
        var right = new Rectangle(left, top, launcherWidth, height);
        // Deliberate rectangle overlap closes the INvisible Win32 resize
        // borders, so the two visible (DWM) frames actually touch.
        var leftPanel = new Rectangle(left - modsWidth + seam,
            top, modsWidth, height);
        return (right, leftPanel);
    }

    private void ConnectCustomModsDocking(Form mods)
    {
        mods.LocationChanged += (_, _) => FollowCustomModsMovement();
        mods.SizeChanged += (_, _) => FollowCustomModsMovement();
        mods.Activated += (_, _) => RaiseDockCompanion(this);
        // An owned WinForms window can become temporarily invisible when
        // its owner is minimized. Keep docking active across restore.
        // FollowLauncherMovement already ignores an intentionally hidden panel.
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
        var target = CalculateCustomModsDock(Bounds, mods.Width, workingArea,
            CurrentVisibleSeamOverlap());
        syncingCustomModsDock = true;
        try
        {
            // First reposition the launcher if the sidecar would be offscreen,
            // then attach the sidecar to its actual left border.
            if (Bounds != target.Launcher) Bounds = target.Launcher;
            // Preserve the pre-existing user-resize minimum: this change
            // only makes 730px the DEFAULT, not a new narrower limit.
            mods.MinimumSize = new Size(
                Math.Min(730, target.Mods.Width), Math.Min(530, target.Mods.Height));
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
            var overlap = CurrentVisibleSeamOverlap();
            var desiredLauncher = new Rectangle(
                mods.Right - overlap, mods.Top, Width, mods.Height);
            var area = Screen.FromRectangle(mods.Bounds).WorkingArea;
            var layout = CalculateCustomModsDock(desiredLauncher, mods.Width,
                area, overlap);
            if (Bounds != layout.Launcher) Bounds = layout.Launcher;
            if (mods.Bounds != layout.Mods) mods.Bounds = layout.Mods;
        }
        finally { syncingCustomModsDock = false; }
    }
}
