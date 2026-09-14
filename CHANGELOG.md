# Changelog

All notable changes to this fork are documented here.

## [1.0.0] - 2026-09-14

First production release of the selective promo/DLC unlocker.

### Added

- Selective `OnlineUnlocker` entitlement hook for discontinued promotional/DLC ownership content.
- Exact runtime `OfferId` allow-list for Carbon, Most Wanted, Underground, Heroes & Villains, Supercar, Dr Pepper, Old Spice and AEM content.
- Exact subtype discrimination so derived `GaragePurchaseUnlocker` objects stay on original game logic.
- Original-method trampoline for every non-target entitlement.
- Expected-byte verification before installing the executable hook.
- ASLR-aware address rebasing.
- Production GitHub Actions build with SHA-256 output.
- Detailed reverse-engineering documentation in `RESEARCH.md`.

### Preserved

- Driver-level progression.
- Career/stage progression.
- Boss/story rewards.
- Challenge-medal requirements.
- Multiplayer-objective rewards.
- Autolog recommendation rewards.
- Ordinary garage-car progression.

### Explicitly excluded

- Time Savers.
- XP/profile grants.
- VIP/demo entitlements.
- `olp_*` online-pass entitlements.
- Generic `Unlockers[]` bypass behavior.
- Blanket car unlocks.
- Stage-select unlocks.
- Ebisu/Autolog network patches.

### Research milestones

- Proved `IsPromoContent` / `IsHiddenUnlock` are visibility controls rather than ownership grants.
- Recovered the reflected `Unlockable`, `Unlocker` and `OnlineUnlocker` layouts from Frostbite metadata.
- Identified `OnlineUnlocker::OfferId` as the relevant discontinued content entitlement gate.
- Identified the game's stock successful grant routine and reused it instead of manually forcing car state.
- Demonstrated that `GaragePurchaseUnlocker` shares the same stock method, requiring subtype discrimination.
- Removed the provisional `olp_le_free` grant after A/B testing confirmed Limited Edition content does not require it.

[1.0.0]: https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.0.0
