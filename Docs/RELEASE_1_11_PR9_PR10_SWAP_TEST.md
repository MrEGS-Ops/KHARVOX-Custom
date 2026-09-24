# KHARVOX 1.11 PR9/PR10 + Jump/Crouch Swap Test

This local test build starts from the 1.11 ODevStudio rollback comparison build.

- PR #9: The DOOM child process disables the ReShade OpenXR API layer through its per-process environment switch.
- PR #10: SFS image-barrier ownership reads use a separate shared registry, image metadata readers use shared locks, and command-buffer lookups use an invalidated per-thread cache. The rollback branch's descriptor replay behavior remains unchanged.
- Movement: The new **Swap Jump/Crouch** checkbox swaps the gameplay actions on A/B (or the corresponding face buttons in full Left Hand mode). Menu confirm/cancel and Hands Jump remain unchanged. The option is off by default and saved per profile.

This is a performance and control test, not a hardware compatibility claim. No desktop mirror is included. The game and its assets are not included.
