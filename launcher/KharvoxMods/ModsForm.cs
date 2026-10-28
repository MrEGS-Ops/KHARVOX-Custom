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
    private bool _closingDockPair;
    private bool _centerPairOnNextAttach = true;

    // Exact frame and panel palette used by KHARVOX MainForm.
    private static readonly Color DarkPanel = Color.FromArgb(30, 30, 33);
    private static readonly Color Foreground = Color.Gainsboro;
    private const int DesiredModsWidth = 850; // VR-only single-column window

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
        MinimumSize = new Size(735, 530);
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
        Shown += async (_, _) =>
        {
            if (_bootstrapped) return;
            _bootstrapped = true;
            await StartUpAsync();
        };
        FormClosing += (_, _) => CloseDockCompanion();
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

    private void CloseDockCompanion()
    {
        if (_closingDockPair) return;
        _closingDockPair = true;
        _dockTimer.Stop();

        // Only close the KHARVOX window we paired with. Don't touch DOOM or
        // any unrelated KHARVOX process, and never use Process.Kill().
        if (!_state.WindowAttachmentApproved || _launcher == null) return;
        try
        {
            if (_launcher.HasExited) return;
            _launcher.Refresh();
            var handle = _launcher.MainWindowHandle;
            if (handle == IntPtr.Zero || !NativeDock.IsWindow(handle))
                handle = _launcherWindow;
            if (handle != IntPtr.Zero)
                NativeDock.RequestClose(handle, _launcher.Id);
        }
        catch (Exception error) when (error is InvalidOperationException ||
            error is System.ComponentModel.Win32Exception)
        {
            // The original window may have already closed. Nothing to kill.
        }
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
            _centerPairOnNextAttach = true;
            return true;
        }
    }

    private async System.Threading.Tasks.Task StartUpAsync()
    {
        // Docking is automatic; no attach prompt, bridge install or restart.
        _state.ConsentRecorded = true;
        _state.WindowAttachmentApproved = true;
        UpdateAttachmentButton();

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
            SetStatus("Select your original KHARVOX EXE once to continue.");
            if (!SelectLauncher())
            {
                Close();
                return;
            }
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
            _lastDockForeground = IntPtr.Zero;
            _lastLauncherBounds = Rectangle.Empty;
            _lastModsBounds = Rectangle.Empty;
            _centerPairOnNextAttach = true;
            SetStatus("Custom KHARVOX started — linking windows…");
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
                if (_state.WindowAttachmentApproved && !_closingDockPair)
                {
                    // The linked KHARVOX window exited (for example its X).
                    // Close this sidecar too, but don't terminate DOOM.
                    Close();
                }
                else
                {
                    SetStatus("KHARVOX closed. Use Start KHARVOX to reopen it.");
                }
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

            // Start a fresh two-window pair in the center of the screen.
            // Initial direct custom-launcher startup only. Never recenter the
            // windows during normal dragging.
            if (_centerPairOnNextAttach)
            {
                var target = NativeDock.CalculateCentered(launcherRect.Value,
                    Screen.FromControl(this).WorkingArea, DesiredModsWidth, seam);
                if (launcherRect.Value != target.Launcher &&
                    !NativeDock.Move(handle, target.Launcher))
                {
                    SetStatus("Couldn't center KHARVOX launcher; retrying…");
                    return;
                }
                if (Bounds != target.Mods) Bounds = target.Mods;
                _centerPairOnNextAttach = false;
            }
            // Dragging VRMods moves our custom KHARVOX launcher.
            else if (modsMoved && !launcherMoved)
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

            // Keep the original Mods Folder's custom viewer docked to the
            // pair; dragging it can also move both main windows together.
            var actualLauncher = NativeDock.Bounds(handle);
            if (actualLauncher.HasValue) _lastLauncherBounds = actualLauncher.Value;
            _lastModsBounds = Bounds;
            SyncCompanionFocus();
            SetStatus("Windows linked — custom KHARVOX active; mods apply on next DOOM launch.");
        }
        catch (Exception error) when (error is InvalidOperationException ||
            error is System.ComponentModel.Win32Exception)
        {
            SetStatus("KHARVOX window unavailable; no patches were made.");
        }
    }
}
