#pragma comment(linker, "/ENTRY:DllMain")
#pragma comment(linker, "/SECTION:.text,ERW /MERGE:.rdata=.text /MERGE:.data=.text")
#pragma comment(linker, "/SECTION:.Asuna,ERW /MERGE:.text=.Asuna")
#pragma comment(linker, "/EXPORT:InitCommonControlsEx=COMCTL32.InitCommonControlsEx")

#include "MyLibrary.cpp"
#include <Psapi.h>

ULONG_PTR ParentPid;

TYPE_OF(GetModuleFileNameExW)*      StubGetModuleFileNameExW;
TYPE_OF(NtQueryInformationProcess)* StubNtQueryInformationProcess;

ULONG
WINAPI
QqGetModuleFileNameExW(
    HANDLE  Process,
    PVOID   Module,
    PWSTR   Filename,
    ULONG   Size
)
{
    ULONG       Length;
    PWSTR       File;
    NTSTATUS    Status;
    PROCESS_BASIC_INFORMATION BasicInfo;

    static WCHAR QQProtect[] = L"QQProtect.exe";

    Length = StubGetModuleFileNameExW(Process, (HMODULE)Module, Filename, Size);
    if (Length == 0 || Filename == NULL || Size == 0)
        return Length;

    Status = ZwQueryInformationProcess(Process, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (NT_FAILED(Status) || BasicInfo.UniqueProcessId != CurrentPid())
        return Length;

    File = findnamew(Filename);
    CopyStruct(File, QQProtect, sizeof(QQProtect));

    return File - Filename + CONST_STRLEN(QQProtect);
}

NTSTATUS
NTAPI
QqNtQueryInformationProcess(
    HANDLE              ProcessHandle,
    PROCESSINFOCLASS    ProcessInformationClass,
    PVOID               ProcessInformation,
    ULONG               ProcessInformationLength,
    PULONG              ReturnLength
)
{
    NTSTATUS Status;

    union
    {
        PVOID Information;
        PPROCESS_BASIC_INFORMATION Basic;
    };

    Status = StubNtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
    FAIL_RETURN(Status);

    Information = ProcessInformation;

    switch (ProcessInformationClass)
    {
        case ProcessBasicInformation:
            if (Basic->UniqueProcessId == CurrentPid())
            {
                Basic->InheritedFromUniqueProcessId = Basic->UniqueProcessId;
            }
            break;
    }

    return Status;
}

BOOL UnInitialize(PVOID BaseAddress)
{
    return FALSE;
}

BOOL Initialize(PVOID BaseAddress)
{
    NTSTATUS    Status;
    PVOID       psapi, module;
    PROCESS_BASIC_INFORMATION BasicInfo;

    LdrDisableThreadCalloutsForDll(BaseAddress);

    Status = ZwQueryInformationProcess(CurrentProcess, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (NT_SUCCESS(Status))
        ParentPid = BasicInfo.InheritedFromUniqueProcessId;

    psapi = Ldr::LoadDll(L"psapi.dll");
    if (psapi == NULL)
        return FALSE;

    MEMORY_FUNCTION_PATCH f[] =
    {
        INLINE_HOOK_JUMP(ZwQueryInformationProcess, QqNtQueryInformationProcess, StubNtQueryInformationProcess),
        EAT_HOOK_JUMP_HASH(psapi, PSAPI_GetModuleFileNameExW, QqGetModuleFileNameExW, StubGetModuleFileNameExW),
    };

    MEMORY_PATCH Patch_HummerEngine[] =
    {
        PATCH_MEMORY(0xD74C033, 4, 0x4D29),
    };

    typedef struct
    {
        PCWSTR                  DllName;
        PMEMORY_PATCH           Patch;
        ULONG_PTR               PatchCount;
        PMEMORY_FUNCTION_PATCH  FunctionPatch;
        ULONG_PTR               FunctionCount;

    } PATCH_ARRAY, *PPATCH_ARRAY;

    PATCH_ARRAY *Entry, Array[] = 
    {
        { L"HummerEngine.dll", Patch_HummerEngine, countof(Patch_HummerEngine) },
    };

    FOR_EACH(Entry, Array, countof(Array))
    {
        Nt_PatchMemory(Entry->Patch, Entry->PatchCount, Entry->FunctionPatch, Entry->FunctionCount, Ldr::LoadDll(Entry->DllName));
    }

    Nt_PatchMemory(NULL, 0, f, countof(f), psapi);

    return TRUE;
}

BOOL WINAPI DllMain(PVOID BaseAddress, ULONG Reason, PVOID Reserved)
{
    switch (Reason)
    {
        case DLL_PROCESS_ATTACH:
            return Initialize(BaseAddress) || UnInitialize(BaseAddress);

        case DLL_PROCESS_DETACH:
            UnInitialize(BaseAddress);
            break;
    }

    return TRUE;
}