using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;

namespace KharvoxLauncher;

// Read-only startup timestamps. No change to upstream MainForm or VR runtime.
internal static class LauncherStartupTrace
{
    private static readonly Stopwatch Watch = new Stopwatch();
    private static readonly object Gate = new object();
    private static string _path = "";
    private static long _previousMs;

    internal static void Begin()
    {
        lock (Gate)
        {
            _path = Environment.GetEnvironmentVariable("KHARVOX_MODS_STARTUP_LOG")
                ?? Path.Combine(AppContext.BaseDirectory, "Logs",
                    "KHARVOX-launcher-startup.log");
            Watch.Restart();
            _previousMs = 0;
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
                File.WriteAllText(_path,
                    "KHARVOX modified launcher startup timings (UTC, elapsed, delta)" +
                    Environment.NewLine);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
        Mark("Modified launcher GUI entry");
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
                File.AppendAllText(_path,
                    DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture) +
                    " +" + elapsed + "ms (+" + delta + "ms) " + stage +
                    Environment.NewLine);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }
}
