# KHARVOX – DOOM (2016) Save Protection

Resource mods can require developer mode and can leave normal campaign saves unusable. KHARVOX protects saves BEFORE loading selected resource mods. It cannot repair damage that predates the first protected baseline.

## Source and backups

On Windows, KHARVOX detects the Saved Games Known Folder, including redirects when supported. It backs up both folders beneath id Software/DOOM/base: savegame and savegame.user. These are kept together, including profile and checkpoint data.

Backups persist outside both the DOOM installation and KHARVOX build folders:

    %LOCALAPPDATA%\KHARVOX\DOOM-2016\save-protection\snapshots\

Snapshots are never automatically deleted. Monitor disk usage and back up the directory separately if needed.

## Switching between modded and clean

1. Before the first modded launch, KHARVOX captures and verifies a CLEAN BASELINE. If no campaign saves exist, modded launch is blocked; play normal DOOM once or select the right save folder.
2. Later modded launches create additional snapshots without replacing the clean baseline.
3. Untick all DOOM resource mods and press Launch. The launcher offers **Yes** (back up modded saves and restore clean), **No** (keep the current possibly affected saves) or **Cancel** (do not launch).
4. Restore verifies SHA-256 file hashes before touching the game. It takes a pre-restore rescue snapshot, stages both save folders, swaps same-volume directories, and rolls back prior directories if a change fails.
5. After restoring, normal gameplay can create a newer clean baseline before another modded cycle.

Use **Backup saves** and **Manage / Restore saves…** in DOOM MODS for manual snapshots, browsing history, restoring a chosen version, opening backups and selecting a nonstandard save folder.

## Steam Cloud warning

Steam Cloud can overwrite locally restored saves; KHARVOX cannot control it. Close DOOM and ideally temporarily disable Steam Cloud for the game before restoring. If Steam presents a conflict, select the intended LOCAL files. Keep an independent backup.

Unchecking mods does not itself convert or fix developer-mode saves; use the restore prompt to return to the protected clean snapshot.

## Automated verification

The Windows build workflow runs KharvoxLauncher.exe --self-test-save-protection against temporary synthetic save folders, covering both save trees, hash mismatch rejection and the folder-level restore. UI smoke tests confirm the controls remain accessible. Steam Cloud interactions and actual headset/game behaviour still require Windows testing.
