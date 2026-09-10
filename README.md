# NFS The Run - Selective Promo/DLC Unlocker

A selective fork of [xan1242/NFSTR_UltimateUnlocker](https://github.com/xan1242/NFSTR_UltimateUnlocker) for **Need for Speed: The Run v1.1.0.0 (DRM-free executable)**.

## Goal

Expose installed promotional / hidden DLC content **without** turning the game into a Time Saver / unlock-everything build.

The test branch intentionally preserves normal progression:

- **No forced car unlocks**
- **No stage / Challenge Series unlock bypasses**
- **No generic `Unlockers` requirement bypass**
- **No Time Saver-style all-content unlock**
- **No online / Autolog patches enabled**

The only active patches are the two metadata changes inherited from the original Ultimate Unlocker that neutralize:

- `IsPromoContent`
- `IsHiddenUnlock`

This is intentionally narrow so we can verify whether those two flags alone expose the legitimate promo/DLC content the user wants while ordinary cars, stages and progression remain locked normally.

## Why the original broad unlock patches were removed

The original plugin contains several independent patch families. The broad progression-changing ones are deliberately excluded here:

- `CarUnlockHook1` / `CarUnlockHook2` force a garage car's unlocked byte to `1`.
- The two `stage select unlock` NOPs bypass stage-selection locks.
- Corrupting the reflected `Unlockers` property disables generic unlock requirements; FusionFix independently implements its `UnlockEverything` option by intercepting this same `Unlockers` property.

See [`RESEARCH.md`](RESEARCH.md) for the complete patch map, including the commented Ebisu / Autolog / network code.

## Installation

Build or download `NFSTR_SelectiveUnlocker.asi` and place it in the game's ASI-loaded plugins/scripts location. Do **not** run the original `NFSTR_UltimateUnlocker.asi` at the same time.

If using NFS The Run FusionFix, keep its broad unlock options disabled if you want normal progression:

```ini
[UNLOCKS]
UnlockAllCars = 0
UnlockChallenges = 0
UnlockEverything = 0
```

## Status

This branch is a targeted **test build**. The next check is simple: verify that desired promo/DLC cars/content appear, while ordinary progression-locked cars, tracks/stages and Challenge Series entries remain locked until earned.
