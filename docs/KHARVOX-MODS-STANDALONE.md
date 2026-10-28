# KHARVOX Mods — VR-only portable standalone preview

## In this build
- The compiled standalone **KharvoxMods.exe** is **VR MODS ONLY**. It has one VR panel with all 22 original VR mod entries, four verified selectable checkboxes, plus a separate Glory Kill speed override. The window is a narrower 850 px, centered alongside the launcher on every fresh start or automatic restart.
- **ACTIVE MODS** is pinned at the top. Checking one of the five supported VR mod controls moves it there immediately; unchecking moves it back to its original category. Active mods support a drag grip, drop targets, right-click Move Up/Down/Top/Bottom, Undo/Reset display order, and clickable dependency information. Order is saved to the local bridge-settings JSON; it controls **presentation**, not runtime execution order.
- The five selectable VR mods are **Weapon Wheel Button Remap**, **Hand Focus**, **Physical Crouch**, **PAINSAW: RIPSHIFT OVERDRIVE**, and **Glory Kill Speed Override**. All other original entries remain visible but disabled until confirmed. Original dependencies for postponed features are displayed without enabling those features.
- The Glory Kill slider is **165 px wide**, appears **only when its checkbox is checked**, and ranges from 240 Hz (slower) to 30 Hz (faster). It affects Glory Kills only.
- The original **DOOM resource mods, USER MODS panel, Nexus browser, ZIP explorer and folder-ordering code are not compiled into this Mods EXE**. Their source is preserved on the development branch: the archived original layout is in `ArchivedDoomMods.Parked.cs`, and the separate Nexus/folder sources are explicitly excluded by `KharvoxMods.csproj`. They can be restored later without reimplementing them.
- The **newly uploaded original MrEGS rEGS gothic-ring artwork** (scaled for Windows icon formats) replaces the previous logo resource. Its shared source provides the icon for **KharvoxMods.exe**, **KharvoxModsUpdater.exe** and the optional **NativeBridge/KharvoxLauncher.exe**, plus the Mods footer watermark. The original separately installed KHARVOX icon and binaries are not touched.
- The two desktop windows now share the same native **Windows dark caption bar** and Windows 11 **matching 0x0038383B DWM border-colour settings**, and identical **RGB 30,30,33** WinForms group-box background with Gainsboro border captions and 8px inner padding. The updater uses the same window and button palette. The two applications retain their distinct functions and control arrangements. The GitHub build number appears in the title.
- The two linked desktop windows move, minimize, raise, and close together without making either always-on-top. The VR-only window automatically docks on the left of **our custom launcher**; the pair is centered at launch.

## VR native bridge
- The VR mod selections are stored locally in `kharvox-mods-bridge.json`. Our bundled bridge-aware KHARVOX launcher reads them on the next DOOM launch. Stock KHARVOX is never started by VRMods.
- **First launch:** open `KharvoxMods.exe`, select the original `KharvoxLauncher.exe` location if asked, and the already-bundled custom launcher starts automatically. No docking the original, no Install Bridge, no restart. The original KHARVOX folder remains untouched.
- **The bundled `NativeBridge/` folder still contains the existing full KHARVOX game runtime.** VR-only refers to **KharvoxMods.exe** and its UI/compiled code; it does not mean the underlying KHARVOX runtime has been stripped of all its existing DOOM game integration.
- Actual headset behavior still needs verification after build tests pass.

## Updating
- Download `KHARVOX-Mods-ReadyToRun.zip` for a fresh installation (includes our custom launcher). For future updates, close the app and run `KharvoxModsUpdater.exe`. Older installations with no `NativeBridge/` automatically retrieve and verify the runtime on startup without any extra install/restart window.
- Settings, NativeBridge, Logs, Backups and **pre-existing** `Mods/` directories are preserved by lightweight updates. The VR-only build does not create or use a new `Mods/` folder.
- Unsigned builds can still trigger Windows SmartScreen.

## Portable folder
```text
KHARVOX Mods/
├── KharvoxMods.exe
├── KharvoxModsUpdater.exe
├── build-info.txt
├── settings.json
├── kharvox-mods-bridge.json
├── NativeBridge/                 # bundled verified custom KHARVOX runtime
├── Logs/
├── Backups/
└── UpdateCache/
```

The original KHARVOX source and native gameplay branch have not been changed by this standalone work. Do not delete an existing `Mods/` directory without checking its saved user files first.
