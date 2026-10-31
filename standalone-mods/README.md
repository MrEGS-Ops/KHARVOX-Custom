# KHARVOX external VR mods — independent modules

These are **two independently built, opt-in DLL mods**, not gameplay code added to the clean KHARVOX launcher/native VR runtime. No existing core source or launcher settings were changed on this feature branch.

* `kharvox_mod_behind_head_wheel.dll` — an individual right/left behind-head gesture weapon-wheel mod. Enter zone to open; motion selects; Grip freezes selection, withdraw with Grip confirms, withdraw without Grip cancels. Each open resets its motion anchor, plus hand-specific haptic requests.
* `kharvox_mod_quick_holsters.dll` — an individual hip inventory mod. Grip edge at an empty hip stores an eligible weapon; Grip edge at an occupied hip draws it without swapping. Gauss, Chaingun and BFG are excluded, and stored weapons remain inventory identities rather than world props.

### Development status

**Code implemented and separately compiled; not yet functional in live DOOM.** Each DLL is a stateful feature module with a stable C ABI, but it needs an external mod host/bridge to provide HMD-relative controller tracking, map game weapon IDs and execute its output requests. Merely copying a DLL to a DOOM mod folder won't activate it. Do not enable the existing native wheel/holster flags as a substitute.

The host must explicitly load **only** DLLs selected in the detached mod window. It must translate the wheel's open/hold/select/confirm/cancel signals into DOOM inputs, including neutral-frame draining for cancellation and suppressing the confirming Grip press for other actions. It must translate holster draw to a verified weapon-selection call and hide/show weapon presentation. If a draw cannot be executed, the host must not pretend it succeeded. Holsters require rendered hands.

### Build

```sh
cmake -S standalone-mods -B standalone-mods/build
cmake --build standalone-mods/build --config Release
ctest --test-dir standalone-mods/build -C Release --output-on-failure
```

On Windows the two distinct DLL files are under `standalone-mods/build/mods/vr/`. The independent workflow packages them separately from the launcher.

The shared `include/KharvoxModV1.h` protocol is intentionally independent of KHARVOX internals and DOOM game data; both DLLs export `kharvox_mod_abi_version`, `kharvox_mod_id`, `kharvox_mod_create`, `kharvox_mod_frame`, and `kharvox_mod_destroy`. A bridge should unload a module when unticked and never apply its output while paused or during a menu.

### Safety and branch policy

Everything here lives under `standalone-mods/` plus its dedicated CI workflow. The known-clean launcher branch `work/oct18-live-motion-chainsaw-hz` remains unchanged. Do not merge until a compatible external host is available and both features have been tested in a headset.
