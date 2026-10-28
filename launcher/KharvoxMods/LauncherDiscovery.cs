using System;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace KharvoxMods;

internal static class LauncherDiscovery
{
    private const string Executable = "KharvoxLauncher.exe";

    internal static bool IsLauncherFile(string? path)
    {
        if (string.IsNullOrWhiteSpace(path)) return false;
        try
        {
            return Path.GetFileName(path).Equals(Executable, StringComparison.OrdinalIgnoreCase)
                && File.Exists(path);
        }
        catch (Exception exception) when (exception is ArgumentException ||
            exception is NotSupportedException || exception is PathTooLongException) { return false; }
    }

    internal static string? Locate(string? preferredPath)
    {
        if (IsLauncherFile(preferredPath)) return Path.GetFullPath(preferredPath!);
        var baseDir = AppContext.BaseDirectory;
        // A sibling KHARVOX installation is recommended. No writes or directory scans.
        var candidates = new[]
        {
            Path.Combine(baseDir, Executable),
            Path.Combine(baseDir, "KHARVOX", Executable),
            Path.Combine(baseDir, "..", "KHARVOX", Executable),
            Path.Combine(baseDir, "..", "..", "KHARVOX", Executable),
            Path.Combine(baseDir, "..", "KharvoxLauncher", Executable)
        };
        return candidates.Select(Path.GetFullPath).FirstOrDefault(IsLauncherFile);
    }

    internal static Process? FindRunning(string launcherPath)
    {
        foreach (var process in Process.GetProcessesByName("KharvoxLauncher"))
        {
            try
            {
                // Avoid attaching to an unrelated KHARVOX installation by name alone.
                var candidate = process.MainModule?.FileName;
                if (candidate != null && string.Equals(Path.GetFullPath(candidate),
                    Path.GetFullPath(launcherPath), StringComparison.OrdinalIgnoreCase)
                    && !process.HasExited) return process;
            }
            catch (Exception exception) when (exception is InvalidOperationException ||
                exception is System.ComponentModel.Win32Exception ||
                exception is System.IO.FileNotFoundException) { }
            process.Dispose();
        }
        return null;
    }

    internal static Process StartOrFind(string launcherPath)
    {
        if (!IsLauncherFile(launcherPath))
            throw new FileNotFoundException("Select the original KharvoxLauncher.exe.", launcherPath);
        var existing = FindRunning(launcherPath);
        if (existing != null) return existing;
        var info = new ProcessStartInfo
        {
            FileName = launcherPath,
            WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(launcherPath))!,
            UseShellExecute = false
        };
        // Only a custom KHARVOX launcher containing StandaloneModsBridge.cs
        // consumes this variable; stock KHARVOX ignores it harmlessly.
        info.EnvironmentVariables["KHARVOX_MODS_BRIDGE_FILE"] =
            StandaloneModPreferences.DefaultPath;
        return Process.Start(info)
            ?? throw new InvalidOperationException("Windows did not start KHARVOX.");
    }
}
