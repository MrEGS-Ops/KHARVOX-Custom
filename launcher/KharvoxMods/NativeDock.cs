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

    // Existing KHARVOX Custom Mods docks to the LEFT, with matching top/bottom.
    // Keep launcher width intact and shrink the companion to fit smaller screens.
    internal static (Rectangle Launcher, Rectangle Mods) Calculate(
        Rectangle originalLauncher, Rectangle workArea, int desiredModsWidth)
    {
        if (originalLauncher.Width <= 0 || originalLauncher.Height <= 0 ||
            workArea.Width <= 0 || workArea.Height <= 0 || desiredModsWidth <= 0)
            throw new ArgumentOutOfRangeException(nameof(originalLauncher));

        const int seamOverlap = 8; // Windows invisible frame edges
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
