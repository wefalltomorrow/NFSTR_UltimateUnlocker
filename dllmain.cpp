//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 4)
//
// Test 3 proved the promo/hidden reflection patches expose DLC/promo content,
// but its diagnostic garage hook was installed at only one caller. The Test 3
// log showed that caller never ran while browsing View Cars.
//
// Test 4 keeps the proven visibility behaviour and redirects every known
// EXTERNAL direct caller of NFSUIVehicleComp::getMatchingGarageCar to the same
// selective wrapper. This lets View Cars and the other garage paths report the
// real Unlockable flags instead of observing only one unrelated call site.
//
// Normal progression remains intentionally untouched. There is still NO generic
// Unlockers bypass, NO stage-select unlock, NO blanket car unlock, and NO
// online/Ebisu patching.
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
    constexpr uintptr_t kIsPromoContentVA = 0x025A3620;
    constexpr uintptr_t kIsHiddenUnlockVA = 0x025A3630;

    // Direct calls to getMatchingGarageCar in the supported DRM-free v1.1 EXE.
    // 0x00932437 is deliberately excluded because it is inside/adjacent to the
    // target routine and redirecting it could recurse through our wrapper.
    constexpr uintptr_t kGarageCallSites[] = {
        0x008848ED,
        0x00885E34,
        0x00894A13,
        0x0093C4DC,
        0x0093C659,
        0x0093C805,
        0x0093D17F,
        0x0093D612,
        0x0093D714,
        0x0093F199,
    };

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

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test4.log",
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

        // Keep this deliberately narrow. If the visibility reflection patch has
        // caused m_isPromoContent to stop being populated, the log will prove it;
        // do NOT compensate by broadly unlocking hidden/progression rewards.
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
        DeleteFileA("NFSTR_SelectiveUnlocker_test4.log");
        AppendLog("NFSTR Selective Unlocker test 4\r\n");
        AppendLog("Visibility: neutralise IsPromoContent + IsHiddenUnlock reflection names\r\n");
        AppendLog("Cars: hook all known external getMatchingGarageCar callers; unlock promo=true only\r\n");
        AppendLog("No generic Unlockers bypass; no stage unlock; no online/Ebisu patches.\r\n\r\n");

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

        // Resolve the original target from the same signature used by FusionFix.
        const uintptr_t anchorCall = pattern::get_first(
            "E8 ? ? ? ? 83 C4 ? 80 7C 24 ? ? 74 ? 80 78");
        if (!anchorCall)
        {
            AppendLog("ERROR: getMatchingGarageCar anchor pattern not found.\r\n");
            return;
        }

        const uintptr_t originalTarget =
            static_cast<uintptr_t>(injector::GetBranchDestination(anchorCall));
        g_GetMatchingGarageCar = reinterpret_cast<GetMatchingGarageCarFn>(originalTarget);

        if (!g_GetMatchingGarageCar)
        {
            AppendLog("ERROR: could not resolve original getMatchingGarageCar target.\r\n");
            return;
        }

        unsigned patchedCalls = 0;
        for (const uintptr_t preferredCallVA : kGarageCallSites)
        {
            const uintptr_t callSite = RebaseGameAddress(preferredCallVA);
            if (!IsReadableRange(reinterpret_cast<const void*>(callSite), 5))
            {
                AppendLog("caller %08X skipped: unreadable\r\n",
                          static_cast<unsigned>(preferredCallVA));
                continue;
            }

            const uint8_t opcode = *reinterpret_cast<const uint8_t*>(callSite);
            if (opcode != 0xE8)
            {
                AppendLog("caller %08X skipped: opcode=%02X, expected E8\r\n",
                          static_cast<unsigned>(preferredCallVA), opcode);
                continue;
            }

            const uintptr_t destination =
                static_cast<uintptr_t>(injector::GetBranchDestination(callSite));
            if (destination != originalTarget)
            {
                AppendLog("caller %08X skipped: target=%08X expected=%08X\r\n",
                          static_cast<unsigned>(preferredCallVA),
                          static_cast<unsigned>(destination),
                          static_cast<unsigned>(originalTarget));
                continue;
            }

            injector::MakeCALL(callSite, GetMatchingGarageCarHook, true);
            ++patchedCalls;
            AppendLog("caller %08X hooked\r\n", static_cast<unsigned>(preferredCallVA));
        }

        AppendLog("garage original target=%08X; external callers hooked=%u/%u\r\n",
                  static_cast<unsigned>(originalTarget),
                  patchedCalls,
                  static_cast<unsigned>(sizeof(kGarageCallSites) / sizeof(kGarageCallSites[0])));

        if (patchedCalls == 0)
            AppendLog("ERROR: no garage callers were hooked.\r\n");

        OutputDebugStringA(
            "[NFSTR_SelectiveUnlocker_test4] Visibility patches + multi-caller garage diagnostics installed.\n");
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
