//
// Need for Speed The Run - Selective Promo/DLC Unlocker
//
// Based on NFSTR_UltimateUnlocker / "Unlock All Things" by Xan / Tenjoin.
// This fork intentionally keeps normal progression intact: it does NOT force cars
// unlocked, unlock stage select, or disable the generic Unlockers requirement list.
//
// The only active patches are the two original metadata patches that neutralize
// IsPromoContent and IsHiddenUnlock. This is the smallest useful test for exposing
// installed promotional / hidden DLC content without emulating a Time Saver pack.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>

#include "includes/injector/injector.hpp"

namespace
{
    constexpr uintptr_t kPreferredImageBase = 0x00400000;

    // String addresses in the supported DRM-free v1.1.0.0 executable.
    // Xan's original code zeroed the final character of these names:
    //   IsPromoContent  -> IsPromoConten
    //   IsHiddenUnlock  -> IsHiddenUnloc
    constexpr uintptr_t kIsPromoContentVA = 0x025A3620;
    constexpr uintptr_t kIsHiddenUnlockVA = 0x025A3630;

    uintptr_t RebaseGameAddress(uintptr_t preferredVA)
    {
        const uintptr_t gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        return gameBase + (preferredVA - kPreferredImageBase);
    }

    bool IsReadableRange(const void* address, size_t size)
    {
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

    bool NeutralizeReflectedBoolField(uintptr_t preferredStringVA, const char* expectedName)
    {
        const size_t length = std::strlen(expectedName);
        char* const liveString = reinterpret_cast<char*>(RebaseGameAddress(preferredStringVA));

        // Refuse to patch an unexpected executable. The original mod wrote to fixed
        // addresses unconditionally; this fork verifies the full field name first.
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

    void Init()
    {
        // Keep only the two targeted promo/hidden-content patches from the original
        // Ultimate Unlocker. Normal car/stage/progression checks are left untouched.
        const bool promoPatched = NeutralizeReflectedBoolField(
            kIsPromoContentVA,
            "IsPromoContent");

        const bool hiddenPatched = NeutralizeReflectedBoolField(
            kIsHiddenUnlockVA,
            "IsHiddenUnlock");

        if (promoPatched && hiddenPatched)
        {
            OutputDebugStringA(
                "[NFSTR_SelectiveUnlocker] Promo/hidden content metadata patches applied.\n");
        }
        else
        {
            OutputDebugStringA(
                "[NFSTR_SelectiveUnlocker] Patch verification failed; unsupported or modified executable.\n");
        }
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
