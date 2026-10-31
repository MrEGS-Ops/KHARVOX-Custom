using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;

namespace KharvoxMods;

// Optional startup diagnostics only: never controls launcher success/failure.
// One session per manager run, stored locally alongside portable settings.
internal static class StartupTrace
{
    private static readonly Stopwatch Watch = new Stopwatch();
    private static readonly object Gate = new object();
    private static long _previousMs;
    internal static string LogPath =>
        Path.Combine(AppContext.BaseDirectory, "Logs", "KHARVOX-manager-startup.log");

    internal static void Begin()
    {
        lock (Gate)
        {
            Watch.Restart();
            _previousMs = 0;
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(LogPath)!);
                File.WriteAllText(LogPath,
                    "KHARVOX VR Mod Manager startup timings (UTC, elapsed, delta)" +
                    Environment.NewLine);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
        Mark("Manager entry");
    }

    internal static void Mark(string stage)
    {
        lock (Gate)
        {
            if (!Watch.IsRunning) return;
            var ms = Watch.ElapsedMilliseconds;
            var elapsed = ms.ToString(CultureInfo.InvariantCulture);
            var delta = (ms - _previousMs).ToString(CultureInfo.InvariantCulture);
            _previousMs = ms;
            try
            {
                File.AppendAllText(LogPath,
                    DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture) +
                    " +" + elapsed + "ms (+" + delta + "ms) " + stage +
                    Environment.NewLine);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }
}
