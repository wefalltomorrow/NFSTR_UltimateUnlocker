//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 8)
//
// Test 7 visually achieved the intended selective behavior: DLC/promo Challenge
// Series and ownership-gated cars became available while normal level, boss,
// challenge-medal, multiplayer-objective and Autolog requirements remained
// locked.  Test 8 tightens the allow-list one step further by removing the
// suspicious olp_le_free entitlement.  The "olp" family is an online-pass
// control, not content ownership, so it should follow the original game logic.
//
// Only exact OnlineUnlocker objects with known discontinued content OfferIds are
// granted.  GaragePurchaseUnlocker and all other OnlineUnlocker offers execute
// the original game method through a trampoline.
//
// NO generic Unlockers bypass. NO blanket car unlock. NO stage-select unlock.
// NO Time Savers. NO online/Ebisu zeroing.
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
        0x51,
        0x55,
        0x8B, 0xE9,
        0x8B, 0x55, 0x20,
        0x56
    };

    struct OnlineUnlocker
    {
        uint8_t pad[0x20];
        const char* offerId;
        const char* ps3Sku;
        const char* xenonSku;
        const char* pcSku;
    };

    using OnlineUnlockerMethodFn = void (__thiscall *)(OnlineUnlocker* self);
    using GrantOnlineUnlockFn = void (__thiscall *)(void* unlockManager, OnlineUnlocker* unlocker);

    OnlineUnlockerMethodFn g_OriginalOnlineUnlockerMethod = nullptr;
    volatile LONG g_ObservationCount = 0;

    // Runtime-discovered content ownership offers only.
    const char* const kWhitelistedOffers[] = {
        "r_carbon",
        "r_mostwanted",
        "r_underground",
        "handv_pack",
        "supercar_pack",
        "dp_fordgt",
        "dp_chevrolet",
        "dp_porsche",
        "os_pack",
        "aem_adsales",
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

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test8.log",
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
            else if (std::strncmp(offer, "olp_", 4) == 0)
                AppendLog("  -> ORIGINAL: online-pass entitlement excluded\r\n");
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

        for (size_t i = 5; i < kOnlineUnlockerPatchSize; ++i)
            injector::WriteMemory<uint8_t>(liveMethod + i, 0x90, true);

        AppendLog("OnlineUnlocker selective entitlement hook installed at %08X\r\n",
                  static_cast<unsigned>(kOnlineUnlockerMethodVA));
        return true;
    }

    void Init()
    {
        DeleteFileA("NFSTR_SelectiveUnlocker_test8.log");
        AppendLog("NFSTR Selective Unlocker test 8\r\n");
        AppendLog("Visibility: neutralise IsPromoContent + IsHiddenUnlock reflection names\r\n");
        AppendLog("Entitlements: grant only allow-listed discontinued content OnlineUnlockers\r\n");
        AppendLog("GaragePurchaseUnlocker, Time Savers, XP/profile, VIP/demo and all olp_* controls use original logic.\r\n");
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
            "[NFSTR_SelectiveUnlocker_test8] Whitelisted DLC/promo entitlement bypass installed.\n");
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
