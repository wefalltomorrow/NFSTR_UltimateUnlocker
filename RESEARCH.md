# NFSTR Ultimate Unlocker patch map / selective-unlock research

This document records what the original `xan1242/NFSTR_UltimateUnlocker` actually patches so the fork can keep useful DLC/promo behaviour without inheriting broad progression cheats.

## Active patches in the original plugin

### 1. `0x00834303` and `0x0083434F` - two NOPs

Original code:

```cpp
injector::MakeNOP(0x834303, 2);
injector::MakeNOP(0x83434F, 2);
```

These are **not documented by Xan**. Previous disassembly work places both in a path that ultimately participates in `OnNetworkConnected` handling, so they look more like legacy online/offline compatibility than ordinary car/stage unlocking.

They are **not enabled in the selective branch**. They should be treated as a separate online research target rather than silently bundled with DLC exposure.

---

### 2. Reflected field-name patches

Original code:

```cpp
injector::WriteMemory<uint8_t>(0x25A35EC, 0, true);
injector::WriteMemory<uint8_t>(0x25A362D, 0, true);
injector::WriteMemory<uint8_t>(0x25A363D, 0, true);
```

They terminate the final character of three reflected field names:

| Address | Original field | Result after patch | Selective branch |
|---|---|---|---|
| `0x025A35EC` | `Unlockers` | `Unlocker` | **Removed** |
| `0x025A362D` | `IsPromoContent` | `IsPromoConten` | **Kept** |
| `0x025A363D` | `IsHiddenUnlock` | `IsHiddenUnloc` | **Kept** |

The `Unlockers` change is a broad unlock-requirement bypass, not a Time Saver-specific patch. This is independently corroborated by NFS The Run FusionFix: its `UnlockEverything` option specifically intercepts the reflected property named `Unlockers` and substitutes an undefined property name.

FusionFix also exposes the relevant unlockable layout:

```cpp
bool m_isUnlocked;
bool m_hideHowTo;
bool m_isHiddenUnlock;
bool m_isPromoContent;
```

That makes `IsPromoContent` and `IsHiddenUnlock` the two best candidates for a narrow promo/hidden-content experiment without touching `m_isUnlocked` or generic unlock requirements.

The exact effect still needs in-game verification. Neutralising those fields may expose installed promo/DLC content, but the fork should not claim it grants every legitimate DLC until tested.

---

### 3. Car unlock hooks

Original hooks:

```text
0x0093E00D  CarUnlockHook1
0x0093F214  CarUnlockHook2
```

Both write `1` to the unlockable object's byte at offset `+0x18`, then bypass the normal locked path.

These are the clearest **unlock-all-cars** cheats in the plugin and are completely removed from the selective branch.

FusionFix uses the same conceptual mechanism for its `UnlockAllCars` option: it forces `m_isUnlocked = true` on the matching garage car.

---

### 4. Stage-select unlocks

Original code:

```cpp
injector::MakeNOP(0x930C00, 2);
injector::MakeNOP(0x9313A2, 2);
```

Xan explicitly labels these `stage select unlock`.

They are completely removed from the selective branch so Story / stage / Challenge Series progression is not bypassed.

FusionFix separately has an `UnlockChallenges` system that forces UI `isLocked` / `IsUnlocked` values, reinforcing that challenge/stage unlocking is a distinct patch family from promo/hidden content.

---

## Commented / inactive code in the original repository

None of the following is active in the published Ultimate Unlocker binary/source as currently committed.

### `0x00DD76CB` network NOP

```cpp
// injector::MakeNOP(0xDD76CB, 2); // something with network?
```

Xan's own comment is uncertain. Do not enable this until the surrounding function and branch condition are identified.

### Ebisu / Autolog return-zero stubs

The source contains a commented block that would replace these functions with a function returning `0`:

```text
0x018AEF80
0x018AF060
0x018AF0E0
0x018AF160
0x018AF1E0
0x018AF260
0x018AF2D0
0x018AF340
0x018AF3A0
0x018AF420
0x018AF460
0x018AF4A0
0x018AF520
0x018AF5B0
0x018AF640
```

The block is labelled `Ebisu / Autolog stuff`, but there are no function names, signatures or behavioural notes. Returning zero from all of them wholesale would be an aggressive compatibility hack, so the selective branch deliberately leaves them alone.

For an offline-only player, the safer current direction is the structured `NfsOnlineSettings` work in the separate `NFS-TheRun-DE` research branch: disable explicit Autolog/telemetry/matchmaking/VOIP/fallback/upload settings while leaving Frostbite core networking untouched. That is more targeted than blindly replacing fifteen unknown functions with `return 0`.

The Ebisu addresses are still worth reverse-engineering individually. Useful candidates would be routines that only perform dead service connection, telemetry or Autolog requests; anything involved in profile, event flow or local UI should not be bypassed.

---

## Other Xan NFSTR repository checked

`xan1242/NFSTR_Loadless` is **not a faster-loading patch**. It hooks loading/start/continue/stage events and exposes boolean/counter state for speedrunning / LiveSplit loadless timing. It is useful tooling for speedrunners and possibly for detecting loading state in future mods, but it does not itself shorten game load times.

A potentially useful idea from it is the **loading-state detector**: if another feature needs to act only during genuine loading, those hooks/patterns may provide a better signal than guessing from FPS or menu state.

---

## Current selective branch policy

The branch `selective-promo-dlc` currently applies only:

```text
IsPromoContent  -> neutralised
IsHiddenUnlock  -> neutralised
```

It deliberately does **not** apply:

```text
Unlockers bypass
CarUnlockHook1
CarUnlockHook2
stage select unlock NOPs
0x834303 / 0x83434F online NOPs
0xDD76CB network NOP
Ebisu / Autolog return-zero stubs
```

This gives us the cleanest possible A/B test for the user's goal: installed promotional/hidden DLC content should become available if those two flags are the relevant gate, while normal cars/stages/challenges should continue following ordinary progression.

## Next research steps

1. Build and test the two-field selective plugin.
2. Confirm which promo/DLC items appear and confirm ordinary progression remains locked.
3. If some legitimate DLC is still missing, identify its specific gating field/event rather than restoring generic `Unlockers` or car/stage bypasses.
4. Reverse-engineer `0x834303`, `0x83434F` and `0xDD76CB` as a separate online/offline compatibility task.
5. Map the fifteen Ebisu/Autolog functions one at a time before deciding whether any belong in the Definitive Edition offline mode.
6. Consider reusing `NFSTR_Loadless`'s loading-state hooks only where a reliable loading-only signal is genuinely useful.
