using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace KharvoxMods;

// First-run setup is deliberately shown before ModsForm is instantiated.
// The selected CactusVRStudios installation is a reference only; we never
// overwrite or launch the original executable. The separate modified VR
// launcher is supplied by NativeBridgeInstaller.
internal static class OriginalLauncherSetup
{
    internal static bool NeedsSelection(AppState state) =>
        LauncherDiscovery.Locate(state.KharvoxLauncherPath) == null;

    internal static bool EnsureSelected()
    {
        var state = AppState.Load();
        var found = LauncherDiscovery.Locate(state.KharvoxLauncherPath);
        if (found != null)
        {
            if (!string.Equals(state.KharvoxLauncherPath, found,
                StringComparison.OrdinalIgnoreCase))
            {
                state.KharvoxLauncherPath = found;
                state.Save();
            }
            return true;
        }

        using (var setup = new Form
        {
            Text = "KHARVOX VR Mod Manager — Setup",
            ClientSize = new Size(478, 161),
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false, MinimizeBox = false,
            StartPosition = FormStartPosition.CenterScreen,
            ShowInTaskbar = true,
            BackColor = Color.FromArgb(30, 30, 33),
            ForeColor = Color.WhiteSmoke,
            Font = new Font("Segoe UI", 9f)
        })
        {
            setup.Controls.Add(new Label
            {
                Text = "Select your original KHARVOX installation.",
                Location = new Point(18, 65), Size = new Size(440, 24),
                Font = new Font("Segoe UI", 10f, FontStyle.Bold),
                ForeColor = Color.WhiteSmoke
            });
            setup.Controls.Add(new Label
            {
                Text = "An Unofficial VR Mod Manager for KHARVOX.",
                Location = new Point(18, 16), Size = new Size(440, 24),
                Font = new Font("Segoe UI", 10f, FontStyle.Bold),
                ForeColor = Color.WhiteSmoke
            });
            setup.Controls.Add(new Label
            {
                Text = "Original by CactusVRStudios; existing files remain unchanged.",
                Location = new Point(18, 40), Size = new Size(440, 18),
                ForeColor = Color.FromArgb(174, 174, 180)
            });
            var link = new LinkLabel
            {
                Text = "Don't have KHARVOX? Click here",
                Location = new Point(18, 119), Size = new Size(285, 30),
                BackColor = setup.BackColor,
                LinkColor = Color.LightSkyBlue,
                ActiveLinkColor = Color.White,
                TextAlign = ContentAlignment.MiddleLeft
            };
            link.LinkClicked += (_, _) =>
            {
                try
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = "https://github.com/CactusVRStudios/KHARVOX",
                        UseShellExecute = true
                    })?.Dispose();
                }
                catch (Win32Exception) { }
            };
            setup.Controls.Add(link);

            var ok = new Button
            {
                Text = "OK", Size = new Size(100, 30),
                Location = new Point(358, 116),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(43, 43, 47),
                ForeColor = Color.WhiteSmoke
            };
            ok.Click += (_, _) =>
            {
                using (var chooser = new OpenFileDialog
                {
                    Title = "Select original CactusVRStudios KharvoxLauncher.exe",
                    Filter = "KHARVOX launcher (KharvoxLauncher.exe)|KharvoxLauncher.exe",
                    FileName = "KharvoxLauncher.exe",
                    CheckFileExists = true,
                    Multiselect = false
                })
                {
                    // A cancelled or invalid file picker leaves this setup
                    // dialog open. Mods never appears until a valid selection.
                    if (chooser.ShowDialog(setup) != DialogResult.OK) return;
                    if (!LauncherDiscovery.IsOriginalLauncherFile(chooser.FileName))
                    {
                        MessageBox.Show(setup,
                            "Select the original KharvoxLauncher.exe, not the modified VR launcher.",
                            "KHARVOX VR Mod Manager Setup",
                            MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                    try
                    {
                        state.KharvoxLauncherPath = Path.GetFullPath(chooser.FileName);
                        state.Save();
                        setup.DialogResult = DialogResult.OK;
                        setup.Close();
                    }
                    catch (Exception ex) when (ex is IOException ||
                        ex is UnauthorizedAccessException ||
                        ex is ArgumentException)
                    {
                        MessageBox.Show(setup,
                            "Could not save the KHARVOX location: " + ex.Message,
                            "KHARVOX VR Mod Manager Setup",
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            };
            if (setup.ClientSize.Height > 170 || link.Top != ok.Top + 3 ||
                link.Bottom > setup.ClientSize.Height ||
                ok.Bottom > setup.ClientSize.Height)
                throw new InvalidDataException("Compact first-run setup layout regressed.");
            setup.AcceptButton = ok;
            setup.Controls.Add(ok);
            // X exits the program. Do not create the main Mods window.
            return setup.ShowDialog() == DialogResult.OK;
        }
    }
}
