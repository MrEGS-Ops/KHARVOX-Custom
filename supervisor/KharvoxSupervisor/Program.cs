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
    public string PersistPath { get; set; } = string.Empty;
}

internal static class Program
{
    private static volatile bool _cancel;

    private sealed class FileCursor
    {
        public long Position;
        public DateTime LastWriteUtc;
        public string LastSnapshot = string.Empty;
        public readonly List<byte> PendingLine = new();
        public bool SkippingLongLine;
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
            else if (string.Equals(args[i], "--self-test", StringComparison.OrdinalIgnoreCase))
                return RunTailSelfTest();
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

        if (config.Schema != 1)
        {
            Console.Error.WriteLine(
                "Unsupported Supervisor config schema: " + config.Schema
                + " (expected 1)");
            return 4;
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
            if (!string.IsNullOrWhiteSpace(watch.PersistPath))
            {
                var persistPath = ResolvePath(watch.PersistPath);
                Directory.CreateDirectory(
                    Path.GetDirectoryName(persistPath) ?? AppContext.BaseDirectory);
                File.WriteAllText(persistPath, text, Encoding.UTF8);
                WriteEvent(outputPath, "file-persisted", new Dictionary<string, object>
                {
                    ["name"] = watch.Name,
                    ["source"] = path,
                    ["persistedTo"] = persistPath
                });
            }
            return;
        }

        // Keep incomplete lines as bytes so a partial write (including a split
        // UTF-8 character) cannot turn into two corrupted diagnostic records.
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length < cursor.Position)
        {
            cursor.Position = 0;
            cursor.PendingLine.Clear();
            cursor.SkippingLongLine = false;
        }
        stream.Position = cursor.Position;
        const int maximumLineBytes = 256 * 1024;
        var buffer = new byte[8192];
        int count;
        while ((count = stream.Read(buffer, 0, buffer.Length)) > 0)
        {
            for (var i = 0; i < count; i++)
            {
                var current = buffer[i];
                if (current == (byte)'\n')
                {
                    if (cursor.SkippingLongLine)
                    {
                        WriteEvent(outputPath, "file-line-skipped", new Dictionary<string, object>
                        {
                            ["name"] = watch.Name,
                            ["path"] = path,
                            ["reason"] = "line exceeded 256 KiB"
                        });
                        cursor.SkippingLongLine = false;
                    }
                    else
                    {
                        var lineBytes = cursor.PendingLine.Count;
                        if (lineBytes > 0 && cursor.PendingLine[lineBytes - 1] == (byte)'\r')
                            lineBytes--;
                        var line = Encoding.UTF8.GetString(cursor.PendingLine.ToArray(), 0, lineBytes);
                        WriteEvent(outputPath, "file-line", new Dictionary<string, object>
                        {
                            ["name"] = watch.Name,
                            ["path"] = path,
                            ["line"] = line
                        });
                    }
                    cursor.PendingLine.Clear();
                }
                else if (!cursor.SkippingLongLine)
                {
                    if (cursor.PendingLine.Count >= maximumLineBytes)
                    {
                        cursor.PendingLine.Clear();
                        cursor.SkippingLongLine = true;
                    }
                    else cursor.PendingLine.Add(current);
                }
            }
        }
        cursor.Position = stream.Position;
    }

    // Runs without DOOM or an installed config; exercised by the Windows build.
    private static int RunTailSelfTest()
    {
        var directory = Path.Combine(Path.GetTempPath(), "KHARVOX-Supervisor-Test-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(directory);
            var inputPath = Path.Combine(directory, "tail.log");
            var outputPath = Path.Combine(directory, "events.ndjson");
            File.WriteAllBytes(inputPath, Array.Empty<byte>());
            var watch = new FileWatchConfig { Name = "SelfTest", Path = inputPath, Mode = "tail" };
            var cursors = new Dictionary<string, FileCursor>(StringComparer.OrdinalIgnoreCase);
            PollFile(watch, cursors, outputPath); // existing file: start following from its end

            const string line = "partial-⚔-complete";
            var utf8 = Encoding.UTF8.GetBytes(line + "\r\n");
            var split = Encoding.UTF8.GetByteCount("partial-") + 1;
            using (var writer = new FileStream(inputPath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite))
                writer.Write(utf8, 0, split); // split inside the multibyte character
            PollFile(watch, cursors, outputPath);
            if (File.Exists(outputPath) && File.ReadAllLines(outputPath).Length != 0)
                throw new InvalidOperationException("Partial line emitted before newline.");

            using (var writer = new FileStream(inputPath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite))
                writer.Write(utf8, split, utf8.Length - split);
            PollFile(watch, cursors, outputPath);
            AssertTailLine(outputPath, 1, line);

            // Truncation must discard any stale cursor and follow the new log.
            File.WriteAllBytes(inputPath, Encoding.UTF8.GetBytes("reset\n"));
            PollFile(watch, cursors, outputPath);
            AssertTailLine(outputPath, 2, "reset");

            Console.WriteLine("KHARVOX Supervisor tail self-test passed (partial write, UTF-8, CRLF, truncation).");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("KHARVOX Supervisor tail self-test FAILED: " + ex);
            return 1;
        }
        finally
        {
            try { Directory.Delete(directory, true); } catch { }
        }
    }

    private static void AssertTailLine(string outputPath, int expectedCount, string expectedLine)
    {
        var entries = File.ReadAllLines(outputPath);
        if (entries.Length != expectedCount)
            throw new InvalidOperationException("Expected " + expectedCount + " complete lines; found " + entries.Length);
        var fields = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(entries[expectedCount - 1]);
        if (!string.Equals(fields["event"] as string, "file-line", StringComparison.Ordinal)
            || !string.Equals(fields["line"] as string, expectedLine, StringComparison.Ordinal))
            throw new InvalidOperationException("Tail line content was corrupted.");
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
