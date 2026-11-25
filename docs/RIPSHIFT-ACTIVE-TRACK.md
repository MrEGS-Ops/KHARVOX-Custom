# RIPSHIFT — active experimental build

This is the current gameplay add-on, not a separate Mod Manager application.

- Active branch: `feature/object-discovery-full-grip-calibration-20261123`
- Distribution workflow: `.github/workflows/build-ripshift.yml`
- Native build workflow: `.github/workflows/build-custom.yml`
- Experimental artifact: `RIPSHIFT-Experimental`
- Application: `_RIPSHIFT.exe`
- Runtime DLL: `KharvoxLayer.dll`, source-matched and verified
- Original KHARVOX remains a separate required VR installation.
- Player configuration is in the RIPSHIFT in-headset Mods menu.
- Existing portable UserData and native calibration are preserved.

The package rejects retired external Mod Manager or updater binaries and
does not contain a NativeBridge wrapper. GitHub Actions accepts only a
successful same-branch native build with unchanged runtime source and
matching experimental DLL provenance.

Historical Git branches and builds remain available for rollback. They
are not current build instructions or release sources.
