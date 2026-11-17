using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxLauncher;

// A separate, pre-installed CactusVRStudios KHARVOX is required.
// The source installation is READ-ONLY. We remember its location under our
// own portable UserData and reference its original SFS shader assets.
// No updater, Mod Manager or background source downloader is involved.
internal static class OriginalKharvoxSetup
{
    private const string Executable = "KharvoxLauncher.exe";
    private const string OfficialPage = "https://github.com/CactusVRStudios/KHARVOX";
    internal static string LocationPath =>
        LauncherUserData.PathFor("original-kharvox-location.txt");

    internal static string? Validate(string? launcherPath,
        string? currentCustomExecutable = null)
    {
        if (string.IsNullOrWhiteSpace(launcherPath)) return null;
        try
        {
            var candidate = Path.GetFullPath(launcherPath);
            var self = Path.GetFullPath(currentCustomExecutable ??
                Application.ExecutablePath);
            if (!Path.GetFileName(candidate).Equals(Executable,
                    StringComparison.OrdinalIgnoreCase)
                || candidate.Equals(self, StringComparison.OrdinalIgnoreCase)
                || !File.Exists(candidate)) return null;
            var original = Path.GetDirectoryName(candidate);
            if (string.IsNullOrWhiteSpace(original) ||
                original.TrimEnd(Path.DirectorySeparatorChar).Equals(
                    Path.GetFullPath(AppContext.BaseDirectory).TrimEnd(
                        Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase))
                return null;
            // Require a real KHARVOX installation and its original shader
            // profile, rather than accepting any EXE with the same filename.
            if (!File.Exists(Path.Combine(original, "KharvoxLayer.dll"))
                || !File.Exists(Path.Combine(original, "KharvoxLayer.json"))
                || !VulkanSfs.HasCompleteShaderProfile(original)) return null;
            return original;
        }
        catch (Exception error) when (error is IOException ||
            error is UnauthorizedAccessException ||
            error is ArgumentException || error is NotSupportedException)
        {
            return null;
        }
    }

    internal static string? SavedOriginalDirectory()
    {
        try
        {
            if (!File.Exists(LocationPath)) return null;
            return Validate(File.ReadAllText(LocationPath).Trim());
        }
        catch (Exception error) when (error is IOException ||
            error is UnauthorizedAccessException || error is ArgumentException)
        {
            return null;
        }
    }

    internal static string RequireSelectedOriginal() =>
        SavedOriginalDirectory() ?? throw new InvalidDataException(
            "Original CactusVRStudios KHARVOX is required. Open RIPSHIFT VR and select the original KharvoxLauncher.exe from its separately installed folder.");

    private static void SaveOriginalFile(string originalLauncherFile)
    {
        if (Validate(originalLauncherFile) == null)
            throw new InvalidDataException(
                "That is not a complete original KHARVOX installation with the required 75 SFS shaders.");
        Directory.CreateDirectory(LauncherUserData.Root);
        var target = LocationPath;
        var staging = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(staging, Path.GetFullPath(originalLauncherFile));
            if (File.Exists(target)) File.Replace(staging, target, null);
            else File.Move(staging, target);
        }
        finally { if (File.Exists(staging)) File.Delete(staging); }
    }

    internal static bool EnsureSelected()
    {
        if (SavedOriginalDirectory() != null) return true;

        using var setup = new Form
        {
            Text = "RIPSHIFT VR | First-Time Setup",
            ClientSize = new Size(528, 238),
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false, MinimizeBox = false,
            StartPosition = FormStartPosition.CenterScreen,
            ShowInTaskbar = true,
            BackColor = Color.FromArgb(30, 30, 33),
            ForeColor = Color.WhiteSmoke,
            Font = new Font("Segoe UI", 9f)
        };

        // RIPSHIFT.exe uses the owner's original black-M MrEGS_ emblem.
        try { setup.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); }
        catch (ArgumentException) { }
        setup.Controls.Add(new Label {
            Text = "Locate Original KHARVOX",
            Location = new Point(24, 20), Size = new Size(480, 30),
            Font = new Font("Segoe UI Semibold", 12f, FontStyle.Bold)
        });
        setup.Controls.Add(new Label {
            Text = "This add-on requires an existing installation of KHARVOX by CactusVRStudios.",
            Location = new Point(24, 62), Size = new Size(480, 44)
        });
        setup.Controls.Add(new Label {
            Text = "Select the original KharvoxLauncher.exe to continue. Your existing installation will remain unchanged.",
            Location = new Point(24, 112), Size = new Size(480, 52),
            ForeColor = Color.Gainsboro
        });

        var link = new LinkLabel {
            Text = "Get KHARVOX on GitHub",
            Location = new Point(24, 190), Size = new Size(310, 28),
            LinkColor = Color.LightSkyBlue, ActiveLinkColor = Color.White,
            BackColor = setup.BackColor
        };
        link.LinkClicked += (_, _) => {
            try {
                Process.Start(new ProcessStartInfo {
                    FileName = OfficialPage, UseShellExecute = true
                })?.Dispose();
            }
            catch (Exception error) when (error is Win32Exception ||
                error is InvalidOperationException) { }
        };
        setup.Controls.Add(link);

        var select = new Button {
            Text = "Browse…", Size = new Size(141, 32),
            Location = new Point(361, 184),
            BackColor = Color.FromArgb(43, 43, 47), ForeColor = Color.WhiteSmoke,
            FlatStyle = FlatStyle.Flat
        };
        select.Click += (_, _) => {
            using var chooser = new OpenFileDialog {
                Title = "Select the original CactusVRStudios KharvoxLauncher.exe",
                Filter = "Original KHARVOX launcher (KharvoxLauncher.exe)|KharvoxLauncher.exe",
                FileName = Executable, CheckFileExists = true, Multiselect = false
            };
            if (chooser.ShowDialog(setup) != DialogResult.OK) return;
            if (Validate(chooser.FileName) == null) {
                MessageBox.Show(setup,
                    "That is not the original KHARVOX installation. Select its KharvoxLauncher.exe from the folder containing the original SFS shader profile.",
                    "RIPSHIFT VR — Original KHARVOX required",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            try {
                SaveOriginalFile(chooser.FileName);
                setup.DialogResult = DialogResult.OK;
                setup.Close();
            }
            catch (Exception error) when (error is IOException ||
                error is UnauthorizedAccessException ||
                error is InvalidDataException || error is ArgumentException) {
                MessageBox.Show(setup,
                    "Unable to save the original KHARVOX location: " + error.Message,
                    "RIPSHIFT VR — Setup",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        };
        setup.Controls.Add(select);
        setup.AcceptButton = select;
        return setup.ShowDialog() == DialogResult.OK;
    }

    internal static int RunSelfTest()
    {
        var temp = Path.Combine(Path.GetTempPath(), "kharvox-original-setup-test-" +
            Guid.NewGuid().ToString("N"));
        try
        {
            var original = Path.Combine(temp, "original");
            var custom = Path.Combine(temp, "custom");
            Directory.CreateDirectory(original);
            Directory.CreateDirectory(custom);
            var originalExe = Path.Combine(original, Executable);
            var customExe = Path.Combine(custom, Executable);
            File.WriteAllText(originalExe, "original-test-fixture");
            File.WriteAllText(customExe, "custom-test-fixture");
            File.WriteAllText(Path.Combine(original, "KharvoxLayer.dll"), "dll-fixture");
            File.WriteAllText(Path.Combine(original, "KharvoxLayer.json"), "json-fixture");
            var shaderDir = Path.Combine(original, "sfs-profile");
            Directory.CreateDirectory(shaderDir);
            var shader = new byte[20];
            shader[0]=3; shader[1]=2; shader[2]=35; shader[3]=7;
            for (int i=0; i<75; ++i)
                File.WriteAllBytes(Path.Combine(shaderDir, "test" + i.ToString("D2") + ".spv"), shader);
            if (!string.Equals(Validate(originalExe, customExe), original,
                StringComparison.OrdinalIgnoreCase)) return 1;
            if (Validate(customExe, customExe) != null) return 2;
            File.Delete(Path.Combine(shaderDir, "test00.spv"));
            if (Validate(originalExe, customExe) != null) return 3;
            return 0;
        }
        finally {
            if (Directory.Exists(temp)) Directory.Delete(temp, true);
        }
    }
}
