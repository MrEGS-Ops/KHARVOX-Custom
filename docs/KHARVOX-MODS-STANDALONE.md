# KHARVOX Mods — Portable Standalone Preview

## What this build does
- Runs **KharvoxMods.exe** independently, launches the normal KHARVOX launcher, and docks both Windows desktop windows at their **visible** borders. Uses a native owned-window relationship so focusing either raises both without pinning them TopMost. **Clicking X on either attached window closes the linked pair** (the game process is not terminated).
- Uses the original two-panel **DOOM MODS / VR MODS** layout, with all 22 numbered legacy mod entries. Only the four verified numbered mods plus the separate **Glory Kill Speed Override** are selectable.
- Those five configurable mods are: **Weapon Wheel Button Remap, Hand Focus, Physical Crouch, PAINSAW: RIPSHIFT OVERDRIVE**, and **Glory Kill Speed Override** (30–240 Hz; right is faster).
- Stores selections as `kharvox-mods-bridge.json` **beside the Mods EXE**, not in AppData. Only a KHARVOX launcher built with the optional portable native bridge understands this file. **Stock Cactus v1.1.1 cannot apply these custom native features**, so selections alone do not enable gameplay mods.
- Restores Nexus Mods to the original Windows-default-browser **new-window** behaviour, with the local Mod Folder available separately.
- Uses the user-supplied original circular **MrEGS** image (cropped and embedded) as the app icon and bottom-centre watermark.
- Displays the GitHub Actions **build ID in the application title bar**.

## Install the game-enabled native bridge (optional)
1. Run `KharvoxMods.exe` and select **Yes** to attach the desktop windows; **No** exits.
2. Click **Install Bridge** beside the **NATIVE BRIDGE NOT INSTALLED** message in VR MODS and approve the separate native-installation prompt. The existing **Connection → Install / Update Native Bridge…** menu remains available.
3. This downloads the SHA-256-verified native package pinned to known-good native build **#640**, replaces its launcher with the bridge-aware one, and extracts everything into `KHARVOX Mods/NativeBridge/`. **It does not overwrite, change or delete the original KHARVOX installation.**
4. After successful installation, **KHARVOX Mods automatically closes the old linked windows and reopens itself with the new mod-enabled launcher**. No manual restart is necessary. If the old launcher refuses to close, the app reports the problem rather than starting a duplicate.
5. Check the five selections in VR MODS. They are passed to the bridge-aware launcher on the next DOOM launch. Testing actual headset behaviour is still required; the Windows build check does not prove mod compatibility in-game.

The bridge-aware KHARVOX launcher reads the separate configuration file using an inherited process environment variable and otherwise preserves normal launch behaviour. The original custom launcher has its own source and the original Cactus executable is not patched.

## One-click updates and build confirmation
- Run `KharvoxModsUpdater.exe` beside `KharvoxMods.exe`.
- It checks the current public preview Release asset and SHA-256 checksum, compares to the installed update state, and displays **New update found — Build #…** with a **Yes/No confirmation** before downloading.
- If already current, it says so rather than silently reinstalling. New builds show their **build ID in the title bar**.
- The newer Mods EXE **automatically refreshes the updater EXE** from a separately verified release asset after the old updater process has exited; no manual replacement should be necessary after this initial build.
- It never deletes `settings.json`, `kharvox-mods-bridge.json`, `Mods/`, `Logs/`, `Backups/`, or `NativeBridge/`.
- NativeBridge updates are opt-in through **Connection**, not silently installed through the ordinary app updater.
- Browser-downloaded initial unsigned binaries may still trigger Windows SmartScreen; this feature does not disable security checks.

## Portable folder layout
```text
KHARVOX Mods/
├── KharvoxMods.exe
├── KharvoxModsUpdater.exe
├── build-info.txt
├── settings.json
├── kharvox-mods-bridge.json
├── NativeBridge/                 # optional, verified local KHARVOX copy
├── Mods/
├── Logs/
├── Backups/
└── UpdateCache/
```

Put the folder somewhere writable outside Program Files. To remove this installation, close the application and delete the **KHARVOX Mods** folder. Files created by the separate original Cactus launcher are not managed or deleted by this utility.

## Still pending
- Real headset verification of the integrated native bridge, controls and shared window foreground behaviour.
- Full re-use of the old Custom Mods internals (configuration ratings, DOOM resource generation and save-management controls).
- A stable plugin ABI that would allow each gameplay DLL to be replaced independently without updating the core native KHARVOX runtime.
- Live Dev override checkboxes and validation of retired experimental mods.

The original functioning custom native source branch remains untouched. The native bundle is a **separate, explicit local install**, not a stealth patch.
