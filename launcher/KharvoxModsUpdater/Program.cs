using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace KharvoxModsUpdater;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Any(a => a.Equals("--self-test", StringComparison.OrdinalIgnoreCase)))
        {
            try { UpdateService.RunSelfTest(); return 0; }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new UpdaterWindow());
        return 0;
    }
}

internal sealed class UpdaterWindow : Form
{
    private readonly Label _status = new Label();
    private readonly ProgressBar _progress = new ProgressBar();
    private readonly Button _runApp = new Button();
    private readonly Button _close = new Button();
    private bool _finished;

    internal UpdaterWindow()
    {
        Text = "KHARVOX Mods Updater — Installed " + UpdateService.InstalledBuild;
        ClientSize = new Size(490, 172);
        MinimumSize = MaximumSize = Size;
        MaximizeBox = false;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterScreen;
        // Use the same KHARVOX desktop palette instead of a slightly
        // different charcoal updater frame.
        BackColor = Color.Black;
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9f);
        try { Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

        var heading = new Label
        {
            Text = "KHARVOX MODS — PORTABLE UPDATER",
            Location = new Point(18, 12), Size = new Size(450, 25),
            Font = new Font("Segoe UI", 10f, FontStyle.Bold),
            ForeColor = Color.WhiteSmoke
        };
        Controls.Add(heading);
        _status.Text = "Checking for updates…";
        _status.Location = new Point(18, 48);
        _status.Size = new Size(450, 43);
        _status.ForeColor = Color.Gainsboro;
        Controls.Add(_status);
        _progress.Location = new Point(18, 94);
        _progress.Size = new Size(452, 17);
        _progress.Style = ProgressBarStyle.Marquee;
        Controls.Add(_progress);

        _close.Text = "Close";
        _close.Location = new Point(377, 125);
        _close.Size = new Size(93, 30);
        _close.FlatStyle = FlatStyle.Flat;
        _close.BackColor = Color.FromArgb(43, 43, 47);
        _close.FlatAppearance.BorderColor = Color.DimGray;
        _close.ForeColor = Color.Gainsboro;
        _close.Click += (_, _) => Close();
        Controls.Add(_close);

        _runApp.Text = "Launch existing";
        _runApp.Location = new Point(215, 125);
        _runApp.Size = new Size(150, 30);
        _runApp.FlatStyle = FlatStyle.Flat;
        _runApp.BackColor = Color.FromArgb(43, 43, 47);
        _runApp.FlatAppearance.BorderColor = Color.DimGray;
        _runApp.ForeColor = Color.Gainsboro;
        _runApp.Visible = false;
        _runApp.Click += (_, _) => LaunchMods();
        Controls.Add(_runApp);

        Shown += async (_, _) => await RunUpdate();
    }

    [System.Runtime.InteropServices.DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr window,
        int attribute, ref int value, int size);

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        try
        {
            var dark = 1;
            if (DwmSetWindowAttribute(Handle, 20, ref dark, sizeof(int)) != 0)
                DwmSetWindowAttribute(Handle, 19, ref dark, sizeof(int));
            var border = 0x0038383B;
            DwmSetWindowAttribute(Handle, 34, ref border, sizeof(int));
        }
        catch (DllNotFoundException) { }
        catch (EntryPointNotFoundException) { }
    }

    private async Task RunUpdate()
    {
        try
        {
            var result = await UpdateService.RunAsync(
                message => _status.Text = message,
                prompt => MessageBox.Show(this, prompt, "KHARVOX Mods — Update Found",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Information,
                    MessageBoxDefaultButton.Button1) == DialogResult.Yes);
            _progress.Style = ProgressBarStyle.Continuous;
            _progress.Value = 100;
            if (result == "Update skipped.")
            {
                _status.Text = result;
                _runApp.Text = "Launch existing";
                _runApp.Visible = File.Exists(UpdateService.AppExe);
                return;
            }
            Text = "KHARVOX Mods Updater — Installed " + UpdateService.InstalledBuild;
            _status.Text = result + "\nOpening KHARVOX Mods…";
            await Task.Delay(300);
            LaunchMods();
        }
        catch (Exception error)
        {
            _progress.Style = ProgressBarStyle.Continuous;
            _progress.Value = 0;
            _finished = true;
            _status.Text = "Update not completed: " + error.Message;
            _runApp.Visible = File.Exists(UpdateService.AppExe);
            _runApp.Text = "Launch existing";
        }
    }

    private void LaunchMods()
    {
        if (!File.Exists(UpdateService.AppExe))
        {
            MessageBox.Show(this, "KHARVOX Mods is not installed. Check for updates again.",
                "KHARVOX Mods Updater", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = UpdateService.AppExe,
                WorkingDirectory = UpdateService.InstallFolder,
                UseShellExecute = true
            });
            _finished = true;
            Close();
        }
        catch (Exception error) when (error is System.ComponentModel.Win32Exception ||
            error is InvalidOperationException)
        {
            _finished = true;
            _status.Text = "Update finished, but couldn't start KHARVOX Mods: " + error.Message;
            _runApp.Visible = true;
        }
    }
}
