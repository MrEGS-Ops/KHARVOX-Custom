using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace KharvoxMods;

// Auto-restart the independent Mods app after a successful native bridge install.
// A short-lived copy of this SAME EXE waits until BOTH old desktop processes
// exit, then starts a clean Mods session. No batch files, registry, AppData,
// forced process termination or secondary permanent updater.
internal static class BridgeRestartHandoff
{
    internal const string Switch = "--bridge-restart-handoff";
    private const int ShutdownTimeoutMs = 30000;

    private static long StartedAt(Process process)
    {
        try { return process.StartTime.ToUniversalTime().Ticks; }
        catch (Exception ex) when (ex is InvalidOperationException ||
            ex is System.ComponentModel.Win32Exception) { return 0; }
    }

    internal static bool Start(Process? launcher)
    {
        var current = Process.GetCurrentProcess();
        try
        {
            var launcherId = 0;
            long launcherStarted = 0;
            if (launcher != null)
            {
                try
                {
                    if (!launcher.HasExited)
                    {
                        launcherId = launcher.Id;
                        launcherStarted = StartedAt(launcher);
                    }
                }
                catch (InvalidOperationException) { }
            }

            var start = new ProcessStartInfo
            {
                FileName = Application.ExecutablePath,
                WorkingDirectory = AppContext.BaseDirectory,
                Arguments = Switch + " " + current.Id + " " + StartedAt(current) +
                    " " + launcherId + " " + launcherStarted,
                UseShellExecute = false,
                CreateNoWindow = true
            };
            using (var helper = Process.Start(start))
                return helper != null;
        }
        finally { current.Dispose(); }
    }

    private static bool WaitForInstance(int id, long startedAt, string expectedName)
    {
        if (id <= 0) return true;
        if (startedAt <= 0) return false;
        try
        {
            using (var process = Process.GetProcessById(id))
            {
                if (process.HasExited) return true;
                if (!string.Equals(process.ProcessName, expectedName,
                    StringComparison.OrdinalIgnoreCase)) return true; // PID reused
                if (StartedAt(process) != startedAt) return true; // PID reused
                return process.WaitForExit(ShutdownTimeoutMs);
            }
        }
        catch (ArgumentException) { return true; } // already exited
        catch (InvalidOperationException) { return true; }
        catch (System.ComponentModel.Win32Exception) { return false; }
    }

    internal static int Complete(string[] args)
    {
        if (args.Length != 5 || args[0] != Switch ||
            !int.TryParse(args[1], out var modsId) || modsId <= 0 ||
            !long.TryParse(args[2], out var modsStarted) || modsStarted <= 0 ||
            !int.TryParse(args[3], out var launcherId) || launcherId < 0 ||
            !long.TryParse(args[4], out var launcherStarted) || launcherStarted < 0 ||
            modsId == Process.GetCurrentProcess().Id)
            return 2;

        if (!WaitForInstance(modsId, modsStarted, "KharvoxMods") ||
            !WaitForInstance(launcherId, launcherStarted, "KharvoxLauncher"))
        {
            LogProblem("The previous KHARVOX desktop window did not close within 30 seconds.");
            MessageBox.Show(
                "KHARVOX didn't finish closing. Please close the old launcher, then open KHARVOX Mods again.",
                "KHARVOX Mods — Restart", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return 3;
        }

        // Never start a bridge-enabled session unless a complete verified bridge
        // exists and settings point to the new launcher.
        var settings = AppState.Load();
        if (string.IsNullOrWhiteSpace(settings.KharvoxLauncherPath) ||
            !string.Equals(Path.GetFullPath(settings.KharvoxLauncherPath),
                Path.GetFullPath(NativeBridgeInstaller.TargetLauncher),
                StringComparison.OrdinalIgnoreCase) ||
            !File.Exists(NativeBridgeInstaller.TargetLauncher) ||
            !File.Exists(Path.Combine(NativeBridgeInstaller.TargetDirectory,
                "KharvoxLayer.dll")) ||
            !File.Exists(Path.Combine(NativeBridgeInstaller.TargetDirectory,
                "KharvoxModsBridge.v1")))
        {
            LogProblem("Native Bridge was incomplete or not selected for restart.");
            return 4;
        }

        try
        {
            using (var relaunched = Process.Start(new ProcessStartInfo
            {
                FileName = Application.ExecutablePath,
                WorkingDirectory = AppContext.BaseDirectory,
                UseShellExecute = true
            }))
                return relaunched != null ? 0 : 5;
        }
        catch (Exception ex) when (ex is InvalidOperationException ||
            ex is System.ComponentModel.Win32Exception)
        {
            LogProblem("Couldn't reopen KHARVOX Mods: " + ex.Message);
            MessageBox.Show("Couldn't restart KHARVOX Mods: " + ex.Message,
                "KHARVOX Mods — Restart", MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
            return 6;
        }
    }

    internal static void SelfTest()
    {
        if (Complete(new[] { Switch, "0", "0", "0", "0" }) != 2 ||
            Complete(new[] { Switch, "oops", "0", "0", "0" }) != 2 ||
            Complete(new[] { Switch }) != 2)
            throw new InvalidDataException("Restart helper accepted malformed process identifiers.");
        using (var self = Process.GetCurrentProcess())
        {
            if (WaitForInstance(self.Id, 0, "KharvoxMods"))
                throw new InvalidDataException("Restart helper accepted a missing process identity.");
            if (!WaitForInstance(self.Id, self.StartTime.ToUniversalTime().Ticks,
                "does-not-exist"))
                throw new InvalidDataException("Restart helper failed reused-process detection.");
        }
    }

    private static void LogProblem(string message)
    {
        try
        {
            Directory.CreateDirectory(AppState.LogsDirectory);
            File.AppendAllText(Path.Combine(AppState.LogsDirectory, "bridge-restart.log"),
                DateTime.UtcNow.ToString("o") + " " + message + Environment.NewLine);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
