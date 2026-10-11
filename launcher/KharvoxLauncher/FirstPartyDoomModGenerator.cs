using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace KharvoxLauncher;

// Launches the existing first-party generator against this user's installed
// original game resources. Never silently downloads or ships game declarations.
internal static class FirstPartyDoomModGenerator
{
    internal static string ScriptPath =>
        Path.Combine(AppContext.BaseDirectory, "tools", "Generate-KHARVOX-Mods.py");

    internal static bool CanOffer(string? gameDirectory) =>
        !string.IsNullOrWhiteSpace(gameDirectory)
        && File.Exists(Path.Combine(gameDirectory, "DOOMx64vk.exe"))
        && File.Exists(ScriptPath);

    internal static async Task<string> GenerateAsync(string gameDirectory)
    {
        if (!CanOffer(gameDirectory))
            throw new InvalidOperationException(
                "Select the DOOM installation and install a full KHARVOX build containing "
                + "tools/Generate-KHARVOX-Mods.py before generating resource mods.");
        Directory.CreateDirectory(DoomUserMods.PackagedFolder);

        // Python 3.9+ must already be installed. The two well-known Windows
        // launchers are tried without hardcoding a local machine's path.
        Exception? missingPython = null;
        foreach (var executable in new[] { "py", "python" })
        {
            try
            {
                var args = executable == "py" ? "-3 " : "";
                args += Quote(ScriptPath) + " --game-dir " + Quote(gameDirectory)
                    + " --output-dir " + Quote(DoomUserMods.PackagedFolder)
                    + " --only both";
                using var process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = executable,
                        Arguments = args,
                        WorkingDirectory = AppContext.BaseDirectory,
                        CreateNoWindow = true,
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        RedirectStandardError = true
                    }
                };
                if (!process.Start())
                    throw new InvalidOperationException("Python did not start.");
                var stdout = process.StandardOutput.ReadToEndAsync();
                var stderr = process.StandardError.ReadToEndAsync();
                var completed = await Task.Run(() => process.WaitForExit(90000))
                    .ConfigureAwait(false);
                if (!completed)
                {
                    try { process.Kill(); } catch { }
                    throw new TimeoutException("The generator timed out after 90 seconds.");
                }
                var report = (await stdout.ConfigureAwait(false)).Trim();
                var errors = (await stderr.ConfigureAwait(false)).Trim();
                if (process.ExitCode != 0)
                    throw new InvalidOperationException(
                        "The generator stopped safely with code " + process.ExitCode
                        + Environment.NewLine + (errors.Length > 0 ? errors : report));
                return report.Length > 0 ? report
                    : "The KHARVOX resource-mod generator completed.";
            }
            catch (Win32Exception ex)
            {
                missingPython = ex;
            }
        }
        throw new InvalidOperationException(
            "Python 3.9 or newer is required to generate KHARVOX resource ZIPs. "
            + "Install Python, then retry. No DOOM resources were modified.",
            missingPython);
    }

    private static string Quote(string text) => "\"" + text.Replace("\"", "\\\"") + "\"";

    internal static bool RunSelfTest()
    {
        return ScriptPath.EndsWith(Path.Combine("tools", "Generate-KHARVOX-Mods.py"),
                StringComparison.OrdinalIgnoreCase)
            && !CanOffer("")
            && Quote(@"C:\Test Folder\DOOM").StartsWith("\"");
    }
}
