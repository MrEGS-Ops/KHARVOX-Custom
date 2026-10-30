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
├── README.md
├── build-info.txt
├── kharvox-mods-bridge.json        (created locally)
├── settings.json                 (created locally)
├── NativeBridge/                 (bundled modified launcher + native runtime)
├── Logs/                         (created when needed)
├── Backups/                      (created when needed)
└── UpdateCache/                  (created when needed)
```

All settings and game-adjacent content are kept in these local folders. **No AppData installation** is required.

## Updates

Close the manager, then run **KharvoxVRModManagerUpdater.exe**. It checks the official project release, downloads the build-specific ZIP and verifies its SHA-256 before replacing managed app files. Local user settings and native files are preserved; a previous managed version is kept for rollback.

This is a clean rename of the actual executables. Old `KharvoxMods.exe` and `KharvoxModsUpdater.exe` are not included. For an existing installation, extract into a new folder; optionally copy your existing settings and Mods folder yourself, rather than overwriting the old installation.

## Testing / runtime

The native gameplay runtime remains pinned to a separately verified KHARVOX build. Cactus's original launcher UI is checked against its known source hash by the Windows build workflow. Real headset interaction and resizing at different DPI configurations still require testing on Windows.

Unsigned application builds may trigger Windows SmartScreen.
