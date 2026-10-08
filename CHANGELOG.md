# KHARVOX Custom Changelog

Permanent history for the custom branch. Old successful workflow runs and artifacts may be removed, while this file stays in the repository.

## Clean custom core

Base: official KHARVOX main at `ff348bb`.

### Retained custom features
- Custom Mods button and separate Custom Mods window.
- Weapon Wheel Remap.
- Glory Kill Slow-Mo slider.

### Build and update infrastructure
- Install-once watcher with fast patching for later builds.
- Full build artifact for first install.
- Small patch artifact for later updates.
- Watcher waits for DOOM to close before patching.
- Previous patched files retained under `.rollback\previous`.
- Build/patch number shown in the launcher title bar.
- Runtime and diagnostic logs stored under the installed build's `logs\` folder instead of `%TEMP%`.
- Build caching enabled for vcpkg dependencies and native CMake output.
- Retention policy keeps only the latest 5 successful build runs and their artifacts.

## VEGA
Future local KHARVOX integration work uses the codename **VEGA** in the repository and codebase.
