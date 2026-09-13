//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 5)
//
// Test 4 proved that redirecting getMatchingGarageCar callers is not the path
// used by the View Cars lock presentation: all ten verified callers were hooked,
// yet browsing locked cars produced no runtime records.
//
// Test 5 moves to the two exact locked-car branches patched by Xan's original
// Ultimate Unlocker. Unlike the original mod, these hooks DO NOT blanket-unlock
// every car. They inspect the same Unlockable object and skip the locked path only
// when m_isPromoContent is true. All non-promo cars stay on the original path.
//
// The proven IsPromoContent / IsHiddenUnlock reflection-name patches are retained
// only to keep DLC/promo entries visible. The two car-gate hooks log all adjacent
// flags so we can determine whether that visibility patch also destroys the promo
// discriminator.
//
// NO generic Unlockers bypass. NO stage-select unlock. NO online/Ebisu patches.
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

    // These are the two broad car-unlock sites used by the original Ultimate
    // Unlocker. Both execute only after the game has already determined the car
    // is locked. We replace exactly five bytes at each site and preserve the
    // original instructions for non-promo cars.
    constexpr uintptr_t kCarGate1VA = 0x0093E00D;
    constexpr uintptr_t kCarGate1LockedContinueVA = 0x0093E012;
    constexpr uintptr_t kCarGate1UnlockedContinueVA = 0x0093E0D8;

    constexpr uintptr_t kCarGate2VA = 0x0093F214;
    constexpr uintptr_t kCarGate2LockedContinueVA = 0x0093F219;
    constexpr uintptr_t kCarGate2UnlockedContinueVA = 0x0093F2A0;

    const uint8_t kCarGate1Expected[5] = { 0x8B, 0x54, 0x24, 0x7C, 0x51 }; // mov edx,[esp+7C]; push ecx
    const uint8_t kCarGate2Expected[5] = { 0x51, 0x8B, 0x4C, 0x24, 0x50 }; // push ecx; mov ecx,[esp+50]

    struct Unlockable
    {
        uint8_t pad[0x18];
        bool m_isUnlocked;
        bool m_hideHowTo;
        bool m_isHiddenUnlock;
        bool m_isPromoContent;
    };

    uintptr_t g_CarGate1LockedContinue = 0;
    uintptr_t g_CarGate1UnlockedContinue = 0;
    uintptr_t g_CarGate2LockedContinue = 0;
    uintptr_t g_CarGate2UnlockedContinue = 0;
    volatile LONG g_ObservationCount = 0;

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

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test5.log",
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

    void __cdecl ObserveUnlockable(uint32_t site, Unlockable* car)
    {
        if (!car)
        {
            AppendLog("gate%u car=null\r\n", site);
            return;
        }

        const LONG observation = InterlockedIncrement(&g_ObservationCount);
        if (observation > 500)
            return;

        AppendLog("gate%u car=%08X unlocked=%u hideHowTo=%u hidden=%u promo=%u action=%s\r\n",
                  site,
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(car)),
                  car->m_isUnlocked ? 1u : 0u,
                  car->m_hideHowTo ? 1u : 0u,
                  car->m_isHiddenUnlock ? 1u : 0u,
                  car->m_isPromoContent ? 1u : 0u,
                  car->m_isPromoContent ? "SELECTIVE_UNLOCK" : "VANILLA_LOCKED_PATH");
    }

    // Site 1 enters with ECX = Unlockable*. The five overwritten stock bytes are:
    //   mov edx,[esp+7C]
    //   push ecx
    // Non-promo entries execute those instructions and return to 0x93E012.
    // Promo entries set +0x18 and jump to the stock already-unlocked continuation.
    void __declspec(naked) CarGateHook1()
    {
        __asm
        {
            pushfd
            pushad
            push ecx
            push 1
            call ObserveUnlockable
            add esp, 8
            popad
            popfd

            cmp byte ptr [ecx + 1Bh], 0
            je gate1_vanilla

            mov byte ptr [ecx + 18h], 1
            jmp dword ptr [g_CarGate1UnlockedContinue]

        gate1_vanilla:
            mov edx, dword ptr [esp + 7Ch]
            push ecx
            jmp dword ptr [g_CarGate1LockedContinue]
        }
    }

    // Site 2 enters with EBP = Unlockable*. The five overwritten stock bytes are:
    //   push ecx
    //   mov ecx,[esp+50]
    void __declspec(naked) CarGateHook2()
    {
        __asm
        {
            pushfd
            pushad
            push ebp
            push 2
            call ObserveUnlockable
            add esp, 8
            popad
            popfd

            cmp byte ptr [ebp + 1Bh], 0
            je gate2_vanilla

            mov byte ptr [ebp + 18h], 1
            jmp dword ptr [g_CarGate2UnlockedContinue]

        gate2_vanilla:
            push ecx
            mov ecx, dword ptr [esp + 50h]
            jmp dword ptr [g_CarGate2LockedContinue]
        }
    }

    bool InstallCarGateHook(uintptr_t preferredVA,
                            const uint8_t (&expected)[5],
                            void* hook,
                            const char* name)
    {
        const uintptr_t liveVA = RebaseGameAddress(preferredVA);
        if (!IsReadableRange(reinterpret_cast<const void*>(liveVA), sizeof(expected)))
        {
            AppendLog("%s FAILED: site unreadable\r\n", name);
            return false;
        }

        if (std::memcmp(reinterpret_cast<const void*>(liveVA), expected, sizeof(expected)) != 0)
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(liveVA);
            AppendLog("%s FAILED: bytes=%02X %02X %02X %02X %02X\r\n",
                      name, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
            return false;
        }

        injector::MakeJMP(liveVA, reinterpret_cast<uintptr_t>(hook), true);
        AppendLog("%s installed at %08X\r\n", name, static_cast<unsigned>(preferredVA));
        return true;
    }

    void Init()
    {
        DeleteFileA("NFSTR_SelectiveUnlocker_test5.log");
        AppendLog("NFSTR Selective Unlocker test 5\r\n");
        AppendLog("Visibility: neutralise IsPromoContent + IsHiddenUnlock reflection names\r\n");
        AppendLog("Cars: hook Xan's two original locked-car branches; skip lock path ONLY for promo=true\r\n");
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

        g_CarGate1LockedContinue = RebaseGameAddress(kCarGate1LockedContinueVA);
        g_CarGate1UnlockedContinue = RebaseGameAddress(kCarGate1UnlockedContinueVA);
        g_CarGate2LockedContinue = RebaseGameAddress(kCarGate2LockedContinueVA);
        g_CarGate2UnlockedContinue = RebaseGameAddress(kCarGate2UnlockedContinueVA);

        const bool gate1 = InstallCarGateHook(
            kCarGate1VA,
            kCarGate1Expected,
            reinterpret_cast<void*>(&CarGateHook1),
            "car gate 1");

        const bool gate2 = InstallCarGateHook(
            kCarGate2VA,
            kCarGate2Expected,
            reinterpret_cast<void*>(&CarGateHook2),
            "car gate 2");

        AppendLog("car gates installed=%u/2\r\n", (gate1 ? 1u : 0u) + (gate2 ? 1u : 0u));

        OutputDebugStringA(
            "[NFSTR_SelectiveUnlocker_test5] Visibility + selective original car-gate hooks installed.\n");
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
