# NFS The Run - Ultimate Unlocker

ASI plugin for the DRM-free **Need for Speed: The Run v1.1.0.0** executable.

This keeps the original "unlock everything" behavior, but also adds a DLC-only mode so the old preorder/promo/DLC content can be enabled without skipping the normal game progression.

## Install

Copy these files to the folder used by your ASI loader:

- `NFSTR_UltimateUnlocker.asi`
- `NFSTR_UltimateUnlocker.ini`

## Settings

```ini
[UNLOCKS]
UnlockDLC = 1
UnlockAll = 0
```

### UnlockDLC

Turns on the old DLC, preorder and promo content that no longer unlocks normally.

This includes the Carbon, Most Wanted and Underground preorder content, Heroes & Villains, Supercar, Dr Pepper, Old Spice and AEM content.

Normal requirements still work, so level-gated cars, boss rewards, challenge rewards, multiplayer rewards and similar progression are left alone.

### UnlockAll

Turns on the original full Ultimate Unlocker behavior and also enables the Time Savers entitlement.

This unlocks cars, challenges, stage select and the other broad unlocks from the original mod.

## Notes

The DLC-only mode uses the game's own DLC entitlement path rather than forcing every car to be unlocked.

The current implementation is built for the DRM-free v1.1.0.0 PC executable layout and checks the expected `OnlineUnlocker` bytes before installing the DLC hook.
