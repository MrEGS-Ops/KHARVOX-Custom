using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32;

namespace KharvoxLauncher;

// Opens the DOOM Nexus catalogue as a real Edge app window, positioned like
// the current Custom Mods form. If Edge does not create a window, don't silently
// succeed just because Process.Start returned a process handle.
internal static class NexusModsPopup
{
    internal const string PageUrl = "https://www.nexusmods.com/games/doom/mods";

    // Used solely by the Windows UI smoke test. Intercepts the click before
    // launching a real browser and confirms the link is wired up.
    internal static Action<Rectangle>? TestOpenRequested;

    internal static ProcessStartInfo MakePopupStartInfo(Rectangle customModsBounds,
        string browserExecutable = "msedge.exe")
    {
        if (customModsBounds.Width < 1 || customModsBounds.Height < 1)
            throw new ArgumentOutOfRangeException(nameof(customModsBounds));

        var x = customModsBounds.X.ToString(CultureInfo.InvariantCulture);
        var y = customModsBounds.Y.ToString(CultureInfo.InvariantCulture);
        var w = customModsBounds.Width.ToString(CultureInfo.InvariantCulture);
        var h = customModsBounds.Height.ToString(CultureInfo.InvariantCulture);
        return new ProcessStartInfo
        {
            FileName = browserExecutable,
            Arguments = "--new-window --app=\"" + PageUrl + "\""
                + " --window-position=" + x + "," + y
                + " --window-size=" + w + "," + h,
            UseShellExecute = true
        };
    }

    internal static bool TestPopupArguments()
    {
        var b = new Rectangle(-780, 92, 780, 612);
        var info = MakePopupStartInfo(b);
        return info.UseShellExecute
            && info.FileName == "msedge.exe"
            && info.Arguments.Contains("--new-window")
            && info.Arguments.Contains("--app=\"" + PageUrl + "\"")
            && info.Arguments.Contains("--window-position=-780,92")
            && info.Arguments.Contains("--window-size=780,612");
    }

    // Returns true for the requested same-sized Edge popup, false when the
    // default browser had to be used. Both paths must visibly open a page or
    // throw an error that the UI can report to the user.
    internal static async Task<bool> OpenAsync(Rectangle customModsBounds)
    {
        if (TestOpenRequested is { } probe)
        {
            probe(customModsBounds);
            return true;
        }

        var executable = FindEdgeExecutable() ?? "msedge.exe";
        var oldWindows = GetVisibleEdgeWindows();
        try
        {
            using var process = Process.Start(
                MakePopupStartInfo(customModsBounds, executable));
            if (process is null)
                throw new Win32Exception("Windows did not start Edge.");
        }
        catch (Win32Exception)
        {
            OpenDefaultBrowser();
            return false;
        }

        // Edge typically relays CLI requests to its already running process.
        // Process.Start can report success even when the actual new app window
        // is still hidden, behind the launcher or never created.
        for (var attempt = 0; attempt < 60; attempt++)
        {
            var newWindow = GetVisibleEdgeWindows()
                .FirstOrDefault(window => !oldWindows.Contains(window));
            if (newWindow != IntPtr.Zero)
            {
                ShowWindow(newWindow, 9); // SW_RESTORE
                SetWindowPos(newWindow, IntPtr.Zero,
                    customModsBounds.X, customModsBounds.Y,
                    customModsBounds.Width, customModsBounds.Height,
                    0x0040); // SWP_SHOWWINDOW
                SetForegroundWindow(newWindow);
                return true;
            }
            await Task.Delay(100);
        }

        // If Edge discarded the app-style request, the user must still get
        // the page. Avoid an apparent no-op.
        OpenDefaultBrowser();
        return false;
    }

    private static void OpenDefaultBrowser()
    {
        using var process = Process.Start(new ProcessStartInfo(PageUrl)
            { UseShellExecute = true });
        // ShellExecute URL handlers can legally return null even on success;
        // no Process object is required here.
    }

    private static HashSet<IntPtr> GetVisibleEdgeWindows()
    {
        var result = new HashSet<IntPtr>();
        EnumWindows((hwnd, _) =>
        {
            if (!IsWindowVisible(hwnd)) return true;
            var cls = new StringBuilder(64);
            if (GetClassName(hwnd, cls, cls.Capacity) == 0
                || !string.Equals(cls.ToString(), "Chrome_WidgetWin_1",
                    StringComparison.Ordinal))
                return true;
            GetWindowThreadProcessId(hwnd, out var pid);
            if (pid == 0) return true;
            try
            {
                using var process = Process.GetProcessById(unchecked((int)pid));
                if (string.Equals(process.ProcessName, "msedge",
                    StringComparison.OrdinalIgnoreCase))
                    result.Add(hwnd);
            }
            catch (ArgumentException) { }
            catch (InvalidOperationException) { }
            catch (Win32Exception) { }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    private static string? FindEdgeExecutable()
    {
        const string key = @"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe";
        foreach (var root in new[] { Registry.CurrentUser, Registry.LocalMachine })
        {
            try
            {
                using var entry = root.OpenSubKey(key);
                var candidate = entry?.GetValue(null) as string;
                if (!string.IsNullOrWhiteSpace(candidate) && File.Exists(candidate))
                    return candidate;
            }
            catch (System.Security.SecurityException) { }
        }
        foreach (var baseDir in new[]
            { Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
              Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) })
        {
            if (string.IsNullOrWhiteSpace(baseDir)) continue;
            var candidate = Path.Combine(baseDir, "Microsoft", "Edge",
                "Application", "msedge.exe");
            if (File.Exists(candidate)) return candidate;
        }
        return null;
    }

    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hwnd, StringBuilder name, int size);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hwnd, int command);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y,
        int width, int height, uint flags);
}
