# NFS The Run - Ultimate Unlocker

Fork of [xan1242/NFSTR_UltimateUnlocker](https://github.com/xan1242/NFSTR_UltimateUnlocker) for the DRM-free **Need for Speed: The Run v1.1.0.0** executable.

The original mod unlocks basically everything. This fork keeps that option, but also lets you unlock just the old DLC/preorder/promo content so you can still play through the game normally.

## Install

Copy these two files to the folder used by your ASI loader:

- `NFSTR_UltimateUnlocker.asi`
- `NFSTR_UltimateUnlocker.ini`

Don't load this together with another copy of the original Ultimate Unlocker.

## Settings

```ini
[UNLOCKS]
UnlockDLC = 1
UnlockAll = 0
```

### UnlockDLC

Default: `1`

Turns on the old DLC, preorder and promo content that can no longer be unlocked normally.

This includes the Carbon, Most Wanted and Underground preorder content, Heroes & Villains, Supercar, Dr Pepper, Old Spice and AEM content.

Normal unlock requirements still work. For example, cars that need a certain driver level, boss win, challenge medal, multiplayer objective or Autolog recommendation will still need those requirements.

If you want to play through the game normally, leave this on and keep `UnlockAll` off.

### UnlockAll

Default: `0`

Turns on the original full Ultimate Unlocker behavior and also enables the Time Savers entitlement.

This unlocks cars, challenges, stage select and the other broad unlocks from the original mod. Use this if you just want everything unlocked.

## Notes

The DLC-only mode uses the game's own DLC entitlement system instead of forcing every car to be unlocked. That's why normal progression still works with `UnlockDLC = 1`.

The current build is made for the DRM-free v1.1.0.0 PC executable layout.

For the reverse-engineering notes and test history, see [RESEARCH.md](RESEARCH.md).

This code is also planned to be used later in our NFS The Run Definitive Edition patch.

## Building

Build `NFSTR_UltimateUnlocker.sln` as **Release | x86**.

## Credits

- **xan1242 / Lovro Pleše** - original Ultimate Unlocker and injector implementation
- **ThirteenAG** - NFS The Run FusionFix, useful as a reference while tracing the unlock system
- **wefalltomorrow** - DLC entitlement research/testing and this fork

## License

MIT - see [LICENSE](LICENSE).
