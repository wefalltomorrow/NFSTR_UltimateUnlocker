# Contributing

This fork intentionally keeps its scope narrow: restore discontinued promo/DLC ownership without changing normal progression.

## Before changing unlock behavior

Please read `RESEARCH.md`. The game's unlock system contains several independent mechanisms, and broad patches can easily make unrelated cars, stages or rewards available.

Changes that affect entitlement handling should preserve these invariants unless the PR explicitly proposes a separate optional feature:

- `GaragePurchaseUnlocker` remains vanilla.
- Driver level, career/stage, boss/story, challenge-medal, multiplayer-objective and Autolog requirements remain vanilla.
- Time Savers, XP/profile, VIP/demo and `olp_*` offers are not silently force-granted.
- The generic reflected `Unlockers[]` system is not disabled.

## Evidence expected for reverse-engineering changes

When adding or changing an address, offset, vtable, function or `OfferId`, include the evidence used to identify it. Useful evidence includes:

- executable version / SHA-256;
- expected instruction bytes;
- reflected Frostbite type/field metadata;
- runtime logs;
- A/B in-game tests with clear progression controls.

Please update `RESEARCH.md` when a reverse-engineering conclusion changes.

## Building

Build `NFSTR_UltimateUnlocker.sln` as **Release | x86**. CI publishes the resulting plugin under the user-facing name `NFSTR_SelectiveUnlocker.asi` and generates a SHA-256 checksum.

## Pull requests

Keep PRs focused. For unlock-related changes, list both the content expected to become available and at least one normal-progression control that must remain locked.
