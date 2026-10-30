using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace KharvoxMods;

// Standalone phase-one shell. Native gameplay features remain owned by the
// original KHARVOX runtime until a separately approved, versioned bridge exists.
internal sealed partial class ModsForm : Form
{
    private readonly AppState _state = AppState.Load();
    private readonly System.Windows.Forms.Timer _dockTimer = new System.Windows.Forms.Timer { Interval = 150 };
    private readonly Label _status = new Label();
    private readonly Button _attachButton = new Button();
    private readonly Button _startButton = new Button();
    private readonly ToolTip _tooltips = new ToolTip
    {
        ShowAlways = true, InitialDelay = 400, ReshowDelay = 120,
        AutoPopDelay = 18000
    };

    private Process? _launcher;
    private IntPtr _launcherWindow;
    private bool _launcherEmbedded;
    private bool _bootstrapped;
    private bool _closingHost;
    private int _fixedDesktopWidth;
    private int _minimumDesktopHeight;
    private bool _resizingToLimits;
    // Keep KHARVOX running from NativeBridge but display its HWND as a
    // real child of our single, normal desktop window.
    private readonly Panel _launcherHost = new Panel
    {
        Dock = DockStyle.Fill, BackColor = Color.Black,
        Margin = Padding.Empty
    };

    // Exact frame and panel palette used by KHARVOX MainForm.
    private static readonly Color DarkPanel = Color.FromArgb(30, 30, 33);
    private static readonly Color Foreground = Color.Gainsboro;
    private const int DesiredModsWidth = 500; // Slimmer mod column in unified window
    // Original Cactus form is 548 × 904 client pixels. Allow room for its
    // scrollable viewport and DPI/frame rounding without showing scrollbars.
    private const int EmbeddedLauncherWidth = 606;
    private const int EmbeddedLauncherHeight = 946;

    private static string EmbeddedBuildId()
    {
        var attr = (System.Reflection.AssemblyInformationalVersionAttribute?)
            Attribute.GetCustomAttribute(typeof(ModsForm).Assembly,
                typeof(System.Reflection.AssemblyInformationalVersionAttribute));
        var raw = attr?.InformationalVersion ?? "";
        var plus = raw.IndexOf('+');
        if (plus >= 0) raw = raw.Substring(0, plus);
        return long.TryParse(raw, out var id) && id > 0 ? id.ToString() : "";
    }

    private static string BuildLabel()
    {
        var embedded = EmbeddedBuildId();
        if (embedded.Length > 0) return "Build #" + embedded.Substring(Math.Max(0, embedded.Length - 4));
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "build-info.txt");
            if (File.Exists(path) && long.TryParse(File.ReadAllText(path).Trim(), out var id)
                && id > 0)
            {
                var text = id.ToString();
                return "Build #" + text.Substring(Math.Max(0, text.Length - 4));
            }
        }
        catch (IOException) { }
        return "Development";
    }

    internal ModsForm()
    {
        // Em spaces spread the two identifiers across the native Windows
        // caption, without adding duplicate heading bars below it.
        Text = "KHARVOX Mods  ·  " + BuildLabel() +
            new string('\u2003', 22) +
            "MODIFIED KHARVOX LAUNCHER  ·  CactusVRStudios / MrEGS";
        FormBorderStyle = FormBorderStyle.Sizable;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(DesiredModsWidth + EmbeddedLauncherWidth,
            EmbeddedLauncherHeight);
        BackColor = Color.Black;
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9F);
        AutoScaleMode = AutoScaleMode.Dpi;
        try { Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

        BuildLayout();
        // Lock width completely while allowing users to expand the height.
        // A MaximumSize height of 0 collapses WinForms to the caption!
        LockHorizontalSize();
        TopMost = false;
        _dockTimer.Tick += (_, _) => SyncEmbeddedLauncher();
        _launcherHost.SizeChanged += (_, _) => ResizeEmbeddedLauncher();
        Shown += async (_, _) =>
        {
            if (_bootstrapped) return;
            _bootstrapped = true;
            // Reapply exact constraints after DPI and native scaling finish.
            LockHorizontalSize();
            await StartUpAsync();
        };
        FormClosing += (_, _) => CloseEmbeddedLauncher();
        FormClosed += (_, _) =>
        {
            _dockTimer.Stop();
            _dockTimer.Dispose();
            _launcher?.Dispose();
        };
    }

    private void LockHorizontalSize()
    {
        if (_resizingToLimits) return;
        _resizingToLimits = true;
        try
        {
            var baseline = Size;
            _fixedDesktopWidth = baseline.Width;
            _minimumDesktopHeight = baseline.Height;
            // A zero MaximumSize height collapses a WinForms window. Use
            // Windows' actual maximum tracking height instead.
            var allowedHeight = Math.Max(baseline.Height,
                SystemInformation.MaxWindowTrackSize.Height);
            MaximumSize = new Size(_fixedDesktopWidth, allowedHeight);
            MinimumSize = new Size(_fixedDesktopWidth, _minimumDesktopHeight);
        }
        finally { _resizingToLimits = false; }
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        if (_resizingToLimits || _fixedDesktopWidth <= 0 ||
            !Visible || WindowState != FormWindowState.Normal ||
            (Width == _fixedDesktopWidth && Height >= _minimumDesktopHeight))
            return;
        _resizingToLimits = true;
        try
        {
            // On-screen resize can move by a Windows nonclient/DPI offset.
            // Always restore the desired width, never limit upward height.
            Size = new Size(_fixedDesktopWidth,
                Math.Max(_minimumDesktopHeight, Height));
        }
        finally { _resizingToLimits = false; }
    }

    private void CloseEmbeddedLauncher()
    {
        if (_closingHost) return;
        _closingHost = true;
        _dockTimer.Stop();
        // Close only our modified launcher. DOOM and stock KHARVOX stay alone.
        try
        {
            if (_launcher != null && !_launcher.HasExited &&
                NativeDock.IsWindow(_launcherWindow))
                NativeDock.RequestClose(_launcherWindow, _launcher.Id);
        }
        catch (InvalidOperationException) { }
        catch (System.ComponentModel.Win32Exception) { }
    }

    // KHARVOX MainForm.CustomModsDocking.cs uses these SAME DWM
    // caption and border settings. Without this, the title bars contrast
    // even though the WinForms panels share the same RGB background.
    [System.Runtime.InteropServices.DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr window,
        int attribute, ref int value, int size);

    private void MatchKharvoxWindowFrame()
    {
        if (!IsHandleCreated) return;
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

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        MatchKharvoxWindowFrame();
    }

    private static Label MakeLabel(string text, int fontSize = 9)
    {
        return new Label
        {
            Text = text,
            Dock = DockStyle.Fill,
            AutoEllipsis = true,
            ForeColor = Foreground,
            Font = new Font("Segoe UI", fontSize),
            TextAlign = ContentAlignment.MiddleLeft
        };
    }

    private static Button MakeButton(string title, int width)
    {
        var button = new Button
        {
            Text = title, Width = width, Height = 29,
            Margin = new Padding(4, 1, 4, 1),
            FlatStyle = FlatStyle.Flat,
            ForeColor = Color.WhiteSmoke,
            BackColor = Color.FromArgb(24, 24, 27)
        };
        button.FlatAppearance.BorderColor = Color.DimGray;
        return button;
    }

    private void SetStatus(string message)
    {
        if (_status.Text != message) _status.Text = message;
    }

    private bool SelectLauncher()
    {
        using (var dialog = new OpenFileDialog
        {
            Title = "Select the existing KHARVOX launcher",
            Filter = "KHARVOX Launcher (KharvoxLauncher.exe)|KharvoxLauncher.exe",
            FileName = "KharvoxLauncher.exe",
            CheckFileExists = true
        })
        {
            if (dialog.ShowDialog(this) != DialogResult.OK) return false;
            if (!LauncherDiscovery.IsOriginalLauncherFile(dialog.FileName))
            {
                MessageBox.Show(this, "Choose the existing KharvoxLauncher.exe.",
                    "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return false;
            }
            _state.KharvoxLauncherPath = Path.GetFullPath(dialog.FileName);
            _state.Save();
            // Original installation location is a reference only.
            return true;
        }
    }

    private async System.Threading.Tasks.Task StartUpAsync()
    {
        // First-run selection finished before the combined window was created.
        _state.ConsentRecorded = true;
        _state.WindowAttachmentApproved = true;
        // No docking or attachment controls in the unified window.

        try
        {
            var id = EmbeddedBuildId();
            if (id.Length > 0)
                File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "build-info.txt"), id);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
        BeginInvoke(new Action(async () => await UpdaterBootstrap.TryRefreshAsync()));

        // Only use the selected original executable as an installation reference.
        // Never run or modify it. Old settings pointing to NativeBridge are
        // migrated by asking for the original location once.
        var original = LauncherDiscovery.Locate(_state.KharvoxLauncherPath);
        if (original == null)
        {
            SetStatus("Original KHARVOX installation not selected.");
            // Program.Main handles first-run setup before constructing or
            // showing this Mods form. A missing path here means it changed
            // after setup (e.g. the original was moved), so fail closed.
            Close();
            return;
        }
        else _state.KharvoxLauncherPath = original;

        _state.Save();
        await StartLauncherAsync();
    }

    private async System.Threading.Tasks.Task StartLauncherAsync()
    {
        if (_runtimePreparing) return;
        if (LauncherDiscovery.Locate(_state.KharvoxLauncherPath) == null)
        {
            SetStatus("Select the original KHARVOX executable first.");
            if (!SelectLauncher()) return;
        }
        try
        {
            _runtimePreparing = true;
            RefreshBridgeState();
            // Ready-to-run releases bundle this already. An existing older
            // install downloads the verified copy once without a restart.
            if (!NativeBridgeInstaller.IsReady())
                await NativeBridgeInstaller.InstallAsync(SetStatus);
            if (!NativeBridgeInstaller.IsReady())
                throw new InvalidDataException("Custom KHARVOX runtime did not pass verification.");

            if (_launcher != null && !_launcher.HasExited)
            {
                SetStatus("Custom KHARVOX already running.");
                _dockTimer.Start();
                return;
            }
            _launcher?.Dispose();
            // Always run OUR bridge-enabled launcher, not the selected original.
            _launcher = LauncherDiscovery.StartOrFind(NativeBridgeInstaller.TargetLauncher);
            _launcherWindow = IntPtr.Zero;
            _launcherEmbedded = false;
            SetStatus("Modified KHARVOX started — loading launcher panel…");
            _dockTimer.Start();
        }
        catch (Exception error)
        {
            SetStatus("Custom KHARVOX couldn't start — original installation untouched.");
            MessageBox.Show(this,
                "Couldn't prepare or start the custom KHARVOX launcher:\n" +
                error.Message + "\n\nCheck the connection or use Connection > Open Custom KHARVOX to retry.",
                "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
        finally
        {
            _runtimePreparing = false;
            if (!IsDisposed) RefreshBridgeState();
        }
    }

    private void ResizeEmbeddedLauncher()
    {
        if (!_launcherEmbedded || _launcherWindow == IntPtr.Zero ||
            !NativeDock.IsWindow(_launcherWindow) || !_launcherHost.IsHandleCreated)
            return;
        NativeDock.ResizeEmbedded(_launcherWindow,
            _launcherHost.ClientSize.Width, _launcherHost.ClientSize.Height);
    }

    private void SyncEmbeddedLauncher()
    {
        if (_launcher == null || _closingHost) return;
        try
        {
            if (_launcher.HasExited)
            {
                _launcherWindow = IntPtr.Zero;
                _launcherEmbedded = false;
                _dockTimer.Stop();
                SetStatus("Launcher closed · reopen from Connection menu.");
                return;
            }
            // A child HWND disappears from Process.MainWindowHandle enumeration.
            // Keep the handle we captured while KHARVOX was still top-level.
            if (_launcherEmbedded && NativeDock.IsWindow(_launcherWindow))
            {
                return;
            }
            _launcher.Refresh();
            var hwnd = _launcher.MainWindowHandle;
            if (hwnd == IntPtr.Zero || !NativeDock.IsWindow(hwnd))
            {
                SetStatus("Waiting for modified KHARVOX launcher…");
                return;
            }
            if (!NativeDock.EmbedWindow(hwnd, _launcherHost.Handle,
                _launcherHost.ClientSize.Width, _launcherHost.ClientSize.Height))
            {
                _dockTimer.Stop();
                SetStatus("Modified KHARVOX could not be embedded.");
                MessageBox.Show(this,
                    "Windows could not embed the modified KHARVOX launcher. " +
                    "Check DPI compatibility or retry from Connection.",
                    "KHARVOX combined launcher", MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }
            _launcherWindow = hwnd;
            _launcherEmbedded = true;
            TopMost = false;
            SetStatus("Connected · KHARVOX embedded.");
        }
        catch (Exception error) when (error is InvalidOperationException ||
            error is System.ComponentModel.Win32Exception)
        {
            SetStatus("Modified KHARVOX unavailable.");
        }
    }
}
