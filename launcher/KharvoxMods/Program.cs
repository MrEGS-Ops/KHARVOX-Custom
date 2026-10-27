using System;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxMods;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Any(a => string.Equals(a, "--self-test", StringComparison.OrdinalIgnoreCase)))
            return SelfTest();

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new ModsForm());
        return 0;
    }

    private static int SelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(), "kharvox-mods-selftest-" +
            Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            var path = Path.Combine(root, "settings.json");
            var clean = AppState.Load(path);
            if (clean.ConsentRecorded || clean.WindowAttachmentApproved)
                throw new InvalidDataException("First run must not assume attachment consent.");
            clean.KharvoxLauncherPath = Path.Combine(root, "KharvoxLauncher.exe");
            clean.Save(path);
            var reloaded = AppState.Load(path);
            if (reloaded.WindowAttachmentApproved || reloaded.ConsentRecorded)
                throw new InvalidDataException("Saving a path must never enable bridge attachment.");
            reloaded.ConsentRecorded = true;
            reloaded.WindowAttachmentApproved = true;
            reloaded.Save(path);
            if (!AppState.Load(path).WindowAttachmentApproved)
                throw new InvalidDataException("Explicit attachment consent must persist.");
            File.WriteAllText(path, "invalid JSON {");
            if (AppState.Load(path).WindowAttachmentApproved)
                throw new InvalidDataException("Malformed state must fail closed.");
            if (LauncherDiscovery.IsLauncherFile(Path.Combine(root, "KharvoxMods.exe")))
                throw new InvalidDataException("Non-KHARVOX executables must be rejected.");
            Console.WriteLine("KHARVOX Mods standalone tests passed: consent, isolation and fail-closed settings.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
