using System.Diagnostics;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

internal sealed class KharvoxSupervisorSession : IDisposable
{
    private readonly Process process;
    private readonly string generatedConfigPath;
    private int disposed;

    private KharvoxSupervisorSession(Process process, string generatedConfigPath)
    {
        this.process = process;
        this.generatedConfigPath = generatedConfigPath;
    }

    internal static KharvoxSupervisorSession? TryStart(
        CustomModSettings mods, string runtimeDirectory, Action<string>? statusUpdate)
    {
        if (!mods.SupervisorRequired) return null;

        var executable = Path.Combine(runtimeDirectory, "KharvoxSupervisor.exe");
        if (!File.Exists(executable))
        {
            statusUpdate?.Invoke("Supervisor-assisted mod enabled, but KharvoxSupervisor.exe is missing.");
            return null;
        }

        var overrideConfig = Path.Combine(runtimeDirectory, "KharvoxSupervisor.override.json");
        var generatedConfig = Path.Combine(runtimeDirectory, "KharvoxSupervisor.active.json");
        var config = File.Exists(overrideConfig)
            ? overrideConfig
            : WriteGeneratedConfig(mods, generatedConfig);

        try
        {
            var start = new ProcessStartInfo(executable)
            {
                UseShellExecute = false,
                WorkingDirectory = runtimeDirectory,
                CreateNoWindow = true,
                Arguments = "--config \"" + config.Replace("\"", "\\\"") + "\""
            };
            var process = Process.Start(start);
            if (process is null)
            {
                statusUpdate?.Invoke("KHARVOX Supervisor could not be started.");
                return null;
            }

            statusUpdate?.Invoke(File.Exists(overrideConfig)
                ? "KHARVOX Supervisor started with override config."
                : "KHARVOX Supervisor started.");
            return new KharvoxSupervisorSession(
                process, File.Exists(overrideConfig) ? string.Empty : generatedConfig);
        }
        catch (Exception ex)
        {
            statusUpdate?.Invoke("KHARVOX Supervisor start failed: " + ex.GetType().Name);
            return null;
        }
    }

    private static string WriteGeneratedConfig(CustomModSettings mods, string path)
    {
        var files = new List<object>
        {
            new { Name = "KHARVOX", Path = "logs/KHARVOX.log", Mode = "tail", Optional = true }
        };
        if (mods.RevengeDemon)
            files.Add(new
            {
                Name = "RevengeDemonBridge",
                Path = "supervisor-bridge/revenge-demon.json",
                Mode = "snapshot",
                Optional = true,
                PersistPath = "supervisor-state/revenge-demon-last.json"
            });
        if (mods.PhysicalChainsawGestures)
            files.Add(new
            {
                Name = "ChainsawGestureBridge",
                Path = "supervisor-bridge/chainsaw-gesture.json",
                Mode = "snapshot",
                Optional = true
            });

        var value = new
        {
            Schema = 1,
            Enabled = true,
            ProcessName = "DOOMx64vk",
            PollIntervalMs = mods.PhysicalChainsawGestures ? 25 : 50,
            OutputFile = "logs/KHARVOX-Supervisor-active.ndjson",
            IncludeProcessLifecycle = true,
            IncludeProcessMetrics = false,
            MetricsIntervalMs = 1000,
            Notes = "Generated from launcher custom-mod selections. KharvoxSupervisor.override.json takes precedence when present.",
            Files = files
        };
        var serializer = new JavaScriptSerializer();
        File.WriteAllText(path, serializer.Serialize(value));
        return path;
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, 1) != 0) return;
        try
        {
            if (!process.HasExited)
            {
                process.Kill();
                process.WaitForExit(1500);
            }
        }
        catch { }
        try { process.Dispose(); } catch { }
        if (!string.IsNullOrWhiteSpace(generatedConfigPath))
        {
            try { File.Delete(generatedConfigPath); } catch { }
        }
    }
}
