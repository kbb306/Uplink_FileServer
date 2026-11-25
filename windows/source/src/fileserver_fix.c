/*
 * UplinkFileServer 0.7C2 — populated volumes with the proven 0.7B2 reverse-index fix.
 * SPDX-License-Identifier: MIT. Not affiliated with Introversion.
 * Experimental and STATICALLY VERIFIED ONLY for the exact Steam 1.55 EXE.
 *
 * Retains the proven 0.6 FILESERVER parser and direct native browser route.
 * Data1 controls storage: -1/0 = shared, 1 = node-private, >=2 = group-private.
 * The original Uplink DataBank remains the save owner. Private volumes occupy
 * separate ranges guarded by native Data records stored in the existing .usr.
 * Native file-browser calls are filtered, native FindValidPlacement is fenced.
 * On first partition allocation, each private/group volume receives native
 * Uplink Data records; existing saved partitions are never re-populated.
 *
 * DO NOT test on a valuable agent. The global storage hook and use of native
 * constructors have NOT yet been exercised under Windows/Steam. DataBank::Format
 * and native data randomization are not supported with these partitions.
 */
#include "win32_min.h"

#define VERSION "0.7C2"
#define MAX_FILE 4096
#define MAX_CAND 64
#define RVA_ADDLANSYSTEM 0x00052DD0u
#define TYPE_MAINSERVER 5u
#define TYPE_FILESERVER 7u
#define RVA_PASSWORD_TITLE 0x0005383Au
#define RVA_FILE_TITLE 0x000539C6u
#define RVA_PASSWORD_NEXT 0x00053970u
#define ORIGINAL_MAIN_TITLE 0x00573884u
#define TITLE_INSN_LENGTH 5u
#define ORIGINAL_PASSWORD_NEXT 0x55u

static HMODULE g_dll;
static U8 *g_image;
typedef struct { U8 *address; U8 original; U32 length; U32 kind; } EML_SITE;
static EML_SITE g_emulate[3];
static const char g_file_title[] = "File Server";
static U8 *g_bp_address;
static U8 g_bp_original;
static U8 *g_call_address;
static U8 g_call_original[6];
static U32 g_call_length;
static DWORD g_tls = TLS_OUT_OF_INDEXES;
static volatile DWORD g_step_thread;
static HANDLE g_veh;
static char *(__cdecl *g_orig_strstr)(const char*, const char*);
static volatile U32 g_count_detected, g_count_converted;

/* The values below are mapped from the user's exact EXE, SHA-256
 * 837eb6465eb9538018f9b213bbd8d3f385fafd22182626ba6f21858223a34087.
 * The game address is image_base + RVA (ImageBase is 0x00400000).
 * 0x00433120 = row enumerator, not 0x00432120.
 * Four native browser calls all target this same function.
 * LanMonitor::SetCurrentSystem and ResetAll confirm the static globals. */
#define RVA_ROW_SCAN 0x00033120u
#define RVA_CURRENT_NODE 0x001DC36Cu
#define RVA_LAN_ACTIVE 0x001E8150u
#define RVA_LAN_PTR 0x001E8154u
#define NUM_ROW_CALLS 4u
static const U32 g_row_rva[NUM_ROW_CALLS]={0x000333EDu,0x000339ECu,0x00033FBAu,0x00034386u};
static U8 g_row_orig[NUM_ROW_CALLS][5];
static U32 g_row_installed;

/* MSVC passes five cdecl stack arguments and caller adjusts ESP by 20. */
typedef int (__cdecl *ROW_SCAN)(void*,int,void**,int*,int*);
static ROW_SCAN g_original_row_scan;

static int g_installed;

static U32 nlen(const char *s) { U32 n=0; if(!s) return 0; while(s[n]) ++n; return n; }
static int eq(const char *a, const char *b) {
    if(!a || !b) return 0;
    while(*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static int contains(const char *a, const char *b) {
    U32 i,j,n=nlen(a),m=nlen(b);
    if (!m) return 1;
    for(i=0;i+m<=n;i++) { for(j=0;j<m && a[i+j]==b[j];j++){} if(j==m) return 1; }
    return 0;
}
static void copy_bytes(void *d, const void *s, U32 n) { U32 i; for(i=0;i<n;i++) ((U8*)d)[i]=((const U8*)s)[i]; }
static U32 read_u32(const void *p) {U32 v; copy_bytes(&v,p,4); return v;}
static U16 read_u16(const void *p) {U16 v; copy_bytes(&v,p,2); return v;}
static void store_u32(void *p,U32 v) {copy_bytes(p,&v,4);}

/* Small logging facility, always next to the DLL. */
static void log_text(const char *s) {
    char path[1024]; DWORD n,w; HANDLE h;
    n=GetModuleFileNameA(g_dll,path,(DWORD)sizeof(path));
    if (!n || n>=sizeof(path)-24) return;
    while (n && path[n-1]!='\\' && path[n-1]!='/') --n;
    path[n]=0;
    { const char *tail="UplinkFileServer.log"; U32 i=0; while(tail[i]) {path[n+i]=tail[i];i++;} path[n+i]=0; }
    h=CreateFileA(path,GENERIC_WRITE,3,0,4,FILE_ATTRIBUTE_NORMAL,0); /* OPEN_ALWAYS=4 */
    if(h==INVALID_HANDLE_VALUE) return;
    SetFilePointer(h,0,0,2); /* FILE_END */
    WriteFile(h,s,nlen(s),&w,0);
    WriteFile(h,"\r\n",2,&w,0);
    CloseHandle(h);
}
static void hx(U32 v,char *out) {
    static const char d[]="0123456789ABCDEF"; U32 i;
    out[0]='0';out[1]='x';
    for(i=0;i<8;i++) out[2+i]=d[(v >> ((7-i)*4))&15];
    out[10]=0;
}
static void log_hex(const char *prefix, U32 v) {
    char b[256];U32 i=0,j=0;char h[11];hx(v,h);
    while(prefix[i] && i<200) {b[i]=prefix[i];i++;}
    while(h[j])b[i++]=h[j++];b[i]=0;log_text(b);
}

/* SHA-256 of the executable, to reject all unknown binary variants. */
typedef struct {U32 state[8]; U64 bytes; U8 block[64]; U32 used;} SHA;
static const U32 sha_k[64] = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
static U32 rr(U32 a,U32 b) {return (a>>b)|(a<<(32-b));}
static U32 be32(const U8 *p) {return ((U32)p[0]<<24)|((U32)p[1]<<16)|((U32)p[2]<<8)|p[3];}
static void setbe32(U8 *p,U32 v) {p[0]=(U8)(v>>24);p[1]=(U8)(v>>16);p[2]=(U8)(v>>8);p[3]=(U8)v;}
static void sha_block(SHA *c,const U8* in) {
    U32 w[64], a,b,d,e,f,g,h,cc,t1,t2,i;
    for(i=0;i<16;i++) w[i]=be32(in+4*i);
    for(i=16;i<64;i++) {U32 x=w[i-15],y=w[i-2];w[i]=(rr(y,17)^rr(y,19)^(y>>10))+w[i-7]+(rr(x,7)^rr(x,18)^(x>>3))+w[i-16];}
    a=c->state[0];b=c->state[1];cc=c->state[2];d=c->state[3];e=c->state[4];f=c->state[5];g=c->state[6];h=c->state[7];
    for(i=0;i<64;i++) {
       t1=h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+sha_k[i]+w[i];
       t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&cc)^(b&cc));
       h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->state[0]+=a;c->state[1]+=b;c->state[2]+=cc;c->state[3]+=d;
    c->state[4]+=e;c->state[5]+=f;c->state[6]+=g;c->state[7]+=h;
}
static void sha_init(SHA *c) {
    static const U32 init[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    U32 i;for(i=0;i<8;i++)c->state[i]=init[i];c->bytes=0;c->used=0;
}
static void sha_update(SHA *c,const U8 *p,U32 len) {
    U32 i;for(i=0;i<len;i++) {c->block[c->used++]=p[i];if(c->used==64){sha_block(c,c->block);c->used=0;}}
    c->bytes+=len;
}
static void sha_final(SHA *c,U8 out[32]) {
    U64 bits=c->bytes*8;U32 i;
    c->block[c->used++]=0x80;
    if(c->used>56){while(c->used<64)c->block[c->used++]=0;sha_block(c,c->block);c->used=0;}
    while(c->used<56)c->block[c->used++]=0;
    for(i=0;i<8;i++)c->block[56+i]=(U8)(bits>>(56-8*i));
    sha_block(c,c->block);
    for(i=0;i<8;i++)setbe32(out+4*i,c->state[i]);
}
static int verify_exe_hash(void) {
    static const U8 expected[32]={
      0x83,0x7e,0xb6,0x46,0x5e,0xb9,0x53,0x80,0x18,0xf9,0xb2,0x13,0xbb,0xd8,0xd3,0xf3,
      0x85,0xfa,0xfd,0x22,0x18,0x26,0x26,0xba,0x6f,0x21,0x85,0x82,0x23,0xa3,0x40,0x87};
    char path[1024]; U8 buf[4096],hash[32]; SHA sh;HANDLE file;DWORD n,r;U32 i;
    n=GetModuleFileNameA(0,path,sizeof(path)); if(!n || n>=sizeof(path))return 0;
    file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE)return 0;
    sha_init(&sh);
    do {r=0;if(!ReadFile(file,buf,sizeof(buf),&r,0)){CloseHandle(file);return 0;}if(r)sha_update(&sh,buf,r);}while(r);
    CloseHandle(file);sha_final(&sh,hash);
    for(i=0;i<32;i++)if(hash[i]!=expected[i]){
        static const char *d="0123456789abcdef";char msg[90];U32 j=0,k;
        const char *prefix="EXE SHA-256 mismatch: ";while(prefix[j]){msg[j]=prefix[j];j++;}
        for(k=0;k<32;k++){msg[j++]=d[hash[k]>>4];msg[j++]=d[hash[k]&15];}msg[j]=0;log_text(msg);
        return 0;
    }
    log_text("EXE SHA-256 accepted: Steam 1.55 exact binary");return 1;
}

typedef struct {U8 *code;U32 target;} CALLSITE;
static CALLSITE g_router[MAX_CAND],g_hub[MAX_CAND],g_main[MAX_CAND];
static U32 g_nr,g_nh,g_nm;
static U8 *g_text;static U32 g_text_len;static U8 *g_rdata;static U32 g_rdata_len;
static int is_section(const U8 *p,const char *name) {
    U32 i;for(i=0;i<8;i++){if(!name[i])return p[i]==0; if(p[i]!=(U8)name[i])return 0;}return 1;
}
static int parse_image(void) {
    U8 *p=(U8*)GetModuleHandleA(0);U32 pe,sections,opt_size,i;U8 *sh;
    if(!p || read_u16(p)!=0x5a4d)return 0;
    pe=read_u32(p+0x3c);if(pe>0x1000 || read_u32(p+pe)!=0x4550)return 0;
    if(read_u16(p+pe+4)!=0x14c || read_u16(p+pe+24)!=0x10b)return 0;
    sections=read_u16(p+pe+6); opt_size=read_u16(p+pe+20);
    if(!sections || sections>96)return 0;
    sh=p+pe+24+opt_size;
    for(i=0;i<sections;i++) {
        U8 *s=sh+i*40;U32 va=read_u32(s+12),sz=read_u32(s+8);
        if(is_section(s,".text")){g_text=p+va;g_text_len=sz;}
        if(is_section(s,".rdata")){g_rdata=p+va;g_rdata_len=sz;}
    }
    if(!g_text || !g_rdata || g_text_len<4096 || g_rdata_len<128)return 0;
    g_image=p;log_hex("image base ",(U32)p);return 1;
}
static int find_token(const char *needle,U8 **out) {
    U32 i,j,len=nlen(needle),num=0;
    for(i=0;i+len+1<=g_rdata_len;i++){
        if(g_rdata[i]){for(j=0;j<len && g_rdata[i+j]==(U8)needle[j];j++){}if(j==len && g_rdata[i+len]==0){*out=g_rdata+i;num++;}}
    }
    return num==1;
}
static void find_calls(const char *needle,CALLSITE *out,U32 *total) {
    U8 *str;U32 i,j,addr;
    *total=0;
    if(!find_token(needle,&str)){log_text("missing or ambiguous LAN token string");return;}
    addr=(U32)str;
    for(i=0;i+5<g_text_len;i++) {
        if(g_text[i]!=0x68 || read_u32(g_text+i+1)!=addr)continue;
        /* The tokenizer puts the literal on the stack, then calls strstr.
           Support direct call rel32 and imported call [absolute IAT]. */
        for(j=i+5;j<i+28 && j+6<g_text_len;j++) {
            U32 target=0;
            if(g_text[j]==0xE8) {
                target=(U32)(g_text+j+5)+(U32)read_u32(g_text+j+1);
            } else if(g_text[j]==0xff && g_text[j+1]==0x15) {
                U32 thunk=read_u32(g_text+j+2);
                /* Import thunk must point into the process's EXE image.
                   Only accept the common 0x400000--0x10000000 image range. */
                if(thunk<(U32)g_image || thunk>=(U32)g_image+0x1000000)continue;
                target=read_u32((void*)thunk);
            } else continue;
            if(*total<MAX_CAND) {out[*total].code=g_text+j;out[*total].target=target;}
            (*total)++;
            break;
        }
    }
}
static int discover_site(void) {
    U32 i,j,k;CALLSITE selected;U32 n=0;
    find_calls("ROUTER",g_router,&g_nr);
    find_calls("HUB",g_hub,&g_nh);
    find_calls("MAINSERVER",g_main,&g_nm);
    log_hex("ROUTER call candidates ",g_nr);
    log_hex("HUB call candidates ",g_nh);
    log_hex("MAINSERVER call candidates ",g_nm);
    if(g_nr>MAX_CAND || g_nh>MAX_CAND || g_nm>MAX_CAND)return 0;
    for(i=0;i<g_nm;i++) for(j=0;j<g_nr;j++) for(k=0;k<g_nh;k++) {
        U32 a=(U32)g_main[i].code,b=(U32)g_router[j].code,c=(U32)g_hub[k].code;
        if(g_main[i].target != g_router[j].target || g_main[i].target != g_hub[k].target)continue;
        if(a>b+8192 || b>a+8192 || a>c+8192 || c>a+8192)continue;
        selected=g_main[i];n++;
    }
    if(n!=1){log_hex("ambiguous parser calls; safe abort, matches=",n);return 0;}
    if(!(selected.code[0]==0xe8 || (selected.code[0]==0xff && selected.code[1]==0x15)))return 0;
    g_call_address=selected.code;
    g_call_length=(selected.code[0]==0xff)?6:5;
    g_orig_strstr=(char*(__cdecl *)(const char*,const char*))selected.target;
    log_hex("parser MAINSERVER call @ ",(U32)g_call_address);
    log_hex("native strstr target @ ",(U32)g_orig_strstr);
    return 1;
}
static int patch_mem(U8 *site,const U8 *bytes,U32 size) {
    DWORD old,unused;if(!VirtualProtect(site,size,PAGE_EXECUTE_READWRITE,&old))return 0;
    copy_bytes(site,bytes,size);FlushInstructionCache(GetCurrentProcess(),site,size);
    VirtualProtect(site,size,old,&unused);return 1;
}

/*
 * 0.7B private volumes: non-overlapping ranges in the native LAN databank.
 * Data1: -1/0 = root shared, 1 = per-node private, >=2 = group-private.
 * Each private volume is bounded by native Data records (size 1), preserving
 * the original save layout; the existing file copier still writes absolute
 * memory indexes. The marker records are hidden by the browser filter.
 *
 * Experiment ONLY; do not enable on permanent game saves. Formatting the
 * shared LAN bank clears all marker files and invalidates private volumes.
 */
#define MAX_VOLUMES 64
#define PRIVATE_CAPACITY 100
#define MAX_BANK_SIZE 20000
#define RVA_BANK_SETSIZE 0x00022570u
#define RVA_BANK_GETSIZE 0x00022580u
#define RVA_BANK_GETDATASIZE 0x000B3F00u
#define RVA_BANK_GETDATAFILE 0x00022EE0u
#define RVA_BANK_PUTAT 0x000225C0u
#define RVA_BANK_ISVALID 0x00022660u
#define RVA_BANK_FINDVALID 0x000226E0u
#define RVA_DATA_CTOR 0x00022230u
#define RVA_DATA_SETTITLE 0x00022270u
#define RVA_NATIVE_NEW 0x0013914Eu
#define BANK_OFFSET 0xF0u
#define SYS_DARRAY_OFFSET 0x168u
#define TYPE_MAIN 5
#define TYPE_FILE 7
#define NATIVE_DATA_SIZE 0x5Cu
#define DATA_TYPE 0x44u
#define DATA_SIZE 0x48u
#define DATA_TITLE 4u
#define DATA_ENCRYPTED 0x4Cu
#define DATA_COMPRESSED 0x50u
#define SYS_TYPE 4u
#define SYS_DATA1 0x24u
#define COMPUTER_COMPANY_NAME 0x54u

typedef void (__attribute__((thiscall)) *BANK_SET_SIZE)(void*,int);
typedef int (__attribute__((thiscall)) *BANK_GET_SIZE)(void*);
typedef int (__attribute__((thiscall)) *BANK_GET_DATA_SIZE)(void*);
typedef void* (__attribute__((thiscall)) *BANK_GET_DATA_FILE)(void*,int);
typedef void (__attribute__((thiscall)) *BANK_PUT_AT)(void*,void*,int);
typedef int (__attribute__((thiscall)) *BANK_IS_VALID)(void*,void*,int);
typedef void* (__attribute__((thiscall)) *DATA_CTOR)(void*);
typedef void (__attribute__((thiscall)) *DATA_SET_TITLE)(void*,const char*);
typedef void* (__cdecl *NATIVE_NEW)(U32);

static BANK_SET_SIZE n_set_size;
static BANK_GET_SIZE n_get_size;
static BANK_GET_DATA_SIZE n_get_data_size;
static BANK_GET_DATA_FILE n_get_data_file;
static BANK_PUT_AT n_put_at;
static BANK_IS_VALID n_is_valid;
static DATA_CTOR n_data_ctor;
static DATA_SET_TITLE n_set_title;
static NATIVE_NEW n_new;
static U8 g_find_original[5];
static U32 g_find_installed;
static U32 g_log_lan, g_log_bank;
static int g_log_node=-1;

typedef struct { char mode; U32 key; int begin, end; } VOL;
typedef struct { VOL vol[MAX_VOLUMES]; int num, root_end, active_start, active_end; int state; } LAYOUT;

static char hex_digit(U32 v) { return "0123456789ABCDEF"[v & 15u]; }
static void marker_name(char name[32],char boundary, char mode,U32 key) {
    U32 i; const char *prefix="UFS2";
    for(i=0;i<4;i++)name[i]=prefix[i];
    name[4]='.';name[5]=boundary;name[6]='.';name[7]=mode;
    for(i=0;i<8;i++)name[8+i]=hex_digit(key>>((7-i)*4));
    name[16]=0;
}
static int native_bank_signature(void) {
    static const struct {U32 rva; U8 sig[4];} signatures[]={
        {RVA_BANK_SETSIZE,{0x83,0xC1,0x14,0xE9}},
        {RVA_BANK_GETSIZE,{0x8B,0x41,0x18,0xC3}},
        {RVA_BANK_GETDATASIZE,{0x8B,0x41,0x08,0xC3}},
        {RVA_DATA_CTOR,{0x56,0x8B,0xF1,0xE8}},
        {RVA_BANK_GETDATAFILE,{0x8B,0x44,0x24,0x04}},
        {RVA_BANK_ISVALID,{0x56,0x8B,0x74,0x24}},
        {RVA_BANK_FINDVALID,{0x53,0x8B,0x5C,0x24}},
        {RVA_BANK_PUTAT,{0x51,0x55,0x56,0x57}}
    };
    U32 i,j;
    for(i=0;i<sizeof(signatures)/sizeof(signatures[0]);i++)
        for(j=0;j<4;j++)if(g_image[signatures[i].rva+j]!=signatures[i].sig[j]) {
            log_hex("DISKS DISABLED: native method fingerprint mismatch RVA ",signatures[i].rva);
            return 0;
        }
    n_set_size=(BANK_SET_SIZE)(g_image+RVA_BANK_SETSIZE);
    n_get_size=(BANK_GET_SIZE)(g_image+RVA_BANK_GETSIZE);
    n_get_data_size=(BANK_GET_DATA_SIZE)(g_image+RVA_BANK_GETDATASIZE);
    n_get_data_file=(BANK_GET_DATA_FILE)(g_image+RVA_BANK_GETDATAFILE);
    n_put_at=(BANK_PUT_AT)(g_image+RVA_BANK_PUTAT);
    n_is_valid=(BANK_IS_VALID)(g_image+RVA_BANK_ISVALID);
    n_data_ctor=(DATA_CTOR)(g_image+RVA_DATA_CTOR);
    n_set_title=(DATA_SET_TITLE)(g_image+RVA_DATA_SETTITLE);
    n_new=(NATIVE_NEW)(g_image+RVA_NATIVE_NEW);
    return 1;
}
/* The DArray layout and the type-7 node offsets were individually checked
 * against the verified Steam binary. Never dereference unbounded indexes. */
static void* get_lan_system(U32 lan,int index) {
    U8 *base=(U8*)lan+SYS_DARRAY_OFFSET;
    int count=(int)read_u32(base+4);
    U32 ptrs=read_u32(base+8),valid=read_u32(base+12);
    if(index<0 || index>=count || count>4096 || !ptrs || !valid)return 0;
    if(((U8*)valid)[index]!=1)return 0;
    return (void*)read_u32((U8*)ptrs+index*4);
}
static int get_current(U32 *lanout,int *nodeout,U32 bank) {
    U32 lan; int node;
    if(!g_image || !*(volatile U8*)(g_image+RVA_LAN_ACTIVE))return 0;
    lan=read_u32(g_image+RVA_LAN_PTR);
    node=(int)read_u32(g_image+RVA_CURRENT_NODE);
    if(!lan || node<0 || node>4095 || bank!=lan+BANK_OFFSET)return 0;
    if(!get_lan_system(lan,node))return 0;
    *lanout=lan;*nodeout=node;return 1;
}
static int gather_volumes(U32 lan,LAYOUT *l) {
    int i,j,count=(int)read_u32((void*)(lan+SYS_DARRAY_OFFSET+4));
    l->num=0;
    if(count<0 || count>4096)return 0;
    for(i=0;i<count;i++){
        void *s=get_lan_system(lan,i);int mode;
        char group;U32 key;
        if(!s || (int)read_u32((U8*)s+SYS_TYPE)!=TYPE_FILE)continue;
        mode=(int)read_u32((U8*)s+SYS_DATA1);
        if(mode<=0)continue;
        group=(mode==1)?'P':'G';key=(mode==1)?(U32)i:(U32)mode;
        for(j=0;j<l->num;j++)if(l->vol[j].mode==group && l->vol[j].key==key)break;
        if(j<l->num)continue;
        if(l->num>=MAX_VOLUMES)return 0;
        l->vol[l->num].mode=group;l->vol[l->num].key=key;
        l->vol[l->num].begin=-1;l->vol[l->num].end=-1;
        l->num++;
    }
    return 1;
}
/*
 * Reverse lookup for a native databank file slot.
 *
 * IMPORTANT: 0x00422F10 was misidentified by 0.7B1 as GetMemoryIndex.
 * The original instructions at that address implement GetDataIndex:
 * given a MEMORY index, they return the stored DATA index. Passing a data
 * index there yielded small, nonsensical memory positions (1, 2, 2, 2),
 * causing layout validation error -42 and a blank browser.
 *
 * DataBank's memory DArray starts at +0x14; size is +0x18,
 * values pointer +0x1c, validity-byte pointer +0x20. These offsets are
 * independently visible in Steam EXE DataBank::PutData (0x004225C0),
 * GetSize (0x00422580), and GetDataIndex (0x00422F10).
 * A free-space block may have no data slot; only file records use this.
 */
#define BANK_MEM_DATA_PTR 0x1Cu
#define BANK_MEM_VALID_PTR 0x20u
static int file_slot_to_memory_index(void *bank,int dataindex) {
    U8 *b=(U8*)bank;
    int i,n;
    U32 ptr,valid;
    if(!bank || dataindex<0)return -1;
    n=n_get_size(bank);
    if(n<0 || n>MAX_BANK_SIZE || dataindex>=n_get_data_size(bank))return -1;
    ptr=read_u32(b+BANK_MEM_DATA_PTR);
    valid=read_u32(b+BANK_MEM_VALID_PTR);
    if(n && (!ptr || !valid))return -1;
    for(i=0;i<n;i++){
        if(((U8*)valid)[i] && (int)read_u32((U8*)ptr+(U32)i*4u)==dataindex)
            return i;
    }
    return -1;
}

static int is_marker(void *obj) {
    const char *p;
    if(!obj)return 0;
    p=(const char*)((U8*)obj+DATA_TITLE);
    return read_u32((U8*)obj+DATA_TYPE)==1u &&
           read_u32((U8*)obj+DATA_SIZE)==1u &&
           read_u32((U8*)obj+DATA_ENCRYPTED)==0u &&
           read_u32((U8*)obj+DATA_COMPRESSED)==0u &&
           p[0]=='U' && p[1]=='F' && p[2]=='S' && p[3]=='2' && p[4]=='.';
}
/* Markers are real Uplink Data objects and are saved by native databank Save.
 * We never touch other Data1/2/3 values or add new sections to a .usr file. */
/* Read back actual native marker records; the first 0.7B build trusted its
 * intended positions after allocation, which was not a valid verification. */
static int find_markers(U32 bank,LAYOUT *l) {
    int i,j,total=n_get_data_size((void*)bank),found=0;
    if(total<0 || total>MAX_BANK_SIZE)return -1;
    for(i=0;i<total;i++){
        void *f=n_get_data_file((void*)bank,i);
        const char *name;int pos,match=0;
        if(!f || !is_marker(f))continue;
        found++;
        name=(const char*)((U8*)f+DATA_TITLE);
        pos=file_slot_to_memory_index((void*)bank,i);
        if(pos<0)return -2;  /* Marker exists but has no memory location. */
        for(j=0;j<l->num;j++){
            char candidate[32];
            marker_name(candidate,'B',l->vol[j].mode,l->vol[j].key);
            if(eq(name,candidate)){
                if(l->vol[j].begin!=-1)return -3;  /* Duplicate beginning. */
                l->vol[j].begin=pos;match=1;break;
            }
            marker_name(candidate,'E',l->vol[j].mode,l->vol[j].key);
            if(eq(name,candidate)){
                if(l->vol[j].end!=-1)return -4;  /* Duplicate ending. */
                l->vol[j].end=pos;match=1;break;
            }
        }
        if(!match)return -5; /* Unexpected partition metadata. */
    }
    return found;
}
/* Diagnostic only: inspect native Data records without mutating them.
 * Log at most once per browser failure, not once per draw call. */
static void log_marker_inventory(U32 bank,LAYOUT *l) {
    int i,n,total=n_get_data_size((void*)bank),hits=0;
    log_hex("DISKS DIAG: native bank size ",(U32)n_get_size((void*)bank));
    log_hex("DISKS DIAG: native file slots ",(U32)total);
    log_hex("DISKS DIAG: configured volumes ",(U32)l->num);
    if(total<0 || total>MAX_BANK_SIZE)return;
    for(i=0;i<l->num && i<6;i++){
        char target[32];
        marker_name(target,'B',l->vol[i].mode,l->vol[i].key);
        log_text(target);
        log_hex("DISKS DIAG: expected begin ",(U32)l->vol[i].begin);
        marker_name(target,'E',l->vol[i].mode,l->vol[i].key);
        log_text(target);
        log_hex("DISKS DIAG: expected end ",(U32)l->vol[i].end);
    }
    for(i=0;i<total && hits<16;i++) {
        void *f=n_get_data_file((void*)bank,i);
        const char *p;
        if(!f)continue;
        p=(const char*)((U8*)f+DATA_TITLE);
        if(p[0]!='U' || p[1]!='F' || p[2]!='S' || p[3]!='2' || p[4]!='.')continue;
        hits++;
        log_hex("DISKS DIAG: marker file index ",(U32)i);
        log_hex("DISKS DIAG: marker memory index ",(U32)file_slot_to_memory_index((void*)bank,i));
        log_hex("DISKS DIAG: marker type ",read_u32((U8*)f+DATA_TYPE));
        log_hex("DISKS DIAG: marker size ",read_u32((U8*)f+DATA_SIZE));
        log_hex("DISKS DIAG: marker encryption ",read_u32((U8*)f+DATA_ENCRYPTED));
        log_hex("DISKS DIAG: marker compression ",read_u32((U8*)f+DATA_COMPRESSED));
        {char name[40];for(n=0;n<31 && p[n];n++)name[n]=p[n];name[n]=0;log_text(name);}
    }
    log_hex("DISKS DIAG: UFS2 marker candidates ",(U32)hits);
}

static void *new_marker(const char *name) {
    void *p=n_new(NATIVE_DATA_SIZE);
    if(!p)return 0;
    n_data_ctor(p);n_set_title(p,name);
    store_u32((U8*)p+DATA_TYPE,1u);
    store_u32((U8*)p+DATA_SIZE,1u);
    return p;
}
/* Match the original LAN generator's *shape* (company-based data/prog names,
 * variable file sizes, occasional encryption/compression), without relying on
 * addresses for its C++ NameGenerator/NumberGenerator methods.  Files are real
 * Data objects, so native copy, delete, and save/load continue to recognize
 * them.  This is not a byte-for-byte call to the game's random generator. */
static U32 pop_rng(U32 *s) {
    U32 x=*s;
    if(!x)x=0x9E3779B9u;
    x^=x<<13;x^=x>>17;x^=x<<5;
    *s=x;
    return x;
}
static U32 pop_random(U32 *seed,U32 range) {
    if(!range)return 0;
    return pop_rng(seed)%range;
}
static U32 initial_seed(U32 lan,const VOL *v) {
    U32 seed=0x9E3779B9u ^ GetTickCount() ^ (U32)v->key * 0x85EBCA6Bu;
    const U8 *name=(const U8*)(lan+COMPUTER_COMPANY_NAME);
    U32 i;
    for(i=0;i<64 && name[i];i++)seed=(seed^name[i])*16777619u;
    seed^=(v->mode=='G')?0xB5297A4Du:0x68E31DA4u;
    if(seed==0)seed=0x4C957F2Du;
    return seed;
}
static void pop_name(char *title,U32 limit,U32 lan,int kind,U32 file_num) {
    const U8 *company=(const U8*)(lan+COMPUTER_COMPANY_NAME);
    const char *middle=(kind==1)?"-data-":"-prog-";
    U32 i=0,j,k;
    if(limit<18)return;
    for(j=0;j<3;j++) {
        U8 c=company[j];
        title[i++]=(c>=32 && c<127)?(char)c:'X';
    }
    for(j=0;middle[j];j++)title[i++]=middle[j];
    /* 0 <= file_num < 99999: the original naming scheme prints decimal. */
    {char digits[7];j=0;
      do {digits[j++]=(char)('0'+file_num%10u);file_num/=10u;} while(file_num && j<6);
      while(j){k=j-1;title[i++]=digits[k];j--;}
    }
    title[i]=0;
}
/* Generate only within this volume.  Never call FindValidPlacement here:
 * that hook interprets an absent browser as the shared/root bank. */
static int populate_volume(U32 lan,U32 bank,const VOL *v) {
    U32 rng=initial_seed(lan,v);
    int requested=7+(int)pop_random(&rng,6u),inserted=0;
    int i,first=v->begin+1,last=v->end,at=first;
    U32 suffix_base=pop_random(&rng,99999u);
    for(i=0;i<requested;i++) {
        int size=2+(int)pop_random(&rng,5u)+(int)pop_random(&rng,4u);
        int type=1+(int)pop_random(&rng,2u),encrypted,compressed;
        U32 file_num=(suffix_base+(U32)i*7919u)%99999u;
        int pos,found=-1;
        char title[32];
        void *file;
        encrypted=pop_random(&rng,2u)?(int)pop_random(&rng,5u):0;
        compressed=pop_random(&rng,2u)?(int)pop_random(&rng,5u):0;
        if(at+size>last)break;
        file=n_new(NATIVE_DATA_SIZE);
        if(!file){log_text("POPULATE ERROR: native Data allocation failed");break;}
        n_data_ctor(file);
        pop_name(title,sizeof(title),lan,type,file_num);
        n_set_title(file,title);
        store_u32((U8*)file+DATA_TYPE,(U32)type);
        store_u32((U8*)file+DATA_SIZE,(U32)size);
        store_u32((U8*)file+DATA_ENCRYPTED,(U32)encrypted);
        store_u32((U8*)file+DATA_COMPRESSED,(U32)compressed);
        /* Data::Data() supplies version 1.0 and SOFTWARETYPE_NONE. */
        /* Call native placement rules on a fully constructed Data object. */
        for(pos=at;pos+size<=last;pos++) {
            if(n_is_valid((void*)bank,file,pos)==0){found=pos;break;}
        }
        if(found<0){
            log_text("POPULATE: disk full or occupied before requested file count");
            /* One unplaced native Data allocation is intentionally leaked
               instead of risking a mismatched C++ deallocation ABI. */
            break;
        }
        n_put_at((void*)bank,file,found);
        at=found+size;
        inserted++;
    }
    log_hex("POPULATE: volume mode ",(U32)(unsigned char)v->mode);
    log_hex("POPULATE: volume key ",v->key);
    log_hex("POPULATE: native files created ",(U32)inserted);
    return inserted;
}

/* Only allocate on the game thread when a file-browser row is requested.
 * If saved markers exist, never resize or rewrite the bank. */
static int initialize_volumes(U32 bank,LAYOUT *l) {
    int oldsize=n_get_size((void*)bank),i,pos,groups=l->num;
    int newsize;
    if(groups<=0 || groups>MAX_VOLUMES || oldsize<=0 || oldsize>12000)return 0;
    if(*(U8*)(bank+0x24u)) {log_text("DISKS: databank formatted; refusing to allocate partitions over a wiped LAN");return 0;}
    if(groups*(PRIVATE_CAPACITY+2)>MAX_BANK_SIZE-oldsize)return 0;
    newsize=oldsize+groups*(PRIVATE_CAPACITY+2);
    /* SetSize retains all the original native files/memory allocation table. */
    n_set_size((void*)bank,newsize);
    if(n_get_size((void*)bank)!=newsize){log_text("DISKS ERROR: native SetSize verification failed");return 0;}
    pos=oldsize;
    for(i=0;i<groups;i++){
        char name[32];void *b,*e;
        marker_name(name,'B',l->vol[i].mode,l->vol[i].key);
        b=new_marker(name);
        marker_name(name,'E',l->vol[i].mode,l->vol[i].key);
        e=new_marker(name);
        if(!b || !e) {log_text("DISKS ERROR: could not allocate volume marker");return 0;}
        n_put_at((void*)bank,b,pos);l->vol[i].begin=pos;
        pos+=1+PRIVATE_CAPACITY;
        n_put_at((void*)bank,e,pos);l->vol[i].end=pos;
        pos++;
    }
    l->root_end=oldsize;
    log_hex("DISKS: native databank expanded; original main disk size ",(U32)oldsize);
    log_hex("DISKS: private/group volume count ",(U32)groups);
    return 1;
}
static int valid_layout(U32 bank,LAYOUT *l) {
    int i,j,min_begin=0x7fffffff;
    int bank_size=n_get_size((void*)bank);
    if(bank_size<=0 || bank_size>MAX_BANK_SIZE)return -1;
    for(i=0;i<l->num;i++){
        VOL *v=&l->vol[i];
        if(v->begin<1 || v->end!=v->begin+PRIVATE_CAPACITY+1 || v->end>=bank_size)return -2;
        if(v->begin<min_begin)min_begin=v->begin;
        for(j=0;j<i;j++){
            VOL *w=&l->vol[j];
            if(v->begin<=w->end && w->begin<=v->end)return -3;
        }
    }
    l->root_end=min_begin;
    return 1;
}
static int g_diag_error_latched;
/* l->state = 1: configured, 0: no private volumes, -1: fail closed. */
static int resolve_layout(U32 lan,U32 bank,int node,LAYOUT *l,int may_create) {
    void *sys=get_lan_system(lan,node);
    int mode,found,i,expected,integrity,newly_created=0;
    if(!sys)return -11;
    mode=(int)read_u32((U8*)sys+SYS_DATA1);
    if(!gather_volumes(lan,l))return -12;
    if(!l->num){
        int j,total=n_get_data_size((void*)bank);
        if(total<0 || total>MAX_BANK_SIZE)return -13;
        for(j=0;j<total;j++){void *f=n_get_data_file((void*)bank,j);if(f && is_marker(f))return -14;}
        l->state=0;return 0;
    }
    found=find_markers(bank,l);
    if(found<0)return found-20;
    expected=l->num*2;
    if(found==0 && may_create) {
        if(!initialize_volumes(bank,l))return -15;
        newly_created=1;
        /* Verify actual data records before reporting first successful row.
         * Do not allocate again if our marker writes did not persist. */
        for(i=0;i<l->num;i++){l->vol[i].begin=-1;l->vol[i].end=-1;}
        found=find_markers(bank,l);
        if(found<0)return found-30;
        if(found!=expected)return -16;
        log_text("DISKS: native partition markers verified immediately after allocation");
        for(i=0;i<l->num && i<6;i++){
            log_hex("DISKS: verified begin marker memory index ",(U32)l->vol[i].begin);
            log_hex("DISKS: verified end marker memory index ",(U32)l->vol[i].end);
        }
    }
    if(found!=expected)return -17;
    integrity=valid_layout(bank,l);
    if(integrity<0)return integrity-40;
    /* All disk boundaries have been recovered with the CORRECT native
     * memory-table reverse lookup and passed layout validation.  Only then
     * create content, only for partitions first allocated in this call.
     * Existing 0.7B2 volumes and previously saved player edits stay intact. */
    if(newly_created) {
        for(i=0;i<l->num;i++)
            (void)populate_volume(lan,bank,&l->vol[i]);
        log_text("POPULATE: first-time generation complete; marker layout recheck");
        for(i=0;i<l->num;i++){l->vol[i].begin=-1;l->vol[i].end=-1;}
        found=find_markers(bank,l);
        if(found!=expected || valid_layout(bank,l)<0){
            log_text("POPULATE ERROR: disk boundary check failed after writing files");
            return -60;
        }
        log_text("POPULATE: marker layout intact after native file placement");
    }
    if(mode<=0 || (int)read_u32((U8*)sys+SYS_TYPE)==TYPE_MAIN) {
        l->active_start=0;l->active_end=l->root_end;
    } else {
        char marker=(mode==1)?'P':'G';U32 key=(mode==1)?(U32)node:(U32)mode;
        for(i=0;i<l->num;i++)if(l->vol[i].mode==marker && l->vol[i].key==key)break;
        if(i==l->num)return -18;
        l->active_start=l->vol[i].begin+1;
        l->active_end=l->vol[i].end;
    }
    l->state=1;
    return 1;
}

/* Call the original row enumerator and select only rows intersecting the
 * chosen partition. Absolute databank indices remain unchanged for native
 * FileCopier/FileDeleter/Decrypter. */
static int __cdecl partition_row_scan(void *bank,int row,void **data,int *size,int *memindex) {
    U32 lan;int node,all,i,result=0;
    LAYOUT l;l.num=0;
    if(!get_current(&lan,&node,(U32)bank) || !g_find_installed)
        return g_original_row_scan(bank,row,data,size,memindex);
    {
        void *sys=get_lan_system(lan,node);
        int typ=(int)read_u32((U8*)sys+SYS_TYPE);
        if(typ!=TYPE_MAIN && typ!=TYPE_FILE)return g_original_row_scan(bank,row,data,size,memindex);
    }
    {
        int layout_result=resolve_layout(lan,(U32)bank,node,&l,1);
        if(layout_result<0) {
            if(data)*data=0;if(size)*size=0;if(memindex)*memindex=-1;
            if(!g_diag_error_latched) {
                g_diag_error_latched=1;
                log_text("DISKS INCOMPLETE: restricted view unavailable, native files untouched");
                log_hex("DISKS DIAG: exact failure code ",(U32)layout_result);
                log_hex("DISKS DIAG: current LAN ",lan);
                log_hex("DISKS DIAG: current node ",(U32)node);
                log_hex("DISKS DIAG: active bank ",(U32)bank);
                {
                    void *active_sys=get_lan_system(lan,node);
                    if(active_sys)log_hex("DISKS DIAG: active node Data1 ",read_u32((U8*)active_sys+SYS_DATA1));
                }
                log_marker_inventory((U32)bank,&l);
            }
            return 0;
        }
    }
    if(l.state==0)return g_original_row_scan(bank,row,data,size,memindex);
    if(data)*data=0;if(size)*size=0;if(memindex)*memindex=-1;
    {void *dummy=0;int dsize=0,dmem=-1;all=g_original_row_scan(bank,0,&dummy,&dsize,&dmem);}
    if(all<0 || all>MAX_BANK_SIZE)return 0;
    for(i=0;i<all;i++){
        void *f=0;int len=0,start=-1,end,clip_start,clip_end;
        (void)g_original_row_scan(bank,i,&f,&len,&start);
        if(start<0 || len<=0 || start>MAX_BANK_SIZE || len>MAX_BANK_SIZE)continue;
        end=start+len;
        if(f) {
            if(start<l.active_start || end>l.active_end)continue;
            if(is_marker(f))continue;
        } else {
            if(end<=l.active_start || start>=l.active_end)continue;
        }
        clip_start=start>l.active_start?start:l.active_start;
        clip_end=end<l.active_end?end:l.active_end;
        if(clip_start>=clip_end)continue;
        if(result==row && data && size && memindex) {
            *data=f;*size=clip_end-clip_start;*memindex=clip_start;
        }
        result++;
    }
    if(lan!=g_log_lan || node!=g_log_node || (U32)bank!=g_log_bank) {
        g_log_lan=lan;g_log_node=node;g_log_bank=(U32)bank;
        log_text("DISKS: selected FILESERVER/MAINSERVER volume");
        log_hex("DISKS: LAN pointer ",lan);
        log_hex("DISKS: active node ",(U32)node);
        log_hex("DISKS: start index ",(U32)l.active_start);
        log_hex("DISKS: exclusive end index ",(U32)l.active_end);
        log_hex("DISKS: visible row count ",(U32)result);
        if(row==0) {
            log_hex("DISKS DIAG: first returned row file pointer ",data?(U32)*data:0u);
            log_hex("DISKS DIAG: first returned row size ",size?(U32)*size:0u);
            log_hex("DISKS DIAG: first returned row memory index ",memindex?(U32)*memindex:0u);
        }
    }
    return result;
}
/* Also fence background/uncontextualized auto-placement. A LAN may receive
 * native file generation when no LAN browser is open. Look for the first
 * UFS2 boundary in that bank and keep the default allocator in root memory.
 * This routine only reads the native file table; it does not generate disks. */
static int first_native_boundary(U32 bank) {
    int i,total=n_get_data_size((void*)bank),first=0x7fffffff;
    if(total<0 || total>MAX_BANK_SIZE)return -1;
    for(i=0;i<total;i++){
        void *f=n_get_data_file((void*)bank,i);
        if(f && is_marker(f)){
            const char *name=(const char*)((U8*)f+DATA_TITLE);
            if(name[5]=='B' && name[6]=='.' && (name[7]=='P' || name[7]=='G')){
                int pos=file_slot_to_memory_index((void*)bank,i);
                if(pos>=0 && pos<first)first=pos;
            }
        }
    }
    return first==0x7fffffff?-1:first;
}
/* Native DataBank::FindValidPlacement equivalent, but constrained to the
 * active disk for remote transfers and root for uncontextualized placement.
 * The original Uplink implementation also searches from the first position. */
static int __attribute__((thiscall)) partition_find_valid(void *bank,void *file) {
    int size,i,first=0,last,root;U32 lan;int node;LAYOUT l;
    if(!bank || !file)return -1;
    size=(int)read_u32((U8*)file+DATA_SIZE);
    last=n_get_size(bank);
    if(size<0 || last<=0 || last>MAX_BANK_SIZE)return -1;
    root=first_native_boundary((U32)bank);
    if(root>=0 && root<last)last=root; /* root-only when context is absent */
    if(get_current(&lan,&node,(U32)bank)) {
        void *sys=get_lan_system(lan,node);
        if(sys && ((int)read_u32((U8*)sys+SYS_TYPE)==TYPE_FILE ||
                   (int)read_u32((U8*)sys+SYS_TYPE)==TYPE_MAIN)) {
            if(resolve_layout(lan,(U32)bank,node,&l,0)<0)return -1;
            if(l.state==1){first=l.active_start;last=l.active_end;}
        }
    }
    for(i=first;i<last;i++) {
        if(i+size>last)break;
        if(n_is_valid(bank,file,i)==0)return i;
    }
    return -1;
}
static int install_row_partitions(void) {
    U32 i,j; U8 replacement[5]; U32 scan=(U32)(g_image+RVA_ROW_SCAN);
    static const U8 entry[8]={0x51,0x8b,0x44,0x24,0x10,0x8b,0x4c,0x24};
    static const U8 find_entry[5]={0x53,0x8b,0x5c,0x24,0x08};
    if(!native_bank_signature())return 0;
    for(j=0;j<8;j++)if(g_image[RVA_ROW_SCAN+j]!=entry[j]) {log_text("DISKS DISABLED: unexpected row scan entry");return 0;}
    for(i=0;i<NUM_ROW_CALLS;i++) {
        U8 *site=g_image+g_row_rva[i];
        if(site[0]!=0xe8 || (U32)(site+5)+read_u32(site+1)!=scan){log_text("DISKS DISABLED: unexpected row scan call");return 0;}
    }
    for(j=0;j<5;j++)if(g_image[RVA_BANK_FINDVALID+j]!=find_entry[j]) {log_text("DISKS DISABLED: FindValidPlacement fingerprint");return 0;}
    /* Install every callsite, with automatic rollback on a failed write. */
    replacement[0]=0xe8;
    g_original_row_scan=(ROW_SCAN)scan;
    for(i=0;i<NUM_ROW_CALLS;i++) {
        U8 *site=g_image+g_row_rva[i];
        store_u32(replacement+1,(U32)partition_row_scan-((U32)site+5));
        copy_bytes(g_row_orig[i],site,5);
        if(!patch_mem(site,replacement,5)){
            while(i){--i;patch_mem(g_image+g_row_rva[i],g_row_orig[i],5);}
            log_text("DISKS DISABLED: row patch failed, original calls restored");return 0;
        }
    }
    g_row_installed=NUM_ROW_CALLS;
    /* Global placement hook: patch on the EXACT function boundary and preserve
       its thiscall calling convention. No trampoline is needed because this is
       an equivalent loop using the unpatched native IsValidPlacement function. */
    { U8 *site=g_image+RVA_BANK_FINDVALID;
      U8 jmp[5];jmp[0]=0xE9;
      store_u32(jmp+1,(U32)partition_find_valid-((U32)site+5));
      copy_bytes(g_find_original,site,5);
      if(!patch_mem(site,jmp,5)){
          for(i=0;i<NUM_ROW_CALLS;i++)patch_mem(g_image+g_row_rva[i],g_row_orig[i],5);
          g_row_installed=0;log_text("DISKS DISABLED: placement hook failed; restored rows");return 0;
      }
    }
    g_find_installed=1;
    log_text("DISKS ACTIVE: four row calls filtered; native automatic placement fenced");
    return 1;
}

static char * __cdecl hooked_strstr(const char*haystack,const char*needle) {
    if(eq(needle,"MAINSERVER") && contains(haystack,"FILESERVER")) {
        /* Set only on genuine standalone FILESERVER tokens; never turn
           other server subtypes into FILESERVERs. */
        TlsSetValue(g_tls,(LPVOID)1);
        g_count_detected++;
        return (char*)haystack;
    }
    return g_orig_strstr(haystack,needle);
}
static long WINAPI on_exception(UFS_EXCEPTION_POINTERS *ep) {
    UFS_CONTEXT *c=ep->ContextRecord;
    U32 code=ep->ExceptionRecord->ExceptionCode;
    if (code==EXCEPTION_BREAKPOINT) {
        U32 address=(U32)ep->ExceptionRecord->ExceptionAddress;
        U32 i;
        for(i=0;i<3;i++) {
            EML_SITE *p=&g_emulate[i];
            if (p->address && address==(U32)p->address) {
                U32 value;
                int isfile=(g_tls!=TLS_OUT_OF_INDEXES && TlsGetValue(g_tls)!=0);
                /* Emulate exactly one 32-bit PUSH.  The original breakpoint
                   stays installed, so normal and FILESERVER paths coexist. */
                if(p->kind==1u) value=isfile ? (U32)g_file_title : ORIGINAL_MAIN_TITLE;
                else value=c->Ebp+(isfile ? 1u : 0u); /* hsIndex+1 = fssIndex */
                *(U32 *)(c->Esp-4)=value;
                c->Esp-=4;
                c->Eip=(U32)p->address+p->length;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    if(code==EXCEPTION_BREAKPOINT && (U32)ep->ExceptionRecord->ExceptionAddress==(U32)g_bp_address && g_bp_address) {
        U32 *stack=(U32*)c->Esp;
        if(g_tls!=TLS_OUT_OF_INDEXES && TlsGetValue(g_tls) && stack[1]==TYPE_MAINSERVER) {
            /* Entry stack: return, TYPE, x, y, security, screenIndex, dataScreenIndex.
               GenerateMainServer creates message, password, voice, high-security,
               file-server, log, console and menu screens in that order.
               A FILESERVER begins at the password screen (message index + 1).
               PasswordScreen::SetNextPage has been redirected to the stock file
               browser by our emulated push at 00453970.  Other screens are unused. */
            stack[1]=TYPE_FILESERVER;
            stack[5]+=1u;
            g_count_converted++;
            log_hex("TYPE 7 FILESERVER: password -> file browser, count=",g_count_converted);
            TlsSetValue(g_tls,0);
        }
        g_step_thread=GetCurrentThreadId();
        if(!patch_mem(g_bp_address,&g_bp_original,1))return EXCEPTION_CONTINUE_SEARCH;
        c->Eip=(DWORD)g_bp_address;
        c->EFlags|=0x100u;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if(code==EXCEPTION_SINGLE_STEP && g_step_thread==GetCurrentThreadId()) {
        const U8 trap=0xcc;
        if(!patch_mem(g_bp_address,&trap,1))return EXCEPTION_CONTINUE_SEARCH;
        c->EFlags &= ~0x100u;
        g_step_thread=0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static int install_hook(void) {
    U8 call[6],bp=0xcc;U32 rel,i,j;
    static const U8 push_title[5]={0x68,0x84,0x38,0x57,0x00};
    if (!discover_site())return 0;
    g_emulate[0].address=g_image+RVA_PASSWORD_TITLE;
    g_emulate[1].address=g_image+RVA_FILE_TITLE;
    g_emulate[2].address=g_image+RVA_PASSWORD_NEXT;
    for(i=0;i<3;i++) {
        EML_SITE *p=&g_emulate[i];
        p->length=(i==2)?1:TITLE_INSN_LENGTH;
        p->kind=(i==2)?2:1;
        p->original=*p->address;
        if (i!=2) {
            for(j=0;j<5;j++)if(p->address[j]!=push_title[j]) {
                log_text("Unexpected Main Server title instruction; safe abort");return 0;
            }
        } else if(p->address[0]!=ORIGINAL_PASSWORD_NEXT) {
            log_text("Unexpected PasswordScreen next-page instruction; safe abort");return 0;
        }
    }
    g_bp_address=g_image+RVA_ADDLANSYSTEM;
    /* Actual, statically checked Steam 1.55 prologue (MSVC 2005 SEH frame):
       00452DD0: 6A FF              push -1
       00452DD2: 68 2B 02 56 00     push 0056022B
       The earlier 0.1/0.2 guard rejected this real prologue! */
    if(g_bp_address[0]!=0x6a || g_bp_address[1]!=0xff ||
       g_bp_address[2]!=0x68 || read_u32(g_bp_address+3)!=0x0056022bu) {
        log_text("Unexpected AddLanSystem prologue: safe abort");return 0;
    }
    log_hex("AddLanSystem @ ",(U32)g_bp_address);
    g_tls=TlsAlloc();if(g_tls==TLS_OUT_OF_INDEXES)return 0;
    g_veh=AddVectoredExceptionHandler(1,(LPVOID)on_exception);
    if(!g_veh)return 0;
    copy_bytes(g_call_original,g_call_address,g_call_length);
    rel=(U32)hooked_strstr-((U32)g_call_address+5);
    call[0]=0xe8;store_u32(call+1,rel);call[5]=0x90;
    if(!patch_mem(g_call_address,call,g_call_length))return 0;
    g_bp_original=*g_bp_address;
    if(!patch_mem(g_bp_address,&bp,1)) {
        patch_mem(g_call_address,g_call_original,g_call_length);return 0;
    }
    /* Install tiny emulated-push breakpoints.  The matched instructions have
       exact known bytes in the hashed executable, and are handled without
       single-stepping or modifying data shared with regular MAINSERVERs. */
    for(i=0;i<3;i++) {
        if(!patch_mem(g_emulate[i].address,&bp,1)) {
            while(i) {--i;patch_mem(g_emulate[i].address,&g_emulate[i].original,1);}
            patch_mem(g_bp_address,&g_bp_original,1);
            patch_mem(g_call_address,g_call_original,g_call_length);
            log_text("UI hook failed; original parser restored");
            return 0;
        }
    }
    g_installed=1;
    log_text("PATCH ACTIVE: type-7 FILESERVER, password and dedicated file-browser route");
    return 1;
}
static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    log_text("=== UplinkFileServer 0.7C2 populated volumes + B2 reverse-index DLL loaded ===");
    if(!verify_exe_hash()){log_text("No hooks installed: unknown EXE. Original game remains unchanged.");return 0;}
    if(!parse_image()){log_text("No hooks installed: bad PE section layout.");return 0;}
    if(!install_hook()) {log_text("No hooks installed: parser site not verified.");return 0;}
    install_row_partitions();
    return 0;
}
BOOL WINAPI DllMain(HMODULE mod, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if(reason==1){DWORD id;HANDLE h;g_dll=mod;DisableThreadLibraryCalls(mod);
        h=CreateThread(0,0,(LPVOID)worker,0,0,&id);if(h)CloseHandle(h);
    }
    /* Deliberately don't touch patched memory on detach, because exiting game
       may already have torn down library/API state. The process is exiting. */
    return TRUE;
}
