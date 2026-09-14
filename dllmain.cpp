//
// Need for Speed: The Run - Ultimate Unlocker
// Fork of xan1242/NFSTR_UltimateUnlocker
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>

#include "includes/injector/injector.hpp"
#include "includes/injector/assembly.hpp"

namespace
{
    constexpr uintptr_t kPreferredImageBase = 0x00400000;

    constexpr uintptr_t kUnlockersVA = 0x025A35E4;
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

    struct Settings
    {
        bool unlockDLC = true;
        bool unlockAll = false;
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

    Settings g_Settings{};
    OnlineUnlockerMethodFn g_OriginalOnlineUnlockerMethod = nullptr;

    const char* const kDlcOffers[] = {
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

    void BuildIniPath(HMODULE module, char* outPath, size_t outSize)
    {
        if (!outPath || outSize == 0)
            return;

        outPath[0] = '\0';

        const DWORD length = GetModuleFileNameA(module, outPath, static_cast<DWORD>(outSize));
        if (length == 0 || length >= outSize)
        {
            const char fallback[] = ".\\NFSTR_UltimateUnlocker.ini";
            if (sizeof(fallback) <= outSize)
                std::memcpy(outPath, fallback, sizeof(fallback));
            return;
        }

        char* slash = std::strrchr(outPath, '\\');
        const char iniName[] = "NFSTR_UltimateUnlocker.ini";

        if (!slash)
        {
            if (sizeof(iniName) <= outSize)
                std::memcpy(outPath, iniName, sizeof(iniName));
            return;
        }

        ++slash;
        const size_t prefixLength = static_cast<size_t>(slash - outPath);
        if (prefixLength + sizeof(iniName) <= outSize)
            std::memcpy(slash, iniName, sizeof(iniName));
    }

    Settings LoadSettings(HMODULE module)
    {
        Settings settings{};
        char iniPath[MAX_PATH]{};
        BuildIniPath(module, iniPath, sizeof(iniPath));

        settings.unlockDLC = GetPrivateProfileIntA(
            "UNLOCKS", "UnlockDLC", settings.unlockDLC ? 1 : 0, iniPath) != 0;
        settings.unlockAll = GetPrivateProfileIntA(
            "UNLOCKS", "UnlockAll", settings.unlockAll ? 1 : 0, iniPath) != 0;

        return settings;
    }

    bool IsDlcOffer(const char* offer)
    {
        if (!offer || !offer[0])
            return false;

        for (const char* allowed : kDlcOffers)
        {
            if (std::strcmp(offer, allowed) == 0)
                return true;
        }

        return false;
    }

    bool ShouldGrantOffer(const char* offer)
    {
        if (!offer || !offer[0])
            return false;

        if (g_Settings.unlockDLC && IsDlcOffer(offer))
            return true;

        if (g_Settings.unlockAll)
        {
            if (IsDlcOffer(offer))
                return true;

            if (std::strcmp(offer, "timesavers_pack") == 0)
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

        // GaragePurchaseUnlocker derives from OnlineUnlocker and uses the same
        // method, so only grant exact OnlineUnlocker objects here.
        if (vtable == onlineVtable)
        {
            char offer[128]{};
            if (CopyReadableCString(self->offerId, offer, sizeof(offer)) &&
                ShouldGrantOffer(offer) &&
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

    bool ApplyUnlockAllPatches()
    {
        static constexpr char kUnlockersName[] = "Unlockers";

        char* const unlockers = reinterpret_cast<char*>(RebaseGameAddress(kUnlockersVA));
        const uintptr_t carHook1 = RebaseGameAddress(0x0093E00D);
        const uintptr_t carHook2 = RebaseGameAddress(0x0093F214);

        if (!IsReadableRange(unlockers, sizeof(kUnlockersName)) ||
            std::memcmp(unlockers, kUnlockersName, sizeof(kUnlockersName)) != 0 ||
            !IsReadableRange(reinterpret_cast<void*>(RebaseGameAddress(0x00834303)), 2) ||
            !IsReadableRange(reinterpret_cast<void*>(RebaseGameAddress(0x0083434F)), 2) ||
            !IsReadableRange(reinterpret_cast<void*>(carHook1), 6) ||
            !IsReadableRange(reinterpret_cast<void*>(carHook2), 6) ||
            !IsReadableRange(reinterpret_cast<void*>(RebaseGameAddress(0x00930C00)), 2) ||
            !IsReadableRange(reinterpret_cast<void*>(RebaseGameAddress(0x009313A2)), 2))
        {
            return false;
        }

        // Original Ultimate Unlocker behavior.
        injector::MakeNOP(RebaseGameAddress(0x00834303), 2);
        injector::MakeNOP(RebaseGameAddress(0x0083434F), 2);

        // "Unlockers" -> "Unlocker", removing the generic unlocker list.
        injector::WriteMemory<uint8_t>(RebaseGameAddress(0x025A35EC), 0, true);

        struct CarUnlockHook1
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint8_t*)(regs.ecx + 0x18) = 1;
            }
        };

        injector::MakeInline<CarUnlockHook1>(carHook1, carHook1 + 6);
        injector::MakeJMP(carHook1 + 6, RebaseGameAddress(0x0093E0D8));

        struct CarUnlockHook2
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint8_t*)(regs.ebp + 0x18) = 1;
            }
        };

        injector::MakeInline<CarUnlockHook2>(carHook2, carHook2 + 6);
        injector::MakeJMP(carHook2 + 6, RebaseGameAddress(0x0093F2A0));

        // Stage select unlocks from the original project.
        injector::MakeNOP(RebaseGameAddress(0x00930C00), 2);
        injector::MakeNOP(RebaseGameAddress(0x009313A2), 2);

        return true;
    }

    void Init(HMODULE module)
    {
        g_Settings = LoadSettings(module);

        bool ok = true;

        if (g_Settings.unlockDLC || g_Settings.unlockAll)
            ok = PatchVisibilityMetadata() && ok;

        if (g_Settings.unlockDLC || g_Settings.unlockAll)
            ok = InstallOnlineUnlockerHook() && ok;

        if (g_Settings.unlockAll)
            ok = ApplyUnlockAllPatches() && ok;

        if (ok)
        {
            OutputDebugStringA("[NFSTR_UltimateUnlocker] Loaded.\n");
        }
        else
        {
            OutputDebugStringA(
                "[NFSTR_UltimateUnlocker] Compatibility check failed; one or more patches were not installed.\n");
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        Init(hModule);
    }

    return TRUE;
}
