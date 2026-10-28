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
    private Rectangle _lastLauncherBounds;
    private Rectangle _lastModsBounds;
    private bool _syncingWindowState;
    private bool _lastLauncherMinimized;
    private bool _bootstrapped;
    private IntPtr _lastDockForeground;
    private IntPtr _ownerLauncher;

    private static readonly Color DarkPanel = Color.FromArgb(30, 30, 33);
    private static readonly Color Foreground = Color.Gainsboro;
    private const int DesiredModsWidth = 1180;

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
        if (embedded.Length > 0) return "Build #" + embedded;
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "build-info.txt");
            if (File.Exists(path) && long.TryParse(File.ReadAllText(path).Trim(), out var id)
                && id > 0) return "Build #" + id;
        }
        catch (IOException) { }
        return "Development";
    }

    internal ModsForm()
    {
        Text = "KHARVOX Custom Mods — " + BuildLabel();
        ClientSize = new Size(DesiredModsWidth, 600);
        MinimumSize = new Size(890, 530);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.Black;
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9F);
        AutoScaleMode = AutoScaleMode.Dpi;
        try { Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

        BuildLayout();
        _dockTimer.Tick += (_, _) => SyncDock();
        Shown += (_, _) =>
        {
            if (_bootstrapped) return;
            _bootstrapped = true;
            BeginInvoke(new Action(StartUp));
        };
        FormClosed += (_, _) =>
        {
            if (_ownerLauncher != IntPtr.Zero && IsHandleCreated)
                NativeDock.SetOwner(Handle, IntPtr.Zero);
            _dockTimer.Stop();
            _dockTimer.Dispose();
            _launcher?.Dispose(); // Never terminate the independent KHARVOX process.
        };
        SizeChanged += (_, _) => SyncMinimizeFromMods();
        Activated += (_, _) => SyncCompanionFocus();
        UpdateAttachmentButton();
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
            if (!LauncherDiscovery.IsLauncherFile(dialog.FileName))
            {
                MessageBox.Show(this, "Choose the existing KharvoxLauncher.exe.",
                    "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return false;
            }
            _state.KharvoxLauncherPath = Path.GetFullPath(dialog.FileName);
            _state.Save();
            _dockTimer.Stop();
            _launcher?.Dispose();
            _launcher = null;
            if (_ownerLauncher != IntPtr.Zero && IsHandleCreated)
                NativeDock.SetOwner(Handle, IntPtr.Zero);
            _ownerLauncher = IntPtr.Zero;
            _launcherWindow = IntPtr.Zero;
            _lastDockForeground = IntPtr.Zero;
            _lastLauncherBounds = Rectangle.Empty;
            _lastModsBounds = Rectangle.Empty;
            StartLauncher();
            return true;
        }
    }

    private void StartUp()
    {
        // A first-run refusal is an exit, not a saved "run undocked" choice.
        // Previously declined installs also get this gate on their next launch.
        if (!_state.ConsentRecorded || !_state.WindowAttachmentApproved)
        {
            var choice = MessageBox.Show(this,
                "Attach KHARVOX Mods to KHARVOX?\n\nYes: Attach   No: Exit",
                "KHARVOX Mods — First install", MessageBoxButtons.YesNo,
                MessageBoxIcon.Question, MessageBoxDefaultButton.Button1);
            if (choice != DialogResult.Yes)
            {
                Close();
                return;
            }
            _state.ConsentRecorded = true;
            _state.WindowAttachmentApproved = true;
            _state.Save();
        }
        UpdateAttachmentButton();
        // The release ZIP stays compatible with the oldest updater, which
        // accepts only the original files. Stamp the build ID locally on the
        // first successful Mods launch instead of adding a new ZIP entry.
        try
        {
            var id = EmbeddedBuildId();
            if (id.Length > 0)
                File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "build-info.txt"), id);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
        // The old updater can only update Mods.exe while it is running.
        // Once this refreshed app opens, replace the updater EXE after its
        // process exits. Never pause launcher/game startup for this step.
        BeginInvoke(new Action(async () => await UpdaterBootstrap.TryRefreshAsync()));
        var discovered = LauncherDiscovery.Locate(_state.KharvoxLauncherPath);
        if (discovered == null)
        {
            SetStatus("Choose the original KHARVOX launcher to continue.");
            SelectLauncher();
            return;
        }
        _state.KharvoxLauncherPath = discovered;
        _state.Save();
        StartLauncher();
    }

    private async System.Threading.Tasks.Task InstallNativeBridge()
    {
        var accepted = MessageBox.Show(this,
            "Install mod-enabled KHARVOX beside this app?\\n\\n" +
            "Your original KHARVOX installation will not be changed.",
            "Install KHARVOX Native Bridge", MessageBoxButtons.YesNo,
            MessageBoxIcon.Question, MessageBoxDefaultButton.Button2);
        if (accepted != DialogResult.Yes) return;

        try
        {
            SetStatus("Installing verified native bridge in local folder…");
            var message = await NativeBridgeInstaller.InstallAsync(SetStatus);
            _state.KharvoxLauncherPath = NativeBridgeInstaller.TargetLauncher;
            _state.Save();
            RefreshBridgeState();
            SetStatus("Native bridge installed. Five confirmed mod selections available.");
            if (_launcher != null && !_launcher.HasExited)
            {
                MessageBox.Show(this, message +
                    "\\n\\nClose the currently running KHARVOX launcher, then use " +
                    "Connection → Start / Find KHARVOX to switch to the new mod-enabled copy.",
                    "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            else
            {
                MessageBox.Show(this, message, "KHARVOX Mods",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                StartLauncher();
            }
        }
        catch (Exception error)
        {
            SetStatus("Bridge installation failed — original KHARVOX untouched.");
            MessageBox.Show(this,
                "Could not install the optional native bridge:\\n" + error.Message,
                "KHARVOX Mods", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void StartLauncher()
    {
        var launcherPath = LauncherDiscovery.Locate(_state.KharvoxLauncherPath);
        if (launcherPath == null)
        {
            SetStatus("KHARVOX not found — select its EXE.");
            return;
        }
        try
        {
            if (_launcher != null && !_launcher.HasExited)
            {
                SetStatus("KHARVOX already running; attaching window only.");
                _dockTimer.Start();
                return;
            }
            _launcher?.Dispose();
            _launcher = LauncherDiscovery.StartOrFind(launcherPath);
            _state.KharvoxLauncherPath = launcherPath;
            _state.Save();
            _launcherWindow = IntPtr.Zero;
            _lastDockForeground = IntPtr.Zero;
            _lastLauncherBounds = Rectangle.Empty;
            _lastModsBounds = Rectangle.Empty;
            SetStatus("KHARVOX started — waiting for launcher window…");
            _dockTimer.Start();
        }
        catch (Exception error) when (error is InvalidOperationException ||
            error is System.ComponentModel.Win32Exception || error is IOException)
        {
            SetStatus("Cannot start KHARVOX.");
            MessageBox.Show(this, error.Message, "KHARVOX Mods",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void ToggleAttachment()
    {
        if (_state.WindowAttachmentApproved)
        {
            _state.WindowAttachmentApproved = false;
        }
        else
        {
            var confirmed = MessageBox.Show(this,
                "Dock KHARVOX Mods beside KHARVOX?",
                "Attach to KHARVOX", MessageBoxButtons.YesNo,
                MessageBoxIcon.Question) == DialogResult.Yes;
            if (!confirmed) return;
            _state.WindowAttachmentApproved = true;
        }
        _state.ConsentRecorded = true;
        _state.Save();
        if (!_state.WindowAttachmentApproved && _ownerLauncher != IntPtr.Zero)
        {
            NativeDock.SetOwner(Handle, IntPtr.Zero);
            _ownerLauncher = IntPtr.Zero;
        }
        _lastLauncherBounds = Rectangle.Empty;
        _lastModsBounds = Rectangle.Empty;
        _lastDockForeground = IntPtr.Zero;
        UpdateAttachmentButton();
    }

    private void UpdateAttachmentButton()
    {
        _attachButton.Text = _state.WindowAttachmentApproved ? "Detach Window" : "Attach Window";
    }

    private void SyncMinimizeFromMods()
    {
        if (_syncingWindowState || !_state.WindowAttachmentApproved ||
            _launcherWindow == IntPtr.Zero || !NativeDock.IsWindow(_launcherWindow)) return;
        if (WindowState != FormWindowState.Minimized && WindowState != FormWindowState.Normal)
            return;
        if (WindowState == FormWindowState.Minimized && !NativeDock.IsIconic(_launcherWindow))
            NativeDock.ShowWindow(_launcherWindow, NativeDock.Minimize);
        if (WindowState == FormWindowState.Normal && NativeDock.IsIconic(_launcherWindow))
            NativeDock.ShowWindow(_launcherWindow, NativeDock.Restore);
    }

    private void SyncCompanionFocus()
    {
        if (!_state.WindowAttachmentApproved || !IsHandleCreated ||
            WindowState == FormWindowState.Minimized ||
            _launcherWindow == IntPtr.Zero || !NativeDock.IsWindow(_launcherWindow) ||
            NativeDock.IsIconic(_launcherWindow))
        {
            _lastDockForeground = IntPtr.Zero;
            return;
        }

        var focused = NativeDock.ForegroundRoot();
        if (focused != Handle && focused != _launcherWindow)
        {
            // Don't steal foreground when another application is selected.
            _lastDockForeground = IntPtr.Zero;
            return;
        }

        if (_lastDockForeground == focused) return;
        var other = focused == Handle ? _launcherWindow : Handle;
        if (NativeDock.RaiseDockPair(focused, other))
            _lastDockForeground = focused;
    }

    private void SyncDock()
    {
        if (_launcher == null) return;
        try
        {
            if (_launcher.HasExited)
            {
                _dockTimer.Stop();
                _launcherWindow = IntPtr.Zero;
                _lastDockForeground = IntPtr.Zero;
                SetStatus("KHARVOX closed. Use Start KHARVOX to reopen it.");
                return;
            }
            _launcher.Refresh();
            var handle = _launcher.MainWindowHandle;
            if (handle == IntPtr.Zero || !NativeDock.IsWindow(handle))
            {
                SetStatus("Waiting for KHARVOX desktop launcher…");
                return;
            }
            if (_launcherWindow != handle)
            {
                _launcherWindow = handle;
                _lastDockForeground = IntPtr.Zero;
                _lastLauncherBounds = Rectangle.Empty;
                _lastModsBounds = Rectangle.Empty;
            }
            if (!_state.WindowAttachmentApproved)
            {
                if (_ownerLauncher != IntPtr.Zero)
                {
                    NativeDock.SetOwner(Handle, IntPtr.Zero);
                    _ownerLauncher = IntPtr.Zero;
                }
                _lastDockForeground = IntPtr.Zero;
                SetStatus("KHARVOX running independently — docking disabled.");
                return;
            }
            if (_ownerLauncher != handle)
            {
                if (NativeDock.SetOwner(Handle, handle))
                    _ownerLauncher = handle;
            }
            if (NativeDock.IsIconic(handle))
            {
                _lastLauncherMinimized = true;
                if (WindowState != FormWindowState.Minimized)
                {
                    _syncingWindowState = true;
                    try { WindowState = FormWindowState.Minimized; }
                    finally { _syncingWindowState = false; }
                }
                return;
            }
            if (_lastLauncherMinimized && WindowState == FormWindowState.Minimized)
            {
                _syncingWindowState = true;
                try { WindowState = FormWindowState.Normal; }
                finally { _syncingWindowState = false; }
            }
            _lastLauncherMinimized = false;
            if (WindowState == FormWindowState.Minimized) return;

            var launcherRect = NativeDock.Bounds(handle);
            if (!launcherRect.HasValue) return;
            var modsMoved = _lastModsBounds != Rectangle.Empty && Bounds.Location != _lastModsBounds.Location;
            var launcherMoved = _lastLauncherBounds != launcherRect.Value;
            var workArea = Screen.FromRectangle(launcherRect.Value).WorkingArea;
            var seam = NativeDock.VisibleSeamOverlap(Handle, handle);

            // Dragging our sidecar moves the original launcher, as before.
            if (modsMoved && !launcherMoved)
            {
                var desired = new Rectangle(Right - seam, Top,
                    launcherRect.Value.Width, Height);
                var arranged = NativeDock.Calculate(desired, workArea, Width, seam);
                NativeDock.Move(handle, arranged.Launcher);
                if (Bounds != arranged.Mods) Bounds = arranged.Mods;
            }
            else if (launcherMoved || _lastLauncherBounds == Rectangle.Empty)
            {
                var arranged = NativeDock.Calculate(launcherRect.Value, workArea,
                    Math.Min(DesiredModsWidth, Math.Max(Width, 540)), seam);
                if (launcherRect.Value != arranged.Launcher)
                    NativeDock.Move(handle, arranged.Launcher);
                if (Bounds != arranged.Mods) Bounds = arranged.Mods;
            }

            var actualLauncher = NativeDock.Bounds(handle);
            if (actualLauncher.HasValue) _lastLauncherBounds = actualLauncher.Value;
            _lastModsBounds = Bounds;
            SyncCompanionFocus();
            SetStatus(File.Exists(NativeBridgeInstaller.TargetLauncher)
                ? "Windows linked — native bridge installed; mod selections apply on next DOOM launch."
                : "Windows linked — install Native Bridge from Connection to activate five mod selections.");
        }
        catch (Exception error) when (error is InvalidOperationException ||
            error is System.ComponentModel.Win32Exception)
        {
            SetStatus("KHARVOX window unavailable; no patches were made.");
        }
    }
}
