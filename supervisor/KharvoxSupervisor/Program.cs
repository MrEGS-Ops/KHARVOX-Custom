using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Web.Script.Serialization;

namespace KharvoxSupervisor;

internal sealed class SupervisorConfig
{
    public int Schema { get; set; } = 1;
    public bool Enabled { get; set; } = true;
    public string ProcessName { get; set; } = "DOOMx64vk";
    public int PollIntervalMs { get; set; } = 100;
    public string OutputFile { get; set; } = "logs/KHARVOX-Supervisor.ndjson";
    public bool IncludeProcessLifecycle { get; set; } = true;
    public bool IncludeProcessMetrics { get; set; } = true;
    public int MetricsIntervalMs { get; set; } = 1000;
    public string Notes { get; set; } = string.Empty;
    public List<FileWatchConfig> Files { get; set; } = new();
}

internal sealed class FileWatchConfig
{
    public string Name { get; set; } = string.Empty;
    public string Path { get; set; } = string.Empty;
    public string Mode { get; set; } = "tail";
    public bool Optional { get; set; } = true;
}

internal static class Program
{
    private static volatile bool _cancel;

    private sealed class FileCursor
    {
        public long Position;
        public DateTime LastWriteUtc;
        public string LastSnapshot = string.Empty;
    }

    private static int Main(string[] args)
    {
        var configPath = Path.Combine(AppContext.BaseDirectory, "KharvoxSupervisor.json");
        var once = false;
        for (var i = 0; i < args.Length; i++)
        {
            if (string.Equals(args[i], "--config", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                configPath = Path.GetFullPath(args[++i]);
            else if (string.Equals(args[i], "--once", StringComparison.OrdinalIgnoreCase))
                once = true;
        }

        SupervisorConfig config;
        try
        {
            if (!File.Exists(configPath))
            {
                Console.Error.WriteLine("Supervisor config not found: " + configPath);
                return 2;
            }

            var serializer = new JavaScriptSerializer();
            config = serializer.Deserialize<SupervisorConfig>(File.ReadAllText(configPath))
                ?? new SupervisorConfig();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("Invalid supervisor config: " + ex.Message);
            return 3;
        }

        if (!config.Enabled) return 0;
        config.PollIntervalMs = Math.Max(25, Math.Min(5000, config.PollIntervalMs));
        config.MetricsIntervalMs = Math.Max(250, Math.Min(60000, config.MetricsIntervalMs));

        var outputPath = ResolvePath(config.OutputFile);
        Directory.CreateDirectory(Path.GetDirectoryName(outputPath) ?? AppContext.BaseDirectory);
        var cursors = new Dictionary<string, FileCursor>(StringComparer.OrdinalIgnoreCase);

        Console.CancelKeyPress += (_, e) =>
        {
            e.Cancel = true;
            _cancel = true;
        };

        WriteEvent(outputPath, "supervisor-start", new Dictionary<string, object>
        {
            ["config"] = configPath,
            ["process"] = config.ProcessName,
            ["fileWatchCount"] = config.Files.Count
        });

        Process? observed = null;
        var nextMetrics = DateTime.MinValue;

        while (!_cancel)
        {
            try
            {
                var current = Process.GetProcessesByName(config.ProcessName).FirstOrDefault();

                if (observed is null && current is not null)
                {
                    observed = current;
                    if (config.IncludeProcessLifecycle)
                        WriteEvent(outputPath, "process-attached", ProcessFields(observed));
                    nextMetrics = DateTime.MinValue;
                }
                else if (observed is not null && (observed.HasExited || current is null))
                {
                    if (config.IncludeProcessLifecycle)
                    {
                        var fields = new Dictionary<string, object> { ["pid"] = observed.Id };
                        WriteEvent(outputPath, "process-detached", fields);
                    }
                    observed.Dispose();
                    observed = null;
                }
                else if (current is not null && observed is not null && current.Id != observed.Id)
                {
                    observed.Dispose();
                    observed = current;
                    if (config.IncludeProcessLifecycle)
                        WriteEvent(outputPath, "process-attached", ProcessFields(observed));
                    nextMetrics = DateTime.MinValue;
                }
                else
                {
                    current?.Dispose();
                }

                if (observed is not null && config.IncludeProcessMetrics
                    && DateTime.UtcNow >= nextMetrics)
                {
                    observed.Refresh();
                    WriteEvent(outputPath, "process-metrics", ProcessFields(observed));
                    nextMetrics = DateTime.UtcNow.AddMilliseconds(config.MetricsIntervalMs);
                }

                foreach (var file in config.Files)
                    PollFile(file, cursors, outputPath);

                if (once) break;
            }
            catch (Exception ex)
            {
                WriteEvent(outputPath, "supervisor-error", new Dictionary<string, object>
                {
                    ["error"] = ex.GetType().Name + ": " + ex.Message
                });
                if (once) break;
            }

            Thread.Sleep(config.PollIntervalMs);
        }

        observed?.Dispose();
        WriteEvent(outputPath, "supervisor-stop", new Dictionary<string, object>());
        return 0;
    }

    private static Dictionary<string, object> ProcessFields(Process process)
    {
        var fields = new Dictionary<string, object>
        {
            ["pid"] = process.Id
        };
        try { fields["workingSetBytes"] = process.WorkingSet64; } catch { }
        try { fields["privateBytes"] = process.PrivateMemorySize64; } catch { }
        try { fields["threads"] = process.Threads.Count; } catch { }
        try { fields["totalProcessorMs"] = process.TotalProcessorTime.TotalMilliseconds; } catch { }
        try { fields["startTimeUtc"] = process.StartTime.ToUniversalTime().ToString("O", CultureInfo.InvariantCulture); } catch { }
        return fields;
    }

    private static void PollFile(
        FileWatchConfig watch, Dictionary<string, FileCursor> cursors, string outputPath)
    {
        if (string.IsNullOrWhiteSpace(watch.Path)) return;
        var path = ResolvePath(watch.Path);
        if (!File.Exists(path))
        {
            if (!watch.Optional && !cursors.ContainsKey(path))
            {
                cursors[path] = new FileCursor();
                WriteEvent(outputPath, "file-missing", new Dictionary<string, object>
                {
                    ["name"] = watch.Name,
                    ["path"] = path
                });
            }
            return;
        }

        if (!cursors.TryGetValue(path, out var cursor))
        {
            cursor = new FileCursor();
            cursors[path] = cursor;
            if (string.Equals(watch.Mode, "tail", StringComparison.OrdinalIgnoreCase))
                cursor.Position = new FileInfo(path).Length;
        }

        if (string.Equals(watch.Mode, "snapshot", StringComparison.OrdinalIgnoreCase))
        {
            var info = new FileInfo(path);
            if (cursor.LastWriteUtc == info.LastWriteTimeUtc) return;
            cursor.LastWriteUtc = info.LastWriteTimeUtc;
            var text = ReadTextSafely(path, 256 * 1024);
            if (text == cursor.LastSnapshot) return;
            cursor.LastSnapshot = text;
            WriteEvent(outputPath, "file-snapshot", new Dictionary<string, object>
            {
                ["name"] = watch.Name,
                ["path"] = path,
                ["content"] = text
            });
            return;
        }

        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length < cursor.Position) cursor.Position = 0;
        stream.Position = cursor.Position;
        using var reader = new StreamReader(stream, Encoding.UTF8, true, 4096, leaveOpen: true);
        string? line;
        while ((line = reader.ReadLine()) is not null)
        {
            WriteEvent(outputPath, "file-line", new Dictionary<string, object>
            {
                ["name"] = watch.Name,
                ["path"] = path,
                ["line"] = line
            });
        }
        cursor.Position = stream.Position;
    }

    private static string ReadTextSafely(string path, int maximumBytes)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        var count = (int)Math.Min(maximumBytes, stream.Length);
        var buffer = new byte[count];
        var read = stream.Read(buffer, 0, count);
        return Encoding.UTF8.GetString(buffer, 0, read);
    }

    private static string ResolvePath(string value)
    {
        if (string.IsNullOrWhiteSpace(value)) return AppContext.BaseDirectory;
        return Path.IsPathRooted(value)
            ? value
            : Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,
                value.Replace('/', Path.DirectorySeparatorChar)));
    }

    private static void WriteEvent(string path, string kind, Dictionary<string, object> values)
    {
        try
        {
            values["timeUtc"] = DateTime.UtcNow.ToString("O", CultureInfo.InvariantCulture);
            values["event"] = kind;
            var serializer = new JavaScriptSerializer();
            File.AppendAllText(path, serializer.Serialize(values) + Environment.NewLine, Encoding.UTF8);
        }
        catch
        {
            // Diagnostics must never affect the game or launcher.
        }
    }
}
