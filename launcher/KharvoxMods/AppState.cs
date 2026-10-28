using System;
using System.IO;
using System.Web.Script.Serialization;

namespace KharvoxMods;

internal sealed class AppState
{
    // Only desktop-window attachment consent; NOT permission to patch native binaries.
    public bool ConsentRecorded { get; set; }
    public bool WindowAttachmentApproved { get; set; }
    public string KharvoxLauncherPath { get; set; } = "";

    // Portable-only storage: resolve relative to the executable, never the
    // current working directory or %LOCALAPPDATA%. No hidden user data.
    internal static string DataDirectory => Path.GetFullPath(AppContext.BaseDirectory);
    internal static string DefaultPath => Path.Combine(DataDirectory, "settings.json");
    internal static string LogsDirectory => Path.Combine(DataDirectory, "Logs");
    internal static string BackupsDirectory => Path.Combine(DataDirectory, "Backups");

    private static void EnsurePortableFolders(string root)
    {
        Directory.CreateDirectory(root);
        Directory.CreateDirectory(Path.Combine(root, "Logs"));
        Directory.CreateDirectory(Path.Combine(root, "Backups"));
    }

    internal static AppState Load(string? path = null)
    {
        var target = path ?? DefaultPath;
        if (!File.Exists(target)) return new AppState();
        try
        {
            var data = new JavaScriptSerializer().Deserialize<AppState>(File.ReadAllText(target));
            return data ?? new AppState();
        }
        catch (Exception exception) when (exception is IOException ||
            exception is InvalidOperationException || exception is ArgumentException)
        {
            // Corrupt state must never silently grant installation/attachment consent.
            return new AppState();
        }
    }

    internal void Save(string? path = null)
    {
        var target = path ?? DefaultPath;
        EnsurePortableFolders(Path.GetDirectoryName(Path.GetFullPath(target))!);
        var temp = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temp, new JavaScriptSerializer().Serialize(this));
            if (File.Exists(target)) File.Replace(temp, target, null);
            else File.Move(temp, target);
        }
        finally
        {
            if (File.Exists(temp)) File.Delete(temp);
        }
    }
}
