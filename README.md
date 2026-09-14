# NFS The Run - Ultimate Unlocker

An ASI plugin for **Need for Speed: The Run v1.1.0.0**.

This implementation restores access to installed promotional, preorder and discontinued DLC ownership content while deliberately preserving the game's normal progression.

## What changed

The original implementation broadly bypassed the game's `Unlockers[]` system, blanket car lock checks and stage-select checks. Reverse engineering of the v1.1 PC executable showed that discontinued ownership content is instead handled by Frostbite `OnlineUnlocker` objects carrying `OfferId`, `PCSku`, `XenonSku` and `PS3Sku` fields.

The current implementation hooks that specific entitlement path, grants only a validated allow-list of discontinued content offers through the game's own successful unlock routine, and sends everything else back through the original game logic.

## Restored ownership offers

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

The plugin also preserves the original visibility behavior for installed promo/hidden content so the relevant entries can appear in the UI.

## Progression deliberately preserved

The implementation does **not** bypass:

- ordinary garage/car progression;
- driver level requirements;
- stage/career progression;
- boss/story rewards;
- challenge medal requirements;
- multiplayer objective rewards;
- Autolog recommendation rewards;
- Time Savers;
- XP/profile grants;
- VIP/demo flags;
- `olp_*` online-pass entitlements;
- generic `Unlockers[]` processing;
- blanket car unlocks;
- stage-select unlocks;
- Ebisu/Autolog network code.

This distinction matters because `GaragePurchaseUnlocker` shares the same underlying entitlement method as `OnlineUnlocker`; the hook therefore verifies the exact runtime subtype before granting anything.

## Compatibility

This is currently validated against the DRM-free **v1.1.0.0 PC executable layout** used by the original project. The entitlement hook verifies the expected stock instruction bytes before installing, so it will fail closed rather than blindly patching an unknown executable revision.

## Validation

The selective implementation was tested in-game against normal progression controls. DLC/promo ownership content becomes available, while level-gated cars, boss rewards, challenge medal rewards, multiplayer objective rewards and Autolog recommendation rewards remain locked until their normal requirements are satisfied.

The implementation was developed and validated in the fork at:

https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker

The corresponding production release is:

https://github.com/wefalltomorrow/NFSTR_UltimateUnlocker/releases/tag/v1.0.0
