using System;
using System.IO;
using System.Text;

namespace KharvoxMods;

// Debugging belongs entirely to our app and never writes into KHARVOX or DOOM.
internal static class SetupDiagnostics
{
    private static readonly object Gate = new object();
    internal static string LogDirectory => Path.Combine(AppState.DataDirectory, "logs");
    internal static string LogPath => Path.Combine(LogDirectory, "setup-debug.log");

    internal static void Write(string message)
    {
        try
        {
            lock (Gate)
            {
                Directory.CreateDirectory(LogDirectory);
                var path = LogPath;
                if (File.Exists(path) && new FileInfo(path).Length > 1024 * 1024)
                {
                    var rotated = Path.Combine(LogDirectory, "setup-debug.previous.log");
                    if (File.Exists(rotated)) File.Delete(rotated);
                    File.Move(path, rotated);
                }
                File.AppendAllText(path,
                    "[" + DateTimeOffset.Now.ToString("yyyy-MM-dd HH:mm:ss.fff zzz") +
                    "] " + message + Environment.NewLine, Encoding.UTF8);
            }
        }
        catch (Exception error) when (error is IOException ||
            error is UnauthorizedAccessException || error is ArgumentException)
        {
            // Diagnostics must never prevent either launcher from running.
        }
    }

    internal static string Summary(AppState state, bool processRunning, bool windowFound)
    {
        return "Consent recorded: " + (state.ConsentRecorded ? "Yes" : "No") +
            Environment.NewLine +
            "Desktop attachment approved: " + (state.WindowAttachmentApproved ? "Yes" : "No") +
            Environment.NewLine +
            "Selected KHARVOX launcher: " +
                (string.IsNullOrWhiteSpace(state.KharvoxLauncherPath) ?
                    "(not selected)" : state.KharvoxLauncherPath) + Environment.NewLine +
            "KHARVOX launcher process running: " + (processRunning ? "Yes" : "No") +
            Environment.NewLine +
            "Launcher desktop window detected: " + (windowFound ? "Yes" : "No") +
            Environment.NewLine +
            "Native bridge installed by KHARVOX Mods: No (not implemented)" +
            Environment.NewLine +
            "Gameplay mods installed by KHARVOX Mods: No" +
            Environment.NewLine +
            "Setup log: " + LogPath;
    }
}
