# KHARVOX Mods — VR-only portable standalone preview

## In this build
- The compiled standalone **KharvoxMods.exe** is **VR MODS ONLY**. It has one VR panel with all 22 original VR mod entries, four verified selectable checkboxes, plus a separate Glory Kill speed override. The window is a narrower 850 px, centered alongside the launcher on every fresh start or automatic restart.
- **ACTIVE MODS** is pinned at the top. Checking one of the five supported VR mod controls moves it there immediately; unchecking moves it back to its original category. Active mods support a drag grip, drop targets, right-click Move Up/Down/Top/Bottom, Undo/Reset display order, and clickable dependency information. Order is saved to the local bridge-settings JSON; it controls **presentation**, not runtime execution order.
- The five selectable VR mods are **Weapon Wheel Button Remap**, **Hand Focus**, **Physical Crouch**, **PAINSAW: RIPSHIFT OVERDRIVE**, and **Glory Kill Speed Override**. All other original entries remain visible but disabled until confirmed. Original dependencies for postponed features are displayed without enabling those features.
- The Glory Kill slider is **165 px wide**, appears **only when its checkbox is checked**, and ranges from 240 Hz (slower) to 30 Hz (faster). It affects Glory Kills only.
- The original **DOOM resource mods, USER MODS panel, Nexus browser, ZIP explorer and folder-ordering code are not compiled into this Mods EXE**. Their source is preserved on the development branch: the archived original layout is in `ArchivedDoomMods.Parked.cs`, and the separate Nexus/folder sources are explicitly excluded by `KharvoxMods.csproj`. They can be restored later without reimplementing them.
- The original MrEGS logo is the icon on both standalone EXEs and the centered footer watermark. The GitHub build number appears in the title.
- The two linked desktop windows move, minimize, raise, and close together without making either always-on-top. The VR-only window docks on the left; the group is centered at launch.

## VR native bridge
- The VR mod selections are stored locally in `kharvox-mods-bridge.json`. The optional bridge-aware KHARVOX launcher reads them on the next DOOM launch. Stock KHARVOX does not consume the bridge settings.
- Click **Install Bridge** beside the status warning, approve the optional installation, and the application will automatically close the old linked pair and reopen using the new bridge-aware launcher. It extracts to `NativeBridge/` under the standalone folder, leaving the original KHARVOX installation untouched.
- **The separate NativeBridge package still contains the existing full KHARVOX game runtime.** VR-only refers to **KharvoxMods.exe** and its UI/compiled code; it does not mean the underlying KHARVOX runtime has been stripped of all its existing DOOM game integration.
- Actual headset behavior still needs verification after build tests pass.

## Updating
- Close the app and run `KharvoxModsUpdater.exe`. It reports when it finds a newer build and asks for confirmation. The new Mods EXE refreshes the updater EXE from its separately verified release asset after startup.
- Settings, NativeBridge, Logs, Backups and **pre-existing** `Mods/` directories are preserved by updates. The VR-only build does not create or use a new `Mods/` folder.
- Unsigned builds can still trigger Windows SmartScreen.

## Portable folder
```text
KHARVOX Mods/
├── KharvoxMods.exe
├── KharvoxModsUpdater.exe
├── build-info.txt
├── settings.json
├── kharvox-mods-bridge.json
├── NativeBridge/                 # optional separate KHARVOX copy
├── Logs/
├── Backups/
└── UpdateCache/
```

The original KHARVOX source and native gameplay branch have not been changed by this standalone work. Do not delete an existing `Mods/` directory without checking its saved user files first.
