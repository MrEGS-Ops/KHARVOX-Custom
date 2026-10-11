using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Pair ONLY the newly created File Explorer window with the newly created
// Nexus browser popup. Never terminate explorer.exe or an existing folder.
internal static class NexusFolderCompanion
{
    internal static Action<Rectangle>? TestExplorerOpenRequested;
    private static readonly List<System.Windows.Forms.Timer> activeWatches = new();

    internal static async Task OpenForPopupAsync(string folder, Rectangle launcherBounds,
        IntPtr browserPopup)
    {
        if (TestExplorerOpenRequested is { } probe)
        {
            probe(launcherBounds);
            return;
        }
        System.IO.Directory.CreateDirectory(folder);
        var existing = VisibleFolderWindows();
        try
        {
            using var process = Process.Start(new ProcessStartInfo(
                "explorer.exe", "/n,\"" + folder + "\"")
            {
                UseShellExecute = true
            });
        }
        catch (Win32Exception error)
        {
            throw new InvalidOperationException("Could not open your DOOM mods folder.", error);
        }

        // Explorer is a shared shell process: Process.MainWindowHandle is
        // unreliable. Find the new folder-window HWND instead.
        IntPtr newFolder = IntPtr.Zero;
        for (var attempt = 0; attempt < 35; attempt++)
        {
            foreach (var candidate in VisibleFolderWindows())
                if (!existing.Contains(candidate)) { newFolder = candidate; break; }
            if (newFolder != IntPtr.Zero) break;
            await Task.Delay(100);
        }
        if (newFolder == IntPtr.Zero) return; // don't claim ownership of old folders

        SetWindowPos(newFolder, IntPtr.Zero, launcherBounds.Left,
            launcherBounds.Top, launcherBounds.Width, launcherBounds.Height,
            0x0040 | 0x0010); // SHOWWINDOW | NOACTIVATE

        // If the browser popup could not be identified, keep Explorer open:
        // never close a user's folder on an unreliable lifecycle guess.
        if (browserPopup == IntPtr.Zero || !IsWindow(browserPopup)) return;
        var watcher = new System.Windows.Forms.Timer { Interval = 500 };
        watcher.Tick += (_, _) =>
        {
            if (IsWindow(browserPopup)) return;
            watcher.Stop();
            if (IsWindow(newFolder) && IsTrackedFolderWindow(newFolder))
                PostMessage(newFolder, 0x0010, IntPtr.Zero, IntPtr.Zero); // WM_CLOSE
            activeWatches.Remove(watcher);
            watcher.Dispose();
        };
        activeWatches.Add(watcher);
        watcher.Start();
        // Browser popup should remain above the helper folder when both open.
        SetForegroundWindow(browserPopup);
    }

    private static HashSet<IntPtr> VisibleFolderWindows()
    {
        var found = new HashSet<IntPtr>();
        EnumWindows((handle, _) =>
        {
            if (IsWindowVisible(handle) && IsTrackedFolderWindow(handle))
                found.Add(handle);
            return true;
        }, IntPtr.Zero);
        return found;
    }

    private static bool IsTrackedFolderWindow(IntPtr window)
    {
        var cls = new StringBuilder(80);
        if (GetClassName(window, cls, cls.Capacity) == 0
            || cls.ToString() is not ("CabinetWClass" or "ExploreWClass"))
            return false;
        GetWindowThreadProcessId(window, out var pid);
        if (pid == 0) return false;
        try
        {
            using var process = Process.GetProcessById((int)pid);
            return string.Equals(process.ProcessName, "explorer",
                StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception e) when (e is ArgumentException or InvalidOperationException
            or Win32Exception) { return false; }
    }

    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr parameter);
    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")]
    private static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y,
        int width, int height, uint flags);
    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr window, uint message,
        IntPtr wParam, IntPtr lParam);
}
