# NFSTR Selective Promo/DLC Unlocker v1.0.0

This is the first production release of the selective unlocker for **Need for Speed: The Run v1.1.0.0**.

Unlike the original broad Ultimate Unlocker behavior, this build restores only known discontinued promotional/DLC ownership entitlements and leaves normal progression intact.

## Restored ownership content

- Carbon preorder / Heroes & Villains content
- Most Wanted preorder / Heroes & Villains content
- Underground preorder / Heroes & Villains content
- Heroes & Villains pack
- Supercar pack
- Dr Pepper Ford GT content
- Dr Pepper Chevrolet content
- Dr Pepper Porsche content
- Old Spice content
- AEM content

## Preserved vanilla progression

The release does **not** bypass driver levels, career stages, bosses, challenge medals, multiplayer objectives, Autolog recommendations or ordinary garage-car unlocks.

It also deliberately leaves Time Savers, XP/profile grants, VIP/demo flags and all `olp_*` online-pass entitlements untouched.

## Technical summary

The plugin hooks the game's reflected `OnlineUnlocker` entitlement path, identifies the exact runtime subtype by vtable, matches only known content `OfferId` values and then invokes the game's own successful entitlement grant routine. All non-target entitlements execute the original function through a trampoline.

The hook verifies the expected executable bytes before patching and uses rebased addresses for the validated v1.1 PC executable layout.

## Installation

Place `NFSTR_SelectiveUnlocker.asi` in the game's ASI-loaded plugins/scripts location. Do not load the original `NFSTR_UltimateUnlocker.asi` at the same time.

If using FusionFix, keep its broad unlock settings disabled if you want normal progression:

```ini
[UNLOCKS]
UnlockAllCars = 0
UnlockChallenges = 0
UnlockEverything = 0
```

See the repository README and `RESEARCH.md` for the full behavior and reverse-engineering history.
