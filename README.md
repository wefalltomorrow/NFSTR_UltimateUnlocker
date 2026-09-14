# NFS The Run — Selective Promo/DLC Unlocker

A preservation-focused fork of [xan1242/NFSTR_UltimateUnlocker](https://github.com/xan1242/NFSTR_UltimateUnlocker) for **Need for Speed: The Run v1.1.0.0**.

This fork does **not** unlock the whole game. Its purpose is much narrower: restore access to installed promotional, preorder, Limited Edition and discontinued DLC ownership content whose original EA entitlement checks can no longer succeed, while leaving normal progression intact.

## Why this exists

The Run contains promo/DLC content that can still be present in the game data but remains locked because the original ownership checks depended on EA/Autolog-era commerce services.

Reverse engineering of the v1.1 PC executable showed that this ownership path is handled by Frostbite `OnlineUnlocker` objects carrying `OfferId`, `PCSku`, `XenonSku` and `PS3Sku` fields. The final implementation hooks that specific entitlement path and grants only a small allow-list of known discontinued content offers through the game's own successful unlock routine.

Everything else continues through the original game logic.

## What it restores

The production allow-list currently contains:

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

The plugin also neutralizes the reflected `IsPromoContent` and `IsHiddenUnlock` metadata fields so installed promotional/DLC entries are visible to the UI.

## What it deliberately does **not** unlock

Normal progression remains authoritative. The mod does not bypass:

- Garage purchases or ordinary car progression
- Driver level requirements
- Stage / career progression
- Boss / story rewards
- Challenge medal requirements
- Multiplayer objective rewards
- Autolog recommendation rewards
- Time Savers
- XP/profile grants
- VIP/demo flags
- `olp_*` online-pass entitlements
- Generic `Unlockers[]` processing
- Blanket car unlocks
- Stage-select unlocks
- Ebisu / Autolog network code

This is the main difference from the original broad Ultimate Unlocker behavior.

## Verified behavior

The final implementation was validated in-game against a clean progression profile. Confirmed examples include:

- Limited Edition ownership content becomes available.
- Underground / Most Wanted / Carbon / Old Spice / AEM content is exposed as intended.
- Limited Edition reward cars still require their Challenge Series medals.
- The BMW 1 Series M Coupé still requires Driver Level 18.
- The Cesar DeLeon reward still requires defeating Cesar.
- Multiplayer reward cars still require their multiplayer objectives.
- Autolog reward cars still require their recommendation targets.
- Time Savers remain excluded.
- `olp_*` online-pass entitlements remain on vanilla logic.

## Installation

1. Download `NFSTR_SelectiveUnlocker.asi` from the latest GitHub release.
2. Place it in the game's ASI-loaded plugins/scripts location.
3. Do **not** load the original `NFSTR_UltimateUnlocker.asi` at the same time.

If you use NFS The Run FusionFix and want to preserve normal progression, keep its broad unlock options disabled:

```ini
[UNLOCKS]
UnlockAllCars = 0
UnlockChallenges = 0
UnlockEverything = 0
```

## Compatibility

The current production build targets the **v1.1.0.0 PC executable layout** used during development and verifies the expected bytes before installing the entitlement hook. If the executable layout does not match, the hook is not installed rather than blindly patching an unknown build.

This project is intentionally conservative: compatibility with other executable revisions should be treated as unverified until their relevant addresses and instruction bytes are confirmed.

## Building

Requirements:

- Visual Studio / MSBuild with x86 C++ support
- The included solution/project files

Build `NFSTR_UltimateUnlocker.sln` using **Release | x86**. CI builds the project and publishes the user-facing binary as `NFSTR_SelectiveUnlocker.asi` together with a SHA-256 file.

## Technical overview

The implementation:

1. Verifies the stock `OnlineUnlocker` method bytes.
2. Builds a trampoline so non-target entitlements can execute the original function unchanged.
3. Identifies the exact runtime subtype by vtable, preventing derived `GaragePurchaseUnlocker` objects from being treated as DLC ownership.
4. Reads the runtime `OfferId`.
5. Grants only exact allow-listed discontinued content offers through the game's stock success path.
6. Sends every other entitlement back through vanilla logic.

That distinction is important because ordinary garage-car unlocks share the same underlying method but must not be force-granted.

For the reverse-engineering history, original patch map and reflected class layout, see [RESEARCH.md](RESEARCH.md).

## Project history

This fork went through a series of increasingly narrow test builds. Earlier experiments proved that visibility metadata alone was insufficient, that garage-car matching hooks did not control the relevant UI lock state, and that globally granting the shared entitlement routine was too broad. Runtime logging from the later tests exposed the actual `OfferId` values and allowed the final selective implementation to be reduced to the exact ownership offers above.

See [CHANGELOG.md](CHANGELOG.md) for release history.

## Definitive Edition integration

This standalone ASI is also the reference implementation intended for the broader **Need for Speed: The Run Definitive Edition** project. When that project is complete, the same narrow entitlement logic is expected to be integrated into the DE patch instead of requiring users to install a separate unlocker component.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before changing unlock logic. Reverse-engineering changes should include executable/version evidence and in-game progression controls so the selective behavior remains verifiable.

## Credits

- **xan1242 / Lovro Pleše** — original NFSTR Ultimate Unlocker and injector-based implementation this fork is derived from.
- **ThirteenAG / NFS The Run FusionFix** — useful independent reference for the game's unlock-related systems during reverse engineering.
- **wefalltomorrow** — selective entitlement research, testing and production fork.

## License

MIT. See [LICENSE](LICENSE).
