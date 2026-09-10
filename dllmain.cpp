//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 2)
//
// Experimental build: keep normal progression intact, but conditionally mark
// garage cars unlocked only when the game's own Unlockable metadata says the
// entry is promotional content. This tests whether m_isPromoContent is a useful
// discriminator for DLC/promo cars without using the broad Unlockers bypass or
// Xan's unconditional car/stage unlock patches.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>

#include "includes/injector/injector.hpp"
#include "includes/patterns.hpp"

namespace
{
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

    void AppendLog(const char* fmt, ...)
    {
        char buffer[512]{};
        va_list args;
        va_start(args, fmt);
        _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
        va_end(args);

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test2.log",
                               FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return;

        DWORD written = 0;
        WriteFile(h, buffer, static_cast<DWORD>(strlen(buffer)), &written, nullptr);
        CloseHandle(h);
    }

    Unlockable* __cdecl GetMatchingGarageCarHook(uint32_t attribSysClassKey,
                                                  uint32_t attribSysCollectionKey)
    {
        Unlockable* car = g_GetMatchingGarageCar
            ? g_GetMatchingGarageCar(attribSysClassKey, attribSysCollectionKey)
            : nullptr;

        if (!car)
            return car;

        const bool wasUnlocked = car->m_isUnlocked;
        const bool hidden = car->m_isHiddenUnlock;
        const bool promo = car->m_isPromoContent;

        // TEST 2: use only the game's own promo flag as the discriminator.
        // Do not use m_isHiddenUnlock yet; hidden may include non-DLC rewards.
        if (!car->m_isUnlocked && promo)
            car->m_isUnlocked = true;

        // Log the attribute keys and all four adjacent Unlockable flags. This gives
        // us evidence for the next step without changing non-promo progression.
        AppendLog("class=%08X collection=%08X unlocked:%u->%u hideHowTo=%u hidden=%u promo=%u\r\n",
                  attribSysClassKey,
                  attribSysCollectionKey,
                  wasUnlocked ? 1u : 0u,
                  car->m_isUnlocked ? 1u : 0u,
                  car->m_hideHowTo ? 1u : 0u,
                  hidden ? 1u : 0u,
                  promo ? 1u : 0u);

        return car;
    }

    void Init()
    {
        DeleteFileA("NFSTR_SelectiveUnlocker_test2.log");
        AppendLog("NFSTR Selective Unlocker test 2\r\n");
        AppendLog("Mode: unlock matching garage cars only when m_isPromoContent == true\r\n");
        AppendLog("No Unlockers bypass; no stage unlock; no online/Ebisu patches.\r\n\r\n");

        pattern::Win32::Init();
        if (!pattern::Win32::bIsInited())
        {
            AppendLog("ERROR: pattern scanner failed to initialise.\r\n");
            return;
        }

        // Same call path FusionFix uses to locate NFSUIVehicleComp::getMatchingGarageCar.
        // Hook only this caller so the original function remains available as a clean
        // trampoline target and normal game logic still runs first.
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

        AppendLog("Hook installed at %08X; original target %08X.\r\n",
                  static_cast<unsigned>(callSite),
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(g_GetMatchingGarageCar)));
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
