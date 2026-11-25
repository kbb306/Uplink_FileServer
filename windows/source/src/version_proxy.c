/*
 * Uplink FILESERVER Fix 0.5: VERSION.dll proxy for 32-bit Steam Uplink 1.55.
 * Copyright (c) 2026. MIT license; fan project, not affiliated with Introversion.
 *
 * Exposes 17 modern Windows version.dll API names (instead of just the
 * three imported directly by Uplink.exe), and forwards each call to the
 * genuine DLL in the Windows system directory. Never modify system DLLs.
 *
 * DllMain queues a lightweight worker to wait for the SDL game window, then
 * load UplinkFileServer.dll from the application folder. This avoids runtime
 * patching during UplinkSteamAuth/SteamAPI initialization. It does not call
 * LoadLibrary or wait inside loader lock.
 */
#include "win32_min.h"

static HMODULE g_proxy;
static HMODULE g_real_version;

static U32 str_len(const char *s) { U32 i=0; if(s)while(s[i])i++;return i; }
static int append(char *p,U32 cap,const char *tail) {
    U32 n=str_len(p),m=str_len(tail),i;
    if (n+m>=cap)return 0;
    for(i=0;i<m;i++)p[n+i]=tail[i];
    p[n+m]=0;
    return 1;
}
static void log_msg(const char *s) {
    char path[1024];DWORD n,w;HANDLE h;
    n=GetTempPathA(sizeof(path),path);
    if(n==0 || n>=sizeof(path) || !append(path,sizeof(path),"UplinkFileServerProxy.log"))return;
    h=CreateFileA(path,GENERIC_WRITE,3,0,4,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE)return;
    SetFilePointer(h,0,0,2);
    WriteFile(h,s,str_len(s),&w,0);
    WriteFile(h,"\r\n",2,&w,0);
    CloseHandle(h);
}
static HMODULE get_real_version(void) {
    char path[1024];DWORD n;
    if(g_real_version)return g_real_version;
    n=GetSystemDirectoryA(path,sizeof(path));
    if(n==0 || n>=sizeof(path)-16)return 0;
    if(!append(path,sizeof(path),"\\version.dll"))return 0;
    /* The absolute system path is essential; resolving just "version.dll"
       would load this proxy again. WOW64 supplies the correct 32-bit system
       directory to 32-bit callers. */
    g_real_version=LoadLibraryA(path);
    if(g_real_version)log_msg("System version.dll opened successfully");
    else log_msg("ERROR: cannot open system version.dll");
    if(g_real_version==g_proxy) {
        /* Refuse recursion if a future Windows loader behaves differently. */
        g_real_version=0;
        log_msg("System version.dll resolved to proxy itself; forwarding disabled");
    }
    return g_real_version;
}
static LPVOID resolve(const char *name) {
    HMODULE h=get_real_version();
    LPVOID f=h?GetProcAddress(h,name):0;
    if(!f) {
        char msg[128]="WARNING: unresolved version API: ";
        if(append(msg,sizeof(msg),name))log_msg(msg);
    }
    return f;
}

/* Each wrapper uses its real Win32 stdcall prototype, preserving the stack. */
typedef BOOL (WINAPI *FP_GetFileVersionInfoA)(LPCSTR,DWORD,DWORD,LPVOID);
BOOL WINAPI GetFileVersionInfoA(LPCSTR file, DWORD ignored, DWORD len, LPVOID data) {
    FP_GetFileVersionInfoA f=(FP_GetFileVersionInfoA)resolve("GetFileVersionInfoA");
    return f ? f(file, ignored, len, data) : 0;
}

typedef BOOL (WINAPI *FP_GetFileVersionInfoByHandle)(DWORD,HANDLE,LPVOID*,DWORD*);
BOOL WINAPI GetFileVersionInfoByHandle(DWORD flags, HANDLE file, LPVOID* data, DWORD* len) {
    FP_GetFileVersionInfoByHandle f=(FP_GetFileVersionInfoByHandle)resolve("GetFileVersionInfoByHandle");
    return f ? f(flags, file, data, len) : 0;
}

typedef BOOL (WINAPI *FP_GetFileVersionInfoExA)(DWORD,LPCSTR,DWORD,DWORD,LPVOID);
BOOL WINAPI GetFileVersionInfoExA(DWORD flags, LPCSTR file, DWORD ignored, DWORD len, LPVOID data) {
    FP_GetFileVersionInfoExA f=(FP_GetFileVersionInfoExA)resolve("GetFileVersionInfoExA");
    return f ? f(flags, file, ignored, len, data) : 0;
}

typedef BOOL (WINAPI *FP_GetFileVersionInfoExW)(DWORD,LPCWSTR,DWORD,DWORD,LPVOID);
BOOL WINAPI GetFileVersionInfoExW(DWORD flags, LPCWSTR file, DWORD ignored, DWORD len, LPVOID data) {
    FP_GetFileVersionInfoExW f=(FP_GetFileVersionInfoExW)resolve("GetFileVersionInfoExW");
    return f ? f(flags, file, ignored, len, data) : 0;
}

typedef DWORD (WINAPI *FP_GetFileVersionInfoSizeA)(LPCSTR,DWORD*);
DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR file, DWORD* handle) {
    FP_GetFileVersionInfoSizeA f=(FP_GetFileVersionInfoSizeA)resolve("GetFileVersionInfoSizeA");
    return f ? f(file, handle) : 0;
}

typedef DWORD (WINAPI *FP_GetFileVersionInfoSizeExA)(DWORD,LPCSTR,DWORD*);
DWORD WINAPI GetFileVersionInfoSizeExA(DWORD flags, LPCSTR file, DWORD* handle) {
    FP_GetFileVersionInfoSizeExA f=(FP_GetFileVersionInfoSizeExA)resolve("GetFileVersionInfoSizeExA");
    return f ? f(flags, file, handle) : 0;
}

typedef DWORD (WINAPI *FP_GetFileVersionInfoSizeExW)(DWORD,LPCWSTR,DWORD*);
DWORD WINAPI GetFileVersionInfoSizeExW(DWORD flags, LPCWSTR file, DWORD* handle) {
    FP_GetFileVersionInfoSizeExW f=(FP_GetFileVersionInfoSizeExW)resolve("GetFileVersionInfoSizeExW");
    return f ? f(flags, file, handle) : 0;
}

typedef DWORD (WINAPI *FP_GetFileVersionInfoSizeW)(LPCWSTR,DWORD*);
DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR file, DWORD* handle) {
    FP_GetFileVersionInfoSizeW f=(FP_GetFileVersionInfoSizeW)resolve("GetFileVersionInfoSizeW");
    return f ? f(file, handle) : 0;
}

typedef BOOL (WINAPI *FP_GetFileVersionInfoW)(LPCWSTR,DWORD,DWORD,LPVOID);
BOOL WINAPI GetFileVersionInfoW(LPCWSTR file, DWORD ignored, DWORD len, LPVOID data) {
    FP_GetFileVersionInfoW f=(FP_GetFileVersionInfoW)resolve("GetFileVersionInfoW");
    return f ? f(file, ignored, len, data) : 0;
}

typedef DWORD (WINAPI *FP_VerFindFileA)(DWORD,LPCSTR,LPCSTR,LPCSTR,LPSTR,DWORD*,LPSTR,DWORD*);
DWORD WINAPI VerFindFileA(DWORD flags, LPCSTR filename, LPCSTR winDir, LPCSTR appDir, LPSTR curDir, DWORD* curDirLen, LPSTR destDir, DWORD* destDirLen) {
    FP_VerFindFileA f=(FP_VerFindFileA)resolve("VerFindFileA");
    return f ? f(flags, filename, winDir, appDir, curDir, curDirLen, destDir, destDirLen) : 0;
}

typedef DWORD (WINAPI *FP_VerFindFileW)(DWORD,LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,DWORD*,LPWSTR,DWORD*);
DWORD WINAPI VerFindFileW(DWORD flags, LPCWSTR filename, LPCWSTR winDir, LPCWSTR appDir, LPWSTR curDir, DWORD* curDirLen, LPWSTR destDir, DWORD* destDirLen) {
    FP_VerFindFileW f=(FP_VerFindFileW)resolve("VerFindFileW");
    return f ? f(flags, filename, winDir, appDir, curDir, curDirLen, destDir, destDirLen) : 0;
}

typedef DWORD (WINAPI *FP_VerInstallFileA)(DWORD,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPSTR,DWORD*);
DWORD WINAPI VerInstallFileA(DWORD flags, LPCSTR srcFile, LPCSTR dstFile, LPCSTR srcDir, LPCSTR dstDir, LPCSTR curDir, LPSTR tmpFile, DWORD* tmpFileLen) {
    FP_VerInstallFileA f=(FP_VerInstallFileA)resolve("VerInstallFileA");
    return f ? f(flags, srcFile, dstFile, srcDir, dstDir, curDir, tmpFile, tmpFileLen) : 0;
}

typedef DWORD (WINAPI *FP_VerInstallFileW)(DWORD,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,DWORD*);
DWORD WINAPI VerInstallFileW(DWORD flags, LPCWSTR srcFile, LPCWSTR dstFile, LPCWSTR srcDir, LPCWSTR dstDir, LPCWSTR curDir, LPWSTR tmpFile, DWORD* tmpFileLen) {
    FP_VerInstallFileW f=(FP_VerInstallFileW)resolve("VerInstallFileW");
    return f ? f(flags, srcFile, dstFile, srcDir, dstDir, curDir, tmpFile, tmpFileLen) : 0;
}

typedef DWORD (WINAPI *FP_VerLanguageNameA)(DWORD,LPSTR,DWORD);
DWORD WINAPI VerLanguageNameA(DWORD lang, LPSTR buf, DWORD bufLen) {
    FP_VerLanguageNameA f=(FP_VerLanguageNameA)resolve("VerLanguageNameA");
    return f ? f(lang, buf, bufLen) : 0;
}

typedef DWORD (WINAPI *FP_VerLanguageNameW)(DWORD,LPWSTR,DWORD);
DWORD WINAPI VerLanguageNameW(DWORD lang, LPWSTR buf, DWORD bufLen) {
    FP_VerLanguageNameW f=(FP_VerLanguageNameW)resolve("VerLanguageNameW");
    return f ? f(lang, buf, bufLen) : 0;
}

typedef BOOL (WINAPI *FP_VerQueryValueA)(LPCVOID,LPCSTR,LPVOID*,unsigned int*);
BOOL WINAPI VerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buf, unsigned int* len) {
    FP_VerQueryValueA f=(FP_VerQueryValueA)resolve("VerQueryValueA");
    return f ? f(block, subBlock, buf, len) : 0;
}

typedef BOOL (WINAPI *FP_VerQueryValueW)(LPCVOID,LPCWSTR,LPVOID*,unsigned int*);
BOOL WINAPI VerQueryValueW(LPCVOID block, LPCWSTR subBlock, LPVOID* buf, unsigned int* len) {
    FP_VerQueryValueW f=(FP_VerQueryValueW)resolve("VerQueryValueW");
    return f ? f(block, subBlock, buf, len) : 0;
}

#ifndef UFS_PASSTHROUGH_ONLY
static int uplink_window_exists(void) {
    HANDLE hwnd; DWORD pid=0;
    /* SDL 1.x uses SDL_app as its default Win32 window class.  Uplink sets
       the title to Uplink.  A real game window implies Steam initialization
       and the early asset/SDL initialization have completed. */
    hwnd=FindWindowA("SDL_app",0);
    if(hwnd && GetWindowThreadProcessId(hwnd,&pid) && pid==GetCurrentProcessId()) return 1;
    pid=0;
    hwnd=FindWindowA(0,"Uplink");
    return hwnd && GetWindowThreadProcessId(hwnd,&pid) && pid==GetCurrentProcessId();
}
static DWORD WINAPI load_patch(LPVOID unused) {
    char path[1024];DWORD n,i,attempt;
    (void)unused;
    log_msg("VERSION proxy v0.5 loaded; waiting for game window before patch");
    /* Creating an empty UplinkFileServer.safe alongside Uplink.exe enables
       forwarding-only diagnostic mode without touching the patch DLL. */
    n=GetModuleFileNameA(g_proxy,path,sizeof(path));
    if(n>0 && n<sizeof(path)) {
        HANDLE h;
        i=n;
        while(i && path[i-1]!='\\' && path[i-1]!='/')i--;
        path[i]=0;
        if(append(path,sizeof(path),"UplinkFileServer.safe")) {
            h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
            if(h!=INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                log_msg("SAFE MODE: patch disabled, version.dll forwarding only");
                return 0;
            }
        }
    }
    /* Do not modify game code during InitSteam: wait for actual game window.
       No patch is installed on failure to initialize Steam.  The worker
       intentionally gives up rather than applying a timed blind patch. */
    for (attempt=0; attempt<600; ++attempt) {
        if (uplink_window_exists()) break;
        Sleep(250);
    }
    if(attempt==600) {
        log_msg("No Uplink SDL window in 150 s; patch not loaded");
        return 0;
    }
    log_msg("Game window confirmed; loading patch after startup");
    Sleep(350);
    n=GetModuleFileNameA(g_proxy,path,sizeof(path));
    if(n==0 || n>=sizeof(path)){log_msg("Cannot locate proxy folder");return 0;}
    i=n;
    while(i && path[i-1]!='\\' && path[i-1]!='/')i--;
    path[i]=0;
    if(!append(path,sizeof(path),"UplinkFileServer.dll")){
        log_msg("Patch path too long");return 0;
    }
    if(LoadLibraryA(path))log_msg("Patch DLL loaded (check UplinkFileServer.log for PATCH ACTIVE)");
    else log_msg("Patch DLL load failed. Game can continue unpatched.");
    return 0;
}
#endif /* !UFS_PASSTHROUGH_ONLY */
BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==1) {
        g_proxy=module;
        DisableThreadLibraryCalls(module);
#ifndef UFS_PASSTHROUGH_ONLY
        { HANDLE h;DWORD tid;
          h=CreateThread(0,0,(LPVOID)load_patch,0,0,&tid);
          if(h)CloseHandle(h);
        }
#endif
    }
    return TRUE;
}
