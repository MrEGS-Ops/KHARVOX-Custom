using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace KharvoxMods;

// Standalone phase-one shell. Native gameplay features remain owned by the
// original KHARVOX runtime until a separately approved, versioned bridge exists.
internal sealed class ModsForm : Form
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

    private static readonly Color DarkPanel = Color.FromArgb(25, 25, 28);
    private static readonly Color Foreground = Color.Gainsboro;
    private const int DesiredModsWidth = 1180;

    internal ModsForm()
    {
        Text = "KHARVOX Custom Mods";
        ClientSize = new Size(DesiredModsWidth, 600);
        MinimumSize = new Size(540, 420);
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
            _dockTimer.Stop();
            _dockTimer.Dispose();
            _launcher?.Dispose(); // Never terminate the independent KHARVOX process.
        };
        SizeChanged += (_, _) => SyncMinimizeFromMods();
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

    private void BuildLayout()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3,
            BackColor = Color.Black, Padding = new Padding(12)
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));

        var header = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2,
            Margin = Padding.Empty, BackColor = Color.Black
        };
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 45));
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 55));
        var title = MakeLabel("KHARVOX MODS", 11);
        title.Font = new Font("Segoe UI", 11, FontStyle.Bold);
        header.Controls.Add(title, 0, 0);
        _status.Text = "Standalone application starting...";
        _status.ForeColor = Color.Silver;
        _status.AutoEllipsis = true;
        _status.Dock = DockStyle.Fill;
        _status.TextAlign = ContentAlignment.MiddleRight;
        header.Controls.Add(_status, 1, 0);
        root.Controls.Add(header, 0, 0);

        var columns = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1,
            Margin = Padding.Empty, BackColor = Color.Black
        };
        columns.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 43));
        columns.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 57));
        columns.Controls.Add(BuildDoomColumn(), 0, 0);
        columns.Controls.Add(BuildVrColumn(), 1, 0);
        root.Controls.Add(columns, 0, 1);

        var footer = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2,
            Margin = Padding.Empty
        };
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        var credit = MakeLabel("Made by MrEGS");
        credit.ForeColor = Color.Gray;
        footer.Controls.Add(credit, 0, 0);
        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false, AutoSize = true
        };
        var close = MakeButton("Close", 86);
        close.Click += (_, _) => Close();
        buttons.Controls.Add(close);
        _attachButton.Text = "Attach Window";
        _attachButton.Width = 132;
        _attachButton.Height = 29;
        _attachButton.Margin = new Padding(4, 1, 4, 1);
        _attachButton.FlatStyle = FlatStyle.Flat;
        _attachButton.ForeColor = Color.WhiteSmoke;
        _attachButton.BackColor = Color.FromArgb(24,24,27);
        _attachButton.FlatAppearance.BorderColor = Color.DimGray;
        _attachButton.Click += (_, _) => ToggleAttachment();
        buttons.Controls.Add(_attachButton);
        var change = MakeButton("Select KHARVOX…", 142);
        change.Click += (_, _) => SelectLauncher();
        buttons.Controls.Add(change);
        _startButton.Text = "Start KHARVOX";
        _startButton.Width = 132;
        _startButton.Height = 29;
        _startButton.Margin = new Padding(4, 1, 4, 1);
        _startButton.FlatStyle = FlatStyle.Flat;
        _startButton.ForeColor = Color.WhiteSmoke;
        _startButton.BackColor = Color.FromArgb(24,24,27);
        _startButton.FlatAppearance.BorderColor = Color.DimGray;
        _startButton.Click += (_, _) => StartLauncher();
        buttons.Controls.Add(_startButton);
        footer.Controls.Add(buttons, 1, 0);
        root.Controls.Add(footer, 0, 2);

        Controls.Add(root);
    }

    private Control BuildDoomColumn()
    {
        var panel = new Panel
        {
            Dock = DockStyle.Fill, Padding = new Padding(10),
            Margin = new Padding(0,0,7,0), BackColor = DarkPanel
        };
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 4, ColumnCount = 1 };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 110));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        var header = MakeLabel("DOOM MODS", 10);
        header.Font = new Font("Segoe UI", 10, FontStyle.Bold);
        layout.Controls.Add(header, 0, 0);
        var notice = MakeLabel("No gameplay mods have been installed. This separate application currently handles KHARVOX launching and window docking only.");
        notice.AutoEllipsis = false;
        layout.Controls.Add(notice, 0, 1);
        var separator = MakeLabel("INSTALLATION STATUS", 9);
        separator.ForeColor = Color.Gray;
        layout.Controls.Add(separator, 0, 2);
        var details = MakeLabel("KHARVOX stays independent. No game files, KHARVOX DLLs, configuration or controller bindings are modified by window attachment.");
        details.AutoEllipsis = false;
        details.TextAlign = ContentAlignment.TopLeft;
        layout.Controls.Add(details, 0, 3);
        panel.Controls.Add(layout);
        return panel;
    }

    private Control BuildVrColumn()
    {
        var tabs = new TabControl
        {
            Dock = DockStyle.Fill, Margin = new Padding(7,0,0,0),
            BackColor = DarkPanel, ForeColor = Color.Black
        };
        var mods = new TabPage("VR Mods") { BackColor = DarkPanel };
        var list = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, AutoScroll = true,
            FlowDirection = FlowDirection.TopDown, WrapContents = false,
            Padding = new Padding(10), BackColor = DarkPanel
        };
        AddSection(list, "HANDS & ARMS");
        AddEntry(list, "Weapon Wheel Button Remap",
            "Existing KHARVOX mod. Will be moved only after native bridge verification.");
        AddEntry(list, "Hand Focus",
            "Moves aiming/focus to the controller. Future integration is not installed in this build.");
        AddEntry(list, "Behind-the-Head Weapon Wheel",
            "Gesture-driven weapon wheel. Current gameplay implementation remains in the development KHARVOX build.");
        AddSection(list, "WEAPONS & COMBAT");
        AddEntry(list, "PAINSAW: RIPSHIFT OVERDRIVE",
            "Rev the PAINSAW with up to three separate trigger pulls. The third uses the fastest tested execution setting. Includes intense right-hand haptics.");
        AddEntry(list, "Glory Kill Speed Override",
            "Separate 30–240 Hz action speed control. This shell does not activate the native feature.");
        AddSection(list, "DIAGNOSTICS");
        AddEntry(list, "Chainsaw Animation Investigator",
            "Read-only animation timing diagnostics. Native integration will be added separately.");
        var hint = MakeLabel("Preview only — controls are not enabled until the native bridge is implemented.", 8);
        hint.ForeColor = Color.DarkOrange;
        hint.AutoSize = true;
        hint.MaximumSize = new Size(500, 40);
        list.Controls.Add(hint);
        mods.Controls.Add(list);
        tabs.TabPages.Add(mods);

        var dev = new TabPage("Dev") { BackColor = DarkPanel };
        var devList = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, AutoScroll = true,
            FlowDirection = FlowDirection.TopDown, WrapContents = false,
            Padding = new Padding(10), BackColor = DarkPanel
        };
        AddSection(devList, "GAMEPLAY OVERRIDES — NOT YET CONNECTED");
        AddEntry(devList, "Disable Glory Kill Highlighting",
            "Removes the blue/orange enemy outline so staggered enemies look more natural in VR.");
        AddEntry(devList, "Disable Glory Kill Motion Blur",
            "Removes automatic motion blur during executions to keep the headset picture clear.");
        AddEntry(devList, "Disable Camera Kicks & Shaking",
            "Prevents artificial viewpoint jolts that do not correspond to headset movement.");
        AddEntry(devList, "Disable Native Weapon Kick",
            "Prevents the game's first-person weapon kick from interfering with 6DoF aiming.");
        AddEntry(devList, "Disable Hand-Hit Reactions",
            "Prevents canned hit reactions from interfering with controller-tracked hands.");
        AddEntry(devList, "Protect Native Ledge Speed",
            "Keeps climbing, falling and landing outside experimental kill-animation speed controls.");
        AddEntry(devList, "Protect Scripted Cinematic Speed",
            "Keeps story cinematics outside experimental action-speed controls.");
        dev.Controls.Add(devList);
        tabs.TabPages.Add(dev);
        return tabs;
    }

    private void AddSection(FlowLayoutPanel parent, string title)
    {
        parent.Controls.Add(new Label
        {
            Text = title, Width = 455, Height = 27,
            ForeColor = Color.WhiteSmoke,
            BackColor = DarkPanel,
            Font = new Font("Segoe UI", 9, FontStyle.Bold),
            TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(3,8,3,1)
        });
    }

    private void AddEntry(FlowLayoutPanel parent, string title, string description)
    {
        // Deliberately read-only: a visual checkbox must never imply an installed gameplay patch.
        var label = new Label
        {
            Text = "☐  " + title + "  ⓘ", Width = 455, Height = 27,
            AutoEllipsis = true, TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = Color.FromArgb(155,155,160), BackColor = DarkPanel,
            Margin = new Padding(3,2,3,2),
            AccessibleDescription = description + " This is an uninstalled preview."
        };
        _tooltips.SetToolTip(label, description +
            Environment.NewLine + "Not installed or activated in this standalone test build.");
        parent.Controls.Add(label);
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
            _launcherWindow = IntPtr.Zero;
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
        _lastLauncherBounds = Rectangle.Empty;
        _lastModsBounds = Rectangle.Empty;
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

    private void SyncDock()
    {
        if (_launcher == null) return;
        try
        {
            if (_launcher.HasExited)
            {
                _dockTimer.Stop();
                _launcherWindow = IntPtr.Zero;
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
                _lastLauncherBounds = Rectangle.Empty;
                _lastModsBounds = Rectangle.Empty;
            }
            if (!_state.WindowAttachmentApproved)
            {
                SetStatus("KHARVOX running independently — docking disabled.");
                return;
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

            // Dragging our sidecar moves the original launcher, as before.
            if (modsMoved && !launcherMoved)
            {
                var desired = new Rectangle(Right - 8, Top,
                    launcherRect.Value.Width, Height);
                var arranged = NativeDock.Calculate(desired, workArea, Width);
                NativeDock.Move(handle, arranged.Launcher);
                if (Bounds != arranged.Mods) Bounds = arranged.Mods;
            }
            else if (launcherMoved || _lastLauncherBounds == Rectangle.Empty)
            {
                var arranged = NativeDock.Calculate(launcherRect.Value, workArea,
                    Math.Min(DesiredModsWidth, Math.Max(Width, 540)));
                if (launcherRect.Value != arranged.Launcher)
                    NativeDock.Move(handle, arranged.Launcher);
                if (Bounds != arranged.Mods) Bounds = arranged.Mods;
            }

            var actualLauncher = NativeDock.Bounds(handle);
            if (actualLauncher.HasValue) _lastLauncherBounds = actualLauncher.Value;
            _lastModsBounds = Bounds;
            SetStatus("Window attached — no patches or gameplay mods installed.");
        }
        catch (Exception error) when (error is InvalidOperationException ||
            error is System.ComponentModel.Win32Exception)
        {
            SetStatus("KHARVOX window unavailable; no patches were made.");
        }
    }
}
