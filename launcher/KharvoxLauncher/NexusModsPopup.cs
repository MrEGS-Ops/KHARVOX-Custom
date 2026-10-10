using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using Microsoft.Win32;

namespace KharvoxLauncher;

// A standalone Microsoft Edge app window: no IE WebBrowser dependency, no
// embedded legacy engine and no changes to DOOM mod selection/installation.
internal static class NexusModsPopup
{
    internal const string PageUrl = "https://www.nexusmods.com/games/doom/mods";

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
            // --app creates a separate, chrome-free popup. Both parameters
            // request the outer position and size of the Custom Mods form.
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

    internal static void Open(Rectangle customModsBounds)
    {
        // Windows' App Paths registration resolves Edge even when it is not
        // on PATH. Fallback to the explicit standard installation locations.
        var executable = FindEdgeExecutable() ?? "msedge.exe";
        try
        {
            using var process = Process.Start(
                MakePopupStartInfo(customModsBounds, executable));
            if (process is null)
                throw new InvalidOperationException("Edge did not open a browser window.");
        }
        catch (Win32Exception)
        {
            // If Edge isn't available, still allow visiting the site in the
            // user's default browser; exact popup geometry then isn't assured.
            using var process = Process.Start(new ProcessStartInfo(PageUrl)
                { UseShellExecute = true });
            if (process is null)
                throw new InvalidOperationException("Could not open a browser.");
        }
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
}
