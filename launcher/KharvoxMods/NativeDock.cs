using System;
using System.Drawing;
using System.Runtime.InteropServices;

namespace KharvoxMods;

// Cross-process Win32 docking. Does NOT inject into or patch KHARVOX.
internal static class NativeDock
{
    [StructLayout(LayoutKind.Sequential)]
    private struct Rect
    {
        public int Left, Top, Right, Bottom;
        public Rectangle ToRectangle() => Rectangle.FromLTRB(Left, Top, Right, Bottom);
    }

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hwnd, out Rect rectangle);

    [DllImport("user32.dll")]
    internal static extern bool IsWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    internal static extern bool IsIconic(IntPtr hwnd);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter,
        int x, int y, int cx, int cy, uint flags);

    [DllImport("user32.dll")]
    internal static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PostMessage(IntPtr hwnd, uint message,
        IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    private const uint WmClose = 0x0010;

    // Request a normal window close (equivalent to its X button), never kill
    // a process or close a window belonging to a different KHARVOX instance.
    internal static bool RequestClose(IntPtr hwnd, int expectedProcessId)
    {
        if (hwnd == IntPtr.Zero || !IsWindow(hwnd) || expectedProcessId <= 0)
            return false;
        GetWindowThreadProcessId(hwnd, out var ownerPid);
        return ownerPid == (uint)expectedProcessId &&
            PostMessage(hwnd, WmClose, IntPtr.Zero, IntPtr.Zero);
    }


    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern IntPtr GetAncestor(IntPtr hwnd, uint flags);

    [DllImport("dwmapi.dll")]
    private static extern int DwmGetWindowAttribute(IntPtr hwnd,
        int attribute, out Rect rectangle, int size);

    private const uint GaRoot = 2;
    // Use Windows' real popup owner relationship to keep the two independent
    // processes in the same Z-order group. No injection or executable patch.
    private const int GwlHwndParent = -8;
    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtr", SetLastError = true)]
    private static extern IntPtr SetWindowLongPtr(IntPtr window, int index, IntPtr newValue);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtr", SetLastError = true)]
    private static extern IntPtr GetWindowLongPtr(IntPtr window, int index);
    internal static bool SetOwner(IntPtr mods, IntPtr launcher)
    {
        if (mods == IntPtr.Zero || !IsWindow(mods) ||
            (launcher != IntPtr.Zero && !IsWindow(launcher))) return false;
        try
        {
            SetWindowLongPtr(mods, GwlHwndParent, launcher);
            return GetWindowLongPtr(mods, GwlHwndParent) == launcher;
        }
        catch (EntryPointNotFoundException) { return false; }
    }

    private const int DwmwaExtendedFrameBounds = 9;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpNoSize = 0x0001;

    // One desktop window can own keyboard focus, but both can sit directly
    // above unrelated windows. Maintain that pair without stealing focus.
    internal static IntPtr ForegroundRoot()
    {
        var focused = GetForegroundWindow();
        if (focused == IntPtr.Zero) return IntPtr.Zero;
        var root = GetAncestor(focused, GaRoot);
        return root == IntPtr.Zero ? focused : root;
    }

    internal static bool RaiseDockPair(IntPtr active, IntPtr companion)
    {
        if (active == IntPtr.Zero || active == companion ||
            !IsWindow(active) || !IsWindow(companion)) return false;
        const uint flags = SwpNoMove | SwpNoSize | NoActivate;
        // Bring partner and then the clicked window to the front in order.
        // HWND_TOP is not HWND_TOPMOST; neither process is pinned on top.
        var raisedPartner = SetWindowPos(companion, IntPtr.Zero,
            0, 0, 0, 0, flags);
        var raisedActive = SetWindowPos(active, IntPtr.Zero,
            0, 0, 0, 0, flags);
        return raisedPartner && raisedActive;
    }

    private static int VisibleInset(IntPtr hwnd, bool right)
    {
        if (hwnd == IntPtr.Zero || !IsWindow(hwnd) ||
            !GetWindowRect(hwnd, out var outer)) return 0;
        try
        {
            if (DwmGetWindowAttribute(hwnd, DwmwaExtendedFrameBounds,
                out var visible, Marshal.SizeOf<Rect>()) != 0) return 0;
            return Math.Max(0, Math.Min(24, right
                ? outer.Right - visible.Right
                : visible.Left - outer.Left));
        }
        catch (DllNotFoundException) { return 0; }
        catch (EntryPointNotFoundException) { return 0; }
    }

    internal static int VisibleSeamOverlap(IntPtr mods, IntPtr launcher) =>
        Math.Min(48, VisibleInset(mods, true) + VisibleInset(launcher, false));

    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int virtualKey);
    [DllImport("user32.dll")]
    private static extern bool GetCursorPos(out Point point);

    internal static bool LeftMousePressed() => (GetAsyncKeyState(0x01) & 0x8000) != 0;

    internal static bool MouseOnCaption(IntPtr hwnd)
    {
        if (!LeftMousePressed() || !GetCursorPos(out var cursor)) return false;
        var bounds = Bounds(hwnd);
        // Caption excludes client-side controls and the resize area.
        return bounds.HasValue && cursor.X >= bounds.Value.Left + 12 &&
            cursor.X < bounds.Value.Right - 12 &&
            cursor.Y >= bounds.Value.Top && cursor.Y < bounds.Value.Top + 34;
    }

    internal const int Minimize = 6;
    internal const int Restore = 9;
    private const uint NoZOrder = 0x0004;
    private const uint NoActivate = 0x0010;

    internal static Rectangle? Bounds(IntPtr hwnd)
    {
        if (hwnd == IntPtr.Zero || !IsWindow(hwnd) || !GetWindowRect(hwnd, out var bounds))
            return null;
        return bounds.ToRectangle();
    }

    internal static bool Move(IntPtr hwnd, Rectangle bounds)
    {
        return hwnd != IntPtr.Zero && IsWindow(hwnd) &&
            SetWindowPos(hwnd, IntPtr.Zero, bounds.Left, bounds.Top,
                bounds.Width, bounds.Height, NoZOrder | NoActivate);
    }

    // On each fresh Mods launch (including the bridge restart), center the
    // COMBINED pair as one rectangle in the current monitor's working area.
    // Do this once; following ticks honour user-initiated dragging instead.
    internal static (Rectangle Launcher, Rectangle Mods) CalculateCentered(
        Rectangle launcherBounds, Rectangle workArea, int desiredModsWidth,
        int visibleSeamOverlap = 8)
    {
        if (launcherBounds.Width <= 0 || launcherBounds.Height <= 0 ||
            workArea.Width <= 0 || workArea.Height <= 0 || desiredModsWidth <= 0)
            throw new ArgumentOutOfRangeException(nameof(launcherBounds));

        var seam = Math.Max(0, Math.Min(48, visibleSeamOverlap));
        var launcherWidth = Math.Min(launcherBounds.Width, workArea.Width);
        var launcherHeight = Math.Min(launcherBounds.Height, workArea.Height);
        var modsWidth = Math.Min(desiredModsWidth,
            Math.Max(1, workArea.Width - launcherWidth + seam));
        var combinedWidth = launcherWidth + modsWidth - seam;
        var x = workArea.Left + (workArea.Width - combinedWidth) / 2;
        var y = workArea.Top + (workArea.Height - launcherHeight) / 2;
        return (
            new Rectangle(x + modsWidth - seam, y, launcherWidth, launcherHeight),
            new Rectangle(x, y, modsWidth, launcherHeight)
        );
    }

    // Existing KHARVOX Custom Mods docks to the LEFT, with matching top/bottom.
    // Keep launcher width intact and shrink the companion to fit smaller screens.
    internal static (Rectangle Launcher, Rectangle Mods) Calculate(
        Rectangle originalLauncher, Rectangle workArea, int desiredModsWidth, int visibleSeamOverlap = 8)
    {
        if (originalLauncher.Width <= 0 || originalLauncher.Height <= 0 ||
            workArea.Width <= 0 || workArea.Height <= 0 || desiredModsWidth <= 0)
            throw new ArgumentOutOfRangeException(nameof(originalLauncher));

        var seamOverlap = Math.Max(0, Math.Min(48, visibleSeamOverlap)); // actual DWM invisible frame edges
        var launcherWidth = Math.Min(workArea.Width, originalLauncher.Width);
        var launcherHeight = Math.Min(workArea.Height, originalLauncher.Height);
        var modsWidth = Math.Min(desiredModsWidth,
            Math.Max(1, workArea.Width - launcherWidth + seamOverlap));
        var launcherLeft = Math.Max(workArea.Left + modsWidth - seamOverlap,
            Math.Min(originalLauncher.Left, workArea.Right - launcherWidth));
        var top = Math.Max(workArea.Top,
            Math.Min(originalLauncher.Top, workArea.Bottom - launcherHeight));
        var launcher = new Rectangle(launcherLeft, top, launcherWidth, launcherHeight);
        var mods = new Rectangle(launcher.Left - modsWidth + seamOverlap, top,
            modsWidth, launcherHeight);
        return (launcher, mods);
    }
}
