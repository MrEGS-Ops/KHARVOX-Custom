# RIPSHIFT VR

**A gameplay enhancement for KHARVOX**  
**Developed by MrEGS_**

RIPSHIFT VR adds enhanced physical VR weapon handling, Gunslinger holsters, in-headset weapon calibration, recoil, KILLTIME, Painsaw and related gameplay controls to the original KHARVOX DOOM (2016) VR conversion.

RIPSHIFT VR is **not KHARVOX itself**. The original KHARVOX VR conversion is developed by **CactusVRStudios** and remains a separate, required installation.

## Install

1. Install a legitimate Windows copy of **DOOM (2016)** and the original [KHARVOX by CactusVRStudios](https://github.com/CactusVRStudios/KHARVOX).
2. Download the **RIPSHIFT VR Experimental** artifact from this repository's GitHub Actions. Extract the single ZIP into its **own permanent folder**, separate from DOOM and original KHARVOX.
3. Run `RIPSHIFT.exe`.
4. On first launch, choose the original `KharvoxLauncher.exe` inside your **CactusVRStudios KHARVOX** installation. RIPSHIFT uses required original shader files without modifying the original installation.
5. Select your DOOM installation in RIPSHIFT VR and start the game. Configure and calibrate the RIPSHIFT gameplay enhancements in the in-headset mod menu.

**Upgrading:** Preserve your existing RIPSHIFT `UserData` folder. Clean installs seed the included factory weapon defaults; upgrades will not overwrite user calibrations. The original KHARVOX directory stays separate.

**Packaging:** This release ships one ZIP with `RIPSHIFT.exe` directly inside it, alongside the RIPSHIFT runtime and required dependencies. Original KHARVOX SFS shader files are not bundled.

## Project identity and credits

| Component | Credit |
| --- | --- |
| RIPSHIFT VR gameplay enhancements and launcher | **MrEGS_** |
| Original KHARVOX DOOM (2016) VR conversion | **CactusVRStudios** |
| DOOM (2016) | Its respective publisher and developers |

**Independent community add-on. Requires the original KHARVOX installation. Not affiliated with or endorsed by CactusVRStudios.**

Original KHARVOX: https://github.com/CactusVRStudios/KHARVOX  
Original KHARVOX README (retained for upstream documentation and credits): [Docs/UPSTREAM-KHARVOX-README.md](Docs/UPSTREAM-KHARVOX-README.md)

Review [LICENSE.txt](LICENSE.txt) and [Docs/THIRD_PARTY_NOTICES.txt](Docs/THIRD_PARTY_NOTICES.txt) for applicable licences. This project does not include DOOM itself.

## Compatibility notes

RIPSHIFT depends on KHARVOX's native runtime interfaces. Do not rename `KharvoxLayer.dll`, `KharvoxLayer.json`, protocol environment variables beginning with `KHARVOX_`, or original KHARVOX configuration filenames. They are **technical compatibility identifiers**, not RIPSHIFT product branding.

