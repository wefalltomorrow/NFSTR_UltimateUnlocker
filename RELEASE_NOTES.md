# NFSTR Ultimate Unlocker v1.1.0

This release adds an INI so the unlocker no longer has to be all-or-nothing.

## Default setup

```ini
[UNLOCKS]
UnlockDLC = 1
UnlockTimeSavers = 0
UnlockAll = 0
```

With the defaults, the mod restores the old DLC/preorder/promo entitlements while normal progression still works normally.

## Options

- `UnlockDLC=1` - restores the known discontinued DLC, preorder and promo content.
- `UnlockTimeSavers=1` - grants the game's `timesavers_pack` entitlement.
- `UnlockAll=1` - enables the original broad Ultimate Unlocker patches, including the blanket car/challenge/stage unlock behavior.

`UnlockAll` and `UnlockTimeSavers` are separate, so enable both if you want both behaviors.

## Install

Place these files in the folder used by your ASI loader:

- `NFSTR_UltimateUnlocker.asi`
- `NFSTR_UltimateUnlocker.ini`

The current build is for the DRM-free Need for Speed: The Run v1.1.0.0 PC executable.
