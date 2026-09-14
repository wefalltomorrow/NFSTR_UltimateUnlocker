# NFSTR selective promo/DLC unlock research

This document records the reverse-engineering work behind the production selective unlocker and the reasons it differs from the original broad `NFSTR_UltimateUnlocker` behavior.

The target used for the final work was the **Need for Speed: The Run v1.1.0.0 PC executable layout**.

## Final design

The production plugin does two narrowly scoped things:

1. Neutralizes the reflected visibility metadata names `IsPromoContent` and `IsHiddenUnlock` so installed promotional/DLC content can appear in the UI.
2. Hooks the game's `OnlineUnlocker` entitlement method and grants only exact, known discontinued content `OfferId` values through the game's own successful entitlement path.

Everything else remains on vanilla logic.

The final allow-list is:

```text
r_carbon
r_mostwanted
r_underground
handv_pack
supercar_pack
dp_fordgt
dp_chevrolet
dp_porsche
os_pack
aem_adsales
```

Explicitly excluded from the selective grant path are ordinary `GaragePurchaseUnlocker` objects, Time Savers, XP/profile entitlements, VIP/demo flags and the `olp_*` online-pass family.

---

## Original Ultimate Unlocker patch map

The upstream plugin applies several independent patch families. They are not all part of the same system.

### `0x00834303` and `0x0083434F`

Upstream:

```cpp
injector::MakeNOP(0x834303, 2);
injector::MakeNOP(0x83434F, 2);
```

These remain excluded from this fork. They appear related to legacy online/network-connected handling and were not needed for selective DLC ownership restoration.

### Reflected field-name patches

Upstream:

```cpp
injector::WriteMemory<uint8_t>(0x25A35EC, 0, true); // Unlockers
injector::WriteMemory<uint8_t>(0x25A362D, 0, true); // IsPromoContent
injector::WriteMemory<uint8_t>(0x25A363D, 0, true); // IsHiddenUnlock
```

The resulting string mutations are:

| Address | Original field | Result | Production selective build |
|---|---|---|---|
| `0x025A35EC` | `Unlockers` | `Unlocker` | **Excluded** |
| `0x025A362D` | `IsPromoContent` | `IsPromoConten` | **Included** |
| `0x025A363D` | `IsHiddenUnlock` | `IsHiddenUnloc` | **Included** |

Destroying the reflected `Unlockers` property is a broad requirement bypass. FusionFix independently corroborates this idea with its `UnlockEverything` option, which intercepts the same property name. That patch is intentionally not used here.

The two visibility fields are kept because they expose installed promotional/hidden entries without themselves satisfying progression requirements.

### Broad car hooks

Upstream hooks:

```text
0x0093E00D
0x0093F214
```

Both write `1` to byte `+0x18` on the relevant unlockable object and bypass the stock locked path.

Static reflection metadata later established that the actual `Unlockable` layout for this executable is:

```text
+0x10 Icon
+0x14 DisplayString
+0x18 IsRunTimeUnlocked
+0x19 IsUnlocked
+0x1A HideHowTo
+0x1B IsHiddenUnlock
+0x1C IsPromoContent
```

Therefore the upstream `+0x18` write sets `IsRunTimeUnlocked`, not the reflected persistent `IsUnlocked` byte.

These hooks are completely excluded from the selective production build.

### Stage-select patches

Upstream:

```cpp
injector::MakeNOP(0x930C00, 2);
injector::MakeNOP(0x9313A2, 2);
```

These are broad stage-selection unlocks and are excluded.

### Commented network/Ebisu code

The original repository also contains commented-out network and Ebisu/Autolog patches, including a NOP near `0x00DD76CB` and a group of return-zero stubs around `0x018AEF80`–`0x018AF640`.

They remain excluded. They were not required for the entitlement fix and should be treated as a separate offline-service compatibility research area.

---

## Reflection metadata breakthrough

The v1.1 executable contains Frostbite runtime reflection metadata rather than merely leftover debug names. Parsing the `typeinfo`, `fieldinf` and constructor-table data exposed the unlocker class hierarchy and field layouts.

Relevant unlocker classes include:

```text
Win32Unlocker
PS3NAUnlocker
GaragePurchaseUnlocker
OnlineUnlocker
NewsReadUnlocker
PhotoUploadedUnlocker
SpeedWallLeaderUnlocker
RecommendsUnlocker
ChallengePackCompletionCountUnlocker
ChallengePackCompletionUnlocker
StoryModeCompletionUnlocker
ChallengeCompletionUnlocker
StageCompletionUnlocker
FinishPositionUnlocker
AccoladeUnlocker
CarMileageUnlocker
LevelUpUnlocker
CompleteObjectiveUnlocker
AnyXObjectivesCompleteUnlocker
XPlaygroupObjectivesCompleteUnlocker
XObjectivesCompleteUnlocker
NullUnlocker
```

There is no separate `DLCOwnedUnlocker`, `PromoOwnedUnlocker` or `PreorderOwnedUnlocker`. The entitlement role is handled by `OnlineUnlocker`.

### `Unlockable`

```text
IsRunTimeUnlocked +0x18
IsUnlocked        +0x19
HideHowTo         +0x1A
IsHiddenUnlock    +0x1B
IsPromoContent    +0x1C
DisplayString     +0x14
Icon              +0x10
```

### `Unlocker`

```text
HowToDesc    +0x10
Unlockables  +0x14
Unhideables  +0x18
LogUnlock    +0x1C
```

### `UnlockerArray`

```text
Unlockers +0x00
```

This explains why deleting the reflected `Unlockers` property behaves like a global requirement bypass.

### `OnlineUnlocker`

`OnlineUnlocker` has size `0x30` and the following entitlement fields:

```text
OfferId   +0x20
PS3Sku    +0x24
XenonSku  +0x28
PCSku     +0x2C
```

### Other progression examples

`LevelUpUnlocker`:

```text
Level +0x20
```

`StageCompletionUnlocker`:

```text
Stage         +0x20
AttemptNumber +0x24
```

`ChallengeCompletionUnlocker`:

```text
Challenge +0x20
```

These distinct classes are why the final implementation can leave normal progression untouched rather than trying to infer progression from car metadata.

---

## Vtables and constructors

Relevant vtables:

```text
OnlineUnlocker          0x024791FC
GaragePurchaseUnlocker  0x0247920C
PS3NAUnlocker           0x0247921C
Win32Unlocker           0x0247922C
```

Relevant constructor/type factory functions:

```text
OnlineUnlocker           0x02252E90
GaragePurchaseUnlocker   0x02252F00
PS3NAUnlocker            0x02252F70
Win32Unlocker            0x02252FC0
LevelUpUnlocker          0x022529F0
StageCompletionUnlocker  0x02252BA0
ChallengeCompletion      0x02252C00
```

A critical detail is that `GaragePurchaseUnlocker` derives from / shares the stock entitlement method used by `OnlineUnlocker`. A hook that blindly grants every call to that method therefore also unlocks ordinary garage cars. The production implementation prevents this by requiring the object's vtable to match the exact `OnlineUnlocker` vtable before considering an `OfferId` for the allow-list.

---

## Stock entitlement path

The stock `OnlineUnlocker` method begins at:

```text
0x008D0BA0
```

Its original first eight bytes in the validated executable are:

```text
51 55 8B E9 8B 55 20 56
```

The method reads `this+0x20` (`OfferId`), performs entitlement lookup/validation, and on success reaches the game's unlock manager.

The successful entitlement path ultimately calls:

```text
0x007F3780
```

with the unlock manager located from:

```text
*(void**)0x02882500 + 0x3CB4
```

The production plugin therefore does not manually set car unlock bytes. For allow-listed content it calls the game's own successful grant routine with the original `OnlineUnlocker` object.

For all non-target calls, a trampoline executes the original bytes and returns to the stock function so vanilla behavior is preserved.

---

## Runtime-discovered entitlement IDs

Instrumentation in the late test builds exposed the actual runtime offers.

Known content offers selected for the production allow-list:

```text
r_carbon
r_mostwanted
r_underground
handv_pack
supercar_pack
dp_fordgt
dp_chevrolet
dp_porsche
os_pack
aem_adsales
```

Other observed online offers deliberately left vanilla included:

```text
timesavers_pack
dp_profile
dp_xp
vip_world
vip_hp
vip_shift2
vip_hpios
vip_nfs12ios
demo_nfs12
demo_referral
olp_free
olp_le_free
olp_purchase
```

The `olp_*` trio behaved as a separate online-pass family. `olp_le_free` was temporarily tested because of its `le` name, then removed; Limited Edition content remained available without it, confirming it was unnecessary for the desired DLC/promo restoration.

Many ordinary cars appeared as `GaragePurchaseUnlocker` offers such as:

```text
g_bmw_1m_cou_11
g_che_el_cam_70
g_for_mus_302_69
...
```

This provided direct runtime proof that globally granting the shared method would be too broad.

---

## Test history

### Test 1 — visibility only

Neutralized `IsPromoContent` and `IsHiddenUnlock`.

Result: promo/DLC entries became visible, but ownership-gated content remained locked. Normal progression remained intact.

Conclusion: visibility metadata is necessary for presentation but is not the ownership gate.

### Tests 2–4 — garage matching hooks

Experiments around `getMatchingGarageCar` and its callers produced no useful runtime observations while browsing the relevant car UI.

Conclusion: that path was not authoritative for the ownership state being investigated.

### Test 5 — upstream broad car branches

Hooked the two original car branches selectively.

Result: no useful runtime execution in the target browsing path. This path was retired.

### Test 6 — grant every call to the shared entitlement method

This was the major breakthrough. Limited Edition / promo ownership content became available, proving the correct entitlement routine had been found.

However, ordinary progression cars such as the BMW 1M and Cesar DeLeon El Camino also became available because `GaragePurchaseUnlocker` shares that method. `timesavers_pack` was also observed on the same path.

Conclusion: the target was correct, but subtype and offer discrimination were required.

### Test 7 — exact subtype + allow-list

Restricted grants to exact `OnlineUnlocker` objects and selected content `OfferId` values. All `GaragePurchaseUnlocker` calls returned to the original game logic.

Result: desired promo/DLC ownership content became available while level, boss, challenge-medal, multiplayer-objective and Autolog requirements remained locked normally.

### Test 8 — remove `olp_le_free`

Removed the provisional `olp_le_free` grant.

Result: Limited Edition content remained available and progression remained correct.

Conclusion: `olp_le_free` belongs with the online-pass family and is not needed for the selective content restoration.

Test 8 became the basis of the production implementation.

---

## Production safety properties

The production code intentionally includes several guardrails:

- Rebased addresses are used relative to the executable image base.
- The target method bytes are verified before patching.
- The original method is preserved through a trampoline.
- Only the exact `OnlineUnlocker` vtable is eligible for forced entitlement success.
- Offer matching is exact-string allow-list matching.
- If the stock unlock manager is unavailable, the hook falls back to original behavior.
- `GaragePurchaseUnlocker` and all unrelated offers remain vanilla.

The plugin does not use the broad `Unlockers` bypass, car-unlock hooks, stage-select NOPs or Ebisu/Autolog stubs.

---

## Related projects / references

### NFS The Run FusionFix

FusionFix independently corroborated that the generic `Unlockers` property is associated with broad requirement bypass behavior and that challenge UI unlocking is a separate mechanism.

One correction discovered during this work: a FusionFix-style `Unlockable` struct used during early experiments treated `+0x18` as `IsUnlocked`; the executable's reflected metadata shows `+0x18` is actually `IsRunTimeUnlocked`, with `IsUnlocked` at `+0x19` and `IsPromoContent` at `+0x1C`.

### NFSTR_Loadless

`xan1242/NFSTR_Loadless` is a speedrunning/load-state tool, not a faster-loading patch. Its event hooks may still be useful as references for future Definitive Edition work that needs reliable loading-state detection.

### ALI213 Limited Edition crack

A historical ALI213 release advertised full Limited Edition unlocking, but its executable is heavily packed and reconstructs the original Frostbite sections at runtime. Static comparison was therefore less useful than following the unprotected v1.1 executable's reflection metadata and entitlement path directly.

---

## Definitive Edition note

The standalone ASI is intended to remain the reference implementation for this feature. The same narrow logic can later be folded into the broader **Need for Speed: The Run Definitive Edition** patch without carrying over the original Ultimate Unlocker's global progression cheats.
