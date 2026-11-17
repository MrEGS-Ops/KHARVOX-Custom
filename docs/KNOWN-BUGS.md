# Known Bugs — RIPSHIFT VR

These issues are **unresolved** until headset testing confirms a fix. A successful compile does not close a bug.

| Location or feature | Observed issue | Workaround | Status |
| --- | --- | --- | --- |
| Lazarus Labs, **after completing the interactive minigame** | The held weapon changes position after the minigame finishes | **Restart from checkpoint** | **Open.** Failed control-state recovery and weapon-joint outlier changes removed. |
| Gunslinger weapon wheel (reproduced on experimental **build 658**) | The equipped gun is visibly drawn inside/through the radial wheel | Close the wheel (temporary only) | **Open, fix awaiting headset verification.** Experimental render suppression added after build 658. |
| Completed level / NEW PERSONAL BEST notification (reported 17 Nov 2026) | The result banner remains visible indefinitely, including over the campaign menu | Relaunch to clear the banner | **Open, fix awaiting headset verification.** Five-second elapsed-time expiration added after build 661. |

## Tracking rules

- Record the DOOM level, experimental build number and exact symptom.
- Do not treat compiled code or passing CI as headset confirmation.
- Do not change saved grip profiles or UserData to work around render defects.
- Add new unresolved issues here; move an issue out only after a confirming headset test.
