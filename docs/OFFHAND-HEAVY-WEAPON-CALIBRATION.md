# KHARVOX live heavy-weapon grip calibration (Quest 3)
Active branch: `work/manager-187-wheel-holsters`. Uses the Mod Manager's NativeBridge only.

## RIGHT glove: one live position + rotation capture
1. Equip **Gauss Cannon** or **Chaingun** with Off-Hand Grip Capture enabled.
2. **Hold RIGHT Grip** and click **RIGHT joystick ONCE**. The mode (Normal or alternate) is latched as capture begins.
3. While holding RIGHT Grip, move AND rotate your LEFT controller. The live probe is **15 cm forward of the LEFT controller in its pointing direction**, letting you hold your LEFT controller behind the desired glove location without the two physical controllers colliding. The RIGHT glove previews the shifted target live and follows your LEFT controller's rotation.
4. **Release RIGHT Grip** to confirm. The position and quaternion are atomically written together to `NativeBridge/offhand_right_hand_positions.cfg`, with a `.before-live-capture.bak` backup. Failure/invalid tracking cancels without changing the file. A success haptic confirms the write.

The 15 cm offset changes **only** the live right-glove position preview and its saved position, not the captured rotation. It does **not** redirect weapon aim, controller tracking, or the validated heavy-gun anchor. Saved glove coordinates are gun-local. The glove follows the gun after the capture ends and is restored after a melee/cinematic interruption. Re-entering calibration doesn't overwrite the other weapon or the other mode.

## LEFT support grip, normal and alternate
For each heavy weapon, **hold LEFT Grip + click BOTH joysticks** while placing the LEFT controller at the chosen support point. Release and re-grip LEFT Grip afterwards. This is a separate capture because its grip must track the weapon's moving second handle.

**Normal and alternate profile slots:**
- Gauss Cannon: Normal / Siege. Native Siege-charge declaration selects the alternate right glove and left support grip; if no alternate data was captured it uses normal defaults.
- Chaingun: Normal / Turret. An explicit Turret token in the native active weapon declaration selects the alternate profile; if DOOM does not expose it, the code deliberately retains Normal and logs the missing/changed declaration rather than guessing from Grip input.

Support points for alternate modes are stored alongside the right hand poses; normal support remains in `two_hand_weapon_profiles_saved.cfg`. Existing gun-anchor settings (`gauss_cannon`, `chaingun`) are never altered by live glove calibration.

Chainsaw's established BOTH-click support capture remains unchanged. Single-stick melee/pause are release-triggered while Off-Hand Grip Capture is active, so the live calibration's right-stick click cannot start an ordinary melee animation. Physical Glory Kills' independent no-RS-melee protection is unchanged.

VR visual alignment and the actual existence/identity of the native Turret mode declaration still require in-headset verification.
