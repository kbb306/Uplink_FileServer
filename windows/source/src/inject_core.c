/* Process injection function shared by optional manual injector and Steam auto launcher.
   Only the exact-game-hash checked DLL is loaded, the target game is not patched on disk. */
#include "win32_min.h"
#define TH32CS_SNAPPROCESS 2u
#define TH32CS_SNAPMODULE 8u
#define TH32CS_SNAPMODULE32 16u
#define INJECT_ACCESS (2u|8u|0x10u|0x20u|0x400u)
#define MEM_COMMIT_RESERVE 0x3000u
#define MEM_RELEASE 0x8000u
#define PAGE_READWRITE 4u
#define INFINITE 0xffffffffu
#define STD_OUTPUT_HANDLE ((DWORD)-11)
typedef struct {
 DWORD dwSize,cntUsage,th32ProcessID,th32DefaultHeapID,th32ModuleID,cntThreads,th32ParentProcessID;
 long pcPriClassBase;DWORD dwFlags;
 char szExeFile[260];
} PROCESSENTRY32A_LOCAL;
typedef struct {
 DWORD dwSize,th32ModuleID,th32ProcessID,GlblcntUsage,ProccntUsage;
 unsigned char *modBaseAddr;
 DWORD modBaseSize;
 HMODULE hModule;
 char szModule[256];
 char szExePath[260];
} MODULEENTRY32A_LOCAL;
static U32 nlen(const char *s) { U32 i=0; while(s[i])i++; return i; }
static int eq_casefold(const char *a,const char *b){
    for(;;){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;}
}
static U32 u32(const U8 *p){ return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24); }
static U32 module_size(U8 *m) {
    U32 pe;if(!m || m[0]!=0x4d || m[1]!=0x5a)return 0;
    pe=u32(m+0x3c);if(pe>0x1000 || u32(m+pe)!=0x4550)return 0;
    return u32(m+pe+24+56); /* OptionalHeader.SizeOfImage */
}
static LPVOID remote_load_library(DWORD pid, LPVOID localproc){
    HMODULE kern=GetModuleHandleA("kernel32.dll"),base=GetModuleHandleA("kernelbase.dll"),lib=kern;
    const char *name="kernel32.dll";
    HANDLE snap;MODULEENTRY32A_LOCAL m;LPVOID result=0;
    U32 sz=base ? module_size((U8*)base) : 0;
    if(base && sz && (U32)localproc>=(U32)base && (U32)localproc<(U32)base+sz){lib=base;name="kernelbase.dll";}
    if(!lib)return 0;
    snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snap==INVALID_HANDLE_VALUE)return 0;
    m.dwSize=sizeof(m);
    if(Module32FirstA(snap,&m)){
        do{if(eq_casefold(m.szModule,name)){
            result=(LPVOID)((U32)m.modBaseAddr+((U32)localproc-(U32)lib));
            break;
        }}while(Module32NextA(snap,&m));
    }
    CloseHandle(snap);return result;
}

/* Returns 0 once LoadLibraryA in Uplink completes; in-DLL patch success must be
   checked separately in UplinkFileServer.log. */
int UFS_Inject(DWORD pid, const char *path) {
    HANDLE proc,thread; LPVOID remote,loadlib; DWORD code=0,tid=0;
    SIZE_T written=0; U32 length=nlen(path);
    if (!pid || length==0 || length>1000) return 1;
    proc=OpenProcess(INJECT_ACCESS,FALSE,pid);
    if (!proc) return 2;
    remote=VirtualAllocEx(proc,0,length+1,MEM_COMMIT_RESERVE,PAGE_READWRITE);
    if(!remote){CloseHandle(proc);return 3;}
    if(!WriteProcessMemory(proc,remote,path,length+1,&written) || written!=length+1){
       VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);return 4;
    }
    loadlib=GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryA");
    if(loadlib)loadlib=remote_load_library(pid,loadlib);
    if(!loadlib){VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);return 5;}
    thread=CreateRemoteThread(proc,0,0,loadlib,remote,0,&tid);
    if(!thread){VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);return 6;}
    if(WaitForSingleObject(thread,15000)!=0) {
        /* Still executing: retain the path allocation until the game exits. */
        CloseHandle(thread);CloseHandle(proc);return 7;
    }
    if(!GetExitCodeThread(thread,&code) || !code){
        CloseHandle(thread);VirtualFreeEx(proc,remote,0,MEM_RELEASE);
        CloseHandle(proc);return 8;
    }
    CloseHandle(thread);VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);
    return 0;
}
