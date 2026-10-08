namespace KharvoxLauncher;

internal static class RuntimeStorage
{
    internal static string LogsDirectory
    {
        get
        {
            var path = Path.Combine(AppContext.BaseDirectory, "logs");
            Directory.CreateDirectory(path);
            return path;
        }
    }

    internal static string DiagnosticsDirectory
    {
        get
        {
            var path = Path.Combine(LogsDirectory, "diagnostics");
            Directory.CreateDirectory(path);
            return path;
        }
    }

    internal static string LogPath(string fileName) =>
        Path.Combine(LogsDirectory, fileName);

    internal static string DisplayBuild
    {
        get
        {
            try
            {
                var marker = Path.Combine(AppContext.BaseDirectory, "PATCH-BUILD.txt");
                if (!File.Exists(marker)) return "local";
                var lines = File.ReadAllLines(marker);
                var run = lines.FirstOrDefault(x => x.StartsWith("run=", StringComparison.OrdinalIgnoreCase))?.Substring(4).Trim();
                var commit = lines.FirstOrDefault(x => x.StartsWith("commit=", StringComparison.OrdinalIgnoreCase))?.Substring(7).Trim();
                if (!string.IsNullOrWhiteSpace(commit) && commit.Length > 7)
                    commit = commit.Substring(0, 7);
                if (!string.IsNullOrWhiteSpace(run) && !string.IsNullOrWhiteSpace(commit))
                    return "#" + run + " (" + commit + ")";
                if (!string.IsNullOrWhiteSpace(run)) return "#" + run;
                if (!string.IsNullOrWhiteSpace(commit)) return commit;
            }
            catch { }
            return "local";
        }
    }
}
