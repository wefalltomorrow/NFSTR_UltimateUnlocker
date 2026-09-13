//
// Need for Speed The Run - Selective Promo/DLC Unlocker (test 6)
//
// Test 6 moves away from car-side forcing and targets the game's actual
// entitlement unlocker class.
//
// Reverse engineering of the v1.1 Win32 type metadata shows distinct unlocker
// classes for normal progression (LevelUpUnlocker, StageCompletionUnlocker,
// ChallengeCompletionUnlocker, etc.) and a separate OnlineUnlocker carrying
// OfferId / PS3Sku / XenonSku / PCSku fields.  OnlineUnlocker's stock method at
// 0x008D0BA0 searches the entitlement list and only calls 0x007F3780 when the
// requested offer is owned.
//
// This build replaces ONLY that OnlineUnlocker entitlement check with the stock
// success action.  Normal progression unlockers are untouched.  The derived
// GaragePurchaseUnlocker uses the same virtual method and therefore follows the
// same entitlement-only path.
//
// The proven IsPromoContent / IsHiddenUnlock reflection-name patches are kept
// solely to expose otherwise-hidden promo/DLC UI entries.
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

    // OnlineUnlocker::entitlement-check/update method in DRM-free v1.1.
    constexpr uintptr_t kOnlineUnlockerMethodVA = 0x008D0BA0;

    // Stock success path used by OnlineUnlocker after an entitlement match.
    // Call shape in the game:
    //   ecx = *(void**)0x02882500 + 0x3CB4
    //   push OnlineUnlocker*
    //   call 0x007F3780
    constexpr uintptr_t kUnlockManagerGlobalVA = 0x02882500;
    constexpr uintptr_t kUnlockManagerOffset = 0x00003CB4;
    constexpr uintptr_t kGrantOnlineUnlockVA = 0x007F3780;

    constexpr uintptr_t kOnlineUnlockerVtableVA = 0x024791FC;
    constexpr uintptr_t kGaragePurchaseUnlockerVtableVA = 0x0247920C;

    const uint8_t kOnlineUnlockerExpected[] = {
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

    using GrantOnlineUnlockFn = void (__thiscall *)(void* unlockManager, OnlineUnlocker* unlocker);

    volatile LONG g_ObservationCount = 0;

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

        HANDLE h = CreateFileA("NFSTR_SelectiveUnlocker_test6.log",
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

        void* const rootManager = *reinterpret_cast<void**>(RebaseGameAddress(kUnlockManagerGlobalVA));
        if (!rootManager)
        {
            if (observation <= 500)
                AppendLog("  -> NOT GRANTED: unlock manager root is null\r\n");
            return;
        }

        void* const unlockManager = reinterpret_cast<uint8_t*>(rootManager) + kUnlockManagerOffset;
        const auto grant = reinterpret_cast<GrantOnlineUnlockFn>(RebaseGameAddress(kGrantOnlineUnlockVA));
        grant(unlockManager, self);

        if (observation <= 500)
            AppendLog("  -> GRANTED via stock OnlineUnlocker success path 0x007F3780\r\n");
    }

    bool InstallOnlineUnlockerHook()
    {
        const uintptr_t liveMethod = RebaseGameAddress(kOnlineUnlockerMethodVA);
        if (!IsReadableRange(reinterpret_cast<const void*>(liveMethod), sizeof(kOnlineUnlockerExpected)))
        {
            AppendLog("OnlineUnlocker hook FAILED: method unreadable\r\n");
            return false;
        }

        if (std::memcmp(reinterpret_cast<const void*>(liveMethod),
                        kOnlineUnlockerExpected,
                        sizeof(kOnlineUnlockerExpected)) != 0)
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(liveMethod);
            AppendLog("OnlineUnlocker hook FAILED: unexpected bytes %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                      b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            return false;
        }

        injector::MakeJMP(liveMethod,
                          reinterpret_cast<uintptr_t>(&OnlineUnlockerEntitlementHook),
                          true);

        AppendLog("OnlineUnlocker entitlement hook installed at %08X\r\n",
                  static_cast<unsigned>(kOnlineUnlockerMethodVA));
        return true;
    }

    void Init()
    {
        DeleteFileA("NFSTR_SelectiveUnlocker_test6.log");
        AppendLog("NFSTR Selective Unlocker test 6\r\n");
        AppendLog("Visibility: neutralise IsPromoContent + IsHiddenUnlock reflection names\r\n");
        AppendLog("Entitlements: force ONLY OnlineUnlocker/GaragePurchaseUnlocker through stock success path\r\n");
        AppendLog("Normal progression unlocker classes remain untouched.\r\n");
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

        const bool entitlementHooked = InstallOnlineUnlockerHook();
        AppendLog("selective entitlement hook=%s\r\n",
                  entitlementHooked ? "installed" : "FAILED");

        OutputDebugStringA(
            "[NFSTR_SelectiveUnlocker_test6] Selective OnlineUnlocker entitlement bypass installed.\n");
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();

    return TRUE;
}
