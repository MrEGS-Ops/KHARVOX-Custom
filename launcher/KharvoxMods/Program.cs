using System;
using System.IO;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxMods;

internal static class Program
{
    [DllImport("user32.dll")] private static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    private static void FocusExistingModsWindow()
    {
        using (var self = Process.GetCurrentProcess())
        {
            foreach (var process in Process.GetProcessesByName("KharvoxVRModManager"))
            {
                using (process)
                {
                    if (process.Id == self.Id) continue;
                    var window = process.MainWindowHandle;
                    if (window == IntPtr.Zero) continue;
                    ShowWindow(window, 9); // SW_RESTORE
                    SetForegroundWindow(window);
                    return;
                }
            }
        }
    }

    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Any(a => string.Equals(a, "--self-test-ui", StringComparison.OrdinalIgnoreCase)))
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            try
            {
                ModsForm.RunUiSelfTest();
                Console.WriteLine("Original two-column UI and 22 mod rows verified.");
                return 0;
            }
            catch (Exception error)
            {
                Console.Error.WriteLine(error);
                return 1;
            }
        }
        if (args.Any(a => string.Equals(a, "--self-test", StringComparison.OrdinalIgnoreCase)))
            return SelfTest();

        using (var singleInstance = new Mutex(true, @"Local\KHARVOXMods-Standalone-OnlyOneWindow", out var newInstance))
        {
            if (!newInstance)
            {
                FocusExistingModsWindow();
                return 0;
            }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            // The first-run path picker must finish before ModsForm exists,
            // so users never see a blank or inaccessible Mods window behind it.
            if (!OriginalLauncherSetup.EnsureSelected()) return 0;
            Application.Run(new ModsForm());
            return 0;
        }
    }

    private static int SelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(), "kharvox-mods-selftest-" +
            Guid.NewGuid().ToString("N"));
        try
        {
            StandaloneModPreferences.SelfTest();
            if (!string.Equals(AppState.DataDirectory,
                    Path.GetFullPath(AppContext.BaseDirectory), StringComparison.OrdinalIgnoreCase) ||
                !string.Equals(AppState.DefaultPath,
                    Path.Combine(AppState.DataDirectory, "settings.json"),
                    StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Portable storage must be beside KharvoxVRModManager.exe.");

            Directory.CreateDirectory(root);
            var path = Path.Combine(root, "settings.json");
            var clean = AppState.Load(path);
            if (clean.ConsentRecorded || clean.WindowAttachmentApproved)
                throw new InvalidDataException("First run must not assume attachment consent.");
            clean.KharvoxLauncherPath = Path.Combine(root, "KharvoxLauncher.exe");
            clean.Save(path);
            if (!File.Exists(path) ||
                !Directory.Exists(Path.Combine(root, "Logs")) ||
                !Directory.Exists(Path.Combine(root, "Backups")) ||
                Directory.Exists(Path.Combine(root, "Mods")))
                throw new InvalidDataException("All portable folders must be beside settings.json.");
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
            if (LauncherDiscovery.IsLauncherFile(Path.Combine(root, "KharvoxVRModManager.exe")))
                throw new InvalidDataException("Non-KHARVOX executables must be rejected.");
            if (LauncherDiscovery.IsOriginalLauncherFile(NativeBridgeInstaller.TargetLauncher))
                throw new InvalidDataException("The custom runtime must never be selected as original.");
            Console.WriteLine("KHARVOX VR Mods standalone tests passed: no DOOM user-folder creation, portable storage, consent and fail-closed settings.");
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
