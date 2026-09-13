# NFS The Run - Selective Promo/DLC Unlocker

A selective fork of [xan1242/NFSTR_UltimateUnlocker](https://github.com/xan1242/NFSTR_UltimateUnlocker) for **Need for Speed: The Run v1.1.0.0**.

## What it does

This build exposes installed hidden/promo content and satisfies only the known discontinued DLC/promo ownership entitlements needed by that content.

It is deliberately **not** an unlock-everything or Time Savers mod. Normal progression remains authoritative.

### Granted content ownership offers

- `r_carbon`
- `r_mostwanted`
- `r_underground`
- `handv_pack`
- `supercar_pack`
- `dp_fordgt`
- `dp_chevrolet`
- `dp_porsche`
- `os_pack`
- `aem_adsales`

### Left untouched

- Garage purchases and ordinary car unlock progression
- Driver level requirements
- Stage / career progression
- Challenge medal requirements
- Boss / story rewards
- Multiplayer objective rewards
- Autolog recommendation rewards
- Time Savers
- XP/profile grants
- VIP/demo flags
- `olp_*` online-pass entitlements
- Generic `Unlockers[]` processing
- Broad car/stage unlock patches
- Ebisu / Autolog network patches

The implementation identifies the exact `OnlineUnlocker` subtype and only grants the allow-listed content offers above. Derived `GaragePurchaseUnlocker` objects and all unrelated entitlements execute the game's original logic.

## Visibility

The plugin also preserves the proven Ultimate Unlocker visibility behavior by neutralizing the reflected `IsPromoContent` and `IsHiddenUnlock` property names. This makes installed promotional/DLC entries visible without globally satisfying their progression requirements.

## Installation

Place `NFSTR_SelectiveUnlocker.asi` in the game's ASI-loaded plugins/scripts location.

Do **not** load the original `NFSTR_UltimateUnlocker.asi` at the same time.

If using NFS The Run FusionFix, keep its broad unlock options disabled if you want normal progression:

```ini
[UNLOCKS]
UnlockAllCars = 0
UnlockChallenges = 0
UnlockEverything = 0
```

## Validation

The production behavior was validated against the test 8 build. Confirmed examples include:

- Limited Edition ownership content available
- Underground / Most Wanted / Carbon / Old Spice / AEM content exposed as intended
- Limited Edition reward cars still requiring their Challenge Series medals
- Driver Level 18 BMW still locked until level requirement is met
- Cesar DeLeon reward still locked until the boss requirement is met
- Multiplayer and Autolog reward cars still obeying their original requirements
- Time Savers excluded
- `olp_*` online-pass entitlements excluded

## Definitive Edition integration

This ASI is the standalone production form of the selective unlocker. The implementation is intentionally narrow and self-contained so the same logic can later be integrated into the **Need for Speed: The Run Definitive Edition** patch rather than shipping as a separate user-facing component.

See [`RESEARCH.md`](RESEARCH.md) for the reverse-engineering notes and original patch map.
