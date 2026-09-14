# NFS The Run - Ultimate Unlocker

Fork of [xan1242/NFSTR_UltimateUnlocker](https://github.com/xan1242/NFSTR_UltimateUnlocker) for the DRM-free **Need for Speed: The Run v1.1.0.0** executable.

The original mod just unlocked everything. This fork keeps that option, but also adds a cleaner mode for restoring the old DLC/preorder/promo content without skipping normal game progression.

## Install

Copy these files to the folder your ASI loader uses for The Run:

- `NFSTR_UltimateUnlocker.asi`
- `NFSTR_UltimateUnlocker.ini`

Don't load another copy of the original Ultimate Unlocker at the same time.

## Config

```ini
[UNLOCKS]
UnlockDLC = 1
UnlockTimeSavers = 0
UnlockAll = 0
```

### UnlockDLC

Default: `1`

Restores the discontinued DLC, preorder and promo entitlements we found in the PC executable, while leaving normal unlock requirements alone.

This includes the Carbon, Most Wanted and Underground preorder content, Heroes & Villains, Supercar, Dr Pepper, Old Spice and AEM content.

Cars that still have a normal requirement (driver level, boss win, challenge gold medal, multiplayer objective, Autolog recommendations, etc.) will still require it.

### UnlockTimeSavers

Default: `0`

Enables the game's `timesavers_pack` entitlement.

This is separate from `UnlockAll`, so you can turn the Time Savers entitlement on or off independently.

### UnlockAll

Default: `0`

Enables the original broad Ultimate Unlocker behavior from xan1242's project, including the blanket car/challenge/stage unlock patches.

If you want the old "unlock everything" setup plus the Time Savers entitlement, use:

```ini
[UNLOCKS]
UnlockDLC = 1
UnlockTimeSavers = 1
UnlockAll = 1
```

## Notes

`UnlockDLC = 1` is the recommended setup if you still want to play through the game normally.

The DLC mode works by handling the game's `OnlineUnlocker` entitlement checks for a small list of known discontinued offers. `GaragePurchaseUnlocker` and the normal progression unlockers are left alone.

The current code is built around the DRM-free v1.1.0.0 PC executable layout. It checks the expected `OnlineUnlocker` bytes before installing that hook.

For the reverse-engineering notes and test history, see [RESEARCH.md](RESEARCH.md).

This code will also be used later as the unlocker part of our NFS The Run Definitive Edition patch.

## Building

Build `NFSTR_UltimateUnlocker.sln` as **Release | x86**.

## Credits

- **xan1242 / Lovro Pleše** - original Ultimate Unlocker and injector implementation
- **ThirteenAG** - NFS The Run FusionFix, which was useful as a reference while tracing the unlock system
- **wefalltomorrow** - DLC entitlement research/testing and this fork

## License

MIT - see [LICENSE](LICENSE).
