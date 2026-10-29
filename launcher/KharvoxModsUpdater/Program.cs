using System;
using System.ComponentModel;
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
        if (args.Any(a => a.Equals("--self-test-ui", StringComparison.OrdinalIgnoreCase)))
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            using (var form = new UpdaterWindow())
            {
                if (form.ClientSize.Width > 500 || !form.Controls.OfType<Panel>()
                    .Any(p => p.Controls.OfType<Label>()
                        .Any(l => l.Text == "INSTALLED")))
                    return 1;
                return 0;
            }
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new UpdaterWindow());
        return 0;
    }
}

internal sealed class UpdaterWindow : Form
{
    private static readonly Color Surface = Color.FromArgb(30, 30, 33);
    private static readonly Color Muted = Color.FromArgb(158, 160, 167);
    private static readonly Color Accent = Color.FromArgb(48, 186, 149);
    private static readonly Color ControlBackground = Color.FromArgb(45, 45, 49);

    private readonly Label _installed = new Label();
    private readonly Label _available = new Label();
    private readonly Label _status = new Label();
    private readonly Label _subheading = new Label();
    private readonly ProgressBar _progress = new ProgressBar();
    private readonly Button _update = new Button();
    private readonly Button _skip = new Button();
    private readonly Button _launch = new Button();
    private readonly Button _close = new Button();
    private readonly ToolTip _tips = new ToolTip();
    private TaskCompletionSource<bool>? _decision;

    internal UpdaterWindow()
    {
        Text = "KHARVOX Mods — Updater";
        ClientSize = new Size(476, 270);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.Black;
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9f);
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); }
        catch { /* Unavailable icons should never block installation. */ }

        Controls.Add(new Label
        {
            Text = "KHARVOX MODS", Location = new Point(20, 14),
            Size = new Size(225, 19), Font = new Font("Segoe UI", 9f, FontStyle.Bold)
        });
        Controls.Add(new Label
        {
            Text = "UPDATE MANAGER", Location = new Point(20, 35),
            Size = new Size(270, 26), Font = new Font("Segoe UI", 12f, FontStyle.Bold)
        });
        Controls.Add(new Panel
        {
            BackColor = Accent, Location = new Point(20, 66), Size = new Size(436, 2)
        });

        var installedFull = UpdateService.InstalledBuild;
        Controls.Add(BuildVersionCard(20, "INSTALLED",
            UpdateService.ShortBuildLabel(installedFull), _installed));
        Controls.Add(BuildVersionCard(252, "LATEST", "Checking…", _available));
        _tips.SetToolTip(_installed, "Full installed build ID: " + installedFull);
        Controls.Add(new Label
        {
            Text = "→", Location = new Point(224, 103), Size = new Size(28, 31),
            Font = new Font("Segoe UI", 16f, FontStyle.Bold),
            ForeColor = Muted, TextAlign = ContentAlignment.MiddleCenter
        });

        _status.Text = "Checking GitHub for updates…";
        _status.SetBounds(20, 164, 436, 29);
        _status.AutoEllipsis = true;
        Controls.Add(_status);

        _subheading.Text = "Only KHARVOX Mods files are updated. Your settings are kept.";
        _subheading.SetBounds(20, 192, 436, 19);
        _subheading.ForeColor = Muted;
        _subheading.Font = new Font("Segoe UI", 8f);
        Controls.Add(_subheading);

        _progress.SetBounds(20, 215, 436, 9);
        _progress.Style = ProgressBarStyle.Marquee;
        _progress.MarqueeAnimationSpeed = 24;
        Controls.Add(_progress);

        SetupButton(_update, "Install update", 318, true);
        _update.Click += (_, _) => ChooseUpdate(true);
        _update.Visible = false;
        SetupButton(_skip, "Not now", 222, false);
        _skip.Click += (_, _) => ChooseUpdate(false);
        _skip.Visible = false;

        SetupButton(_launch, "Launch Mods", 318, true);
        _launch.Click += (_, _) => LaunchMods();
        _launch.Visible = false;

        SetupButton(_close, "Close", 358, false);
        _close.Width = 98;
        _close.Click += (_, _) => Close();

        FormClosing += (_, _) => _decision?.TrySetResult(false);
        Shown += async (_, _) => await RunUpdateAsync();
    }

    private static Panel BuildVersionCard(int x, string label, string value, Label output)
    {
        var card = new Panel
        {
            BackColor = Surface, Location = new Point(x, 79),
            Size = new Size(204, 72)
        };
        card.Controls.Add(new Label
        {
            Text = label, ForeColor = Muted,
            Location = new Point(12, 8), Size = new Size(180, 17),
            Font = new Font("Segoe UI", 8f, FontStyle.Bold)
        });
        output.Text = value;
        output.ForeColor = Color.WhiteSmoke;
        output.Font = new Font("Segoe UI", 17f, FontStyle.Bold);
        output.Location = new Point(12, 26);
        output.Size = new Size(180, 40);
        output.AutoEllipsis = true;
        card.Controls.Add(output);
        return card;
    }

    private void SetupButton(Button b, string text, int x, bool primary)
    {
        b.Text = text;
        b.SetBounds(x, 235, primary ? 138 : 88, 28);
        b.FlatStyle = FlatStyle.Flat;
        b.ForeColor = Color.WhiteSmoke;
        b.BackColor = primary ? Color.FromArgb(30, 103, 87) : ControlBackground;
        b.FlatAppearance.BorderColor = primary ? Accent : Color.DimGray;
        Controls.Add(b);
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
        _status.Text = "An update is ready to install.";
        _subheading.Text = "Your settings and existing Mods folder will be preserved.";
        _progress.Visible = false;
        _close.Visible = false;
        _update.Visible = true;
        _skip.Visible = true;
        _launch.Visible = false;
        _decision = new TaskCompletionSource<bool>();
        return _decision.Task;
    }

    private void ChooseUpdate(bool install)
    {
        var decision = _decision;
        _decision = null;
        _update.Visible = false;
        _skip.Visible = false;
        _close.Visible = true;
        if (install)
        {
            _status.Text = "Preparing files…";
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
                fullBuild =>
                {
                    if (IsDisposed) return;
                    _available.Text = UpdateService.ShortBuildLabel(fullBuild);
                    _tips.SetToolTip(_available, "Full latest build ID: " + fullBuild);
                }, AskForUpdateAsync);
            if (IsDisposed) return;
            _installed.Text = UpdateService.ShortBuildLabel(UpdateService.InstalledBuild);
            _tips.SetToolTip(_installed, "Full installed build ID: " +
                UpdateService.InstalledBuild);
            _progress.Visible = false;
            _status.Text = result;
            _subheading.Text = result == "KHARVOX Mods updated successfully."
                ? "Launch Mods to refresh the updater executable as well."
                : "You can launch your installed Mods application.";
            _close.Visible = true;
            _launch.Visible = File.Exists(UpdateService.AppExe);
            _update.Visible = false;
            _skip.Visible = false;
        }
        catch (Exception ex)
        {
            if (IsDisposed) return;
            _progress.Visible = false;
            _status.Text = "Update couldn't finish: " + ex.Message;
            _subheading.Text = "No user settings or Mods folders were removed.";
            _close.Visible = true;
            _launch.Visible = File.Exists(UpdateService.AppExe);
            _update.Visible = false;
            _skip.Visible = false;
        }
    }

    private void LaunchMods()
    {
        if (!File.Exists(UpdateService.AppExe))
        {
            _status.Text = "KHARVOX Mods isn't installed.";
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
        catch (Exception ex) when (ex is Win32Exception || ex is InvalidOperationException)
        {
            _status.Text = "Could not launch Mods: " + ex.Message;
        }
    }
}
