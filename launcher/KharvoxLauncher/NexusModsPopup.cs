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

// Open Nexus in a NEW WINDOW of the Windows DEFAULT browser. Never launch
// Edge unless Edge is the actual default, and never silently fall back to a
// URL shell open (which typically reuses an existing browser tab).
internal static class NexusModsPopup
{
    internal const string PageUrl = "https://www.nexusmods.com/games/doom/mods";

    // Test-only interception: dispatch the genuine LinkClicked handler in CI
    // without opening a browser on the runner.
    internal static Action<Rectangle>? TestOpenRequested;

    internal static ProcessStartInfo MakeDefaultBrowserStartInfo(
        string browserExecutable, Rectangle customModsBounds)
    {
        if (customModsBounds.Width < 1 || customModsBounds.Height < 1)
            throw new ArgumentOutOfRangeException(nameof(customModsBounds));
        if (string.IsNullOrWhiteSpace(browserExecutable))
            throw new ArgumentException("The default browser executable is missing.",
                nameof(browserExecutable));

        var exe = Path.GetFileNameWithoutExtension(browserExecutable).ToLowerInvariant();
        // Firefox (including its forks) supports --new-window URL, Chromium
        // browsers support --new-window URL too. Never use --app, --new-tab,
        // kiosk, a forced Edge path or a plain URL shell-open fallback.
        var firefoxFamily = exe == "firefox" || exe == "waterfox"
            || exe == "librewolf" || exe == "floorp" || exe == "zen";
        var chromiumFamily = exe == "chrome" || exe == "chromium"
            || exe == "brave" || exe == "msedge" || exe == "vivaldi"
            || exe == "opera" || exe == "opera_gx" || exe == "launcher"
            || exe == "arc";
        if (!firefoxFamily && !chromiumFamily)
            throw new NotSupportedException(
                "Your Windows default browser does not expose a known new-window command: "
                + browserExecutable);

        var argument = "--new-window \"" + PageUrl + "\"";
        if (chromiumFamily)
        {
            argument += " --window-position="
                + customModsBounds.X.ToString(CultureInfo.InvariantCulture)
                + "," + customModsBounds.Y.ToString(CultureInfo.InvariantCulture)
                + " --window-size="
                + customModsBounds.Width.ToString(CultureInfo.InvariantCulture)
                + "," + customModsBounds.Height.ToString(CultureInfo.InvariantCulture);
        }

        return new ProcessStartInfo(browserExecutable, argument)
        {
            UseShellExecute = true,
            WorkingDirectory = Path.GetDirectoryName(browserExecutable) ?? ""
        };
    }

    internal static bool TestDefaultBrowserArguments()
    {
        var bounds = new Rectangle(-780, 92, 780, 612);
        var firefox = MakeDefaultBrowserStartInfo(
            @"C:\Program Files\Mozilla Firefox\firefox.exe", bounds);
        var chrome = MakeDefaultBrowserStartInfo(
            @"C:\Program Files\Google\Chrome\Application\chrome.exe", bounds);
        var brave = MakeDefaultBrowserStartInfo(
            @"C:\Program Files\BraveSoftware\Brave-Browser\Application\brave.exe", bounds);
        try
        {
            MakeDefaultBrowserStartInfo(@"C:\Windows\UnknownBrowser.exe", bounds);
            return false;
        }
        catch (NotSupportedException) { }
        return firefox.UseShellExecute && chrome.UseShellExecute
            && firefox.FileName.EndsWith("firefox.exe", StringComparison.OrdinalIgnoreCase)
            && chrome.FileName.EndsWith("chrome.exe", StringComparison.OrdinalIgnoreCase)
            && firefox.Arguments.StartsWith("--new-window \"" + PageUrl + "\"",
                StringComparison.Ordinal)
            && !firefox.Arguments.Contains("--window-size")
            && chrome.Arguments.Contains("--window-position=-780,92")
            && chrome.Arguments.Contains("--window-size=780,612")
            && brave.Arguments.Contains("--new-window")
            && !firefox.Arguments.Contains("--app=")
            && !chrome.Arguments.Contains("--app=")
            && !firefox.Arguments.Contains("--new-tab");
    }

    internal static async Task<bool> OpenAsync(Rectangle customModsBounds)
    {
        if (TestOpenRequested is { } probe)
        {
            probe(customModsBounds);
            return true;
        }

        var browserPath = FindDefaultBrowserExecutable()
            ?? throw new InvalidOperationException(
                "Could not locate your Windows default browser. Check Settings > Apps > Default apps.");
        var command = MakeDefaultBrowserStartInfo(browserPath, customModsBounds);
        var processName = Path.GetFileNameWithoutExtension(browserPath);
        var before = GetVisibleBrowserWindows(processName);

        // This is a direct executable launch. No Edge-first attempt, no
        // six-second timeout and no slow default-URL fallback opening a tab.
        using (var started = Process.Start(command))
        {
            if (started is null)
                throw new Win32Exception("Windows did not start your default browser.");
        }

        // Best-effort focus and matching screen geometry. Normal browser
        // startup already foregrounds a new window; this also handles
        // requests forwarded to an existing Firefox/Chromium process.
        // Cap the foreground search at 1.2 seconds, rather than making users
        // wait six seconds before falling back to a tab.
        for (var attempt = 0; attempt < 12; attempt++)
        {
            var newWindow = GetVisibleBrowserWindows(processName)
                .FirstOrDefault(handle => !before.Contains(handle));
            if (newWindow != IntPtr.Zero)
            {
                ShowWindow(newWindow, 9); // SW_RESTORE
                SetWindowPos(newWindow, IntPtr.Zero,
                    customModsBounds.Left, customModsBounds.Top,
                    customModsBounds.Width, customModsBounds.Height,
                    0x0040); // SWP_SHOWWINDOW
                SetForegroundWindow(newWindow);
                return true;
            }
            await Task.Delay(100);
        }
        // The new-window command has already been sent. On a cold startup the
        // browser can appear later; don't launch the URL again as a tab.
        return true;
    }

    private static HashSet<IntPtr> GetVisibleBrowserWindows(string browserProcessName)
    {
        var result = new HashSet<IntPtr>();
        EnumWindows((handle, _) =>
        {
            if (!IsWindowVisible(handle)) return true;
            var cls = new StringBuilder(64);
            if (GetClassName(handle, cls, cls.Capacity) == 0)
                return true;
            var windowClass = cls.ToString();
            if (windowClass != "Chrome_WidgetWin_1"
                && windowClass != "MozillaWindowClass")
                return true;
            GetWindowThreadProcessId(handle, out var pid);
            if (pid == 0) return true;
            try
            {
                using var process = Process.GetProcessById(unchecked((int)pid));
                if (string.Equals(process.ProcessName, browserProcessName,
                    StringComparison.OrdinalIgnoreCase))
                    result.Add(handle);
            }
            catch (ArgumentException) { }
            catch (InvalidOperationException) { }
            catch (Win32Exception) { }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    private static string? FindDefaultBrowserExecutable()
    {
        // Windows UserChoice reflects the user's actual https preference.
        // Its ProgID command handles per-user browser associations that a
        // generic 'https' AssocQueryString can resolve to LaunchWinApp.exe.
        try
        {
            using var choice = Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\Shell\Associations\UrlAssociations\https\UserChoice");
            var progId = choice?.GetValue("ProgId") as string;
            if (!string.IsNullOrWhiteSpace(progId))
            {
                using var command = Registry.ClassesRoot.OpenSubKey(
                    progId + @"\shell\open\command");
                var commandLine = command?.GetValue(null) as string;
                var executable = ExtractExecutable(commandLine);
                if (executable is not null) return executable;
            }
        }
        catch (System.Security.SecurityException) { }

        // Association API is a backup when the ProgID command is hidden or
        // overridden. It is never used to select a non-default browser.
        const uint executableAssociation = 2; // ASSOCSTR_EXECUTABLE
        uint capacity = 1024;
        var buffer = new StringBuilder((int)capacity);
        if (AssocQueryString(0, executableAssociation, "https", "open",
                buffer, ref capacity) == 0)
        {
            var path = buffer.ToString();
            if (File.Exists(path)) return path;
        }
        return null;
    }

    private static string? ExtractExecutable(string? commandLine)
    {
        if (string.IsNullOrWhiteSpace(commandLine)) return null;
        var command = Environment.ExpandEnvironmentVariables(commandLine.Trim());
        var candidate = "";
        if (command.StartsWith("\"", StringComparison.Ordinal))
        {
            var end = command.IndexOf('"', 1);
            if (end > 1) candidate = command.Substring(1, end - 1);
        }
        else
        {
            var end = command.IndexOf(".exe", StringComparison.OrdinalIgnoreCase);
            if (end >= 0) candidate = command.Substring(0, end + 4);
        }
        return File.Exists(candidate) ? candidate : null;
    }

    [DllImport("shlwapi.dll", CharSet = CharSet.Unicode)]
    private static extern int AssocQueryString(uint flags, uint str,
        string association, string extra, StringBuilder result,
        ref uint resultSize);

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
