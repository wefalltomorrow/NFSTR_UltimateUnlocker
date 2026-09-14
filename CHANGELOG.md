# Changelog

## [1.1.1] - 2026-09-14

- Release downloads now put the ASI and INI together in one ZIP.
- Added a SHA-256 checksum for the release ZIP.
- No unlock behavior changed from v1.1.0.

## [1.1.0] - 2026-09-14

- Added `NFSTR_UltimateUnlocker.ini`.
- Added two simple modes: `UnlockDLC` and `UnlockAll`.
- `UnlockDLC=1` restores the old DLC/preorder/promo content without skipping normal progression.
- `UnlockAll=1` enables the original broad Ultimate Unlocker behavior and the Time Savers entitlement.
- Renamed the release binary back to `NFSTR_UltimateUnlocker.asi`.
- Simplified the README and INI comments for normal users.

## [1.0.0] - 2026-09-14

- Added the selective `OnlineUnlocker` entitlement hook for discontinued DLC/preorder/promo content.
- Kept `GaragePurchaseUnlocker` and normal progression on the game's original logic.
- Added executable-byte checks, ASLR-aware addresses and the original-method trampoline.
- Added `RESEARCH.md` with the reverse-engineering notes and test history.

[1.1.1]: https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.1.1
[1.1.0]: https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.1.0
[1.0.0]: https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.0.0
