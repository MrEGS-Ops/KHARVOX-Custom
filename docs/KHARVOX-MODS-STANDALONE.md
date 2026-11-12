# KHARVOX VR Mod Manager

**An Unofficial VR Mod Manager for KHARVOX**

An independent mod manager by **MrEGS**, for the original KHARVOX VR modification by **CactusVRStudios**. This project is not an official collaboration or CactusVRStudios release.

## Installation

1. Download **KHARVOX-VR-Mod-Manager-ReadyToRun.zip**.
2. Extract its entire contents into a new local folder.
3. Run **KharvoxVRModManager.exe**.
4. On the first launch, select your original CactusVRStudios KHARVOX installation when prompted.

Both the mod manager and the modified KHARVOX launcher appear together inside one normal Windows window. The left pane lists the 22 VR features, with **RIP & TEAR: KILLTIME** for Glory Kill speed adjustment, **PAINSAW: RIPSHIFT OVERDRIVE**, and other verified/paused mods. The modified launcher occupies the right pane and credits CactusVRStudios separately.

The original Cactus **MainForm.cs** is preserved, and the separately installed original KHARVOX files are not changed. Only the bundled modified launcher in the manager's NativeBridge folder is used.

## Portable local files

```text
KHARVOX VR Mod Manager/
├── KharvoxVRModManager.exe
├── KharvoxVRModManagerUpdater.exe
├── UserData/                         (ALL mutable user configurations)
│   ├── settings.json
│   ├── kharvox-mods-bridge.json
│   ├── kharvox-mods-applied.txt
│   ├── kharvox-launcher-settings.json
│   ├── kharvox-custom-mods.json
│   ├── known-configurations.json
│   ├── offhand_right_hand_positions.cfg
│   ├── two_hand_weapon_profiles_saved.cfg
│   └── two_hand_weapon_profiles_left_primary_saved.cfg
├── NativeBridge/                     (replaceable native runtime)
│   └── CalibrationDefaults/          (read-only uploaded owner presets)
├── Logs/
├── Backups/
└── UpdateCache/
```

**Only edit the files under `UserData`.** The NativeBridge runtime can be
replaced by the updater without touching user settings. Existing manager,
launcher and native configuration files are migrated automatically when first
running this version. If an older and newer settings file both exist with
different values, the newer `UserData` file takes precedence and the old
one is kept under `UserData/LegacyBackups` where applicable. There is no
new AppData storage for this mod-manager installation.

## Updates

Close the manager, then run `KharvoxVRModManagerUpdater.exe`. The updater
verifies its release SHA-256, replaces only managed app files and retains the
entire `UserData` directory. When upgrading the bridge, older active CFG
files are migrated out of NativeBridge into `UserData` before the replace.
The existing rollback folder remains `Backups/NativeBridge`.

## Owner-calibrated first-run and Reset Weapon defaults

The three CFG files uploaded by the owner on 12 November 2026 are the
factory calibration baseline. A **new installation** copies the exact saved
left and right weapon grips, quaternions, two-hand support placements, Gauss
Siege and Minigun Turret positions into `UserData`. Once seeded, KHARVOX
never overwrites these editable profiles during normal updates.

In the headset, **RESET THIS WEAPON** restores *only* that weapon's
factory primary poses and normal/alternate support positions, with an
adjacent `.before-vr-reset.bak` backup of the earlier user settings.
Other weapons remain untouched. Read-only factory copies live under
`NativeBridge/CalibrationDefaults`; they are not the editable settings.

To calibrate live: hold right-stick click for one second to open the panel,
choose the weapon and physical hand, select Normal or Siege/Turret, and
start calibration. The **current weapon-hand Grip** still operates DOOM's
native alternate mode while calibrating. Right trigger saves the position
without firing. Hand Focus stays weapon-hand aimed in ordinary gameplay;
short RS clicks remain world Use rather than aim toggles. During native
Glory Kills, chainsaw kills, ledge transitions and cinematics, focus
temporarily returns to the head.

## Testing / runtime

The native gameplay runtime remains pinned to a separately verified KHARVOX build. Cactus's original launcher UI is checked against its known source hash by the Windows build workflow. Real headset interaction and resizing at different DPI configurations still require testing on Windows.

Unsigned application builds may trigger Windows SmartScreen.
