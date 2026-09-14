//
// Need for Speed: The Run - Selective Promo/DLC Unlocker
//
// Production implementation derived from the validated test 8 behavior.
//
// Purpose:
//   * expose installed hidden/promo content;
//   * satisfy only known discontinued DLC/promo ownership entitlements;
//   * preserve the game's normal progression and reward requirements.
//
// Deliberately NOT bypassed:
//   * GaragePurchaseUnlocker / ordinary car progression;
//   * driver level, stage, boss, challenge medal, multiplayer objective,
//     Autolog and other progression unlockers;
//   * Time Savers, profile/XP grants, VIP/demo flags and online-pass offers;
//   * generic Unlockers[] handling;
//   * stage-select, blanket car-unlock or Ebisu/Autolog patches.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
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

    // Validated discontinued content-ownership offers only.
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

    bool CopyReadableCString(const char* src, char* dst, size_t dstSize)
    {
        if (!dst || dstSize == 0)
            return false;

        dst[0] = '\0';
        if (!src)
            return false;

        for (size_t i = 0; i + 1 < dstSize; ++i)
        {
            if (!IsReadableRange(src + i, 1))
            {
                dst[0] = '\0';
                return false;
            }

            const char c = src[i];
            dst[i] = c;
            if (c == '\0')
                return true;
        }

        dst[dstSize - 1] = '\0';
        return false;
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

    bool PatchVisibilityMetadata()
    {
        static constexpr char kPromoName[] = "IsPromoContent";
        static constexpr char kHiddenName[] = "IsHiddenUnlock";

        char* const promo = reinterpret_cast<char*>(RebaseGameAddress(kIsPromoContentVA));
        char* const hidden = reinterpret_cast<char*>(RebaseGameAddress(kIsHiddenUnlockVA));

        if (!IsReadableRange(promo, sizeof(kPromoName)) ||
            !IsReadableRange(hidden, sizeof(kHiddenName)))
            return false;

        if (std::memcmp(promo, kPromoName, sizeof(kPromoName)) != 0 ||
            std::memcmp(hidden, kHiddenName, sizeof(kHiddenName)) != 0)
            return false;

        injector::WriteMemory<uint8_t>(
            reinterpret_cast<uintptr_t>(promo + sizeof(kPromoName) - 2),
            0,
            true);

        injector::WriteMemory<uint8_t>(
            reinterpret_cast<uintptr_t>(hidden + sizeof(kHiddenName) - 2),
            0,
            true);

        return true;
    }

    bool GrantThroughStockSuccessPath(OnlineUnlocker* self)
    {
        void** const rootManagerAddress =
            reinterpret_cast<void**>(RebaseGameAddress(kUnlockManagerGlobalVA));

        if (!IsReadableRange(rootManagerAddress, sizeof(void*)))
            return false;

        void* const rootManager = *rootManagerAddress;
        if (!rootManager)
            return false;

        void* const unlockManager = reinterpret_cast<uint8_t*>(rootManager) + kUnlockManagerOffset;
        const auto grant = reinterpret_cast<GrantOnlineUnlockFn>(
            RebaseGameAddress(kGrantOnlineUnlockVA));

        grant(unlockManager, self);
        return true;
    }

    void __fastcall OnlineUnlockerEntitlementHook(OnlineUnlocker* self, void* /*edx*/)
    {
        if (!self)
            return;

        const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(self);
        const uintptr_t onlineVtable = RebaseGameAddress(kOnlineUnlockerVtableVA);

        // Only the exact OnlineUnlocker subtype is eligible. Derived types such as
        // GaragePurchaseUnlocker must continue through the original game logic.
        if (vtable == onlineVtable)
        {
            char offer[128]{};
            if (CopyReadableCString(self->offerId, offer, sizeof(offer)) &&
                IsWhitelistedOffer(offer) &&
                GrantThroughStockSuccessPath(self))
            {
                return;
            }
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
        const intptr_t jumpFrom = reinterpret_cast<intptr_t>(
            trampoline + kOnlineUnlockerPatchSize + 5);
        const intptr_t jumpTo = static_cast<intptr_t>(
            liveMethod + kOnlineUnlockerPatchSize);
        const int32_t relative = static_cast<int32_t>(jumpTo - jumpFrom);

        std::memcpy(trampoline + kOnlineUnlockerPatchSize + 1,
                    &relative,
                    sizeof(relative));

        FlushInstructionCache(GetCurrentProcess(), trampoline, trampolineSize);
        g_OriginalOnlineUnlockerMethod =
            reinterpret_cast<OnlineUnlockerMethodFn>(trampoline);
        return true;
    }

    bool InstallOnlineUnlockerHook()
    {
        const uintptr_t liveMethod = RebaseGameAddress(kOnlineUnlockerMethodVA);

        if (!IsReadableRange(reinterpret_cast<const void*>(liveMethod),
                             kOnlineUnlockerPatchSize))
            return false;

        if (std::memcmp(reinterpret_cast<const void*>(liveMethod),
                        kOnlineUnlockerExpected,
                        kOnlineUnlockerPatchSize) != 0)
            return false;

        if (!BuildOriginalMethodTrampoline(liveMethod))
            return false;

        injector::MakeJMP(liveMethod,
                          reinterpret_cast<uintptr_t>(&OnlineUnlockerEntitlementHook),
                          true);

        for (size_t i = 5; i < kOnlineUnlockerPatchSize; ++i)
            injector::WriteMemory<uint8_t>(liveMethod + i, 0x90, true);

        return true;
    }

    void Init()
    {
        const bool visibilityPatched = PatchVisibilityMetadata();
        const bool entitlementHooked = InstallOnlineUnlockerHook();

        if (visibilityPatched && entitlementHooked)
        {
            OutputDebugStringA(
                "[NFSTR_SelectiveUnlocker] Selective DLC/promo unlocker installed.\n");
        }
        else
        {
            OutputDebugStringA(
                "[NFSTR_SelectiveUnlocker] Compatibility check failed; one or more patches were not installed.\n");
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        Init();
    }

    return TRUE;
}
