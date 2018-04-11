#include "stdafx.h"

struct Screen
{
    int32_t Width;
    int32_t Height;
    float fWidth;
    float fHeight;
    float fFieldOfView;
    float fAspectRatio;
    int32_t Width43;
    float fWidth43;
    float fHudScale;
    float fHudOffset;
} Screen;

int32_t nLanguage;
int32_t __cdecl SetLanguage(LPCSTR lpValueName)
{
    return nLanguage;
}

DWORD WINAPI Init(LPVOID bDelay)
{
    auto pattern = hook::pattern("BF 94 00 00 00 8B C7");

    if (pattern.count_hint(1).empty() && !bDelay)
    {
        CreateThreadAutoClose(0, 0, (LPTHREAD_START_ROUTINE)&Init, (LPVOID)true, 0, NULL);
        return 0;
    }

    if (bDelay)
        while (pattern.clear().count_hint(1).empty()) { Sleep(0); };

    CIniReader iniReader("");
    bool bDoNotUseRegistryPath = iniReader.ReadInteger("MAIN", "DoNotUseRegistryPath", 1) != 0;
    nLanguage = iniReader.ReadInteger("MAIN", "Language", -1);
    static bool bFixHUD = iniReader.ReadInteger("MAIN", "FixHUD", 1) != 0;
    static bool bFixFOV = iniReader.ReadInteger("MAIN", "FixFOV", 1) != 0;
    static float fGameSpeed = iniReader.ReadFloat("MAIN", "GameSpeed", 128.0f);

    if (bDoNotUseRegistryPath)
    {
        pattern = hook::pattern("B9 20 00 00 00 8D 7C 24 18 F3 AB 8D 44 24 0C"); //0x496F27
        struct RegHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = 0x20;
                regs.edi = (regs.esp + 0x18);

                GetModuleFileNameA(NULL, (char*)regs.edi, MAX_PATH);
                *strrchr((char*)regs.edi, '\\') = '\0';
                strcat((char*)regs.edi, "\\data");
            }
        }; injector::MakeInline<RegHook>(pattern.get_first(0), pattern.get_first(11));
        injector::MakeJMP(pattern.get_first(11), hook::pattern("8D 44 24 18 8D 50 01").count(2).get(1).get<uintptr_t>(0), true); //0x496FD8
    }

    if (nLanguage >= 0)
    {
        pattern = hook::pattern("E8 ? ? ? ? 8B 04 85 ? ? ? ? 83 C4 04 C3"); //0x495E95
        injector::MakeCALL(pattern.count(1).get(0).get<uintptr_t>(0), SetLanguage, true);
        pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 83 C4 04 C3"); //0x495EB5
        injector::MakeCALL(pattern.count(2).get(1).get<uintptr_t>(5), SetLanguage, true);
    }

    pattern = hook::pattern("89 55 00 89 5D 04 C7 45 08 15 00 00 00 89 7D 0C"); //0x649478
    struct ResHook
    {
        void operator()(injector::reg_pack& regs)
        {
            Screen.Width = regs.edx;
            Screen.Height = regs.ebx;
            *(uint32_t*)(regs.ebp + 0x00) = Screen.Width;
            *(uint32_t*)(regs.ebp + 0x04) = Screen.Height;

            Screen.fWidth = static_cast<float>(Screen.Width);
            Screen.fHeight = static_cast<float>(Screen.Height);
            Screen.fAspectRatio = (Screen.fWidth / Screen.fHeight);
            Screen.Width43 = static_cast<uint32_t>(Screen.fHeight * (4.0f / 3.0f));
            Screen.fWidth43 = static_cast<float>(Screen.Width43);
            Screen.fHudOffset = (1.0f / (Screen.fHeight * (4.0f / 3.0f))) * ((Screen.fWidth - Screen.fHeight * (4.0f / 3.0f)) / 2.0f);
        }
    }; injector::MakeInline<ResHook>(pattern.get_first(0), pattern.get_first(6));

    if (bFixHUD)
    {
        uintptr_t dword_654780 = (uintptr_t)hook::pattern("8B 44 24 04 F3 0F 2A C0 0F 28 C8").count(1).get(0).get<uintptr_t>(0);
        struct HudScaleHook
        {
            void operator()(injector::reg_pack& regs)
            {
                int32_t a2 = *(int32_t*)(regs.esp + 4);
                if (a2 == Screen.Width)
                    a2 = Screen.Width43;

                *(float*)(regs.ecx + 0xE0) = (float)a2 * (1.0f / 640.0f);
                *(int32_t*)(regs.ecx + 0xD8) = a2;
                *(float*)(regs.ecx + 0xE8) = (1.0f / (float)a2);
            }
        }; injector::MakeInline<HudScaleHook>(dword_654780, dword_654780 + 53);

        pattern = hook::pattern("F3 0F 11 44 24 24 F3 0F 11 44 24 20 F3 0F 11"); //0x65E870
        struct HudPosHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *(float*)(regs.esp + 0x18) = Screen.fHudOffset;
                *(float*)(regs.esp + 0x20) = 1.0f;
                *(float*)(regs.esp + 0x24) = 1.0f;
                *(float*)(regs.esp + 0x28) = 1.0f;
                *(float*)(regs.esp + 0x2C) = 1.0f;
            }
        }; injector::MakeInline<HudPosHook>(pattern.get_first(0), pattern.get_first(24));
        injector::WriteMemory(pattern.get_first(24 - 4), 0x9001F883, true); //cmp     eax, 1
    }

    if (bFixFOV)
    {
        pattern = hook::pattern("F3 0F 10 45 08 56 8B F1 8B 46 1C"); //0x64BDF9 
        struct FOVHook
        {
            void operator()(injector::reg_pack& regs)
            {
                Screen.fFieldOfView = *(float*)(regs.ebp + 0x8) * (((4.0f / 3.0f)) / (Screen.fAspectRatio));
                _asm movss   xmm0, dword ptr[Screen.fFieldOfView]
            }
        }; injector::MakeInline<FOVHook>(pattern.get_first(0));
    }

    if (fGameSpeed)
    {
        pattern = hook::pattern("D8 2D ? ? ? ? DE F9 C3");
        injector::WriteMemory(pattern.count(3).get(0).get<void>(2), &fGameSpeed, true); //0x40C930 + 0x2 + 0x32
        injector::WriteMemory(pattern.count(3).get(1).get<void>(2), &fGameSpeed, true); //0x40C930 + 0x2 + 0x64
        injector::WriteMemory(pattern.count(3).get(2).get<void>(2), &fGameSpeed, true); //0x40C930 + 0x2 + 0x4D
    }

    return 0;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Init(NULL);
    }
    return TRUE;
}