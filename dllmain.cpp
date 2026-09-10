//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 3)
//
// Test 3 combines the two useful findings from the earlier experiments:
//   1) neutralising IsPromoContent / IsHiddenUnlock exposes hidden promo/DLC UI,
//   2) garage cars are only forced unlocked when their own m_isPromoContent flag is true.
//
// Normal progression remains intentionally untouched. This build does NOT bypass
// the generic Unlockers requirement list, unlock stage select, force every car
// unlocked, or apply any online/Ebisu patches.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "includes/injector/injector.hpp"
#include "includes/patterns.hpp"

namespace
{
    constexpr uintptr_t kPreferredImageBase = 0x00400000;

    // Reflected field-name strings in the supported DRM-free v1.1.0.0 EXE.
    // Xan's original unlocker null-terminated the final character of each name.
    constexpr uintptr_t kIsPromoContentVA = 0x025A3620;
    constexpr uintptr_t kIsHiddenUnlockVA = 0x025A3630;

    struct Unlockable
    {
        uint8_t pad[0x18];
        bool m_isUnlocked;
        bool m_hideHowTo;
        bool m_isHiddenUnlock;
        bool m_isPromoContent;
    };

    using GetMatchingGarageCarFn = Unlockable* (__cdecl *)(uint32_t attribSysClassKey,
                                                            uint32_t attribSysCollectionKey);

    GetMatchingGarageCarFn g_GetMatchingGarageCar = nullptr;

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

    void AppendLog(const char* fmt, ...)
    {
        char buffer[512]{};
        va_list args;
        va_start(args, fmt);
        _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
        va_end(args);

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test3.log",
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

    Unlockable* __cdecl GetMatchingGarageCarHook(uint32_t attribSysClassKey,
                                                  uint32_t attribSysCollectionKey)
    {
        Unlockable* car = g_GetMatchingGarageCar
            ? g_GetMatchingGarageCar(attribSysClassKey, attribSysCollectionKey)
            : nullptr;

        if (!car)
        {
            AppendLog("class=%08X collection=%08X result=null\r\n",
                      attribSysClassKey,
                      attribSysCollectionKey);
            return nullptr;
        }

        const bool wasUnlocked = car->m_isUnlocked;
        const bool hideHowTo = car->m_hideHowTo;
        const bool hidden = car->m_isHiddenUnlock;
        const bool promo = car->m_isPromoContent;

        // Selective vehicle-side experiment: promo cars only. Do not use
        // m_isHiddenUnlock as an unlock condition because normal progression rewards
        // may also be hidden until earned.
        if (!car->m_isUnlocked && promo)
            car->m_isUnlocked = true;

        AppendLog("class=%08X collection=%08X unlocked:%u->%u hideHowTo=%u hidden=%u promo=%u\r\n",
                  attribSysClassKey,
                  attribSysCollectionKey,
                  wasUnlocked ? 1u : 0u,
                  car->m_isUnlocked ? 1u : 0u,
                  hideHowTo ? 1u : 0u,
                  hidden ? 1u : 0u,
                  promo ? 1u : 0u);

        return car;
    }

    void Init()
    {
        DeleteFileA("NFSTR_SelectiveUnlocker_test3.log");
        AppendLog("NFSTR Selective Unlocker test 3\r\n");
        AppendLog("Visibility: neutralise IsPromoContent + IsHiddenUnlock reflection names\r\n");
        AppendLog("Cars: unlock only when m_isPromoContent == true\r\n");
        AppendLog("No generic Unlockers bypass; no stage unlock; no online/Ebisu patches.\r\n\r\n");

        // Restore the visibility behaviour proven by Test 1. These patches expose
        // promo/hidden content but do not themselves satisfy entitlement checks.
        const bool promoPatched = NeutralizeReflectedBoolField(
            kIsPromoContentVA,
            "IsPromoContent");

        const bool hiddenPatched = NeutralizeReflectedBoolField(
            kIsHiddenUnlockVA,
            "IsHiddenUnlock");

        AppendLog("visibility IsPromoContent=%s IsHiddenUnlock=%s\r\n",
                  promoPatched ? "patched" : "FAILED",
                  hiddenPatched ? "patched" : "FAILED");

        pattern::Win32::Init();
        if (!pattern::Win32::bIsInited())
        {
            AppendLog("ERROR: pattern scanner failed to initialise.\r\n");
            return;
        }

        // Same NFSUIVehicleComp::getMatchingGarageCar caller used by FusionFix.
        const uintptr_t callSite = pattern::get_first(
            "E8 ? ? ? ? 83 C4 ? 80 7C 24 ? ? 74 ? 80 78");

        if (!callSite)
        {
            AppendLog("ERROR: getMatchingGarageCar call pattern not found.\r\n");
            return;
        }

        g_GetMatchingGarageCar = reinterpret_cast<GetMatchingGarageCarFn>(
            static_cast<uintptr_t>(injector::GetBranchDestination(callSite)));

        if (!g_GetMatchingGarageCar)
        {
            AppendLog("ERROR: could not resolve original getMatchingGarageCar target.\r\n");
            return;
        }

        injector::MakeCALL(callSite, GetMatchingGarageCarHook, true);

        AppendLog("garage hook installed at %08X; original target %08X\r\n",
                  static_cast<unsigned>(callSite),
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_GetMatchingGarageCar)));

        if (promoPatched && hiddenPatched)
        {
            OutputDebugStringA(
                "[NFSTR_SelectiveUnlocker_test3] Promo/DLC visibility + selective promo-car hook applied.\n");
        }
        else
        {
            OutputDebugStringA(
                "[NFSTR_SelectiveUnlocker_test3] Visibility patch verification failed; check supported EXE.\n");
        }
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
