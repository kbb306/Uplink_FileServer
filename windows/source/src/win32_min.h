#ifndef UFS_WIN32_MIN_H
#define UFS_WIN32_MIN_H

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef unsigned short* LPWSTR;
typedef const unsigned short* LPCWSTR;
typedef const char* LPCSTR;
typedef unsigned long SIZE_T;
typedef unsigned long ULONG;
typedef unsigned long long U64;
#define WINAPI __stdcall
#define API __declspec(dllimport)
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))
#define GENERIC_READ 0x80000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define CREATE_ALWAYS 2u
#define GENERIC_WRITE 0x40000000u
#define PAGE_EXECUTE_READWRITE 0x40u
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_BREAKPOINT 0x80000003u
#define EXCEPTION_SINGLE_STEP 0x80000004u
#define TLS_OUT_OF_INDEXES 0xffffffffu

API DWORD WINAPI GetCurrentProcessId(void);
API DWORD WINAPI GetTickCount(void);
API HANDLE WINAPI FindWindowA(LPCSTR,LPCSTR);
API DWORD WINAPI GetWindowThreadProcessId(HANDLE,DWORD*);
API HMODULE WINAPI GetModuleHandleA(LPCSTR);
API DWORD WINAPI GetModuleFileNameA(HMODULE,LPSTR,DWORD);
API HANDLE WINAPI GetCurrentProcess(void);
API DWORD WINAPI GetCurrentThreadId(void);
API BOOL WINAPI VirtualProtect(LPVOID,SIZE_T,DWORD,DWORD*);
API BOOL WINAPI FlushInstructionCache(HANDLE,LPCVOID,SIZE_T);
API HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
API BOOL WINAPI ReadFile(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
API BOOL WINAPI WriteFile(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
API BOOL WINAPI CloseHandle(HANDLE);
API DWORD WINAPI SetFilePointer(HANDLE,long,long*,DWORD);
API HANDLE WINAPI AddVectoredExceptionHandler(ULONG, LPVOID);
API ULONG WINAPI RemoveVectoredExceptionHandler(HANDLE);
API DWORD WINAPI TlsAlloc(void);
API LPVOID WINAPI TlsGetValue(DWORD);
API BOOL WINAPI TlsSetValue(DWORD,LPVOID);
API BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
API HANDLE WINAPI CreateThread(LPVOID,SIZE_T,LPVOID,LPVOID,DWORD,DWORD*);
API void WINAPI Sleep(DWORD);

typedef struct _UFS_EXCEPTION_RECORD {
    DWORD ExceptionCode, ExceptionFlags;
    struct _UFS_EXCEPTION_RECORD *ExceptionRecord;
    LPVOID ExceptionAddress;
    DWORD NumberParameters;
    unsigned long ExceptionInformation[15];
} UFS_EXCEPTION_RECORD;
typedef struct _UFS_FLOATSAVE {
    DWORD ControlWord, StatusWord, TagWord, ErrorOffset, ErrorSelector, DataOffset, DataSelector;
    unsigned char RegisterArea[80];
    DWORD Cr0NpxState;
} UFS_FLOATSAVE;
typedef struct _UFS_CONTEXT {
    DWORD ContextFlags, Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    UFS_FLOATSAVE FloatSave;
    DWORD SegGs, SegFs, SegEs, SegDs;
    DWORD Edi, Esi, Ebx, Edx, Ecx, Eax;
    DWORD Ebp, Eip, SegCs, EFlags, Esp, SegSs;
    U8 ExtendedRegisters[512];
} UFS_CONTEXT;
typedef struct _UFS_EXCEPTION_POINTERS {UFS_EXCEPTION_RECORD *ExceptionRecord; UFS_CONTEXT *ContextRecord;} UFS_EXCEPTION_POINTERS;
API HANDLE WINAPI CreateToolhelp32Snapshot(DWORD,DWORD);
API HANDLE WINAPI CreateRemoteThread(HANDLE,LPVOID,SIZE_T,LPVOID,LPVOID,DWORD,DWORD*);
API BOOL WINAPI Process32FirstA(HANDLE,LPVOID);
API BOOL WINAPI Process32NextA(HANDLE,LPVOID);
API BOOL WINAPI Module32FirstA(HANDLE,LPVOID);
API BOOL WINAPI Module32NextA(HANDLE,LPVOID);
API HANDLE WINAPI OpenProcess(DWORD,BOOL,DWORD);
API LPVOID WINAPI VirtualAllocEx(HANDLE,LPVOID,SIZE_T,DWORD,DWORD);
API BOOL WINAPI WriteProcessMemory(HANDLE,LPVOID,LPCVOID,SIZE_T,SIZE_T*);
API BOOL WINAPI VirtualFreeEx(HANDLE,LPVOID,SIZE_T,DWORD);
API DWORD WINAPI GetSystemDirectoryA(LPSTR,DWORD);
API HMODULE WINAPI LoadLibraryA(LPCSTR);
API DWORD WINAPI GetTempPathA(DWORD,LPSTR);
API DWORD WINAPI GetLastError(void);
API LPVOID WINAPI GetProcAddress(HMODULE,LPCSTR);
API DWORD WINAPI WaitForSingleObject(HANDLE,DWORD);
API BOOL WINAPI GetExitCodeThread(HANDLE,DWORD*);
API HANDLE WINAPI GetStdHandle(DWORD);
API LPSTR WINAPI GetCommandLineA(void);
API BOOL WINAPI CreateProcessA(LPCSTR,LPSTR,LPVOID,LPVOID,BOOL,DWORD,LPVOID,LPCSTR,LPVOID,LPVOID);
API BOOL WINAPI GetExitCodeProcess(HANDLE,DWORD*);
API void WINAPI ExitProcess(DWORD);
#endif
