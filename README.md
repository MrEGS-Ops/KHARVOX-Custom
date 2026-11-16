# RIPSHIFT VR

**More physical combat. Better weapon handling.**

RIPSHIFT VR is a gameplay enhancement for **KHARVOX DOOM (2016) VR**, designed to make combat more physical, weapon handling more immersive, and everyday VR interactions more intuitive.

## Gameplay Enhancements

### 1. In-Headset Mod Configuration

Customise gameplay enhancements directly inside VR without constantly exiting DOOM. Adjust supported settings, weapon calibration and gameplay preferences through the in-game mod interface.

### 2. Advanced Physical Weapon Handling

Take control of your weapons with true two-handed aiming. Your supporting hand influences weapon direction and stability, encouraging realistic firearm handling.

Fine-tune individual weapon positions, rotations and support grips for either hand, with precision adjustments and smoother transitions between Gauss Cannon Siege Mode and Chaingun Turret Mode.

### 3. Gunslinger — Ambidextrous Weapon Handling, Selection & Holsters

**Equip and wield weapons in either your left or right hand**, giving you greater freedom over how you fight. Gunslinger combines ambidextrous weapon handling with an intuitive physical weapon-selection and holstering system.

Reach behind either side of your head to open the weapon wheel using the corresponding hand. **Move your arm** to highlight a weapon, hold Grip to equip it, or leave without gripping to cancel.

Store and retrieve weapons from adjustable hip holsters, with tactile controller feedback. Whether you're left-handed, right-handed or simply prefer switching weapon hands, Gunslinger gives you more control over your loadout.

### 4. PAINSAW — Physical Chainsaw Combat

Make chainsaw executions more interactive through trigger-controlled operation, physical cutting movements and responsive haptic feedback.

Bring a more hands-on approach to ripping and tearing.

### 5. Physical Glory Kills & KILLTIME

Finish staggered demons by physically punching with your VR controllers, with configurable hand selection and punch sensitivity.

**KILLTIME** lets you customise Glory Kill execution speed, whether you prefer fast, brutal takedowns or slower, cinematic finishes.

### 6. Physical Movement

Make movement more immersive with natural VR actions.

Physically lower your body to crouch or sharply raise both controllers to jump, while retaining conventional controls when needed.

### 7. Physical Weapon Recoil

One-handed shooting introduces physical muzzle climb, requiring you to compensate by adjusting your controller position.

Grip weapons with both hands for greater stability, rewarding controlled shooting over simply pointing and firing.

### 8. Smarter Controller Reconnection

Improved controller-disconnect handling provides clear in-headset guidance when tracking is lost.

Gameplay-specific acknowledgement controls help you recover without unnecessary interruptions during menus, loading screens or cinematics.

---

**Built on KHARVOX by CactusVRStudios.**

RIPSHIFT VR is an unofficial gameplay enhancement and is not affiliated with CactusVRStudios.

**Requires DOOM (2016) and the original KHARVOX VR mod.**

## Compatibility

Designed and tested with **KHARVOX v1.1.1**. Compatibility with earlier or later versions has not been tested and is not guaranteed.

## Install RIPSHIFT VR

1. Install a legitimate Windows copy of **DOOM (2016)** and the original [KHARVOX by CactusVRStudios](https://github.com/CactusVRStudios/KHARVOX).
2. Download the **RIPSHIFT VR Experimental** artifact from this repository's [GitHub Actions](https://github.com/MrEGS-Ops/KHARVOX-Custom/actions). Extract the ZIP into its **own permanent folder**, separate from DOOM and original KHARVOX.
3. Run `RIPSHIFT.exe`.
4. On first launch, choose the original `KharvoxLauncher.exe` inside your **CactusVRStudios KHARVOX** installation. RIPSHIFT uses required original shader files without modifying the original installation.
5. Select your DOOM installation in RIPSHIFT VR and start the game. Configure and calibrate RIPSHIFT's gameplay enhancements in the in-headset mod menu.

**Upgrading:** Preserve your existing RIPSHIFT `UserData` folder. Clean installs seed the included factory weapon defaults; upgrades will not overwrite user calibrations. The original KHARVOX directory stays separate.

**Packaging:** The add-on ships as one ZIP with `RIPSHIFT.exe` directly inside it, alongside the RIPSHIFT runtime and required dependencies. Original KHARVOX SFS shader files are not bundled.

## Project credits

| Component | Credit |
| --- | --- |
| RIPSHIFT VR gameplay enhancements and launcher | **MrEGS_** |
| Original KHARVOX DOOM (2016) VR conversion | **[CactusVRStudios](https://github.com/CactusVRStudios/KHARVOX)** |
| DOOM (2016) | Its respective publisher and developers |

RIPSHIFT VR is an independent community add-on and is not endorsed by CactusVRStudios.

Review [LICENSE.txt](LICENSE.txt) and [Docs/THIRD_PARTY_NOTICES.txt](Docs/THIRD_PARTY_NOTICES.txt) for applicable licences. This repository does not include DOOM itself.

## Compatibility notes

RIPSHIFT depends on KHARVOX's native runtime interfaces. Do not rename `KharvoxLayer.dll`, `KharvoxLayer.json`, protocol environment variables beginning with `KHARVOX_`, or original KHARVOX configuration filenames. They are **technical compatibility identifiers**, not RIPSHIFT product branding.
