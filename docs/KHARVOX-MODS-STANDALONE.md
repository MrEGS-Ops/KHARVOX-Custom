# KHARVOX Mods — Standalone Application / Separation Phase 1

This is an **experimental standalone shell**, not yet the final independent native gameplay mod system.

## What phase 1 implements
- Builds a separate `KharvoxMods.exe` (Windows .NET Framework 4.8 x64) without recompiling KHARVOX native Vulkan/OpenXR.
- On first start, asks whether the user wants the two desktop windows attached. Choosing No leaves both separate.
- Locates `KharvoxLauncher.exe` in typical sibling folders or lets the user pick it.
- Launches the existing KHARVOX launcher automatically, or reuses its already-running process.
- Shows KHARVOX Mods to the **left** of the main KHARVOX window, matching the current two-window orientation and vertical alignment.
- Synchronizes movement and minimize/restore (best effort; hardware Windows desktop test pending).
- Saves its own state under `%LOCALAPPDATA%\KHARVOXMods\settings.json`.
- Provides a read-only catalogue using **PAINSAW: RIPSHIFT OVERDRIVE** plus a Dev page with explanations of our own custom overrides.

## Not yet implemented
- The original full Custom Mods WinForms control tree has **not been extracted** from the intertwined KHARVOX MainForm. This UI is a standalone preview, not pixel-identical.
- **No native bridge** or stable ABI yet. The first-run "Attach" prompt only means attach desktop windows.
- Preview controls are intentionally **not clickable**: no setting is misrepresented as functional.
- No gameplay mod DLLs, DOOM resource mods, automatic installs, or DOOM launch changes are made.
- No updates to KHARVOX, DOOM, their settings, registry, controller bindings, or the active working project branch.

## Installation test
1. Download the `KHARVOX-Mods-Standalone-Preview` artifact from the **KHARVOX Mods Standalone (Experimental)** GitHub workflow.
2. Extract it to a folder **beside**, not inside, the KHARVOX folder.
3. Start `KharvoxMods.exe`. Choose **Yes** to attach desktop windows or **No** to leave them separate.
4. Select the existing `KharvoxLauncher.exe` if it is not found automatically.
5. Verify KHARVOX opens without launching DOOM; drag/minimize/restore either window.
6. Close KHARVOX Mods and verify KHARVOX continues independently.

## Repeat the first-install setup for manual debugging
- Open **Dev → Integration & Setup → Re-run First Install** to repeat the original Yes/No attachment prompt *without removing saved settings or reselecting KHARVOX*.
- Select **Yes** and verify that the window docks and moves with KHARVOX. Repeat and select **No** to verify that the windows separate while KHARVOX stays running.
- **Show Setup State** displays the saved answer, launcher path, process/window detection, and whether any native bridge or gameplay mods have been installed (none in phase 1).
- **Open Setup Log** opens `%LOCALAPPDATA%\\KHARVOXMods\\logs\\setup-debug.log` in Notepad. The log includes timestamps for consent choices, executable selection, process launch, desktop-window detection, docking and errors.
- To simulate a truly **fresh install**, close the Mods app, delete only `%LOCALAPPDATA%\\KHARVOXMods\\settings.json`, and reopen it. This removes the saved launcher path and attachment preference; it does not modify KHARVOX.
- You can also use **Attach Window / Detach Window** at any time. These buttons control desktop docking only, not game mods or a binary patch.

For a command-line smoke test: `KharvoxMods.exe --self-test` returns exit code 0 when state isolation and consent tests pass.

## Future separation (not silently installed)
- Extract the existing Custom Mods UI source into a reusable UI component; replace this preview with the exact existing layout.
- Design and test a versioned native bridge/plugin interface with explicit user permission.
- Extract independently buildable gameplay DLLs, beginning with PAINSAW: RIPSHIFT OVERDRIVE.
- Add the Dev gameplay overrides as actual opt-in launch/native configuration switches.
- Preserve compatibility by checking the exact KHARVOX version and providing clean removal.
