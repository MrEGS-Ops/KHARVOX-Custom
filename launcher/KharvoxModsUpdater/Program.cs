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
        Text = "KHARVOX Mods Updater";
        ClientSize = new Size(490, 172);
        MinimumSize = MaximumSize = Size;
        MaximizeBox = false;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(24, 24, 27);
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
        _close.ForeColor = Color.Gainsboro;
        _close.Click += (_, _) => Close();
        Controls.Add(_close);

        _runApp.Text = "Launch existing";
        _runApp.Location = new Point(215, 125);
        _runApp.Size = new Size(150, 30);
        _runApp.FlatStyle = FlatStyle.Flat;
        _runApp.ForeColor = Color.Gainsboro;
        _runApp.Visible = false;
        _runApp.Click += (_, _) => LaunchMods();
        Controls.Add(_runApp);

        Shown += async (_, _) => await RunUpdate();
    }

    private async Task RunUpdate()
    {
        try
        {
            var result = await UpdateService.RunAsync(message => _status.Text = message);
            _progress.Style = ProgressBarStyle.Continuous;
            _progress.Value = 100;
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
