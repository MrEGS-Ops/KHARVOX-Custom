using System.Reflection;

namespace KharvoxLauncher;

public sealed partial class MainForm : Form
{
    private const decimal AerDefaultRenderScale = 100m;
    private const decimal FixedTurnDeadzone = .35m;
    private const int DefaultSmoothTurnSpeed = 230;
    private const decimal GloryKillSpeedMinimum = 1.0m;
    private const decimal GloryKillSpeedStep = .2m;
    private const decimal DefaultGloryKillSpeed = 2.8m;
    private static readonly string[] CalibrationWeaponKeys = [
        "pistol", "shotgun", "heavy_assault_rifle", "plasma_rifle", "rocket_launcher",
        "super_shotgun", "gauss_cannon", "chaingun", "bfg", "chainsaw", "fists",
        "assault_rifle", "arc_cannon", "mancubus_gland"
    ];
    private static readonly string[] CalibrationWeaponNames = [
        "Pistol", "Combat Shotgun", "Heavy Assault Rifle", "Plasma Rifle", "Rocket Launcher",
        "Super Shotgun", "Gauss Cannon", "Chaingun", "BFG 9000", "Chainsaw", "Fists",
        "Assault Rifle", "Arc Cannon", "Mancubus Gland"
    ];
    private static readonly string[] BackWeaponKeys = [
        "pistol", "shotgun", "plasma_rifle", "heavy_assault_rifle",
        "rocket_launcher", "super_shotgun", "gauss_cannon", "chaingun",
        "bfg", "chainsaw"
    ];
    private static readonly string[] BackWeaponNames = [
        "Pistol", "Combat Shotgun", "Plasma Rifle", "Heavy Assault Rifle",
        "Rocket Launcher", "Super Shotgun", "Gauss Cannon", "Chaingun",
        "BFG 9000", "Chainsaw"
    ];
    private static readonly Color PanelColor = Color.FromArgb(30, 30, 33);
    private readonly string SettingsPath;

    private readonly ComboBox preset = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox rendererMode = new() { DropDownStyle = ComboBoxStyle.DropDownList, Dock = DockStyle.Fill };
    private readonly ComboBox turnMode = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox movementDirection = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox weaponMode = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox calibrationWeapon = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox gripAlignment = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox leftHandSwapMode = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox physicalGlorykillHands = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox backWeapon = new() { DropDownStyle = ComboBoxStyle.DropDownList, Dock = DockStyle.Fill };
    private readonly TextBox doomPath = new() { ReadOnly = true, Dock = DockStyle.Fill };
    private readonly CheckBox intense = MakeCheck("Immersive Mode", false);
    private readonly CheckBox cinematicFreelook = MakeCheck("Freelook in cinematics and Glory Kills", true);
    private readonly CheckBox otherCinematicsInQuad = MakeCheck(
        "Regular cinematics in Cine Window", false);
    private readonly CheckBox cinewindowFollowsHeadset = MakeCheck(
        "Cine Window follows headset", false);
    private readonly CheckBox virtualGunstock = MakeCheck("Virtual Gunstock", false);
    private readonly CheckBox physicalGlorykill = MakeCheck("Physical Glory Kills", false);
    private readonly CheckBox laserSight = MakeCheck("Laser sight", false);
    private readonly CheckBox leftHanded = MakeCheck("Left Hand mode", false);
    private readonly CheckBox swapJumpCrouch = MakeCheck("Swap Jump/Crouch", false);
    private readonly CheckBox hudDebugging = MakeCheck("Enable HUD debugging / calibration", false);
    private readonly CheckBox extendedLogging = MakeCheck("Extended Logging", false);
    private readonly CheckBox captureEyes = MakeCheck("Eye capture: Ctrl+Shift+P (next launch)", false);
    private readonly CheckBox disableAa = MakeCheck("Disable AA (both renderers, next launch)", false);
    private readonly CheckBox handsJump = MakeCheck("Hands Jump", true);
    private readonly CheckBox disableVrIntro = MakeCheck("Disable VR-Intro", false);
    private readonly CheckBox showHands = MakeCheck("Enable Hands", true);
    private readonly CheckBox calibrateHands = MakeCheck("Calibrate hands", false);
    private readonly CheckBox enableBhaptics = MakeCheck("Enable bHaptics", false);
    private readonly CheckBox usePsvr2Toolkit = MakeCheck("Use PSVR2 Toolkit", false);
    private readonly CheckBox useFsrUpscaling = MakeCheck("Use FSR Upscaling", false);
    private readonly CheckBox weaponWheelRemap = MakeCheck("Weapon Wheel Button Remap", true);
    private readonly CheckBox customDisableHud = MakeCheck("No HUD", false);
    private readonly CheckBox customDisableWeaponWheel = MakeCheck("No Weapon Wheel", false);
    private readonly CheckBox customGaussChargeSlowMovement = MakeCheck("Gauss Charge Slow Movement", false);
    private readonly CheckBox customBackOfHandHud = MakeCheck("Back-of-Hand HUD", false);
    private readonly CheckBox customHandFocusedRs = MakeCheck("Hand Focus", false);
    private readonly CheckBox customDirectionalDash = MakeCheck("Directional Dash", false);
    private readonly CheckBox customBehindHeadWeaponWheel = MakeCheck("Behind-Head Wrist Weapon Wheel", false);
    private readonly CheckBox customLeftWristWeaponWheel = MakeCheck("Left Wrist Weapon Wheel", false);
    private readonly CheckBox customBehindHeadWheelHandSelection = MakeCheck("Behind-Head Wheel: Hand Selection", true);
    private readonly CheckBox customPhysicalCrouch = MakeCheck("Physical Crouch", false);
    private readonly CheckBox customRevengeDemon = MakeCheck("Revenge Demon", false);
    private readonly CheckBox customDynamicShoulderHolster = MakeCheck("Dynamic Quick Holsters", false);
    private readonly CheckBox customPhysicalGrenadeThrow = MakeCheck("Physical Grenade Throw", false);
    private readonly CheckBox customMotionGloryKillSpeed = MakeCheck("Punch-Driven Glory Kill Speed", false);
    private readonly CheckBox customPhysicalChainsawGestures = MakeCheck("Physical Chainsaw Gestures", false);
    private readonly CheckBox customHapticOverhaul = MakeCheck("Haptic Overhaul", false);
    // Retain the stable runtime slider value as the model; only its UI changes.
    private readonly TrackBar gloryKillSlowmo = MakeSlider(0, 10, 10, 1, 1);
    private readonly FlatGloryKillSlider gloryKillSpeedSlider = new();
    private readonly Label gloryKillSlowmoValue = MakeSliderValueLabel();
    private readonly Label weaponStatus = new() { AutoSize = false, ForeColor = Color.Silver, TextAlign = ContentAlignment.MiddleLeft };
    private readonly System.Windows.Forms.Timer runtimeStatusTimer = new() { Interval = 500 };
    private readonly NumericUpDown renderScale = MakeNumber(50, decimal.MaxValue, AerDefaultRenderScale, 0, 10);
    private readonly TrackBar smoothSpeed = MakeSlider(150, 400, DefaultSmoothTurnSpeed, 25, 10);
    private readonly Label smoothSpeedValue = MakeSliderValueLabel();
    private readonly NumericUpDown snapAngle = MakeNumber(10, 180, 45, 0);
    private readonly TrackBar physicalGlorykillSpeed = MakeSlider(0, 15, 9, 1, 1);
    private readonly Label physicalGlorykillSpeedValue = MakeSliderValueLabel();
    private readonly Label status = new() { AutoSize = false, TextAlign = ContentAlignment.MiddleCenter, ForeColor = Color.Silver };
    // This tooltip serves two distinct top-level forms. ShowAlways prevents
    // WinForms suppressing descriptions when the detached Custom Mods form is active.
    private readonly ToolTip statusToolTip = new()
    {
        ShowAlways = true, InitialDelay = 380, ReshowDelay = 95,
        AutoPopDelay = 14000, IsBalloon = false
    };
    private readonly Panel renderScaleHost = new() { Dock = DockStyle.Fill, Margin = Padding.Empty };
    private readonly Button launchButton = new();
    private bool applyingCustomModDependencies;
    private bool customModsSaveFailed;
    private Form? customOptionsForm;
    private FlowLayoutPanel? userDoomModChecks;
    private FlowLayoutPanel? packagedDoomModChecks;
    private Action? refreshDoomModLoaderStatus;
    private const string DoomNexusModsUrl = NexusModsPopup.PageUrl;
    private Label? packagedDoomModHeader;
    private FlowLayoutPanel? packagedDoomModEmpty;
    private FlowLayoutPanel? doomModItemsPanel;
    private Label? userDoomModStatus;
    private Label? userDoomModCount;
    private FileSystemWatcher? userDoomModWatcher;
    private readonly System.Windows.Forms.Timer userDoomModDebounce = new() { Interval = 450 };
    private FileSystemWatcher? doomModLoaderWatcher;
    private readonly System.Windows.Forms.Timer doomModLoaderDebounce = new() { Interval = 180 };
    private DevModeForm? devModeForm;
    private CracktroForm? cracktroForm;
    private bool nextCracktroIsAmiga;
    private InfoForm? infoForm;
    private bool launchOperationInProgress;
    private bool lastKnownDoomRunning;
    private bool applyingPreset;
    private readonly Panel viewport = new() { Dock = DockStyle.Fill, AutoScroll = true };
    private TableLayoutPanel? launcherContent;
    private bool fittingWindow;
    private Rectangle fittedWorkArea;

    public MainForm() : this(LauncherSettingsStore.DefaultPath)
    {
        if (!File.Exists(SettingsPath))
            Shown += (_, _) =>
            {
                using var welcome = new WelcomeForm();
                var choice = welcome.ShowDialog(this);
                SaveSettings();
                if (choice == DialogResult.OK) ShowInfo();
            };
    }

    internal MainForm(string settingsPath)
    {
        SettingsPath = settingsPath;
        Text = "KHARVOX Launcher — Build " + RuntimeStorage.DisplayBuild;
        var applicationIcon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        if (applicationIcon is not null) Icon = applicationIcon;
        ClientSize = new Size(510, 904);
        MinimumSize = new Size(280, 240);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(0, 0, 0);
        ForeColor = Color.WhiteSmoke;
        Font = new Font("Segoe UI", 9F);
        AutoScaleMode = AutoScaleMode.Dpi;

        var root = new TableLayoutPanel
        {
            Location = Point.Empty,
            Size = new Size(510, 904),
            MinimumSize = new Size(510, 904),
            Padding = new Padding(18, 14, 18, 10),
            RowCount = 7,
            ColumnCount = 1
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 130));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 330));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 172));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 24));
        launcherContent = root;
        viewport.Controls.Add(root);
        Controls.Add(viewport);
        viewport.ClientSizeChanged += (_, _) => LayoutViewport();

        var banner = new PictureBox { Dock = DockStyle.Fill, SizeMode = PictureBoxSizeMode.Zoom, BackColor = Color.Black };
        using (var bannerStream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Kharvox.Branding.Logo"))
        {
            if (bannerStream is not null)
            {
                using var embeddedImage = Image.FromStream(bannerStream);
                banner.Image = new Bitmap(embeddedImage);
            }
        }
        // Reputation belongs only to Custom Mods, not the main launcher.
        root.Controls.Add(banner);

        var profileRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, Padding = new Padding(8, 2, 8, 2) };
        profileRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 112));
        profileRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        profileRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 132));
        profileRow.Controls.Add(new Label
        {
            Text = "Launch profile",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = Color.WhiteSmoke
        });
        preset.Items.AddRange(["Recommended", "Comfort", "Intense", "Custom"]);
        preset.Dock = DockStyle.Fill;
        preset.SelectedIndexChanged += (_, _) =>
        {
            if (restoringConfiguration) return;
            ApplyPreset();
            if (preset.SelectedIndex != 3) CheckLiveConfiguration();
        };
        profileRow.Controls.Add(preset);
        var infoButton = new Button
        {
            Text = "📖 Instructions",
            Dock = DockStyle.Fill,
            Margin = new Padding(3),
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(184, 145, 0),
            ForeColor = Color.Black,
            Font = new Font(Font, FontStyle.Bold),
            AccessibleName = "Open KHARVOX Instructions"
        };
        infoButton.FlatAppearance.BorderColor = Color.FromArgb(212, 172, 24);
        infoButton.FlatAppearance.MouseOverBackColor = Color.FromArgb(205, 164, 12);
        infoButton.FlatAppearance.MouseDownBackColor = Color.FromArgb(160, 125, 0);
        infoButton.Click += (_, _) => ShowInfo();
        profileRow.Controls.Add(infoButton, 2, 0);
        root.Controls.Add(profileRow);

        var installRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, Padding = new Padding(8, 2, 8, 6) };
        installRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 112));
        installRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        installRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 132));
        installRow.Controls.Add(new Label { Text = "DOOM installation", Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft });
        installRow.Controls.Add(doomPath);
        var browseButton = new Button { Text = "Browse…", Dock = DockStyle.Fill, FlatStyle = FlatStyle.Flat };
        browseButton.Click += BrowseDoomFolder;
        installRow.Controls.Add(browseButton);
        root.Controls.Add(installRow);

        var options = MakeGroup("VR OPTIONS");
        var optionGrid = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(18, 8, 18, 8), RowCount = 11, ColumnCount = 3 };
        optionGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 187));
        optionGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 194));
        optionGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        for (var i = 0; i < 11; i++) optionGrid.RowStyles.Add(new RowStyle(SizeType.Percent, 100f / 11f));
        rendererMode.Items.AddRange(["AER", VulkanSfs.Label]);
        rendererMode.SelectedIndexChanged += RendererModeChanged;
        renderScale.ValueChanged += OptionChanged;
        var renderingInputWidth = (TextRenderer.MeasureText("Vulkan SFS Source Ring (Test)", Font).Width
            + SystemInformation.VerticalScrollBarWidth + 4) / 2;
        var rendererRow = (TableLayoutPanel)MakeFixedWidth(rendererMode, renderingInputWidth);
        AddField(optionGrid, 0, "Renderer", rendererRow);
        optionGrid.SetColumnSpan(rendererRow, 2);


        optionGrid.Controls.Add(new Label
        {
            Text = "RenderScale (%)",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft
        }, 0, 1);
        var renderScaleRow = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            Margin = Padding.Empty,
            Padding = Padding.Empty
        };
        renderScaleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, renderingInputWidth));
        renderScaleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        renderScaleRow.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        renderScale.Dock = DockStyle.Fill;
        useFsrUpscaling.Dock = DockStyle.Fill;
        useFsrUpscaling.CheckedChanged += OptionChanged;
        renderScaleHost.Margin = renderScale.Margin;
        renderScaleHost.Controls.Add(renderScale);
        renderScaleRow.Controls.Add(renderScaleHost, 0, 0);
        renderScaleRow.Controls.Add(useFsrUpscaling, 1, 0);
        optionGrid.Controls.Add(renderScaleRow, 1, 1);
        optionGrid.SetColumnSpan(renderScaleRow, 2);
        intense.Dock = DockStyle.Fill;
        intense.CheckedChanged += ImmersiveModeChanged;
        optionGrid.Controls.Add(intense, 0, 2);
        optionGrid.SetColumnSpan(intense, 3);
        cinematicFreelook.Dock = DockStyle.Fill;
        cinematicFreelook.Padding = new Padding(22, 0, 0, 0);
        cinematicFreelook.Enabled = false;
        cinematicFreelook.CheckedChanged += OptionChanged;
        optionGrid.Controls.Add(cinematicFreelook, 0, 3);
        optionGrid.SetColumnSpan(cinematicFreelook, 3);
        otherCinematicsInQuad.Dock = DockStyle.Fill;
        otherCinematicsInQuad.Padding = new Padding(22, 0, 0, 0);
        otherCinematicsInQuad.Enabled = false;
        otherCinematicsInQuad.CheckedChanged += OptionChanged;
        optionGrid.Controls.Add(otherCinematicsInQuad, 0, 4);
        optionGrid.SetColumnSpan(otherCinematicsInQuad, 3);
        cinewindowFollowsHeadset.Dock = DockStyle.Fill;
        cinewindowFollowsHeadset.CheckedChanged += OptionChanged;
        optionGrid.Controls.Add(cinewindowFollowsHeadset, 0, 5);
        optionGrid.SetColumnSpan(cinewindowFollowsHeadset, 3);
        physicalGlorykill.Dock = DockStyle.Fill;
        physicalGlorykill.CheckedChanged += PhysicalGlorykillChanged;
        physicalGlorykillSpeed.AccessibleName = "Physical Glory Kill speed";
        physicalGlorykillSpeed.ValueChanged += SpeedSliderChanged;
        optionGrid.Controls.Add(physicalGlorykill, 0, 6);
        var physicalGloryKillSliderHost = MakeSpeedSlider(physicalGlorykillSpeed, physicalGlorykillSpeedValue);
        optionGrid.Controls.Add(physicalGloryKillSliderHost, 1, 6);
        optionGrid.SetColumnSpan(physicalGloryKillSliderHost, 2);
        physicalGlorykillHands.Items.AddRange(["Only Left Hand", "Only Right Hand", "Both Hands"]);
        physicalGlorykillHands.SelectedIndexChanged += OptionChanged;
        AddField(optionGrid, 7, "Glory Kill hands", MakeFixedWidth(physicalGlorykillHands, 126));
        backWeapon.Items.AddRange(BackWeaponNames);
        backWeapon.SelectedIndexChanged += OptionChanged;
        var shoulderAndIntro = new TableLayoutPanel {
            Dock = DockStyle.Fill, Margin = Padding.Empty, Padding = Padding.Empty,
            ColumnCount = 2, RowCount = 1
        };
        shoulderAndIntro.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        shoulderAndIntro.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140));
        shoulderAndIntro.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        shoulderAndIntro.Controls.Add(MakeFixedWidth(backWeapon, 126), 0, 0);
        AddField(optionGrid, 8, "Shoulder Weapon", shoulderAndIntro);
        optionGrid.SetColumnSpan(shoulderAndIntro, 2);

        virtualGunstock.Dock = DockStyle.Fill;
        virtualGunstock.CheckedChanged += OptionChanged;
        laserSight.Dock = DockStyle.Fill;
        laserSight.CheckedChanged += OptionChanged;
        enableBhaptics.Dock = DockStyle.Fill;
        enableBhaptics.CheckedChanged += OptionChanged;
        usePsvr2Toolkit.Dock = DockStyle.Fill;
        usePsvr2Toolkit.CheckedChanged += OptionChanged;
        optionGrid.Controls.Add(virtualGunstock, 0, 9);
        var sightAndHands = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Margin = Padding.Empty, Padding = Padding.Empty,
            ColumnCount = 2, RowCount = 2
        };
        sightAndHands.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
        sightAndHands.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
        sightAndHands.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140));
        sightAndHands.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52));
        disableVrIntro.Dock = DockStyle.Fill;
        disableVrIntro.Visible = VrGameIntroSession.HasSeenCurrentRelease;
        disableVrIntro.CheckedChanged += OptionChanged;
        shoulderAndIntro.Controls.Add(disableVrIntro, 1, 0);
        showHands.Dock = DockStyle.Fill;
        sightAndHands.Controls.Add(laserSight, 0, 0);
        sightAndHands.Controls.Add(showHands, 1, 0);
        optionGrid.Controls.Add(sightAndHands, 1, 9);
        optionGrid.SetColumnSpan(sightAndHands, 2);
        optionGrid.SetRowSpan(sightAndHands, 2);
        optionGrid.Controls.Add(enableBhaptics, 0, 10);
        sightAndHands.Controls.Add(usePsvr2Toolkit, 0, 1);
        handsJump.Dock = DockStyle.Fill;
        handsJump.CheckedChanged += OptionChanged;
        statusToolTip.SetToolTip(handsJump, "Raise both controllers upward at 1.9 m/s to jump. Supplements the jump button.");
        sightAndHands.Controls.Add(handsJump, 1, 1);
        options.Controls.Add(optionGrid);
        root.Controls.Add(options);

        var customMods = MakeGroup("VR MODS");
        customMods.ForeColor = Color.White;
        var customModsGrid = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            BackColor = PanelColor,
            ForeColor = Color.White,
            Padding = new Padding(12, 8, 10, 6),
            ColumnCount = 2,
            AutoScroll = false
        };
        customModsGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 124));
        customModsGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 250));
        customModsGrid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));

        gloryKillSpeedSlider.ValueChanged += (_, _) =>
        {
            if (gloryKillSlowmo.Value != GKMenuToEngine(gloryKillSpeedSlider.Value))
                gloryKillSlowmo.Value = GKMenuToEngine(gloryKillSpeedSlider.Value);
        };
        gloryKillSlowmo.ValueChanged += SpeedSliderChanged;
        statusToolTip.SetToolTip(gloryKillSpeedSlider,
            "Glory Kill speed: 1 = native DOOM speed; 2–4 progressively faster; 5 = fastest. Use mouse or arrow keys.");
        customModsGrid.Controls.Add(new Label
        {
            Text = "Glory Kill speed",
            Dock = DockStyle.Fill,
            ForeColor = Color.Gainsboro,
            TextAlign = ContentAlignment.MiddleLeft
        }, 0, 0);
        var speedScale = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, Margin = Padding.Empty,
            Padding = Padding.Empty, ColumnCount = 3, RowCount = 1,
            BackColor = PanelColor
        };
        speedScale.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 33));
        speedScale.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        speedScale.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 32));
        var slowCaption = new Label {
            Text = "Slow", ForeColor = Color.Silver,
            Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft,
            Margin = Padding.Empty
        };
        var fastCaption = new Label {
            Text = "Fast", ForeColor = Color.Silver,
            Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleRight,
            Margin = Padding.Empty
        };
        gloryKillSpeedSlider.Dock = DockStyle.Fill;
        gloryKillSpeedSlider.Margin = Padding.Empty;
        speedScale.Controls.Add(slowCaption, 0, 0);
        speedScale.Controls.Add(gloryKillSpeedSlider, 1, 0);
        speedScale.Controls.Add(fastCaption, 2, 0);
        customModsGrid.Controls.Add(speedScale, 1, 0);

        var customChecks = new (CheckBox Box, string Tip)[]
        {
            (customDisableHud, "Hide the normal combat HUD while keeping game menus available."),
            (customDisableWeaponWheel, "Disable weapon-wheel presentation/selection for a harder no-wheel mode."),
            (customGaussChargeSlowMovement, "Allow deliberately slow movement while the Gauss Cannon is charging instead of a full movement lock."),
            (customBackOfHandHud, "Rotate/reposition the hand HUD onto the back of the hand in a watch-like viewing pose. Enabling this automatically disables No HUD because they use the same gameplay-HUD surfaces."),
            (customHandFocusedRs, "Use the controller/hand as the focus source; keep RS as the actual activation button."),
            (customDirectionalDash, "UNSAFE NATIVE PHYSICS DASH DISABLED in this build following reports of teleportation into the void and death. The experimental button will not modify DOOM physics until a safe implementation is verified."),
            (customBehindHeadWeaponWheel, "Move the RIGHT wrist behind the head; aim by moving the wrist. Hold right Grip to lock the selection, then pull out to equip into the RIGHT hand. Grenades move to the LEFT hand. Movement, turning, face buttons, jump, crouch and menus do not change."),
            (customLeftWristWeaponWheel, "Enable an additional LEFT wrist wheel. Move the left wrist to choose; hold left Grip to lock the selection, then pull out to equip into the LEFT hand. Grenades move to the RIGHT hand. Does not change the launcher's Left Hand mode, movement, turning, face buttons, jump, crouch or menus."),
            
            (customPhysicalCrouch, "Crouch your body to crouch in DOOM, stand up to stand in DOOM. Uses separate down/up thresholds and emits a native press on both transitions."),
            (customRevengeDemon, "DIAGNOSTIC ONLY: logs death-time enemy references through the Supervisor. Does not yet empower a revenge demon."),
            (customDynamicShoulderHolster, "Experimental: two hip-only quick-holster zones. Store left-wheel weapons on the left hip and right-wheel weapons on the right hip; draw only by gripping inside the appropriate hip zone. Requires Enable Hands in the main launcher: automatically checks and greys it out until this mod is turned off."),
            (customPhysicalGrenadeThrow, "Hold equipment and make a deliberate hand swing. Its deceleration triggers one native grenade throw. Release to rearm; native DOOM controls trajectory."),
            (customMotionGloryKillSpeed, "After a physical Glory Kill begins, a second punch changes the active kill speed based on punch velocity."),
            (customPhysicalChainsawGestures, "Experimental: hand movement drives chainsaw kill speed; stopping motion slows playback to 12% rather than pausing."),
            (customHapticOverhaul, "EXPERIMENTAL: fixed-strength damage vibration with duration based on HP/AP lost (50ms per point, 600ms maximum). Pending a verified live health and armour reader; no damage pulses will occur until that input is available. Existing DOOM weapon feedback remains unchanged.")
        };

        // Preserve the approved mod titles. A separate per-mod status marker
        // identifies diagnostic-only, incomplete and experimental features.
        // This avoids long suffixes crowding the narrow Custom Mods panel.
        // Original mod controls stay intact; this UI-only layout supplies
        // category headers and persistent, within-category drag ordering.
        // Dependency labels are rebuilt from visible positions on every move.
        var availableCustomMods = customChecks
            .Concat(new[] { (Box: weaponWheelRemap,
                Tip: "Remaps weapon selection for VR.\n"
                    + "Tap A to quick-switch weapons. Hold A to open the weapon wheel, "
                    + "use the left stick to choose, then release A to equip.\n"
                    + "Pull the right thumbstick DOWN to toggle crouch; B stays Jump. "
                    + "(Physical Crouch overrides the stick crouch toggle.)\n"
                    + "With Behind-Head Wrist Weapon Wheel enabled, move the right wrist "
                    + "behind your head and select by moving it; hold right Grip to lock "
                    + "the selection before pulling out. Left Wrist Weapon Wheel adds "
                    + "the same gesture on your left without changing Left Hand mode.\n"
                    + "Directional Dash is currently disabled due an unsafe native physics path.\n"
                    + "While enabled, Swap Jump/Crouch in the main launcher is greyed out because this remap controls those buttons; its saved selection is preserved.\n"
                     + "Turn this off to restore the original KHARVOX controls.") })
            .ToArray();
        InitializeGroupedCustomMods(customModsGrid, availableCustomMods);
        var customModsScroll = new Panel
        {
            Dock = DockStyle.Fill, AutoScroll = true,
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = PanelColor
        };
        modOrderScroll = customModsScroll;
        customModsScroll.Controls.Add(customModsGrid);
        customMods.Controls.Add(customModsScroll);
        // Three independent rating buttons sit on the VR MODS header line.
        // Keep the content grid and checkbox order completely unchanged.
        BuildConfigurationRatingHeader(customMods);

        // The content currently occupies 15 x 28px plus compact speed menu.
        // Fit to it instead of putting another nested scrollbar on the form.
        var doomMods = MakeGroup("DOOM MODS");
        doomMods.ForeColor = Color.White;
        var doomGrid = new TableLayoutPanel
        {
            // Keep loader, packaged mods, USER MODS controls and list in
            // separate rows; pin the external browse link at the very bottom
            // so it never becomes part of the scanned/scrolling mod list.
            Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5,
            Padding = new Padding(12, 2, 12, 2), BackColor = PanelColor
        };
        doomGrid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        doomGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        doomGrid.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        doomGrid.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        doomGrid.RowStyles.Add(new RowStyle(SizeType.Absolute, 27));
        var loaderStatusLabel = new Label
        {
            Dock = DockStyle.Fill, ForeColor = Color.Gainsboro,
            TextAlign = ContentAlignment.MiddleLeft
        };
        var loaderInstallButton = new Button
        {
            Dock = DockStyle.Fill, FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(43, 43, 47), ForeColor = Color.White,
            Margin = new Padding(3, 2, 3, 2),
            AccessibleName = "Install DOOMModLoader"
        };
        loaderInstallButton.FlatAppearance.BorderColor = Color.DimGray;
        Action refreshLoaderStatus = () =>
        {
            var result = DoomModLoaderInstaller.CheckInstallation();
            switch (result.State)
            {
                case DoomModLoaderInstaller.InstallationState.Verified:
                    loaderStatusLabel.Text = "DOOMModLoader v"
                        + DoomModLoaderInstaller.Version + " — Verified";
                    loaderStatusLabel.ForeColor = Color.LightGreen;
                    loaderInstallButton.Text = "Verify";
                    break;
                case DoomModLoaderInstaller.InstallationState.RepairRequired:
                    loaderStatusLabel.Text = "DOOMModLoader — Repair required";
                    loaderStatusLabel.ForeColor = Color.Orange;
                    loaderInstallButton.Text = "Repair…";
                    break;
                default:
                    loaderStatusLabel.Text = "DOOMModLoader — Not installed";
                    loaderStatusLabel.ForeColor = Color.Gainsboro;
                    loaderInstallButton.Text = "Install…";
                    break;
            }
            var installHelp = result.Details + Environment.NewLine
                + "DOOM resource mods require DOOMModLoader. "
                + "Install from the verified official release using this button.";
            statusToolTip.SetToolTip(loaderStatusLabel, installHelp);
            statusToolTip.SetToolTip(loaderInstallButton, installHelp);
        };
        refreshDoomModLoaderStatus = refreshLoaderStatus;
        refreshLoaderStatus();
        loaderInstallButton.Click += (_, _) =>
        {
            if (KharvoxRunner.IsRunning)
            {
                MessageBox.Show(customOptionsForm,
                    "End DOOM before installing or changing resource tools.",
                    "KHARVOX", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            var installation = DoomModLoaderInstaller.CheckInstallation();
            if (installation.State == DoomModLoaderInstaller.InstallationState.Verified)
            {
                MessageBox.Show(customOptionsForm,
                    installation.Details + Environment.NewLine + Environment.NewLine
                    + "Executable SHA-256 matches its official-release installation record."
                    + Environment.NewLine + DoomModLoaderInstaller.ExecutablePath,
                    "KHARVOX — Integrity Verified", MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                refreshLoaderStatus();
                return;
            }
            using (var installerDialog = new DoomModLoaderInstallDialog())
                installerDialog.ShowDialog(customOptionsForm);
            refreshLoaderStatus();
            RefreshUserDoomMods();
        };
        var loaderRow = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1,
            Padding = Padding.Empty, Margin = Padding.Empty
        };
        loaderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        loaderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        loaderStatusLabel.AutoEllipsis = true;
        loaderRow.Controls.Add(loaderStatusLabel, 0, 0);
        loaderRow.Controls.Add(loaderInstallButton, 1, 0);
        doomGrid.Controls.Add(loaderRow, 0, 0);
        // There are no planned-mod placeholders. Actual packaged resource mods
        // appear only when present, always above externally supplied user mods.
        var packagedSection = new FlowLayoutPanel
        {
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false, AutoScroll = false,
            Margin = Padding.Empty, Padding = Padding.Empty,
            Visible = true, BackColor = PanelColor
        };
        var doomModItems = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoScroll = false,
            Margin = Padding.Empty,
            Padding = new Padding(2, 8, 2, 0),
            BackColor = PanelColor
        };
        packagedDoomModHeader = new Label
        {
            Text = "KHARVOX MODS",
            AutoSize = false, Width = 340, Height = 26,
            ForeColor = Color.White,
            Font = new Font(Font, FontStyle.Bold),
            TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(3, 0, 3, 0),
            Visible = true
        };
        packagedDoomModChecks = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown, WrapContents = false,
            AutoScroll = false, AutoSize = true,
            MinimumSize = new Size(340, 0),
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = PanelColor, Visible = false
        };
        var packagedToolbar = new TableLayoutPanel
        {
            Width = 406, Height = 30, ColumnCount = 2, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = PanelColor
        };
        packagedToolbar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        packagedToolbar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        packagedDoomModHeader.Width = 0;
        packagedDoomModHeader.Dock = DockStyle.Fill;
        var generateKharvoxMods = new Button
        {
            Text = "Generate", Dock = DockStyle.Fill,
            AccessibleName = "Generate first-party KHARVOX resource mods",
            BackColor = Color.FromArgb(43,43,47),
            ForeColor = Color.Gainsboro, FlatStyle = FlatStyle.Flat,
            Font = new Font(Font.FontFamily, 8.3f),
            Margin = new Padding(1,2,3,2)
        };
        generateKharvoxMods.FlatAppearance.BorderColor = Color.DimGray;
        statusToolTip.SetToolTip(generateKharvoxMods,
            "Generate experimental Aggressive Demons and Enhanced Gibs ZIPs "
            + "from YOUR installed, unmodified DOOM game resources. Requires Python 3.9+."
            + " No resources are bundled or overwritten inside DOOM.");
        generateKharvoxMods.Click += async (_, _) =>
        {
            if (KharvoxRunner.IsRunning)
            {
                MessageBox.Show(customOptionsForm, "Close DOOM before generating "
                    + "first-party resource mods.", "KHARVOX",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            if (!FirstPartyDoomModGenerator.CanOffer(doomPath.Text))
            {
                MessageBox.Show(customOptionsForm,
                    "Select your DOOM installation and use a full KHARVOX build "
                    + "containing tools/Generate-KHARVOX-Mods.py.",
                    "KHARVOX — Generator unavailable", MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }
            if (MessageBox.Show(customOptionsForm,
                "Generate the experimental Aggressive Demons and Enhanced Gibs "
                + "resource ZIPs from the original installed DOOM resources?"
                + Environment.NewLine + Environment.NewLine
                + "Output: " + DoomUserMods.PackagedFolder
                + Environment.NewLine
                + "Existing ZIPs with these two names will be replaced; "
                + "game files will not be modified. Both mods remain unchecked.",
                "KHARVOX — Generate first-party mods",
                MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
                return;
            generateKharvoxMods.Enabled = false;
            generateKharvoxMods.Text = "Working…";
            try
            {
                var report = await FirstPartyDoomModGenerator.GenerateAsync(doomPath.Text);
                RefreshUserDoomMods();
                MessageBox.Show(customOptionsForm, report
                    + Environment.NewLine + "Select generated ZIPs to enable them "
                    + "after checking resource conflicts.",
                    "KHARVOX — Generated experimental mods",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception failure)
            {
                MessageBox.Show(customOptionsForm, failure.Message,
                    "KHARVOX — Could not generate mods",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            finally
            {
                generateKharvoxMods.Text = "Generate";
                generateKharvoxMods.Enabled = true;
            }
        };
        packagedToolbar.Controls.Add(packagedDoomModHeader, 0, 0);
        packagedToolbar.Controls.Add(generateKharvoxMods, 1, 0);
        packagedDoomModEmpty = new FlowLayoutPanel
        {
            Width = 406, Height = 55, AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false, BackColor = PanelColor,
            Margin = Padding.Empty
        };
        foreach (var prototype in new[] {
            "Aggressive Demons", "Enhanced Gibs" })
        {
            var waiting = new CheckBox {
                Text = prototype + " — generate to enable",
                Checked = false, Enabled = false,
                Width = 388, Height = 27,
                AutoSize = false, ForeColor = Color.Gray,
                Margin = new Padding(3, 0, 0, 0)
            };
            statusToolTip.SetToolTip(waiting,
                "Experimental KHARVOX resource mod. Click Generate above "
                + "to create this ZIP from your original installed DOOM files. "
                + "Requires Python and DOOMModLoader. Not verified in-game.");
            packagedDoomModEmpty.Controls.Add(waiting);
        }
        packagedSection.Controls.Add(packagedToolbar);
        packagedSection.Controls.Add(packagedDoomModChecks);
        packagedSection.Controls.Add(packagedDoomModEmpty);
        doomGrid.Controls.Add(packagedSection, 0, 1);

        // USER MODS belongs in its own explicit 32px row, not in the
        // auto-sized mod list. Last column equals loaderRow's 105px column.
        var userHeader = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, RowCount = 1, ColumnCount = 4,
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = PanelColor
        };
        userHeader.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        // The old 93px heading column left invisible padding after USER MODS
        // and shifted "N detected" noticeably to the right. Measure the
        // actual label text so its neighbouring free space is truly centred.
        var userHeaderFont = new Font(Font, FontStyle.Bold);
        var userHeaderWidth = TextRenderer.MeasureText("USER MODS",
            userHeaderFont, Size.Empty,
            TextFormatFlags.NoPadding | TextFormatFlags.SingleLine).Width + 6;
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, userHeaderWidth));
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
        userHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
        userHeader.Controls.Add(new Label
        {
            Text = "USER MODS",
            Dock = DockStyle.Fill,
            ForeColor = Color.White,
            Font = userHeaderFont,
            TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(3, 0, 0, 0)
        }, 0, 0);
        var userHeaderDivider = new Panel
        {
            Dock = DockStyle.Fill,
            Margin = Padding.Empty,
            Padding = Padding.Empty,
            BackColor = PanelColor,
            AccessibleName = "User mods header divider"
        };
        // Clean count only, no decorative lines on either side.
        userDoomModCount = new Label
        {
            AccessibleName = "Detected user mod count",
            Text = FormatDetectedUserMods(0),
            AutoSize = false,
            ForeColor = Color.Silver,
            BackColor = PanelColor,
            Font = new Font(Font.FontFamily, 8.5f, FontStyle.Regular),
            TextAlign = ContentAlignment.MiddleCenter,
            AutoEllipsis = true,
            Margin = Padding.Empty
        };
        userHeaderDivider.Controls.Add(userDoomModCount);
        void FitDetectedCount()
        {
            if (userHeaderDivider.IsDisposed || userDoomModCount.IsDisposed)
                return;
            var width = userHeaderDivider.ClientSize.Width;
            var measured = TextRenderer.MeasureText(userDoomModCount.Text,
                userDoomModCount.Font, Size.Empty,
                TextFormatFlags.NoPadding | TextFormatFlags.SingleLine);
            // Text uses only as much width as it actually needs. Give the
            // remaining width equally to the lines on either side.
            var labelWidth = Math.Min(Math.Max(0, width - 20), measured.Width + 2);
            userDoomModCount.SetBounds((width - labelWidth) / 2, 0,
                labelWidth, userHeaderDivider.ClientSize.Height);
            userHeaderDivider.Invalidate();
        }
        userHeaderDivider.SizeChanged += (_, _) => FitDetectedCount();
        userDoomModCount.TextChanged += (_, _) => FitDetectedCount();
        FitDetectedCount();
        userHeader.Controls.Add(userHeaderDivider, 1, 0);
        var openUserFolder = new Button
        {
            Text = "Mod Folder", Dock = DockStyle.Fill,
            FlatStyle = FlatStyle.Flat, ForeColor = Color.White,
            BackColor = Color.FromArgb(43, 43, 47),
            Margin = new Padding(3, 2, 3, 2)
        };
        openUserFolder.FlatAppearance.BorderColor = Color.DimGray;
        statusToolTip.SetToolTip(openUserFolder,
            "Open a matching KHARVOX Mods Folder window. Drop .zip files from Firefox Downloads here.");
        openUserFolder.Click += (_, _) =>
        {
            try
            {
                var launcherBounds = WindowState == FormWindowState.Normal
                    ? Bounds : RestoreBounds;
                NexusFolderCompanion.OpenStandalone(DoomUserMods.Folder, launcherBounds);
                RefreshUserDoomMods();
            }
            catch (Exception error)
            {
                MessageBox.Show(customOptionsForm,
                    "Could not open mods folder: " + error.Message,
                    "KHARVOX", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        };
        var rescanUserMods = new Button
        {
            // Draw a vector icon instead of a platform-dependent font glyph.
            Text = "", Dock = DockStyle.Fill,
            Image = StockUiIcons.Refresh,
            ImageAlign = ContentAlignment.MiddleCenter,
            AccessibleName = "Rescan mods",
            AccessibleDescription = "Manually rescan USER MODS now. Usually changes are detected automatically.",
            FlatStyle = FlatStyle.Flat, ForeColor = Color.LightSkyBlue,
            BackColor = Color.FromArgb(43, 43, 47),
            Margin = new Padding(2, 1, 4, 1)
        };
        rescanUserMods.FlatAppearance.BorderColor = Color.DimGray;
        rescanUserMods.FlatAppearance.MouseOverBackColor = Color.FromArgb(55, 55, 63);
        rescanUserMods.MouseEnter += (_, _) =>
        {
            rescanUserMods.ForeColor = Color.White;
            rescanUserMods.Image = StockUiIcons.RefreshHover;
        };
        rescanUserMods.MouseLeave += (_, _) =>
        {
            rescanUserMods.ForeColor = Color.LightSkyBlue;
            rescanUserMods.Image = StockUiIcons.Refresh;
        };
        rescanUserMods.EnabledChanged += (_, _) =>
        {
            rescanUserMods.Image = rescanUserMods.Enabled
                ? StockUiIcons.Refresh : StockUiIcons.RefreshHover;
        };
        statusToolTip.SetToolTip(rescanUserMods,
            "Rescan USER MODS now, including replaced ZIP files. The list normally updates automatically.");
        rescanUserMods.Click += (_, _) =>
        {
            // A manual rescan catches ZIP replacements that preserve both
            // length and timestamps and checks unpacked directory contents.
            KharvoxModContentFingerprints.Invalidate();
            RefreshUserDoomMods();
        };
        userHeader.Controls.Add(rescanUserMods, 2, 0);
        userHeader.Controls.Add(openUserFolder, 3, 0);
        doomGrid.Controls.Add(userHeader, 0, 2);

        userDoomModStatus = new Label
        {
            Text = "Drop ZIPs or unpacked mod folders here.",
            AutoSize = true, MaximumSize = new Size(342, 0),
            ForeColor = Color.Silver, Margin = new Padding(3, 0, 3, 0)
        };
        doomModItems.Controls.Add(userDoomModStatus);
        userDoomModChecks = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown, WrapContents = false,
            AutoScroll = false, AutoSize = true,
            MinimumSize = new Size(340, 0),
            Margin = Padding.Empty, Padding = Padding.Empty,
            BackColor = PanelColor
        };
        doomModItems.Controls.Add(userDoomModChecks);
        doomModItemsPanel = doomModItems;
        userDoomModDebounce.Tick += (_, _) =>
        {
            userDoomModDebounce.Stop();
            KharvoxModContentFingerprints.Invalidate();
            if (customOptionsForm?.Visible == true) RefreshUserDoomMods();
            else CheckLiveConfiguration(promptOnBad: false);
        };
        // DML is separate from the mod-list watcher. A missing/renamed
        // loader file must grey out the existing checkboxes immediately,
        // without rebuilding the list or clearing checked selections.
        doomModLoaderDebounce.Tick += (_, _) =>
        {
            doomModLoaderDebounce.Stop();
            if (customOptionsForm?.Visible == true)
                RefreshDoomModLoaderAvailability();
        };

        doomGrid.Controls.Add(doomModItems, 0, 3);

        // Low-key discovery link only; external content is not installed or
        // automatically selected, and the link stays usable without DML.
        var nexusModsLink = new LinkLabel
        {
            AccessibleName = "Browse DOOM mods on Nexus Mods",
            AccessibleDescription = DoomNexusModsUrl,
            Text = "Browse DOOM mods on Nexus Mods",
            Font = new Font(Font.FontFamily, 8.5f, FontStyle.Regular),
            LinkColor = Color.Gray,
            ActiveLinkColor = Color.Gainsboro,
            VisitedLinkColor = Color.Gray,
            LinkBehavior = LinkBehavior.HoverUnderline,
            LinkArea = new LinkArea(0, "Browse DOOM mods on Nexus Mods".Length),
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(3, 1, 3, 2),
            BackColor = PanelColor
        };
        statusToolTip.SetToolTip(nexusModsLink,
            "Open Nexus in your default browser and a matching Explorer mods folder. "
            + "When the popup closes, its newly created folder window also closes.");
        nexusModsLink.LinkClicked += async (_, _) =>
        {
            if (!nexusModsLink.Enabled) return;
            var originalText = nexusModsLink.Text;
            try
            {
                // Provide immediate feedback while Windows opens a new
                // window of the user's own default browser.
                nexusModsLink.Enabled = false;
                nexusModsLink.Text = "Opening Nexus Mods…";
                nexusModsLink.LinkArea = new LinkArea(0, nexusModsLink.Text.Length);
                var dialog = customOptionsForm;
                if (dialog is null || dialog.IsDisposed)
                    throw new InvalidOperationException("Custom Mods window is unavailable.");
                var windowBounds = dialog.WindowState == FormWindowState.Normal
                    ? dialog.Bounds : dialog.RestoreBounds;
                var popupWindow = await NexusModsPopup.OpenTrackedAsync(windowBounds);
                // The folder is a disposable COMPANION of this new browser
                // popup; its bounds match the main KHARVOX launcher window.
                // A reused pre-existing Explorer window is never closed.
                var launcherBounds = WindowState == FormWindowState.Normal
                    ? Bounds : RestoreBounds;
                await NexusFolderCompanion.OpenForPopupAsync(
                    DoomUserMods.Folder, launcherBounds, popupWindow);
            }
            catch (Exception error)
            {
                MessageBox.Show(customOptionsForm,
                    "Could not open Nexus Mods: " + error.Message,
                    "KHARVOX", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            finally
            {
                nexusModsLink.Text = originalText;
                nexusModsLink.LinkArea = new LinkArea(0, originalText.Length);
                nexusModsLink.Enabled = true;
            }
        };
        doomGrid.Controls.Add(nexusModsLink, 0, 4);
        doomMods.Controls.Add(doomGrid);

        var modColumns = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty
        };
        // Give each column only what its actual controls need. The VR column
        // should no longer waste most of the window width on blank space.
        modColumns.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        modColumns.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        customMods.Dock = DockStyle.Fill;
        doomMods.Dock = DockStyle.Fill;
        modColumns.Controls.Add(doomMods, 0, 0);
        modColumns.Controls.Add(customMods, 1, 0);
        customOptionsForm = CreateCustomOptionsForm(modColumns);
        ConnectCustomModsDocking(customOptionsForm);

        var tuning = MakeGroup("MOVEMENT");
        var grid = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(18, 8, 18, 7), RowCount = 5, ColumnCount = 2 };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 187));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        for (var i = 0; i < 5; i++) grid.RowStyles.Add(new RowStyle(SizeType.Percent, 20));
        turnMode.Items.AddRange(["Smooth", "Snap", "Off"]);
        movementDirection.Items.AddRange(["Head direction", "Off hand direction"]);
        movementDirection.SelectedIndexChanged += OptionChanged;
        leftHandSwapMode.Items.AddRange(["Button swap", "Button and Stick swap"]);
        leftHanded.CheckedChanged += LeftHandedChanged;
        leftHandSwapMode.SelectedIndexChanged += OptionChanged;
        var turnAndMovementDirection = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            Margin = Padding.Empty,
            Padding = Padding.Empty
        };
        turnAndMovementDirection.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 43));
        turnAndMovementDirection.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 57));
        turnMode.Dock = DockStyle.Fill;
        movementDirection.Dock = DockStyle.Fill;
        turnAndMovementDirection.Controls.Add(turnMode, 0, 0);
        turnAndMovementDirection.Controls.Add(movementDirection, 1, 0);
        AddField(grid, 0, "Turn mode", turnAndMovementDirection);
        smoothSpeed.AccessibleName = "Turn speed";
        smoothSpeed.ValueChanged += SpeedSliderChanged;
        AddField(grid, 1, "Turn speed", MakeSpeedSlider(smoothSpeed, smoothSpeedValue));
        AddField(grid, 2, "Snap angle (°)", MakeFixedWidth(snapAngle, 126));
        leftHanded.Dock = DockStyle.Fill;
        grid.Controls.Add(leftHanded, 0, 3);
        grid.Controls.Add(leftHandSwapMode, 1, 3);
        swapJumpCrouch.Dock = DockStyle.Fill;
        swapJumpCrouch.CheckedChanged += OptionChanged;
        statusToolTip.SetToolTip(swapJumpCrouch, "Swap the gameplay Jump and Crouch actions on the A/B face buttons. Menu controls stay the same. Disabled while Weapon Wheel Button Remap is active; your saved choice is preserved.");
        grid.Controls.Add(swapJumpCrouch, 0, 4);
        grid.SetColumnSpan(swapJumpCrouch, 2);
        foreach (Control c in new Control[] { turnMode, snapAngle })
        {
            if (c is ComboBox cb) cb.SelectedIndexChanged += OptionChanged;
            if (c is NumericUpDown n) n.ValueChanged += OptionChanged;
        }
        UpdateSpeedSliderLabels();
        tuning.Controls.Add(grid);
        root.Controls.Add(tuning);

        weaponMode.Items.AddRange(["Gameplay test", "Calibration"]);
        calibrationWeapon.Items.AddRange(CalibrationWeaponNames);
        gripAlignment.Items.AddRange(["Barrel / fore-end", "Side grip"]);
        weaponMode.SelectedIndexChanged += WeaponModeChanged;
        calibrationWeapon.SelectedIndexChanged += WeaponModeChanged;
        gripAlignment.SelectedIndexChanged += WeaponModeChanged;
        hudDebugging.CheckedChanged += OptionChanged;
        extendedLogging.CheckedChanged += OptionChanged;
        disableAa.CheckedChanged += OptionChanged;
        captureEyes.CheckedChanged += OptionChanged;

        showHands.CheckedChanged += ShowHandsChanged;
        calibrateHands.CheckedChanged += HandCalibrationModeChanged;

        runtimeStatusTimer.Tick += (_, _) => RefreshRuntimeStatus();
        runtimeStatusTimer.Start();

        var action = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            RowCount = 2,
            ColumnCount = 3,
            Padding = new Padding(0, 7, 0, 0),
            BackColor = Color.Transparent
        };
        action.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
        action.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        action.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        action.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        action.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        var bloodArtwork = new OwnedImagePictureBox
        {
            Dock = DockStyle.Fill,
            SizeMode = PictureBoxSizeMode.Zoom,
            BackColor = Color.Transparent,
            Margin = new Padding(0, 0, 10, 0),
            TabStop = false,
            Enabled = false
        };
        using (var artworkStream = Assembly.GetExecutingAssembly()
                   .GetManifestResourceStream("Kharvox.Branding.BloodSplatter"))
        {
            if (artworkStream is not null)
            {
                using var embeddedArtwork = Image.FromStream(artworkStream);
                bloodArtwork.Image = new Bitmap(embeddedArtwork);
            }
        }
        action.Controls.Add(bloodArtwork, 0, 0);
        action.SetRowSpan(bloodArtwork, 2);
        launchButton.Text = "LAUNCH GAME";
        launchButton.Dock = DockStyle.Fill;
        launchButton.FlatStyle = FlatStyle.Flat;
        launchButton.BackColor = Color.Black;
        launchButton.ForeColor = Color.White;
        launchButton.Font = new Font(Font, FontStyle.Bold);
        launchButton.FlatAppearance.BorderColor = Color.DimGray;
        launchButton.Click += LaunchClicked;
        action.Controls.Add(launchButton, 1, 0);
        status.Dock = DockStyle.Fill;
        status.Text = File.Exists(Path.Combine(AppContext.BaseDirectory, "KharvoxLayer.dll")) ? "Ready." : "KharvoxLayer.dll not found.";
        status.Click += FocusRunningGameClicked;
        action.Controls.Add(status, 1, 1);
        root.Controls.Add(action);

        var footerLabel = new Label
        {
            Text = "Made by Cactus",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleRight,
            ForeColor = Color.Gray,
            Cursor = Cursors.Hand
        };
        footerLabel.MouseClick += (_, e) =>
        {
            if (e.Button != MouseButtons.Left) return;
            if ((ModifierKeys & Keys.Shift) != 0) ShowDevMode();
            else
            {
                ShowNextCracktro();
            }
        };
        footerLabel.MouseEnter += (_, _) => footerLabel.ForeColor = Color.Gainsboro;
        footerLabel.MouseLeave += (_, _) => footerLabel.ForeColor = Color.Gray;
        var footer = new TableLayoutPanel {
            Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1,
            Margin = Padding.Empty, Padding = Padding.Empty
        };
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 126));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        var releaseVersion = VrGameIntroSession.ReleaseVersion;
        var releaseInfo = typeof(MainForm).Assembly
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "";
        footer.Controls.Add(new Label {
            Text = releaseVersion.ToString(releaseVersion.Build == 0 ? 2 : 3)
                + (releaseInfo.Contains("-test") ? " Test" : releaseInfo.Contains("-beta") ? " Beta" : releaseInfo.Contains("-rc") ? " RC" : ""),
            Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = Color.Gray
        }, 0, 0);
        var customModsButton = new Button
        {
            Text = "Custom Mods…",
            Dock = DockStyle.Fill,
            Margin = new Padding(3, 0, 3, 0),
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(24, 24, 27),
            ForeColor = Color.Gainsboro,
            Font = new Font(Font.FontFamily, 8F),
            TabStop = false
        };
        customModsButton.FlatAppearance.BorderColor = Color.DimGray;
        customModsButton.FlatAppearance.MouseOverBackColor = Color.FromArgb(42, 42, 46);
        customModsButton.Click += (_, _) => ShowCustomOptions();
        footer.Controls.Add(customModsButton, 1, 0);
        footer.Controls.Add(footerLabel, 2, 0);
        root.Controls.Add(footer);
        LayoutViewport();
        LoadSettings();
        LoadCustomModSettings();
        RefreshRenderScaleAvailability();
        if (string.IsNullOrWhiteSpace(doomPath.Text) || !File.Exists(Path.Combine(doomPath.Text, "DOOMx64vk.exe")))
            doomPath.Text = KharvoxRunner.FindDoomInstall() ?? string.Empty;
        StartConfigurationTracking();
        RefreshDoomProcessState();
        FormClosing += (_, _) =>
        {
            runtimeStatusTimer.Stop();
            userDoomModDebounce.Stop();
            userDoomModWatcher?.Dispose();
            userDoomModWatcher = null;
            doomModLoaderDebounce.Stop();
            doomModLoaderWatcher?.Dispose();
            doomModLoaderWatcher = null;
            SaveSettings();
            SaveCustomModSettings(notify: false);
        };
    }

    protected override void OnLoad(EventArgs e)
    {
        base.OnLoad(e);
        FitWorkingArea(Screen.FromControl(this).WorkingArea);
    }

    protected override void OnLocationChanged(EventArgs e)
    {
        base.OnLocationChanged(e);
        if (Visible && !fittingWindow)
        {
            var work = Screen.FromControl(this).WorkingArea;
            if (work != fittedWorkArea) FitWorkingArea(work);
        }
        FollowLauncherMovement();
    }

    protected override void OnSizeChanged(EventArgs e)
    {
        base.OnSizeChanged(e);
        FollowLauncherMovementAndSize();
    }

    protected override void OnActivated(EventArgs e)
    {
        base.OnActivated(e);
        if (customOptionsForm is { } mods)
            RaiseDockCompanion(mods);
    }

    protected override void OnDpiChanged(DpiChangedEventArgs e)
    {
        base.OnDpiChanged(e);
        FitWorkingArea(Screen.FromControl(this).WorkingArea);
        FollowLauncherMovementAndSize();
    }

    internal void FitWorkingArea(Rectangle work)
    {
        if (fittingWindow || work.Width <= 0 || work.Height <= 0) return;
        fittingWindow = true;
        try
        {
            fittedWorkArea = work;
            MinimumSize = new Size(Math.Min(280, work.Width), Math.Min(240, work.Height));
            MaximumSize = work.Size;
            var desiredWidth = Width;
            if (launcherContent is {} content && content.MinimumSize.Height >
                Math.Min(ClientSize.Height, work.Height - (Height - ClientSize.Height)))
                desiredWidth = Math.Max(desiredWidth, content.MinimumSize.Width +
                    (Width - ClientSize.Width) + SystemInformation.VerticalScrollBarWidth);
            var width = Math.Min(desiredWidth, work.Width);
            var height = Math.Min(Height, work.Height);
            Bounds = new Rectangle(
                Math.Max(work.Left, Math.Min(Left, work.Right - width)),
                Math.Max(work.Top, Math.Min(Top, work.Bottom - height)), width, height);
            LayoutViewport();
        }
        finally { fittingWindow = false; }
    }

    private void LayoutViewport()
    {
        if (launcherContent is not { } content) return;
        // Preserve the DPI-scaled content height instead of squeezing rows.
        // Extremely narrow monitors retain horizontal access as well.
        content.Size = new Size(Math.Max(content.MinimumSize.Width, viewport.ClientSize.Width),
            Math.Max(content.MinimumSize.Height, viewport.ClientSize.Height));
    }

    private static CheckBox MakeCheck(string text, bool value) => new() { Text = text, Checked = value, AutoSize = true };

    internal void ApplyRendererStatus(string renderer)
    {
        if (renderer.IndexOf("SFS", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            status.Text = renderer.Trim().Replace("Renderer: ", "");
            var fsrStatus = Path.Combine(AppContext.BaseDirectory, "fsr1_status.txt");
            if (useFsrUpscaling.Checked && File.Exists(fsrStatus)
                && File.ReadAllText(fsrStatus).Contains("FSR1 ACTIVE"))
                status.Text += "; FSR1 active";
        }
        else if (renderer.IndexOf("FSR1 ACTIVE", StringComparison.OrdinalIgnoreCase) >= 0)
            status.Text = "Game running, FSR1 active";
    }

    internal void RefreshRenderScaleAvailability()
    {
        var steamVr = KharvoxRunner.UsesSteamVrRuntime();
        renderScale.Enabled = !steamVr;
        var tip = steamVr ? "Renderscale only works from within SteamVR"
            : "Scales scene and XR resolution in both renderers. 100% scene: 3840 x 2160. No application upper limit; GPU/runtime limits apply. Restart required.";
        statusToolTip.SetToolTip(renderScale, tip);
        statusToolTip.SetToolTip(renderScaleHost, tip);
    }

    private void RefreshRuntimeStatus()
    {
        RefreshRenderScaleAvailability();
        disableVrIntro.Visible = VrGameIntroSession.HasSeenCurrentRelease;
        RefreshDoomProcessState();
        try
        {
            if (!KharvoxRunner.IsRunning)
            {
                UpdateWeaponModeDescription();
                return;
            }
            if (calibrateHands.Checked)
            {
                var handPath = Path.Combine(AppContext.BaseDirectory,
                    "hand_calibration_status.txt");
                if (File.Exists(handPath))
                    weaponStatus.Text = File.ReadAllText(handPath).Trim();
            }
            var weaponPath = Path.Combine(AppContext.BaseDirectory, "two_hand_status.txt");
            if (!calibrateHands.Checked
                && File.Exists(weaponPath))
                weaponStatus.Text = File.ReadAllText(weaponPath).Trim();
            var rendererPath = Path.Combine(AppContext.BaseDirectory, "renderer_status.txt");
            if (File.Exists(rendererPath))
            {
                ApplyRendererStatus(File.ReadAllText(rendererPath));
            }
        }
        catch { /* Live status is optional. */ }
    }

    private void FocusRunningGameClicked(object? sender, EventArgs e)
    {
        if (!KharvoxRunner.IsRunning) return;
        if (!KharvoxRunner.FocusDoom())
            status.Text = "Could not focus DOOM.";
    }

    private string SelectedRendererKey() => rendererMode.SelectedIndex == 0 ? "AER" : VulkanSfs.Key;

    private void RendererModeChanged(object? sender, EventArgs e)
    {
        if (rendererMode.SelectedIndex < 0) return;
        statusToolTip.SetToolTip(rendererMode, rendererMode.SelectedIndex == 1
            ? VulkanSfs.Description : "");
        useFsrUpscaling.Enabled = true;
        RefreshRenderScaleAvailability();
        OptionChanged(sender, e);
    }
    private void WeaponModeChanged(object? sender, EventArgs e)
    {
        UpdateWeaponModeDescription();
        CheckLiveConfiguration();
    }
    private void UpdateWeaponModeDescription()
    {
        var calibrating = weaponMode.SelectedIndex == 1;
        calibrationWeapon.Enabled = calibrating;
        gripAlignment.Enabled = calibrating;
        if (calibrating)
            weaponStatus.Text = $"Equip {SelectedCalibrationWeaponName()}, hold both hands in place, then press {(leftHanded.Checked ? "LEFT" : "RIGHT")} TRIGGER to save its profile. The shot is blocked.";
        else
            weaponStatus.Text = leftHanded.Checked
                ? "Left Hand mode: weapon on the left, support on the right. Saved support points are mirrored automatically."
                : "Every recognized weapon uses its own saved support point and aim alignment. Uncalibrated weapons remain safely one-handed.";
    }

    private string SelectedCalibrationWeaponKey()
    {
        var index = ClampInt(calibrationWeapon.SelectedIndex, 0, CalibrationWeaponKeys.Length - 1);
        return CalibrationWeaponKeys[index];
    }

    private string SelectedCalibrationWeaponName()
    {
        var index = ClampInt(calibrationWeapon.SelectedIndex, 0, CalibrationWeaponNames.Length - 1);
        return CalibrationWeaponNames[index];
    }

    private string SelectedBackWeaponKey()
    {
        var index = ClampInt(backWeapon.SelectedIndex, 0, BackWeaponKeys.Length - 1);
        return BackWeaponKeys[index];
    }
    private static NumericUpDown MakeNumber(decimal min, decimal max, decimal value, int decimals, decimal increment = 1) =>
        new() { Minimum = min, Maximum = max, Value = value, DecimalPlaces = decimals, Increment = increment, Dock = DockStyle.Fill, TextAlign = HorizontalAlignment.Left };
    private static TrackBar MakeSlider(int minimum, int maximum, int value,
        int tickFrequency, int largeChange) => new()
    {
        Minimum = minimum,
        Maximum = maximum,
        Value = value,
        TickFrequency = tickFrequency,
        SmallChange = 1,
        LargeChange = largeChange,
        TickStyle = TickStyle.BottomRight,
        AutoSize = false,
        Dock = DockStyle.Fill,
        Margin = Padding.Empty
    };
    private static Label MakeSliderValueLabel() => new()
    {
        AutoSize = true,
        Anchor = AnchorStyles.Right,
        TextAlign = ContentAlignment.MiddleRight,
        ForeColor = Color.Gainsboro,
        Margin = new Padding(0, 0, 0, 1),
        UseCompatibleTextRendering = true
    };
    private static GroupBox MakeGroup(string text) => new() { Text = text, Dock = DockStyle.Fill, ForeColor = Color.Gainsboro, BackColor = PanelColor, Padding = new Padding(8) };

    private static Control MakeFixedWidth(Control control, int width)
    {
        var host = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, Margin = Padding.Empty };
        host.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, width));
        host.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        control.Dock = DockStyle.Fill;
        host.Controls.Add(control, 0, 0);
        return host;
    }

    private static Control MakeSpeedSlider(TrackBar slider, Label valueLabel)
    {
        var host = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 4,
            Margin = Padding.Empty,
            Padding = Padding.Empty
        };
        host.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 40));
        host.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        host.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
        host.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 58));
        host.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        host.Controls.Add(new Label
        {
            Text = "Slow",
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = Color.Silver,
            Margin = new Padding(0, 0, 0, 1),
            UseCompatibleTextRendering = true
        }, 0, 0);
        host.Controls.Add(slider, 1, 0);
        host.Controls.Add(new Label
        {
            Text = "Fast",
            AutoSize = true,
            Anchor = AnchorStyles.Right,
            TextAlign = ContentAlignment.MiddleRight,
            ForeColor = Color.Silver,
            Margin = new Padding(0, 0, 0, 1),
            UseCompatibleTextRendering = true
        }, 2, 0);
        host.Controls.Add(valueLabel, 3, 0);
        return host;
    }

    private decimal SelectedPhysicalGlorykillSpeed() =>
        GloryKillSpeedMinimum + physicalGlorykillSpeed.Value * GloryKillSpeedStep;

    // Preserve the existing 0–10 native setting while exposing a compact
    // 1–5 menu. Level 10 leaves DOOM's original timing alone; level 0
    // explicitly forces 1.0x during sync kills.
    private static int GKMenuToEngine(int menuValue) => menuValue switch
    {
        1 => 10, 2 => 7, 3 => 5, 4 => 3, 5 => 0,
        _ => 10
    };

    private static int GKEngineToMenu(int engineValue) => engineValue switch
    {
        >= 10 => 1, >= 7 => 2, >= 5 => 3, >= 2 => 4, _ => 5
    };

    private void UpdateSpeedSliderLabels()
    {
        smoothSpeedValue.Text = smoothSpeed.Value + "°/s";
        physicalGlorykillSpeedValue.Text = SelectedPhysicalGlorykillSpeed()
            .ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " m/s";
        var speedValue = GKEngineToMenu(gloryKillSlowmo.Value);
        if (gloryKillSpeedSlider.Value != speedValue)
            gloryKillSpeedSlider.Value = speedValue;
        var displayedGloryKillSpeed = GKEngineToMenu(gloryKillSlowmo.Value);
        gloryKillSlowmoValue.Text = displayedGloryKillSpeed switch
        {
            1 => "1 (Native)",
            5 => "5 (Fastest)",
            _ => displayedGloryKillSpeed.ToString()
        };
    }

    private void SpeedSliderChanged(object? sender, EventArgs e)
    {
        UpdateSpeedSliderLabels();
        OptionChanged(sender, e);
    }

    private void CustomModChanged(object? sender, EventArgs e)
    {
        if (applyingCustomModDependencies || restoringConfiguration) return;
        ApplyCustomModDependencies(sender as CheckBox);
        SaveCustomModSettings();
        OptionChanged(sender, e);
    }

    private void WeaponWheelRemapChanged(object? sender, EventArgs e)
    {
        if (applyingCustomModDependencies || restoringConfiguration) return;
        ApplyCustomModDependencies(weaponWheelRemap);
        SaveCustomModSettings();
        OptionChanged(sender, e);
    }

    private void ShowHandsChanged(object? sender, EventArgs e)
    {
        if (!showHands.Checked) calibrateHands.Checked = false;
        if (!applyingCustomModDependencies)
        {
            ApplyCustomModDependencies(showHands);
            SaveCustomModSettings();
        }
        OptionChanged(sender, e);
    }

    private void ApplyCustomModDependencies(CheckBox? changed)
    {
        if (applyingCustomModDependencies) return;
        applyingCustomModDependencies = true;
        try
        {
            // Fail closed: the previous native dash physics setter caused
            // void teleportation/death. Keep this experimental control visible
            // but unavailable until a verified safe dash replaces it.
            customDirectionalDash.Checked = false;
            customDirectionalDash.Enabled = false;
            // Dash consumes the A button that Behind-Head Weapon Wheel frees.
            // Keep that relationship explicit instead of allowing a checkbox
            // combination that can never work in-game.
            if (changed == customDirectionalDash && customDirectionalDash.Checked)
            {
                weaponWheelRemap.Checked = true;
                customBehindHeadWeaponWheel.Checked = true;
                customDisableWeaponWheel.Checked = false;
            }

            // Selecting a child option should automatically enable prerequisites
            // rather than presenting an unavailable/greyed-out checkbox.
            if (changed == customBehindHeadWheelHandSelection
                && customBehindHeadWheelHandSelection.Checked)
            {
                customBehindHeadWeaponWheel.Checked = true;
                customDisableWeaponWheel.Checked = false;
            }

            // A hard "No Weapon Wheel" request wins over every wheel-dependent
            // feature and therefore also releases Dash.
            if (changed == customLeftWristWeaponWheel
                && customLeftWristWeaponWheel.Checked)
                customDisableWeaponWheel.Checked = false;

            if (changed == customDisableWeaponWheel && customDisableWeaponWheel.Checked)
            {
                customBehindHeadWeaponWheel.Checked = false;
                customBehindHeadWheelHandSelection.Checked = false;
                customLeftWristWeaponWheel.Checked = false;
                customDirectionalDash.Checked = false;
            }

            // No HUD and Back-of-Hand HUD use the same gameplay-HUD surfaces.
            // The option explicitly enabled most recently wins.
            if (changed == customDisableHud && customDisableHud.Checked)
                customBackOfHandHud.Checked = false;
            else if (changed == customBackOfHandHud && customBackOfHandHud.Checked)
                customDisableHud.Checked = false;

            // Dynamic Quick Holsters and hand calibration need rendered hands.
            // They force Enable Hands ON and keep it visibly locked while active.
            // The launcher checkbox unlocks when no feature requires it.
            if (customDynamicShoulderHolster.Checked || calibrateHands.Checked)
                showHands.Checked = true;

            // If the user explicitly removes a dependency, that manual
            // action wins and Dash is released. During initial config repair,
            // however, an already-selected Dash enables what it needs.
            if ((changed == customBehindHeadWeaponWheel
                    && !customBehindHeadWeaponWheel.Checked)
                || (changed == weaponWheelRemap && !weaponWheelRemap.Checked))
                customDirectionalDash.Checked = false;
            if (changed == customBehindHeadWeaponWheel
                && !customBehindHeadWeaponWheel.Checked)
                customBehindHeadWheelHandSelection.Checked = false;

            // Final invariant pass also repairs older saved configs.
            if (customDirectionalDash.Checked)
            {
                weaponWheelRemap.Checked = true;
                customBehindHeadWeaponWheel.Checked = true;
                customDisableWeaponWheel.Checked = false;
            }
            else if (customDisableWeaponWheel.Checked)
            {
                customBehindHeadWeaponWheel.Checked = false;
                customLeftWristWeaponWheel.Checked = false;
            }
            else if (customBehindHeadWeaponWheel.Checked)
            {
                customDisableWeaponWheel.Checked = false;
            }

            // Repair legacy saved states: child remains off until its user
            // explicitly selects it, but selecting it auto-checks the parent.
            if (!customBehindHeadWeaponWheel.Checked)
                customBehindHeadWheelHandSelection.Checked = false;

            if (customDisableHud.Checked)
                customBackOfHandHud.Checked = false;

            // Mutual exclusions become visibly disabled, not just
            // auto-unchecked. An unavailable control is greyed out until
            // the conflicting choice is explicitly turned off.
            customBackOfHandHud.Enabled = !customDisableHud.Checked;
            customDisableHud.Enabled = !customBackOfHandHud.Checked;
            customBehindHeadWeaponWheel.Enabled = !customDisableWeaponWheel.Checked;
            customLeftWristWeaponWheel.Enabled = !customDisableWeaponWheel.Checked;
            customDirectionalDash.Enabled = false; // unsafe native dash halted
            customDisableWeaponWheel.Enabled = !customBehindHeadWeaponWheel.Checked
                && !customLeftWristWeaponWheel.Checked
                && !customDirectionalDash.Checked;
            // Main-launcher dependencies: keep the effective required state
            // checked, greyed and explained; unlock as soon as the mod releases it.
            showHands.Enabled = !customDynamicShoulderHolster.Checked
                && !calibrateHands.Checked;
            showHands.AccessibleDescription = !showHands.Enabled
                ? (customDynamicShoulderHolster.Checked
                    ? "Locked ON: Dynamic Quick Holsters requires Enable Hands."
                    : "Locked ON: hand calibration requires Enable Hands.")
                : "Show the VR hand models.";
            statusToolTip.SetToolTip(showHands, showHands.AccessibleDescription);
            // Weapon Wheel Button Remap explicitly owns gameplay A/B routing;
            // Swap Jump/Crouch would have no effect. Preserve its checked value.
            swapJumpCrouch.Enabled = !weaponWheelRemap.Checked;
            swapJumpCrouch.AccessibleDescription = weaponWheelRemap.Checked
                ? "Inactive: Weapon Wheel Button Remap controls Jump and Crouch. The saved Swap Jump/Crouch preference is preserved."
                : "Swap gameplay Jump and Crouch while retaining menu controls.";
            // R-only dependencies must never disable the child's checkbox.
            // Selecting Hand Selection enables its wheel dependency above.
            customBehindHeadWheelHandSelection.Enabled = false; // merged into main wrist-wheel mod
        }
        finally
        {
            applyingCustomModDependencies = false;
        }
    }

    private CustomModSettings ReadCustomModSettingsFromControls() => new()
    {
        DisableHud = customDisableHud.Checked,
        DisableWeaponWheel = customDisableWeaponWheel.Checked,
        GaussChargeSlowMovement = customGaussChargeSlowMovement.Checked,
        BackOfHandHud = customBackOfHandHud.Checked,
        HandFocusedRs = customHandFocusedRs.Checked,
        DirectionalDash = customDirectionalDash.Checked,
        BehindHeadWeaponWheel = customBehindHeadWeaponWheel.Checked,
        LeftWristWeaponWheel = customLeftWristWeaponWheel.Checked,
        BehindHeadWheelHandSelection = customBehindHeadWeaponWheel.Checked,
        PhysicalCrouch = customPhysicalCrouch.Checked,
        RevengeDemon = customRevengeDemon.Checked,
        DynamicShoulderHolster = customDynamicShoulderHolster.Checked,
        PhysicalGrenadeThrow = customPhysicalGrenadeThrow.Checked,
        MotionGloryKillSpeed = customMotionGloryKillSpeed.Checked,
        PhysicalChainsawGestures = customPhysicalChainsawGestures.Checked,
        HapticOverhaul = customHapticOverhaul.Checked
    };

    private void SaveCustomModSettings(bool notify = true)
    {
        try
        {
            CustomModSettingsStore.Save(ReadCustomModSettingsFromControls());
            customModsSaveFailed = false;
        }
        catch (Exception error)
        {
            customModsSaveFailed = true;
            status.Text = "VR mod changes NOT saved — check folder permissions.";
            if (notify && Visible && !IsDisposed)
                MessageBox.Show(this, "KHARVOX could not save your VR mod selections."
                    + Environment.NewLine + "The next DOOM launch would use older selections."
                    + Environment.NewLine + Environment.NewLine + error.Message,
                    "KHARVOX — Settings not saved", MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
        }
    }

    private void LoadCustomModSettings()
    {
        var mods = CustomModSettingsStore.Load();
        applyingCustomModDependencies = true;
        try
        {
            customDisableHud.Checked = mods.DisableHud;
            customDisableWeaponWheel.Checked = mods.DisableWeaponWheel;
            customGaussChargeSlowMovement.Checked = mods.GaussChargeSlowMovement;
            customBackOfHandHud.Checked = mods.BackOfHandHud;
            customHandFocusedRs.Checked = mods.HandFocusedRs;
            customDirectionalDash.Checked = mods.DirectionalDash;
            customBehindHeadWeaponWheel.Checked = mods.BehindHeadWeaponWheel;
            customLeftWristWeaponWheel.Checked = mods.LeftWristWeaponWheel;
            customBehindHeadWheelHandSelection.Checked = mods.BehindHeadWeaponWheel;
            customPhysicalCrouch.Checked = mods.PhysicalCrouch;
            customRevengeDemon.Checked = mods.RevengeDemon;
            customDynamicShoulderHolster.Checked = mods.DynamicShoulderHolster;
            customPhysicalGrenadeThrow.Checked = mods.PhysicalGrenadeThrow;
            customMotionGloryKillSpeed.Checked = mods.MotionGloryKillSpeed;
            customPhysicalChainsawGestures.Checked = mods.PhysicalChainsawGestures;
            customHapticOverhaul.Checked = mods.HapticOverhaul;
        }
        finally
        {
            applyingCustomModDependencies = false;
        }
        ApplyCustomModDependencies(null);
        SaveCustomModSettings();
    }

    private static void AddField(TableLayoutPanel grid, int row, string label, Control control)
    {
        grid.Controls.Add(new Label { Text = label, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft }, 0, row);
        grid.Controls.Add(control, 1, row);
    }

    private void ApplyPreset()
    {
        if (preset.SelectedIndex < 0 || preset.SelectedIndex == 3) return;
        applyingPreset = true;
        try
        {
            showHands.Checked = LauncherPresetPolicy.DefaultEnableHands;
            handsJump.Checked = true;
            disableAa.Checked = false;
            captureEyes.Checked = false;

            if (LauncherPresetPolicy.TryGet(preset.SelectedIndex, out var defaults))
            {
                rendererMode.SelectedIndex = RendererSelection.Index(defaults.RendererMode);
                renderScale.Value = defaults.RenderScale;
                useFsrUpscaling.Checked = defaults.UseFsrUpscaling;
                intense.Checked = defaults.ImmersiveMode;
                cinematicFreelook.Checked = defaults.CinematicFreelook;
                otherCinematicsInQuad.Checked = defaults.RegularCinematicsInCineWindow;
                cinewindowFollowsHeadset.Checked = defaults.CineWindowFollowsHeadset;
                physicalGlorykill.Checked = defaults.PhysicalGloryKills;
                physicalGlorykillSpeed.Value = GloryKillSpeedToSliderValue(
                    defaults.PhysicalGloryKillSpeed);
                physicalGlorykillHands.SelectedIndex = defaults.GloryKillHands;
                backWeapon.SelectedIndex = defaults.ShoulderWeapon;
                virtualGunstock.Checked = defaults.VirtualGunstock;
                laserSight.Checked = defaults.LaserSight;
                enableBhaptics.Checked = defaults.EnableBhaptics;
                usePsvr2Toolkit.Checked = defaults.UsePsvr2Toolkit;
                turnMode.SelectedIndex = defaults.TurnMode;
                movementDirection.SelectedIndex = defaults.MovementDirection;
                smoothSpeed.Value = defaults.TurnSpeed;
                snapAngle.Value = defaults.SnapAngle;
                leftHanded.Checked = defaults.LeftHandMode;
                leftHandSwapMode.SelectedIndex = defaults.LeftHandSwapMode;
            }
            else
            {
                rendererMode.SelectedIndex = RendererSelection.Index(VulkanSfs.Key);
                renderScale.Value = 100m;
                useFsrUpscaling.Checked = false;
                physicalGlorykill.Checked = true;
                physicalGlorykillSpeed.Value = GloryKillSpeedToSliderValue(DefaultGloryKillSpeed);
                physicalGlorykillHands.SelectedIndex = 2;
                otherCinematicsInQuad.Checked = false;
                cinewindowFollowsHeadset.Checked = false;
                smoothSpeed.Value = DefaultSmoothTurnSpeed;
                snapAngle.Value = 45m;
                intense.Checked = false;
                cinematicFreelook.Checked = false;
                turnMode.SelectedIndex = 1;
                movementDirection.SelectedIndex = 0;
                virtualGunstock.Checked = true;
            }
        }
        finally { applyingPreset = false; }
        cinematicFreelook.Enabled = intense.Checked;
        otherCinematicsInQuad.Enabled = intense.Checked;
        smoothSpeed.Enabled = turnMode.SelectedIndex == 0;
        snapAngle.Enabled = turnMode.SelectedIndex == 1;
    }

    private void OptionChanged(object? sender, EventArgs e)
    {
        if (!restoringConfiguration && !applyingPreset && Visible
            && preset.SelectedIndex >= 0 && preset.SelectedIndex != 3)
            preset.SelectedIndex = 3;
        smoothSpeed.Enabled = turnMode.SelectedIndex == 0;
        snapAngle.Enabled = turnMode.SelectedIndex == 1;
        CheckLiveConfiguration();
    }

    private void HandCalibrationModeChanged(object? sender, EventArgs e)
    {
        if (calibrateHands.Checked)
            showHands.Checked = true;
        if (!restoringConfiguration && !applyingCustomModDependencies)
            ApplyCustomModDependencies(null);
        OptionChanged(sender, e);
    }

    private void ImmersiveModeChanged(object? sender, EventArgs e)
    {
        cinematicFreelook.Enabled = intense.Checked;
        otherCinematicsInQuad.Enabled = intense.Checked;
        OptionChanged(sender, e);
    }

    private void PhysicalGlorykillChanged(object? sender, EventArgs e)
    {
        physicalGlorykillSpeed.Enabled = physicalGlorykill.Checked;
        physicalGlorykillHands.Enabled = physicalGlorykill.Checked;
        OptionChanged(sender, e);
    }

    private void LeftHandedChanged(object? sender, EventArgs e)
    {
        leftHandSwapMode.Enabled = leftHanded.Checked;
        UpdateWeaponModeDescription();
        OptionChanged(sender, e);
    }

    private void ShowNextCracktro()
    {
        cracktroForm?.Close();
        cracktroForm = new CracktroForm(nextCracktroIsAmiga);
        cracktroForm.Show(this);
        cracktroForm.Activate();
        nextCracktroIsAmiga = !nextCracktroIsAmiga;
    }

    private void HideCustomOptionsAndFocusLauncher(Form form)
    {
        // Both the Close button and the title-bar X must restore focus to
        // KHARVOX after hiding the sidecar. Preserve the form for reopening.
        form.Hide();
        if (IsDisposed || Disposing || !Visible
            || WindowState == FormWindowState.Minimized) return;
        BringToFront();
        Activate();
        Focus();
    }

    private Form CreateCustomOptionsForm(Control content)
    {
        var form = new Form
        {
            Text = "KHARVOX Custom Mods",
            // Open at the SAME narrowest width the resize border allows:
            // the user should not need to drag the edge inward every time.
            ClientSize = new Size(876, 600),
            MinimumSize = new Size(890, 530),
            FormBorderStyle = FormBorderStyle.FixedSingle,
            StartPosition = FormStartPosition.Manual,
            BackColor = Color.Black,
            ForeColor = Color.WhiteSmoke,
            Font = new Font("Segoe UI", 9F),
            AutoScaleMode = AutoScaleMode.Dpi,
            ShowInTaskbar = false,
            MaximizeBox = false
        };
        if (Icon is not null) form.Icon = Icon;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(12),
            RowCount = 2,
            ColumnCount = 1,
            BackColor = Color.Black
        };
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
        content.Dock = DockStyle.Fill;
        root.Controls.Add(content, 0, 0);

        var closeButton = new Button
        {
            Text = "Close",
            Anchor = AnchorStyles.Right,
            Size = new Size(92, 28),
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(24, 24, 27),
            ForeColor = Color.White
        };
        closeButton.FlatAppearance.BorderColor = Color.DimGray;
        closeButton.Click += (_, _) => HideCustomOptionsAndFocusLauncher(form);

        var footerBar = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Color.Black,
            Margin = Padding.Empty
        };
        var closeRow = new FlowLayoutPanel
        {
            Dock = DockStyle.Right, Width = 238,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Margin = Padding.Empty,
            Padding = new Padding(0, 5, 0, 0)
        };
        closeRow.Controls.Add(closeButton);
        // Keep credit subdued and anchored in the Custom Mods footer, just
        // to the left of Close. Neither mod list nor window sizing is changed.
        var customModsCredit = new Label
        {
            AccessibleName = "Custom Mods creator credit",
            Text = "Made by MrEGS",
            AutoSize = false,
            Size = new Size(108, 28),
            TextAlign = ContentAlignment.MiddleRight,
            ForeColor = Color.Gray,
            BackColor = Color.Black,
            Font = new Font(form.Font.FontFamily, 8.5f, FontStyle.Regular),
            Margin = new Padding(0, 0, 12, 0)
        };
        closeRow.Controls.Add(customModsCredit);
        footerBar.Controls.Add(closeRow);
        // Deliberately centred within the entire sidecar, not one DOOM/VR column.
        var doomguy = new DoomFaceEasterEgg();
        void PlaceDoomguy() => doomguy.Location = new Point(
            (footerBar.ClientSize.Width - doomguy.Width) / 2,
            Math.Max(0, (footerBar.ClientSize.Height - doomguy.Height) / 2));
        footerBar.Controls.Add(doomguy);
        footerBar.SizeChanged += (_, _) => PlaceDoomguy();
        PlaceDoomguy();
        root.Controls.Add(footerBar, 0, 1);
        form.Controls.Add(root);
        form.Shown += (_, _) =>
        {
            // The sidecar uses the main launcher height, not the number of
            // checkboxes. Avoid a first-show content fit that undocks it.
            PlaceCustomModsBesideLauncher();
        };
        form.FormClosing += (_, e) =>
        {
            if (e.CloseReason != CloseReason.UserClosing) return;
            e.Cancel = true;
            HideCustomOptionsAndFocusLauncher(form);
        };
        return form;
    }

    private void QueueUserDoomModScan()
    {
        // Watcher callbacks run off-thread; debounce bursts of Explorer writes.
        if (IsDisposed || !IsHandleCreated) return;
        try
        {
            BeginInvoke((Action)(() =>
            {
                if (IsDisposed) return;
                userDoomModDebounce.Stop();
                userDoomModDebounce.Start();
            }));
        }
        catch (InvalidOperationException) { }
    }

    private void EnsureUserDoomModWatcher()
    {
        if (userDoomModWatcher is not null) return;
        Directory.CreateDirectory(DoomUserMods.Folder);
        userDoomModWatcher = new FileSystemWatcher(DoomUserMods.Folder)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName
                | NotifyFilters.LastWrite | NotifyFilters.Size,
            IncludeSubdirectories = true
        };
        userDoomModWatcher.Created += (_, _) => QueueUserDoomModScan();
        userDoomModWatcher.Deleted += (_, _) => QueueUserDoomModScan();
        userDoomModWatcher.Renamed += (_, _) => QueueUserDoomModScan();
        userDoomModWatcher.Changed += (_, _) => QueueUserDoomModScan();
        userDoomModWatcher.Error += (_, _) => QueueUserDoomModScan();
        userDoomModWatcher.EnableRaisingEvents = true;
    }

    // Watch from the application directory, NOT the loader directory.
    // Deleting the entire "tools" or "doommodloader" folder must not destroy
    // the watcher; restoring the folder must be detected without reopening
    // Custom Mods. Events from unrelated KHARVOX files are filtered out.
    internal static bool IsDoomModLoaderChange(string path, string installDirectory)
    {
        if (string.IsNullOrWhiteSpace(path)) return false;
        try
        {
            var install = Path.GetFullPath(installDirectory).TrimEnd('\\', '/');
            var tools = Path.GetDirectoryName(install);
            var changed = Path.GetFullPath(path).TrimEnd('\\', '/');
            return string.Equals(changed, tools, StringComparison.OrdinalIgnoreCase)
                || string.Equals(changed, install, StringComparison.OrdinalIgnoreCase)
                || changed.StartsWith(install + Path.DirectorySeparatorChar,
                    StringComparison.OrdinalIgnoreCase);
        }
        catch (ArgumentException) { return false; }
        catch (NotSupportedException) { return false; }
        catch (PathTooLongException) { return false; }
    }

    // Called for Created, Deleted, Changed and both ends of Rename events.
    // Windows delivers these off the UI thread and often in short bursts.
    private void QueueDoomModLoaderRefresh(string? path, string? oldPath = null)
    {
        if (!IsDoomModLoaderChange(path ?? string.Empty,
                DoomModLoaderInstaller.InstallDirectory)
            && !IsDoomModLoaderChange(oldPath ?? string.Empty,
                DoomModLoaderInstaller.InstallDirectory)) return;
        if (IsDisposed || !IsHandleCreated) return;
        try
        {
            BeginInvoke((Action)(() =>
            {
                if (IsDisposed) return;
                doomModLoaderDebounce.Stop();
                doomModLoaderDebounce.Start();
            }));
        }
        catch (InvalidOperationException) { }
    }

    private void EnsureDoomModLoaderWatcher()
    {
        if (doomModLoaderWatcher is not null) return;
        // AppContext.BaseDirectory always exists while the launcher runs;
        // do not create a fake DML folder just to monitor its existence.
        var watcher = new FileSystemWatcher(AppContext.BaseDirectory)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName
                | NotifyFilters.LastWrite | NotifyFilters.Size,
            IncludeSubdirectories = true
        };
        watcher.Created += (_, e) => QueueDoomModLoaderRefresh(e.FullPath);
        watcher.Deleted += (_, e) => QueueDoomModLoaderRefresh(e.FullPath);
        watcher.Changed += (_, e) => QueueDoomModLoaderRefresh(e.FullPath);
        watcher.Renamed += (_, e) => QueueDoomModLoaderRefresh(e.FullPath, e.OldFullPath);
        // A notification-buffer overflow can hide a deletion; recheck in
        // that case rather than continuing to report a stale Verified state.
        watcher.Error += (_, _) =>
            QueueDoomModLoaderRefresh(DoomModLoaderInstaller.InstallDirectory);
        watcher.EnableRaisingEvents = true;
        doomModLoaderWatcher = watcher;
    }

    private static void ApplyDoomModLoaderGate(
        FlowLayoutPanel? userMods, FlowLayoutPanel? packagedMods, bool verified)
    {
        foreach (var list in new[] { userMods, packagedMods })
        {
            if (list is null) continue;
            // Rows now contain a checkbox and a hover-only drag grip.
            // Keep missing/deleted mod checkboxes editable as before.
            var options = list.Controls.OfType<CheckBox>()
                .Concat(list.Controls.OfType<Panel>()
                    .SelectMany(row => row.Controls.OfType<CheckBox>()));
            foreach (var option in options)
                SetDoomModCheckboxAvailability(option, verified,
                    missing: option.Tag is bool isMissing && isMissing);
        }
    }

    private void RefreshDoomModLoaderAvailability()
    {
        refreshDoomModLoaderStatus?.Invoke();
        ApplyDoomModLoaderGate(userDoomModChecks, packagedDoomModChecks,
            DoomModLoaderInstaller.IsInstalled);
    }

    // Reusable gate for every resource-mod checkbox (user, packaged or
    // missing). Never mutate Checked: saved choices must survive DML repair.
    private static void SetDoomModCheckboxAvailability(
        CheckBox option, bool loaderVerified, bool missing)
    {
        option.Enabled = loaderVerified;
        option.ForeColor = !loaderVerified ? Color.Gray
            : missing ? Color.Orange : Color.Gainsboro;
    }

    private static string FormatDetectedUserMods(int count) =>
        Math.Max(0, count).ToString(System.Globalization.CultureInfo.InvariantCulture)
        + " detected";

    private void RefreshUserDoomMods()
    {
        if (userDoomModChecks is null || userDoomModStatus is null
            || userDoomModCount is null
            || packagedDoomModChecks is null || packagedDoomModHeader is null) return;
        try
        {
            // Keep the DML status and selection availability in sync on each
            // open, manual refresh, disk rescan and post-install refresh.
            refreshDoomModLoaderStatus?.Invoke();
            var loaderVerified = DoomModLoaderInstaller.IsInstalled;
            var detected = DoomUserMods.Scan(doomPath.Text);
            var selections = DoomUserMods.LoadSelections();
            var list = userDoomModChecks;
            var packagedList = packagedDoomModChecks;
            list.SuspendLayout();
            packagedList.SuspendLayout();
            try
            {
                foreach (var target in new[] { list, packagedList })
                    foreach (Control child in target.Controls.Cast<Control>().ToArray())
                    {
                        target.Controls.Remove(child);
                        child.Dispose();
                    }

                // Persistent DRAG DISPLAY order. Resource installation remains
                // controlled by DOOMModLoader and conflict preflight as before.
                var orderedMods = OrderDoomModRows(detected, ReadDoomDisplayOrder());
                for (var index = 0; index < orderedMods.Length; index++)
                {
                    var mod = orderedMods[index];
                    var option = new CheckBox
                    {
                        Text = (index + 1).ToString("00") + ". " + mod.Name
                            + (mod.IsFromDoom ? "  [DOOM Mods]" : ""),
                        Checked = selections.Contains(mod.Id),
                        AutoSize = true,
                        MaximumSize = new Size(335, 0),
                        Margin = new Padding(3, 2, 3, 2)
                    };
                    option.Tag = false;
                    SetDoomModCheckboxAvailability(option, loaderVerified, missing: false);
                    statusToolTip.SetToolTip(option, mod.FullPath + Environment.NewLine
                        + (loaderVerified
                            ? "Selection is saved and will apply on the next DOOM launch."
                            : "Install or repair DOOMModLoader to enable resource-mod selection."));
                    option.CheckedChanged += (_, _) =>
                    {
                        try
                        {
                            var saved = DoomUserMods.LoadSelections();
                            if (option.Checked) saved.Add(mod.Id);
                            else saved.Remove(mod.Id);
                            DoomUserMods.SaveSelections(saved);
                            CheckLiveConfiguration();
                        }
                        catch (Exception ex)
                        {
                            MessageBox.Show(customOptionsForm, ex.Message,
                                "KHARVOX — Unable to save mod selection",
                                MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        }
                    };
                    var draggableRow = CreateDoomDraggableRow(option, mod);
                    if (mod.IsPackaged) packagedList.Controls.Add(draggableRow);
                    else list.Controls.Add(draggableRow);
                }

                // An enabled mod may have been deleted outside KHARVOX. Keep
                // a visible, checked missing entry so it can be unchecked.
                var found = new HashSet<string>(detected.Select(x => x.Id),
                    StringComparer.OrdinalIgnoreCase);
                var missingList = selections.Except(found,
                        StringComparer.OrdinalIgnoreCase)
                    .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray();
                for (var missingIndex = 0; missingIndex < missingList.Length; missingIndex++)
                {
                    var missingId = missingList[missingIndex];
                    var missingOption = new CheckBox
                    {
                        Text = (orderedMods.Length + missingIndex + 1).ToString("00")
                            + ". " + missingId + " — missing (uncheck to remove)",
                        Checked = true,
                        AutoSize = true,
                        MaximumSize = new Size(335, 0),
                        Margin = new Padding(3, 2, 3, 2)
                    };
                    missingOption.Tag = true;
                    SetDoomModCheckboxAvailability(missingOption,
                        loaderVerified, missing: true);
                    statusToolTip.SetToolTip(missingOption, loaderVerified
                        ? "Uncheck to remove this missing selection."
                        : "Install or repair DOOMModLoader to edit mod selections.");
                    missingOption.CheckedChanged += (_, _) =>
                    {
                        if (missingOption.Checked) return;
                        try
                        {
                            var saved = DoomUserMods.LoadSelections();
                            saved.Remove(missingId);
                            DoomUserMods.SaveSelections(saved);
                            CheckLiveConfiguration();
                            BeginInvoke((Action)RefreshUserDoomMods);
                        }
                        catch (Exception error)
                        {
                            MessageBox.Show(customOptionsForm, error.Message,
                                "KHARVOX — Unable to save selection",
                                MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        }
                    };
                    if (missingId.StartsWith("kharvox:", StringComparison.OrdinalIgnoreCase))
                        packagedList.Controls.Add(missingOption);
                    else list.Controls.Add(missingOption);
                }
                RenumberDoomDisplayRows();
            }
            finally
            {
                list.ResumeLayout();
                packagedList.ResumeLayout();
            }
            var showPackaged = packagedList.Controls.Count != 0;
            packagedDoomModHeader.Visible = true;
            packagedList.Visible = showPackaged;
            if (packagedDoomModEmpty is not null)
                packagedDoomModEmpty.Visible = !showPackaged;
            // Always show the Generate control; missing ZIPs are no longer
            // mistaken for native VR features that vanished from the build.
            if (packagedList.Parent is Control packagedRow)
                packagedRow.Visible = true;

            var countMissing = selections.Count(x => !detected.Any(entry =>
                string.Equals(entry.Id, x, StringComparison.OrdinalIgnoreCase)));
            // Show the user-mod count in the USER MODS toolbar, excluding
            // separately listed packaged KHARVOX resource mods.
            userDoomModCount.Text = FormatDetectedUserMods(
                detected.Count(entry => !entry.IsPackaged));
            // Keep the old status row only for exceptional/empty states;
            // don't waste an entire line repeating the normal count.
            userDoomModStatus.Visible = countMissing != 0 || detected.Count == 0;
            userDoomModStatus.Text = countMissing != 0
                ? countMissing + " selected mod(s) missing."
                : detected.Count == 0
                    ? "No mods found. Add ZIPs or unpacked folders to KHARVOX/mods/doom/user."
                    : string.Empty;
            // This is a passive update: changes on disk may alter reputation,
            // but should never interrupt browsing with a "known bad" popup.
            CheckLiveConfiguration(promptOnBad: false);
            FitCustomOptionsToContent(detected.Count + countMissing);
        }
        catch (Exception error)
        {
            if (userDoomModCount is not null) userDoomModCount.Text = "—";
            userDoomModStatus.Visible = true;
            userDoomModStatus.Text = "Could not scan mods: " + error.Message;
        }
    }

    private void FitCustomOptionsToContent(int userModCount)
    {
        if (customOptionsForm is null || customOptionsForm.IsDisposed) return;

        var work = Screen.FromControl(this).WorkingArea;
        if (customModsDocked)
        {
            // Refreshing files while open must never alter docked geometry.
            if (doomModItemsPanel is not null)
                doomModItemsPanel.AutoScroll = 170 + Math.Max(0, userModCount) * 27
                    > customOptionsForm.ClientSize.Height;
            return;
        }
        var chromeHeight = customOptionsForm.Height - customOptionsForm.ClientSize.Height;
        var maxClientHeight = Math.Max(480, work.Height - chromeHeight - 45);
        // VR column is intentionally fixed-height; DOOM's mod list adds one
        // line per discovered resource. Use available desktop height first.
        // Leave room for each full checkbox row and the installer/status controls;
        // compact spacing must not clip the bottom of a long mod list.
        var preferredHeight = Math.Max(565, 170 + Math.Max(0, userModCount) * 27);
        var height = Math.Min(preferredHeight, maxClientHeight);
        // Larger fixed-width sidecar keeps DOOM and VR labels legible.
        // Its width is locked; height tracks the launcher while docked.
        var outerWidth = Math.Min(890, work.Width);
        var borderWidth = customOptionsForm.Width - customOptionsForm.ClientSize.Width;
        customOptionsForm.MinimumSize = new Size(Math.Min(890, work.Width),
            Math.Min(530, work.Height));
        customOptionsForm.ClientSize = new Size(
            Math.Max(1, outerWidth - borderWidth), height);

        // On small displays or with a very large mod collection, clipping
        // controls would be worse than a scrollbar. Ordinarily no scrollbars
        // are present; this is an accessibility fallback only when needed.
        if (doomModItemsPanel is not null)
            doomModItemsPanel.AutoScroll = preferredHeight > maxClientHeight;
    }

    private void ShowCustomOptions()
    {
        if (customOptionsForm is null || customOptionsForm.IsDisposed) return;
        try
        {
            EnsureUserDoomModWatcher();
            EnsureDoomModLoaderWatcher();
            RefreshUserDoomMods();
            FitCustomOptionsToContent((userDoomModChecks?.Controls.Count ?? 0) + (packagedDoomModChecks?.Controls.Count ?? 0));
        }
        catch (Exception error)
        {
            if (userDoomModStatus is not null)
                userDoomModStatus.Text = "Cannot access mods folder: " + error.Message;
        }

        // Treat Custom Mods as a left-hand sidecar of the launcher.
        // Keep both title bars/bottom edges aligned with no seam gap.
        customModsDocked = true;
        PlaceCustomModsBesideLauncher();
        if (!customOptionsForm.Visible) customOptionsForm.Show(this);
        if (customOptionsForm.WindowState == FormWindowState.Minimized)
            customOptionsForm.WindowState = FormWindowState.Normal;
        PlaceCustomModsBesideLauncher();
        customOptionsForm.BringToFront();
        customOptionsForm.Activate();
    }

    private void ShowDevMode()
    {
        devModeForm ??= new DevModeForm(
            weaponMode, calibrationWeapon, gripAlignment, hudDebugging,
            extendedLogging, calibrateHands, weaponStatus, disableAa, captureEyes);
        if (!devModeForm.Visible) devModeForm.Show(this);
        if (devModeForm.WindowState == FormWindowState.Minimized)
            devModeForm.WindowState = FormWindowState.Normal;
        devModeForm.BringToFront();
        devModeForm.Activate();
    }

    private void ShowInfo()
    {
        infoForm ??= new InfoForm();
        if (!infoForm.Visible)
        {
            infoForm.ShowContents();
            infoForm.Show(this);
        }
        if (infoForm.WindowState == FormWindowState.Minimized)
            infoForm.WindowState = FormWindowState.Normal;
        infoForm.BringToFront();
        infoForm.Activate();
    }

    private async void LaunchClicked(object? sender, EventArgs e)
    {
        if (launchOperationInProgress) return;
        if (KharvoxRunner.IsRunning)
        {
            launchOperationInProgress = true;
            launchButton.Enabled = false;
            status.Text = "Ending DOOM …";
            try
            {
                await KharvoxRunner.EndDoomAsync();
                status.Text = "DOOM ended.";
            }
            finally
            {
                launchOperationInProgress = false;
                lastKnownDoomRunning = KharvoxRunner.IsRunning;
                SetRunningState(lastKnownDoomRunning);
                launchButton.Enabled = true;
            }
            return;
        }
        if (customModsSaveFailed)
        {
            MessageBox.Show(this, "VR mod selections could not be saved."
                + Environment.NewLine + "Correct the settings-file problem before launching.",
                "KHARVOX — Unsaved mods", MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
            return;
        }
        // A fresh CI build cannot invent the separately supplied 75 SFS
        // shaders. Offer a *user-approved* AER launch rather than showing a
        // cryptic file-count exception after the launcher starts work.
        if (RendererSelection.IsSfs(SelectedRendererKey())
            && !VulkanSfs.HasCompleteShaderProfile())
        {
            var shaderCount = VulkanSfs.LocalShaderCount();
            var profilePath = Path.Combine(AppContext.BaseDirectory, "sfs-profile");
            var choice = MessageBox.Show(this,
                "SFS needs 75 valid compiled shader files. This installation has "
                + shaderCount + "." + Environment.NewLine + Environment.NewLine
                + "The SFS profile is a separate runtime input; a native build "
                + "does not recreate its VR-corrected shaders." + Environment.NewLine
                + "Required folder: " + profilePath + Environment.NewLine
                + Environment.NewLine
                + "YES — Locate the sfs-profile from an earlier WORKING KHARVOX installation and restore it."
                + Environment.NewLine
                + "NO — Use AER for this launch instead."
                + Environment.NewLine
                + "CANCEL — Do not launch.",
                "KHARVOX — Repair missing SFS shaders",
                MessageBoxButtons.YesNoCancel, MessageBoxIcon.Warning);
            if (choice == DialogResult.Cancel) return;
            if (choice == DialogResult.Yes)
            {
                if (!SfsProfileRecovery.ChooseAndRestore(this)) return;
                // Keep the original strict validation after copying the files.
                VulkanSfs.EnsureAvailable();
            }
            else
                rendererMode.SelectedIndex = RendererSelection.Index("AER");
        }
        SaveSettings();
        launchOperationInProgress = true;
        launchButton.Enabled = false;
        status.Text = "Launching DOOM …";
        try
        {
            var options = CreateLaunchOptions();
            void OnUi(Action update)
            {
                if (IsDisposed || !IsHandleCreated) return;
                if (InvokeRequired)
                {
                    try { BeginInvoke((Action)(() => { if (!IsDisposed) update(); })); }
                    catch (InvalidOperationException) { }
                }
                else update();
            }
            await KharvoxRunner.LaunchAsync(options, () => OnUi(() => {
                SetRunningState(true);
                launchButton.Enabled = true;
                // Keep the final renderer verification status instead of
                // overwriting warnings with the generic "Game running".
            }), message => OnUi(() => status.Text = message));
            var gameStillRunning = KharvoxRunner.IsRunning;
            SetRunningState(gameStillRunning);
            if (!gameStillRunning) status.Text = "DOOM ended.";
        }
        catch (Exception ex)
        {
            try
            {
                File.AppendAllText(RuntimeStorage.LogPath("KHARVOX-launcher-errors.log"),
                    DateTime.Now.ToString("O") + Environment.NewLine + ex.ToString() + Environment.NewLine);
            }
            catch { /* Diagnostics must preserve the original error. */ }
            status.Text = "Launch failed.";
            MessageBox.Show(this, ex.Message,
                ex is OtherModsDetectedException ? "KHARVOX - Other mods detected"
                    : ex is HeadsetUnavailableException ? "KHARVOX - Headset not connected" : "KHARVOX - Launch error",
                MessageBoxButtons.OK, ex is HeadsetUnavailableException ? MessageBoxIcon.Information : MessageBoxIcon.Error);
        }
        finally
        {
            launchOperationInProgress = false;
            RefreshDoomProcessState();
            launchButton.Enabled = true;
        }
    }

    internal KharvoxLaunchOptions CreateLaunchOptions() => new(
        intense.Checked,
        intense.Checked && cinematicFreelook.Checked,
        intense.Checked && otherCinematicsInQuad.Checked,
        cinewindowFollowsHeadset.Checked,
        SelectedRendererKey(),
        renderScale.Value,
        useFsrUpscaling.Checked,
        doomPath.Text,
        turnMode.SelectedIndex switch { 1 => "Snap", 2 => "Off", _ => "Smooth" },
        movementDirection.SelectedIndex == 1 ? "off-hand" : "head",
        smoothSpeed.Value,
        snapAngle.Value,
        FixedTurnDeadzone,
        weaponMode.SelectedIndex == 1,
        SelectedCalibrationWeaponKey(),
        gripAlignment.SelectedIndex == 1 ? "side-grip" : "barrel",
        virtualGunstock.Checked,
        physicalGlorykill.Checked,
        SelectedPhysicalGlorykillSpeed(),
        physicalGlorykillHands.SelectedIndex switch { 0 => "left", 1 => "right", _ => "both" },
        leftHanded.Checked,
        leftHandSwapMode.SelectedIndex == 1 ? "buttons-and-sticks" : "buttons",
        laserSight.Checked,
        hudDebugging.Checked,
        extendedLogging.Checked,
        showHands.Checked,
        calibrateHands.Checked ? "rotation" : "off",
        enableBhaptics.Checked,
        usePsvr2Toolkit.Checked,
        SelectedBackWeaponKey(), handsJump.Checked, disableAa.Checked, captureEyes.Checked,
        disableVrIntro.Checked, swapJumpCrouch.Checked, weaponWheelRemap.Checked,
        gloryKillSlowmo.Value);

    private void SetRunningState(bool running)
    {
        launchButton.Text = running ? "END GAME" : "LAUNCH GAME";
        launchButton.BackColor = running ? Color.FromArgb(95, 95, 98) : Color.Black;
        status.Cursor = running ? Cursors.Hand : Cursors.Default;
        statusToolTip.SetToolTip(status,
            running ? "Click to bring DOOM to the foreground." : string.Empty);
    }

    private void RefreshDoomProcessState()
    {
        var running = KharvoxRunner.IsRunning;
        var changed = running != lastKnownDoomRunning;
        lastKnownDoomRunning = running;
        SetRunningState(running);
        if (changed && !launchOperationInProgress)
        {
            launchButton.Enabled = true;
            status.Text = running ? "Game running" : "DOOM ended.";
        }
    }

    private void BrowseDoomFolder(object? sender, EventArgs e)
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Select the DOOM (2016) folder containing DOOMx64vk.exe",
            ShowNewFolderButton = false,
            SelectedPath = Directory.Exists(doomPath.Text) ? doomPath.Text : string.Empty
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            if (!File.Exists(Path.Combine(dialog.SelectedPath, "DOOMx64vk.exe")))
            {
                MessageBox.Show(this, "The selected folder does not contain DOOMx64vk.exe.",
                    "KHARVOX", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            doomPath.Text = dialog.SelectedPath;
            SaveSettings();
            CheckLiveConfiguration();
        }
    }

    private void LoadSettings()
    {
        try
        {
            var s = LauncherSettingsStore.Load(SettingsPath);
            intense.Checked = s.Intense;
            cinematicFreelook.Checked = s.SettingsVersion < 6 || s.CinematicFreelook;
            otherCinematicsInQuad.Checked = s.SettingsVersion >= 15 && s.OtherCinematicsInQuad;
            cinewindowFollowsHeadset.Checked = s.SettingsVersion >= 16 && s.CinewindowFollowsHeadset;
            doomPath.Text = s.GamePath ?? string.Empty;
            rendererMode.SelectedIndex = RendererSelection.Index(s.RendererMode);
            var defaultScale = AerDefaultRenderScale;
            var savedScale = s.RenderScale;
            renderScale.Value = savedScale >= renderScale.Minimum && savedScale <= renderScale.Maximum
                ? savedScale : defaultScale;
            turnMode.SelectedIndex = ClampInt(s.TurnMode, 0, 2);
            movementDirection.SelectedIndex = s.SettingsVersion >= 24
                ? ClampInt(s.MovementDirection, 0, 1) : 0;
            smoothSpeed.Value = ClampToSlider(s.SmoothSpeed, smoothSpeed);
            snapAngle.Value = Clamp(s.SnapAngle, snapAngle);
            weaponMode.SelectedIndex = ClampInt(s.WeaponMode, 0, 1);
            calibrationWeapon.SelectedIndex = s.SettingsVersion >= 12
                ? ClampInt(s.CalibrationWeapon, 0, CalibrationWeaponKeys.Length - 1) : 1;
            backWeapon.SelectedIndex = s.SettingsVersion >= 19
                ? ClampInt(s.BackWeapon, 0, BackWeaponKeys.Length - 1) : 1;
            gripAlignment.SelectedIndex = ClampInt(s.GripAlignment, 0, 1);
            virtualGunstock.Checked = s.VirtualGunstock;
            physicalGlorykill.Checked = s.SettingsVersion >= 10 && s.PhysicalGlorykill;
            physicalGlorykillSpeed.Value = GloryKillSpeedToSliderValue(s.SettingsVersion >= 11
                ? s.PhysicalGlorykillSpeed : DefaultGloryKillSpeed);
            physicalGlorykillHands.SelectedIndex = s.SettingsVersion >= 11
                ? ClampInt(s.PhysicalGlorykillHands, 0, 2) : 2;
            leftHandSwapMode.SelectedIndex = s.SettingsVersion >= 13
                ? ClampInt(s.LeftHandSwapMode, 0, 1) : 0;
            leftHanded.Checked = s.SettingsVersion >= 13 && s.LeftHanded;
            swapJumpCrouch.Checked = s.SettingsVersion >= 34 && s.SwapJumpCrouch;
            laserSight.Checked = s.SettingsVersion >= 14 && s.LaserSight;
            hudDebugging.Checked = s.SettingsVersion >= 9 && s.HudDebugging;
            extendedLogging.Checked = s.SettingsVersion >= 25 && s.ExtendedLogging;
            showHands.Checked = s.ShowHands;
            disableVrIntro.Checked = s.DisableVrIntro;
            handsJump.Checked = s.HandsJump;
            disableAa.Checked = s.DisableAa;
            captureEyes.Checked = s.CaptureEyes;

            calibrateHands.Checked = s.SettingsVersion >= 27
                && s.HandCalibrationMode != 0;
            enableBhaptics.Checked = s.SettingsVersion >= 17 && s.EnableBhaptics;
            usePsvr2Toolkit.Checked = s.SettingsVersion >= 21 && s.UsePsvr2Toolkit;
            useFsrUpscaling.Checked = s.SettingsVersion >= 22 && s.UseFsrUpscaling;
            weaponWheelRemap.Checked = s.SettingsVersion >= 35 ? s.WeaponWheelRemapEnabled : true;
            gloryKillSlowmo.Value = ClampInt(s.SettingsVersion >= 35 ? s.GloryKillSlowmoLevel : 10,
                gloryKillSlowmo.Minimum, gloryKillSlowmo.Maximum);
            preset.SelectedIndex = ClampInt(s.Preset, 0, 3);
        }
        catch
        {
            rendererMode.SelectedIndex = RendererSelection.Index(VulkanSfs.Key);
            renderScale.Value = AerDefaultRenderScale;
            weaponMode.SelectedIndex = 0;
            calibrationWeapon.SelectedIndex = 1;
            backWeapon.SelectedIndex = 1;
            gripAlignment.SelectedIndex = 0;
            virtualGunstock.Checked = false;
            physicalGlorykill.Checked = false;
            smoothSpeed.Value = DefaultSmoothTurnSpeed;
            movementDirection.SelectedIndex = 0;
            snapAngle.Value = 45m;
            physicalGlorykillSpeed.Value = GloryKillSpeedToSliderValue(DefaultGloryKillSpeed);
            physicalGlorykillHands.SelectedIndex = 2;
            leftHandSwapMode.SelectedIndex = 0;
            leftHanded.Checked = false;
            swapJumpCrouch.Checked = false;
            laserSight.Checked = false;
            hudDebugging.Checked = false;
            extendedLogging.Checked = false;
            showHands.Checked = LauncherPresetPolicy.DefaultEnableHands;
            handsJump.Checked = true;
            disableAa.Checked = false;
            captureEyes.Checked = false;

            calibrateHands.Checked = false;
            enableBhaptics.Checked = false;
            usePsvr2Toolkit.Checked = false;
            useFsrUpscaling.Checked = false;
            weaponWheelRemap.Checked = true;
            gloryKillSlowmo.Value = 10;
            cinematicFreelook.Checked = true;
            otherCinematicsInQuad.Checked = false;
            cinewindowFollowsHeadset.Checked = false;
            preset.SelectedIndex = 0;
        }
        finally
        {
            UpdateWeaponModeDescription();
            cinematicFreelook.Enabled = intense.Checked;
            otherCinematicsInQuad.Enabled = intense.Checked;
            physicalGlorykillSpeed.Enabled = physicalGlorykill.Checked;
            physicalGlorykillHands.Enabled = physicalGlorykill.Checked;
            leftHandSwapMode.Enabled = leftHanded.Checked;
            useFsrUpscaling.Enabled = true;
            UpdateSpeedSliderLabels();
        }
    }

    private void SaveSettings()
    {
        try
        {
            var s = new LauncherSettings(preset.SelectedIndex, doomPath.Text, intense.Checked,
                cinematicFreelook.Checked,
                otherCinematicsInQuad.Checked,
                cinewindowFollowsHeadset.Checked,
                SelectedRendererKey(), renderScale.Value, useFsrUpscaling.Checked,
                Math.Max(0, turnMode.SelectedIndex),
                Math.Max(0, movementDirection.SelectedIndex),
                smoothSpeed.Value, snapAngle.Value, FixedTurnDeadzone,
                Math.Max(0, weaponMode.SelectedIndex), Math.Max(0, calibrationWeapon.SelectedIndex),
                Math.Max(0, backWeapon.SelectedIndex),
                Math.Max(0, gripAlignment.SelectedIndex),
                virtualGunstock.Checked, physicalGlorykill.Checked,
                SelectedPhysicalGlorykillSpeed(), Math.Max(0, physicalGlorykillHands.SelectedIndex),
                leftHanded.Checked, Math.Max(0, leftHandSwapMode.SelectedIndex),
                laserSight.Checked,
                hudDebugging.Checked,
                extendedLogging.Checked,
                showHands.Checked,
                calibrateHands.Checked ? 1 : 0,
                enableBhaptics.Checked,
                usePsvr2Toolkit.Checked, handsJump.Checked, disableAa.Checked, captureEyes.Checked,
                swapJumpCrouch.Checked, weaponWheelRemap.Checked, gloryKillSlowmo.Value);
            s.DisableVrIntro = disableVrIntro.Checked;
            LauncherSettingsStore.Save(SettingsPath, s);
        }
        catch { /* Settings are optional; launching must remain possible. */ }
    }

    private static decimal Clamp(decimal value, NumericUpDown n) => Math.Max(n.Minimum, Math.Min(n.Maximum, value));
    private static int ClampToSlider(decimal value, TrackBar slider) => ClampInt(
        decimal.ToInt32(decimal.Round(value, 0, MidpointRounding.AwayFromZero)),
        slider.Minimum, slider.Maximum);
    private static int GloryKillSpeedToSliderValue(decimal speed)
    {
        var clamped = Math.Max(GloryKillSpeedMinimum,
            Math.Min(GloryKillSpeedMinimum + 15 * GloryKillSpeedStep, speed));
        return decimal.ToInt32(decimal.Round(
            (clamped - GloryKillSpeedMinimum) / GloryKillSpeedStep,
            0, MidpointRounding.AwayFromZero));
    }
    private static int ClampInt(int value, int minimum, int maximum) => Math.Max(minimum, Math.Min(maximum, value));

    private sealed class OwnedImagePictureBox : PictureBox
    {
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                Image?.Dispose();
                Image = null;
            }
            base.Dispose(disposing);
        }
    }

}
