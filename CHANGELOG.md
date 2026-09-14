# Changelog

## [1.1.0] - 2026-09-14

- Added `NFSTR_UltimateUnlocker.ini`.
- Added separate `UnlockDLC`, `UnlockTimeSavers` and `UnlockAll` options.
- `UnlockDLC=1` keeps the selective DLC/promo entitlement fix while preserving normal progression.
- `UnlockTimeSavers=1` grants the game's `timesavers_pack` entitlement.
- `UnlockAll=1` restores the original broad Ultimate Unlocker patches from xan1242's project.
- Renamed the release binary back to `NFSTR_UltimateUnlocker.asi` now that the plugin supports both selective and full-unlock modes.
- Rewrote the README to match the original project/fork style and document the new config.

## [1.0.0] - 2026-09-14

- Added the selective `OnlineUnlocker` entitlement hook for discontinued DLC/preorder/promo content.
- Kept `GaragePurchaseUnlocker` and normal progression on the game's original logic.
- Added executable-byte checks, ASLR-aware addresses and the original-method trampoline.
- Added `RESEARCH.md` with the reverse-engineering notes and test history.

[1.1.0]: https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.1.0
[1.0.0]: https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.0.0
