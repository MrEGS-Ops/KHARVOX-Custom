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
    private static readonly Color PanelColor = Color.FromArgb(30, 30, 33);
    private static readonly Color ButtonColor = Color.FromArgb(43, 43, 47);
    private static readonly Color MutedColor = Color.FromArgb(170, 170, 176);

    private readonly Label _installedBuild = new Label();
    private readonly Label _availableBuild = new Label();
    private readonly Label _status = new Label();
    private readonly ProgressBar _progress = new ProgressBar();
    private readonly Button _update = new Button();
    private readonly Button _notNow = new Button();
    private readonly Button _launch = new Button();
    private readonly Button _close = new Button();
    private TaskCompletionSource<bool>? _decision;

    internal UpdaterWindow()
    {
        Text = "KHARVOX Mods Updater";
        ClientSize = new Size(520, 282);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.Black;
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9f);
        try { Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath); }
        catch { /* The updater is usable without an icon. */ }

        Controls.Add(new Label
        {
            Text = "KHARVOX MODS",
            Location = new Point(18, 13), Size = new Size(484, 19),
            Font = new Font("Segoe UI", 9f, FontStyle.Bold)
        });
        Controls.Add(new Label
        {
            Text = "Update manager",
            Location = new Point(18, 36), Size = new Size(484, 26),
            Font = new Font("Segoe UI", 12f, FontStyle.Bold)
        });
        Controls.Add(BuildCard("INSTALLED BUILD", UpdateService.InstalledBuild,
            _installedBuild, 18));
        Controls.Add(new Label
        {
            Text = "→", Location = new Point(246, 99), Size = new Size(28, 30),
            TextAlign = ContentAlignment.MiddleCenter,
            ForeColor = MutedColor, Font = new Font("Segoe UI", 13f)
        });
        Controls.Add(BuildCard("AVAILABLE BUILD", "Checking…", _availableBuild, 274));

        _status.Text = "Checking for updates…";
        _status.SetBounds(18, 158, 484, 45);
        _status.ForeColor = Color.Gainsboro;
        Controls.Add(_status);

        _progress.SetBounds(18, 206, 484, 13);
        _progress.Style = ProgressBarStyle.Marquee;
        _progress.MarqueeAnimationSpeed = 22;
        Controls.Add(_progress);

        StyleButton(_update, "Update now", 340, 231, 162, true);
        _update.Visible = false;
        _update.Click += (_, _) => Decide(true);

        StyleButton(_notNow, "Not now", 220, 231, 110, false);
        _notNow.Visible = false;
        _notNow.Click += (_, _) => Decide(false);

        StyleButton(_launch, "Launch Mods", 340, 231, 162, true);
        _launch.Visible = false;
        _launch.Click += (_, _) => LaunchMods();

        StyleButton(_close, "Close", 399, 231, 103, false);
        _close.Click += (_, _) => Close();

        FormClosing += (_, _) => _decision?.TrySetResult(false);
        Shown += async (_, _) => await RunUpdateAsync();
    }

    private static Panel BuildCard(string caption, string value, Label output, int left)
    {
        var panel = new Panel
        {
            Location = new Point(left, 72), Size = new Size(226, 76),
            BackColor = PanelColor
        };
        panel.Controls.Add(new Label
        {
            Text = caption, Location = new Point(12, 9), Size = new Size(202, 18),
            ForeColor = MutedColor, Font = new Font("Segoe UI", 8f, FontStyle.Bold)
        });
        output.Text = value;
        output.Location = new Point(12, 32);
        output.Size = new Size(202, 35);
        output.Font = new Font("Segoe UI", 15f, FontStyle.Bold);
        output.ForeColor = Color.WhiteSmoke;
        output.AutoEllipsis = true;
        panel.Controls.Add(output);
        return panel;
    }

    private void StyleButton(Button button, string label, int x, int y, int width, bool primary)
    {
        button.Text = label;
        button.SetBounds(x, y, width, 33);
        button.FlatStyle = FlatStyle.Flat;
        button.BackColor = primary ? Color.FromArgb(71, 71, 77) : ButtonColor;
        button.ForeColor = Color.WhiteSmoke;
        button.FlatAppearance.BorderColor = Color.DimGray;
        Controls.Add(button);
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

    private Task<bool> AskForUpdateAsync()
    {
        _status.Text = "New build available. Install it?";
        _progress.Visible = false;
        _close.Visible = false;
        _launch.Visible = false;
        _update.Visible = true;
        _notNow.Visible = true;
        _decision = new TaskCompletionSource<bool>();
        return _decision.Task;
    }

    private void Decide(bool install)
    {
        var decision = _decision;
        _decision = null;
        _update.Visible = false;
        _notNow.Visible = false;
        _close.Visible = true;
        if (install)
        {
            _status.Text = "Preparing update…";
            _progress.Visible = true;
            _progress.Style = ProgressBarStyle.Marquee;
        }
        decision?.TrySetResult(install);
    }

    private async Task RunUpdateAsync()
    {
        try
        {
            var result = await UpdateService.RunAsync(
                message => { if (!IsDisposed) _status.Text = message; },
                build => { if (!IsDisposed) _availableBuild.Text = build; },
                AskForUpdateAsync);
            if (IsDisposed) return;
            _installedBuild.Text = UpdateService.InstalledBuild;
            _progress.Visible = false;
            _status.Text = result;
            _update.Visible = false;
            _notNow.Visible = false;
            _close.Visible = true;
            _launch.Visible = File.Exists(UpdateService.AppExe);
        }
        catch (Exception error)
        {
            if (IsDisposed) return;
            _progress.Visible = false;
            _status.Text = "Update not completed: " + error.Message;
            _update.Visible = false;
            _notNow.Visible = false;
            _close.Visible = true;
            _launch.Visible = File.Exists(UpdateService.AppExe);
        }
    }

    private void LaunchMods()
    {
        if (!File.Exists(UpdateService.AppExe))
        {
            _status.Text = "KHARVOX Mods is not installed.";
            return;
        }
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = UpdateService.AppExe,
                WorkingDirectory = UpdateService.InstallFolder,
                UseShellExecute = true
            })?.Dispose();
            Close();
        }
        catch (Exception error) when (error is System.ComponentModel.Win32Exception ||
            error is InvalidOperationException)
        {
            _status.Text = "Could not launch KHARVOX Mods: " + error.Message;
        }
    }
}
