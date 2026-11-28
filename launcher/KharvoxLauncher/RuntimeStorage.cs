namespace KharvoxLauncher;

internal static class RuntimeStorage
{
    private static readonly Dictionary<string,string> SessionFiles = ReadSessionFiles();

    private static Dictionary<string,string> ReadSessionFiles()
    {
        var map = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        using var stream = typeof(RuntimeStorage).Assembly.GetManifestResourceStream("RIPSHIFT.SessionFiles")
            ?? throw new InvalidDataException("Missing session file manifest.");
        using var reader = new StreamReader(stream);
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            var pieces = line.Split('"');
            if (pieces.Length != 5 || !line.StartsWith("SESSION_FILE(", StringComparison.Ordinal)
                || (pieces[1] != "Flags" && pieces[1] != "Settings" && pieces[1] != "Status")
                || Path.GetFileName(pieces[3]) != pieces[3] || pieces[3].Contains(".."))
                throw new InvalidDataException("Invalid session file manifest: " + line);
            map.Add(pieces[3], pieces[1]);
        }
        return map;
    }

    internal static string SessionPath(string root,string name)
    {
        if (!SessionFiles.TryGetValue(name,out var folder))
            throw new InvalidDataException("Unmapped runtime file: " + name);
        return Path.Combine(root,"Runtime",folder,name);
    }

    private static string OwnerPath(string root) => Path.Combine(root,"Runtime","session.owner");

    // Caller holds the launch gate and has verified DOOM is not running.
    internal static string BeginSession(string root)
    {
        foreach (var folder in new[] {"Flags","Settings","Status"})
            Directory.CreateDirectory(Path.Combine(root,"Runtime",folder));
        foreach (var name in SessionFiles.Keys)
            File.Delete(SessionPath(root,name));
        var owner = Guid.NewGuid().ToString("N");
        File.WriteAllText(OwnerPath(root),owner);
        return owner;
    }

    // Caller holds the launch gate. Never delete unknown files, UserData,
    // legacy root overrides or any file belonging to another launch.
    internal static void EndSession(string root,string owner)
    {
        try
        {
            if (!File.Exists(OwnerPath(root)) || File.ReadAllText(OwnerPath(root)) != owner) return;
            foreach (var name in SessionFiles.Keys)
                File.Delete(SessionPath(root,name));
            File.Delete(OwnerPath(root));
        }
        catch { /* Next launch retries cleanup; never interrupt game exit. */ }
    }

    internal static void VerifySessionStorage(string root)
    {
        var sentinel = Path.Combine(root,"UserData","Calibrations","offhand_hud.cfg");
        Directory.CreateDirectory(Path.GetDirectoryName(sentinel)!);
        File.WriteAllText(sentinel,"personal");
        var legacy = Path.Combine(root,"weapon_scale.cfg");
        File.WriteAllText(legacy,"legacy");
        var owner = BeginSession(root);
        foreach (var name in SessionFiles.Keys) File.WriteAllText(SessionPath(root,name),name);
        var unknown = Path.Combine(root,"Runtime","Settings","unmanaged.txt");
        File.WriteAllText(unknown,"keep");
        EndSession(root,"wrong-owner");
        foreach (var name in SessionFiles.Keys)
            if (File.ReadAllText(SessionPath(root,name)) != name)
                throw new InvalidDataException("Session file lost prematurely: " + name);
        EndSession(root,owner);
        if (SessionFiles.Keys.Any(name => File.Exists(SessionPath(root,name)))
            || File.Exists(OwnerPath(root)) || File.ReadAllText(sentinel)!="personal"
            || File.ReadAllText(legacy)!="legacy" || File.ReadAllText(unknown)!="keep")
            throw new InvalidDataException("Session cleanup damaged retained data.");
        var interrupted = BeginSession(root);
        File.WriteAllText(SessionPath(root,"enable_virtual_gunstock"),"stale");
        var next = BeginSession(root);
        EndSession(root,interrupted);
        if (File.Exists(SessionPath(root,"enable_virtual_gunstock"))
            || File.ReadAllText(OwnerPath(root))!=next)
            throw new InvalidDataException("Interrupted session recovery failed.");
        EndSession(root,next);
    }

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
