# KHARVOX 1.11

- Added a Swap Jump/Crouch option to the launcher. Menu controls remain unchanged.
- Disabled the conflicting ReShade OpenXR API layer for the launched game (PR #9).
- Reduced CPU locking and command-buffer lookup overhead in SFS (PR #10).
- Reverted the earlier ODevStudio audit changes, retaining the tested 1.11 baseline.

Known issue: Some systems may still experience low FPS on the first game launch after booting Windows. This release does not claim to resolve that issue.
