//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 7)
//
// Test 6 proved that OnlineUnlocker is the live entitlement path, but it also
// proved that forcing every OnlineUnlocker-derived object is too broad.  The
// diagnostic build granted Time Savers and every GaragePurchaseUnlocker, which
// unlocked normal level/career reward cars as collateral.
//
// Test 7 keeps the same entitlement hook but adds a strict allow-list.  Only
// known discontinued DLC / preorder / advertising-promo ownership offers are
// forced through the game's stock entitlement-success path.  Everything else,
// including GaragePurchaseUnlocker, Time Savers, XP/profile bonuses, VIP/demo
// flags and ordinary online-pass checks, executes the original game method.
//
// Normal LevelUp / StageCompletion / ChallengeCompletion / career unlockers are
// untouched.  The IsPromoContent / IsHiddenUnlock reflection-name patches are
// retained only to expose hidden promo/DLC UI entries.
//
// NO generic Unlockers bypass. NO blanket car unlock. NO stage-select unlock.
// NO online/Ebisu zeroing.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "includes/injector/injector.hpp"

namespace
{
    constexpr uintptr_t kPreferredImageBase = 0x00400000;

    constexpr uintptr_t kIsPromoContentVA = 0x025A3620;
    constexpr uintptr_t kIsHiddenUnlockVA = 0x025A3630;

    constexpr uintptr_t kOnlineUnlockerMethodVA = 0x008D0BA0;
    constexpr size_t kOnlineUnlockerPatchSize = 8;

    constexpr uintptr_t kUnlockManagerGlobalVA = 0x02882500;
    constexpr uintptr_t kUnlockManagerOffset = 0x00003CB4;
    constexpr uintptr_t kGrantOnlineUnlockVA = 0x007F3780;

    constexpr uintptr_t kOnlineUnlockerVtableVA = 0x024791FC;
    constexpr uintptr_t kGaragePurchaseUnlockerVtableVA = 0x0247920C;

    const uint8_t kOnlineUnlockerExpected[kOnlineUnlockerPatchSize] = {
        0x51,                   // push ecx
        0x55,                   // push ebp
        0x8B, 0xE9,             // mov ebp,ecx
        0x8B, 0x55, 0x20,       // mov edx,[ebp+20h] (OfferId)
        0x56                    // push esi
    };

    struct OnlineUnlocker
    {
        uint8_t pad[0x20];
        const char* offerId;    // +0x20
        const char* ps3Sku;     // +0x24
        const char* xenonSku;   // +0x28
        const char* pcSku;      // +0x2C
    };

    using OnlineUnlockerMethodFn = void (__thiscall *)(OnlineUnlocker* self);
    using GrantOnlineUnlockFn = void (__thiscall *)(void* unlockManager, OnlineUnlocker* unlocker);

    OnlineUnlockerMethodFn g_OriginalOnlineUnlockerMethod = nullptr;
    volatile LONG g_ObservationCount = 0;

    // Test-6 runtime discovery gave us the exact OfferIds.  Keep this list
    // intentionally narrow: content ownership only, not progression shortcuts.
    const char* const kWhitelistedOffers[] = {
        // Heroes & Villains / preorder Challenge Series families.
        "r_carbon",
        "r_mostwanted",
        "r_underground",
        "handv_pack",

        // Discontinued content pack.
        "supercar_pack",

        // Dr Pepper vehicle promos.  dp_profile and dp_xp are deliberately NOT
        // included because they affect profile/progression rather than cars.
        "dp_fordgt",
        "dp_chevrolet",
        "dp_porsche",

        // Old Spice and AEM promotional content.
        "os_pack",
        "aem_adsales",

        // Limited-Edition-associated free online-pass entitlement.  This is kept
        // separate from the generic/free/purchased online-pass offers because it
        // is the only discovered entitlement whose name is explicitly LE-linked.
        // Test 7 will tell us whether it is actually needed for LE content.
        "olp_le_free",
    };

    uintptr_t RebaseGameAddress(uintptr_t preferredVA)
    {
        const uintptr_t gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        return gameBase + (preferredVA - kPreferredImageBase);
    }

    bool IsReadableRange(const void* address, size_t size)
    {
        if (!address || size == 0)
            return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(address, &mbi, sizeof(mbi)))
            return false;

        if (mbi.State != MEM_COMMIT)
            return false;

        if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS))
            return false;

        const uintptr_t begin = reinterpret_cast<uintptr_t>(address);
        const uintptr_t end = begin + size;
        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return end >= begin && end <= regionEnd;
    }

    void AppendLog(const char* fmt, ...)
    {
        char buffer[1024]{};
        va_list args;
        va_start(args, fmt);
        _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
        va_end(args);

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test7.log",
                               FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return;

        DWORD written = 0;
        WriteFile(h, buffer, static_cast<DWORD>(std::strlen(buffer)), &written, nullptr);
        CloseHandle(h);
    }

    void CopyReadableCString(const char* src, char* dst, size_t dstSize)
    {
        if (!dst || dstSize == 0)
            return;

        dst[0] = '\0';
        if (!src)
        {
            strcpy_s(dst, dstSize, "<null>");
            return;
        }

        size_t i = 0;
        for (; i + 1 < dstSize; ++i)
        {
            if (!IsReadableRange(src + i, 1))
            {
                if (i == 0)
                    strcpy_s(dst, dstSize, "<unreadable>");
                return;
            }

            const char c = src[i];
            dst[i] = c;
            if (c == '\0')
                return;
        }

        dst[dstSize - 1] = '\0';
    }

    bool NeutralizeReflectedBoolField(uintptr_t preferredStringVA, const char* expectedName)
    {
        const size_t length = std::strlen(expectedName);
        char* const liveString = reinterpret_cast<char*>(RebaseGameAddress(preferredStringVA));

        if (!IsReadableRange(liveString, length + 1))
            return false;

        if (std::memcmp(liveString, expectedName, length + 1) != 0)
            return false;

        injector::WriteMemory<uint8_t>(
            reinterpret_cast<uintptr_t>(liveString + length - 1),
            0,
            true);

        return true;
    }

    bool IsWhitelistedOffer(const char* offer)
    {
        if (!offer || !offer[0])
            return false;

        for (const char* allowed : kWhitelistedOffers)
        {
            if (std::strcmp(offer, allowed) == 0)
                return true;
        }

        return false;
    }

    bool GrantThroughStockSuccessPath(OnlineUnlocker* self)
    {
        void* const rootManager = *reinterpret_cast<void**>(RebaseGameAddress(kUnlockManagerGlobalVA));
        if (!rootManager)
            return false;

        void* const unlockManager = reinterpret_cast<uint8_t*>(rootManager) + kUnlockManagerOffset;
        const auto grant = reinterpret_cast<GrantOnlineUnlockFn>(RebaseGameAddress(kGrantOnlineUnlockVA));
        grant(unlockManager, self);
        return true;
    }

    void __fastcall OnlineUnlockerEntitlementHook(OnlineUnlocker* self, void* /*edx*/)
    {
        if (!self)
            return;

        const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(self);
        const uintptr_t onlineVtable = RebaseGameAddress(kOnlineUnlockerVtableVA);
        const uintptr_t garageVtable = RebaseGameAddress(kGaragePurchaseUnlockerVtableVA);

        const char* kind = "OnlineUnlocker-derived";
        if (vtable == onlineVtable)
            kind = "OnlineUnlocker";
        else if (vtable == garageVtable)
            kind = "GaragePurchaseUnlocker";

        char offer[192]{};
        char pcSku[192]{};
        char xenonSku[192]{};
        char ps3Sku[192]{};
        CopyReadableCString(self->offerId, offer, sizeof(offer));
        CopyReadableCString(self->pcSku, pcSku, sizeof(pcSku));
        CopyReadableCString(self->xenonSku, xenonSku, sizeof(xenonSku));
        CopyReadableCString(self->ps3Sku, ps3Sku, sizeof(ps3Sku));

        const bool exactOnlineUnlocker = (vtable == onlineVtable);
        const bool whitelisted = exactOnlineUnlocker && IsWhitelistedOffer(offer);

        const LONG observation = InterlockedIncrement(&g_ObservationCount);
        if (observation <= 500)
        {
            AppendLog("entitlement #%ld self=%08X kind=%s offer=\"%s\" pcSku=\"%s\" xenonSku=\"%s\" ps3Sku=\"%s\"\r\n",
                      observation,
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(self)),
                      kind,
                      offer,
                      pcSku,
                      xenonSku,
                      ps3Sku);
        }

        if (whitelisted)
        {
            if (GrantThroughStockSuccessPath(self))
            {
                if (observation <= 500)
                    AppendLog("  -> GRANTED: whitelisted DLC/promo entitlement\r\n");
                return;
            }

            if (observation <= 500)
                AppendLog("  -> grant manager unavailable; falling back to original method\r\n");
        }
        else if (observation <= 500)
        {
            if (vtable == garageVtable)
                AppendLog("  -> ORIGINAL: GaragePurchaseUnlocker excluded\r\n");
            else if (std::strcmp(offer, "timesavers_pack") == 0)
                AppendLog("  -> ORIGINAL: Time Savers explicitly excluded\r\n");
            else
                AppendLog("  -> ORIGINAL: entitlement not on DLC/promo allow-list\r\n");
        }

        if (g_OriginalOnlineUnlockerMethod)
            g_OriginalOnlineUnlockerMethod(self);
    }

    bool BuildOriginalMethodTrampoline(uintptr_t liveMethod)
    {
        constexpr size_t trampolineSize = kOnlineUnlockerPatchSize + 5;
        uint8_t* const trampoline = reinterpret_cast<uint8_t*>(
            VirtualAlloc(nullptr,
                         trampolineSize,
                         MEM_COMMIT | MEM_RESERVE,
                         PAGE_EXECUTE_READWRITE));

        if (!trampoline)
            return false;

        std::memcpy(trampoline,
                    reinterpret_cast<const void*>(liveMethod),
                    kOnlineUnlockerPatchSize);

        trampoline[kOnlineUnlockerPatchSize] = 0xE9;
        const intptr_t jumpFrom = reinterpret_cast<intptr_t>(trampoline + kOnlineUnlockerPatchSize + 5);
        const intptr_t jumpTo = static_cast<intptr_t>(liveMethod + kOnlineUnlockerPatchSize);
        const int32_t relative = static_cast<int32_t>(jumpTo - jumpFrom);
        std::memcpy(trampoline + kOnlineUnlockerPatchSize + 1,
                    &relative,
                    sizeof(relative));

        FlushInstructionCache(GetCurrentProcess(), trampoline, trampolineSize);
        g_OriginalOnlineUnlockerMethod = reinterpret_cast<OnlineUnlockerMethodFn>(trampoline);
        return true;
    }

    bool InstallOnlineUnlockerHook()
    {
        const uintptr_t liveMethod = RebaseGameAddress(kOnlineUnlockerMethodVA);
        if (!IsReadableRange(reinterpret_cast<const void*>(liveMethod), kOnlineUnlockerPatchSize))
        {
            AppendLog("OnlineUnlocker hook FAILED: method unreadable\r\n");
            return false;
        }

        if (std::memcmp(reinterpret_cast<const void*>(liveMethod),
                        kOnlineUnlockerExpected,
                        kOnlineUnlockerPatchSize) != 0)
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(liveMethod);
            AppendLog("OnlineUnlocker hook FAILED: unexpected bytes %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                      b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            return false;
        }

        if (!BuildOriginalMethodTrampoline(liveMethod))
        {
            AppendLog("OnlineUnlocker hook FAILED: could not build original-method trampoline\r\n");
            return false;
        }

        injector::MakeJMP(liveMethod,
                          reinterpret_cast<uintptr_t>(&OnlineUnlockerEntitlementHook),
                          true);

        // The trampoline copied eight complete bytes.  The entry jump consumes
        // five; NOP the remaining three so no stale partial prologue remains.
        for (size_t i = 5; i < kOnlineUnlockerPatchSize; ++i)
            injector::WriteMemory<uint8_t>(liveMethod + i, 0x90, true);

        AppendLog("OnlineUnlocker selective entitlement hook installed at %08X\r\n",
                  static_cast<unsigned>(kOnlineUnlockerMethodVA));
        return true;
    }

    void Init()
    {
        DeleteFileA("NFSTR_SelectiveUnlocker_test7.log");
        AppendLog("NFSTR Selective Unlocker test 7\r\n");
        AppendLog("Visibility: neutralise IsPromoContent + IsHiddenUnlock reflection names\r\n");
        AppendLog("Entitlements: grant only allow-listed DLC/promo OnlineUnlockers\r\n");
        AppendLog("GaragePurchaseUnlocker + Time Savers + XP/profile + VIP/demo/online-pass controls pass through original logic.\r\n");
        AppendLog("No generic Unlockers bypass; no blanket car/stage unlock; no Ebisu patches.\r\n\r\n");

        const bool promoPatched = NeutralizeReflectedBoolField(
            kIsPromoContentVA,
            "IsPromoContent");
        const bool hiddenPatched = NeutralizeReflectedBoolField(
            kIsHiddenUnlockVA,
            "IsHiddenUnlock");

        AppendLog("visibility IsPromoContent=%s IsHiddenUnlock=%s\r\n",
                  promoPatched ? "patched" : "FAILED",
                  hiddenPatched ? "patched" : "FAILED");

        AppendLog("allow-list:");
        for (const char* offer : kWhitelistedOffers)
            AppendLog(" %s", offer);
        AppendLog("\r\n");

        const bool entitlementHooked = InstallOnlineUnlockerHook();
        AppendLog("selective entitlement hook=%s\r\n",
                  entitlementHooked ? "installed" : "FAILED");

        OutputDebugStringA(
            "[NFSTR_SelectiveUnlocker_test7] Whitelisted DLC/promo entitlement bypass installed.\n");
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
