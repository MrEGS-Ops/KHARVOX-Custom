using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace KharvoxMods;

// One-time compatibility bridge for the old updater. It can only install
// KharvoxMods.exe because its hard-coded allowlist predates the new filename.
// Copy the newly updated app to the renamed EXE, launch it, and leave all
// settings, backups, NativeBridge and saved folders in place.
internal static class LegacyExecutableMigration
{
    private const string OldApp = "KharvoxMods.exe";
    private const string NewApp = "KharvoxVRModManager.exe";
    private const string OldUpdater = "KharvoxModsUpdater.exe";
    private const string NewUpdater = "KharvoxVRModManagerUpdater.exe";

    internal static bool HandoffIfLegacyName()
    {
        var current = Process.GetCurrentProcess().MainModule?.FileName ?? "";
        if (!string.Equals(Path.GetFileName(current), OldApp,
            StringComparison.OrdinalIgnoreCase)) return false;

        var target = Path.Combine(AppContext.BaseDirectory, NewApp);
        try
        {
            // Copy the updated binary, not the previous cached build.
            // A different running instance may still own the destination.
            if (!File.Exists(target) || !Process.GetProcessesByName(
                Path.GetFileNameWithoutExtension(NewApp)).Any())
                File.Copy(current, target, true);
            Process.Start(new ProcessStartInfo
            {
                FileName = target,
                WorkingDirectory = AppContext.BaseDirectory,
                UseShellExecute = true
            })?.Dispose();
            return true;
        }
        catch (Exception e) when (e is IOException ||
            e is UnauthorizedAccessException ||
            e is System.ComponentModel.Win32Exception)
        {
            // Fail open: old filename can still run the same new application
            // if Windows permissions forbid renaming its binary.
            return false;
        }
    }

    internal static void CleanupLegacyAsync()
    {
        Task.Run(async () =>
        {
            for (var i = 0; i < 20; i++)
            {
                await Task.Delay(400);
                var root = AppContext.BaseDirectory;
                if (IsRunning(OldApp)) continue;
                TryDelete(Path.Combine(root, OldApp));
                TryDelete(Path.Combine(root, OldApp + ".config"));

                // Never remove the old updater until the replacement exists,
                // and never delete an updater process still being used.
                if (File.Exists(Path.Combine(root, NewUpdater)) &&
                    !IsRunning(OldUpdater))
                    TryDelete(Path.Combine(root, OldUpdater));
                if (!File.Exists(Path.Combine(root, OldApp)) &&
                    !File.Exists(Path.Combine(root, OldUpdater))) return;
            }
        });
    }

    private static bool IsRunning(string exe)
    {
        foreach (var p in Process.GetProcessesByName(
            Path.GetFileNameWithoutExtension(exe)))
        {
            using (p)
            {
                try { if (!p.HasExited) return true; }
                catch (InvalidOperationException) { }
            }
        }
        return false;
    }

    private static void TryDelete(string file)
    {
        try { if (File.Exists(file)) File.Delete(file); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
