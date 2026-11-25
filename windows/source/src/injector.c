/* Manual fallback injector: run while Uplink is at its main menu. */
#include "win32_min.h"
#define TH32CS_SNAPPROCESS 2u
#define STD_OUTPUT_HANDLE ((DWORD)-11)
typedef struct {
 DWORD dwSize,cntUsage,th32ProcessID,th32DefaultHeapID,th32ModuleID,cntThreads,th32ParentProcessID;
 long pcPriClassBase;DWORD dwFlags;char szExeFile[260];
} PROCESSENTRY32A_LOCAL;
int UFS_Inject(DWORD pid, const char *path);
static U32 nlen(const char *s){U32 i=0;while(s[i])i++;return i;}
static void say(const char *s) {DWORD n;HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);WriteFile(h,s,nlen(s),&n,0);}
static int eq_casefold(const char *a,const char *b){
  for(;;){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
   if(x!=y)return 0;if(!x)return 1;}
}
static void done(DWORD code){say("\r\n");ExitProcess(code);}
void mainCRTStartup(void){
  HANDLE snap;PROCESSENTRY32A_LOCAL e;DWORD pid=0,n;char path[1024];
  const char* tail="UplinkFileServer.dll";U32 i;
  say("Uplink FileServer 0.2 manual fallback (Steam Win32)\r\n");
  n=GetModuleFileNameA(0,path,sizeof(path));
  if(n<1||n>=sizeof(path)-32){say("Cannot locate program directory.\r\n");done(2);}
  while(n && path[n-1]!='\\' && path[n-1]!='/')n--;
  for(i=0;tail[i];i++)path[n+i]=tail[i];path[n+i]=0;
  snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
  if(snap==INVALID_HANDLE_VALUE){say("Cannot enumerate processes.\r\n");done(3);}
  e.dwSize=sizeof(e);
  if(Process32FirstA(snap,&e)){
    do{if(eq_casefold(e.szExeFile,"Uplink.exe")){pid=e.th32ProcessID;break;}}
    while(Process32NextA(snap,&e));
  }
  CloseHandle(snap);
  if(!pid){say("Uplink.exe not running. Start through Steam first.\r\n");done(4);}
  if(UFS_Inject(pid,path)!=0){say("LoadLibrary failed. See README, and check that the game is still running.\r\n");done(5);}
  say("DLL loaded; inspect UplinkFileServer.log for PATCH ACTIVE.\r\n");
  done(0);
}
