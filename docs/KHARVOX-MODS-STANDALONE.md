# KHARVOX Mods — Standalone Application / Separation Phase 1

This is an **experimental standalone shell**, not yet the final independent native gameplay mod system.

## What phase 1 implements
- Builds a separate `KharvoxMods.exe` (Windows .NET Framework 4.8 x64) without recompiling KHARVOX native Vulkan/OpenXR.
- On first start, asks whether to attach KHARVOX Mods to KHARVOX. **Yes** attaches; **No** closes KHARVOX Mods without starting KHARVOX or saving a refusal. The prompt reappears next time.
- Locates `KharvoxLauncher.exe` in typical sibling folders or lets the user pick it.
- Launches the existing KHARVOX launcher automatically, or reuses its already-running process.
- Shows KHARVOX Mods to the **left** of the main KHARVOX window, matching the current two-window orientation and vertical alignment.
- Synchronizes movement and minimize/restore (best effort; hardware Windows desktop test pending).
- Saves `settings.json`, `Mods/`, `Logs/`, and `Backups/` **beside `KharvoxMods.exe`**. No AppData data store or scattered settings files.
- Provides a read-only catalogue using **PAINSAW: RIPSHIFT OVERDRIVE** plus a Dev page with explanations of our own custom overrides.

## Not yet implemented
- The original full Custom Mods WinForms control tree has **not been extracted** from the intertwined KHARVOX MainForm. This UI is a standalone preview, not pixel-identical.
- **No native bridge** or stable ABI yet. The first-run "Attach" prompt only means attach desktop windows.
- Preview controls are intentionally **not clickable**: no setting is misrepresented as functional.
- No gameplay mod DLLs, DOOM resource mods, automatic installs, or DOOM launch changes are made.
- No updates to KHARVOX, DOOM, their settings, registry, controller bindings, or the active working project branch.

## Installation test
1. Download the `KHARVOX-Mods-Standalone-Preview` artifact from the **KHARVOX Mods Standalone (Experimental)** GitHub workflow **once**. It includes `KharvoxModsUpdater.exe` for subsequent updates.
2. Extract it to a **writable** folder **beside**, not inside, the KHARVOX folder (for example, under Documents or Games). Do not put the portable app in a protected `Program Files` folder.
3. Start `KharvoxMods.exe`. Choose **Yes** to attach desktop windows or **No** to exit the Mods application.
4. Select the existing `KharvoxLauncher.exe` if it is not found automatically.
5. Verify KHARVOX opens without launching DOOM; drag/minimize/restore either window.
6. Close KHARVOX Mods and verify KHARVOX continues independently.

For a command-line smoke test: `KharvoxMods.exe --self-test` returns exit code 0 when state isolation and consent tests pass.

## Portable files and uninstall
```text
KHARVOX Mods/
├── KharvoxMods.exe
├── KharvoxModsUpdater.exe
├── settings.json
├── updater-state.txt (created after successful update)
├── UpdateCache/ (temporary downloads)
├── Mods/
├── Logs/
└── Backups/
```

All files created and owned by this standalone application must stay in its installation folder. To uninstall, close `KharvoxMods.exe` and delete **only** the `KHARVOX Mods` folder. The separate original KHARVOX and DOOM installations are untouched. An earlier experimental preview wrote a settings file to `%LOCALAPPDATA%\\KHARVOXMods`; this portable version does not read, migrate, or create that legacy path. If you used the older preview, you can manually remove its old folder once you no longer need it.

## One-click portable updates

The standalone package also includes `KharvoxModsUpdater.exe`. Run it from the **same writable folder** as `KharvoxMods.exe` to check the latest successful preview published at the project's GitHub Releases page. It downloads a release ZIP, verifies its SHA-256 digest against a separately published manifest, safely extracts only allowlisted application files, and opens KHARVOX Mods.

- **Replaces only:** `KharvoxMods.exe`, the optional `KharvoxMods.exe.config`, and `README.md`.
- **Preserves:** `settings.json`, all mod files, `Mods/`, `Logs/`, `Backups/`, and all unrecognised files. A copy of the previous executable is saved under `Backups/Updater/previous/`.
- Doesn't overwrite the updater while it is running. Updates are downloaded into `UpdateCache/` inside the application's folder, then the staged update directory is deleted.
- If KHARVOX Mods is running, close **KHARVOX Mods**, not the separate original KHARVOX launcher, before applying an update. The updater will refuse to replace an in-use executable.
- A failed download, unexpected archive entry or checksum mismatch **does not replace the application**.
- Windows downloads via browsers can apply a Mark-of-the-Web warning to the **initial downloaded updater**. Further updates are fetched by the updater rather than manually extracted from browser downloads. This does **not** disable SmartScreen, guarantee that unsigned binaries won't be warned about, or bypass Windows security.
- The updater release is a public preview, not the GitHub Actions artifacts endpoint; no personal GitHub access token is required.

## Future separation (not silently installed)
- Extract the existing Custom Mods UI source into a reusable UI component; replace this preview with the exact existing layout.
- Design and test a versioned native bridge/plugin interface with explicit user permission.
- Extract independently buildable gameplay DLLs, beginning with PAINSAW: RIPSHIFT OVERDRIVE.
- Add the Dev gameplay overrides as actual opt-in launch/native configuration switches.
- Preserve compatibility by checking the exact KHARVOX version and providing clean removal.
