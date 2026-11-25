/* Uplink 1.55 Steam Launch Options wrapper.
   Usage: "C:\path\AutoUplinkFileServer.exe" %command%
   Starts the original game command, then loads the existing DLL automatically.
   Stays alive until Uplink exits so Steam can track the game session.
   Note: The DLL does not activate unless its EXE SHA-256 fingerprint matches. */
#include "win32_min.h"
#define INFINITE 0xffffffffu

typedef struct UFS_STARTUPINFOA {
    DWORD cb; LPSTR lpReserved,lpDesktop,lpTitle;
    DWORD dwX,dwY,dwXSize,dwYSize,dwXCountChars,dwYCountChars;
    DWORD dwFillAttribute,dwFlags;
    U16 wShowWindow,cbReserved2;
    U8* lpReserved2;
    HANDLE hStdInput,hStdOutput,hStdError;
} UFS_STARTUPINFOA;
typedef struct UFS_PROCESS_INFORMATION {
    HANDLE hProcess,hThread; DWORD dwProcessId,dwThreadId;
} UFS_PROCESS_INFORMATION;
int UFS_Inject(DWORD pid,const char *path);
static U32 nlen(const char *s){U32 n=0;while(s[n])n++;return n;}
static void logline(HANDLE h,const char *s){DWORD n;if(h && h!=INVALID_HANDLE_VALUE)WriteFile(h,s,nlen(s),&n,0);}
static void zero(void *p,U32 len){U8 *b=p;while(len--)*b++=0;}

/* Locate the part of a raw Win32 command line AFTER this wrapper's argv[0].
   The expanded %command% must not be reformatted, as Steam may append options. */
static const char *original_command(const char *s){
    int quotes=0; U32 i=0;
    while(s[i]==' ' || s[i]=='\t')i++;
    while(s[i]) {
        if(s[i]=='\\') {
            U32 j=i;while(s[j]=='\\')j++;
            if(s[j]=='"') {
                if(((j-i)&1u)==0)quotes=!quotes;
                i=j+1;continue;
            }
            i=j;continue;
        }
        if(s[i]=='"'){quotes=!quotes;i++;continue;}
        if(!quotes && (s[i]==' ' || s[i]=='\t'))break;
        i++;
    }
    while(s[i]==' ' || s[i]=='\t')i++;
    return s+i;
}
void mainCRTStartup(void){
    char apppath[1024],logpath[1024],dllpath[1024],cmd[4096];
    const char *tail, *dll="UplinkFileServer.dll", *log="AutoUplinkFileServer.log";
    DWORD len,j,exit_code=0;HANDLE logfile=INVALID_HANDLE_VALUE;
    UFS_PROCESS_INFORMATION game;
    UFS_STARTUPINFOA si;
    int inject_result;
    len=GetModuleFileNameA(0,apppath,sizeof(apppath));
    if(!len || len>=sizeof(apppath)-64)ExitProcess(2);
    while(len && apppath[len-1]!='\\' && apppath[len-1]!='/')len--;
    /* Copy absolute app-directory prefix. */
    for(j=0;j<len;j++){logpath[j]=apppath[j];dllpath[j]=apppath[j];}
    for(j=0;log[j];j++)logpath[len+j]=log[j];logpath[len+j]=0;
    for(j=0;dll[j];j++)dllpath[len+j]=dll[j];dllpath[len+j]=0;
    logfile=CreateFileA(logpath,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    logline(logfile,"Auto launcher 0.2: starting original Steam command\r\n");
    tail=original_command(GetCommandLineA());
    if(!*tail || nlen(tail)>=sizeof(cmd)){
        logline(logfile,"ERROR: No command from Steam. Launch options must be: ");
        logline(logfile,"\"FULL_PATH_TO_AutoUplinkFileServer.exe\" %command%\r\n");
        CloseHandle(logfile);ExitProcess(3);
    }
    for(j=0;tail[j];j++)cmd[j]=tail[j];cmd[j]=0;
    zero(&si,sizeof(si));zero(&game,sizeof(game));si.cb=sizeof(si);
    if(!CreateProcessA(0,cmd,0,0,FALSE,0,0,0,&si,&game)){
        logline(logfile,"ERROR: CreateProcess failed. Steam game was not started.\r\n");
        CloseHandle(logfile);ExitProcess(4);
    }
    CloseHandle(game.hThread);
    /* Allow SDL/Steam initialization before loading patch. By design no LAN
       should be opened during this initial loading period. */
    Sleep(2500);
    if(WaitForSingleObject(game.hProcess,0)!=0){
        inject_result=UFS_Inject(game.dwProcessId,dllpath);
        if(inject_result==0)
            logline(logfile,"LoadLibrary succeeded. Inspect UplinkFileServer.log for PATCH ACTIVE.\r\n");
        else
            logline(logfile,"ERROR: LoadLibrary did not complete. Game left running unmodified.\r\n");
    }else{
        logline(logfile,"ERROR: Game exited before DLL load could be attempted.\r\n");
    }
    /* Don't exit early: Steam should see an active game until Uplink quits. */
    WaitForSingleObject(game.hProcess,INFINITE);
    if(!GetExitCodeProcess(game.hProcess,&exit_code))exit_code=1;
    CloseHandle(game.hProcess);CloseHandle(logfile);
    ExitProcess(exit_code);
}
