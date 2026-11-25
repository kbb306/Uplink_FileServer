/*
 * Uplink Mac 1.6.9 FILESERVER v0.16.1 - GROUP DELETER ARM FIX.
 * Exact x86_64 Steam binary LC_UUID and instruction signatures only.
 * No game binary patching or code-page writes; uses dyld libc interposition.
 *
 * v0.11 restored the native File_Deleter progress widget and timing: Uplink's
 * original SvbCycle invokes the task's virtual Tick(n) with actual allocated
 * CPU ticks, while the original FileDeleter::SetTarget establishes file-size-
 * dependent numticksrequired. A per-instance vtable clone wraps only Tick:
 * ordinary progress is executed by vanilla Tick; the terminal call is
 * replaced with a validated, bounded safe_delete, preserving UFS2 markers.
 * v0.13 restored Mac native remote-delete compaction inside a validated disk.
 * v0.14 keeps that path, and permits native File_Copier and timed safe
 * File_Deleter on authenticated shared-root browsers without UFS12 switches.
 * v0.16 generalizes validated partitions to all custom LANs and enables
 * DATA1>=2 shared groups. It seeds private/group disks and stamps
 * the hidden BEGIN guard marker version so this migration is persistent.
 * v0.16.1 fixes a v0.14-era animation arm gate that accidentally rejected
 * grouped (G) disks even though click policy and safe deletion supported G.
 * No remote Defrag or browse-time compaction. Live testing is required.
 */
#include <stdarg.h>
#include "lan_layout.h"
#include "volume_plan.h"
#include "native_bank.h"
#include "partition_commit.h"
#include "browser_route.h"
#include "click_policy.h"
#include "safe_delete.h"
#include "ufs_operation_policy.h"
#include "vanilla_delete_timing.h"
#include "ufs_compact_native.h"
#include "private_population.h"

#if !defined(__APPLE__) || !defined(__x86_64__) || !defined(__LP64__)
#error "v0.11 native bank probe is INTEL macOS x86_64 ONLY"
#endif

extern int open(const char *,int,...);
extern long write(int,const void *,unsigned long);
extern int close(int);
extern char *getenv(const char *);
extern int getpid(void);
extern int snprintf(char *,unsigned long,const char *,...);
extern int vsnprintf(char *,unsigned long,const char *,va_list);
extern unsigned long strlen(const char *);
extern long time(long *);
extern const void *_dyld_get_image_header(unsigned int);
extern const char *_dyld_get_image_name(unsigned int);
extern char *strstr(const char *,const char *);
extern int puts(const char *);
extern int sscanf(const char *,const char *,...);
extern int vsscanf(const char *,const char *,va_list);
extern void *dlsym(void *,const char *);
#include "postload_guard.h"

#define O_WRONLY_MAC 1
#define O_APPEND_MAC 8
#define O_CREAT_MAC 512
#define MH_MAGIC_64 0xfeedfacfU
#define MH_EXECUTE 2U
#define LC_UUID 0x1bU
#define CPU_TYPE_X86_64 0x01000007U

struct macho_header64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved;};
struct macho_loadcmd { unsigned int cmd,cmdsize;};
static const unsigned char expected_uuid[16]={
  0xbe,0x67,0x65,0x40,0xd8,0xcc,0x32,0x6d,0x8e,0x69,0x8a,0x25,0xe5,0x7c,0xd1,0x0d};
static unsigned long main_base=0;
static volatile int hook_armed=0;
static volatile unsigned int match_count=0,converted_count=0,failed_count=0;
static struct ufs_volume_plan g_volume_plan;
static int g_plan_ready=0;
static int g_plan_error=0;
static struct ufs_postload_guard g_postload;
static int log_fd=-1;
static __thread int in_native_generator=0;
static int browser_last_screen=-1;
static unsigned long browser_last_comp=0;
static int browser_click_reported=0;
static int browser_operations_optin=0;
static int browser_safe_delete_optin=0;
static int browser_safe_delete_dryrun=0;
static int browser_last_scroll_items=-1;
static int private_population_enabled=1;
static int saved_migration_enabled=0;
static unsigned long migration_last_comp=0;
static int migration_attempted=0;
static unsigned long private_seed_comp=0;
static unsigned long long private_seed_attempts=0;
/* One native File_Deleter may be safely animated at a time. A private
 * vtable CLONE lives in our dylib's writable data; main executable pages
 * and the original class-wide vtable are never modified. */
struct ufs_animated_delete_state {
    void *task;
    unsigned long computer;
    int screen;
    struct ufs_browser_view view;
    struct ufs_browser_row row;
    int active;
    int reported;
};
static struct ufs_animated_delete_state g_delete_animation;
static unsigned long g_deleter_vtable_clone[12];
static int g_deleter_vtable_ready=0;
static void ufs_animated_deleter_tick(void *task,int n);


static void logline(const char *format, ...) {
    char text[768]; va_list ap; int n;
    va_start(ap,format); n=vsnprintf(text,sizeof(text),format,ap); va_end(ap);
    if (n<1) return;
    if (n >= (int)sizeof(text))n=sizeof(text)-1;
    if (log_fd>=0)(void)write(log_fd,text,(unsigned long)n);
}
static int eq(const char *a,const char *b) {
    if(!a || !b)return 0;
    while(*a && *b && *a==*b){++a;++b;}
    return *a==*b;
}
/* Deterministic libc-compatible strstr fallback: do not call an interposed
 * symbol from the interposer.  Includes the empty-needle case. */
static char *fallback_strstr(const char *haystack,const char *needle) {
    const char *p,*q,*r;
    if(!*needle)return (char *)haystack;
    for(p=haystack;*p;p++) {
        if(*p!=*needle)continue;
        q=p;r=needle;
        while(*q && *r && *q==*r){q++;r++;}
        if(!*r)return (char *)p;
    }
    return 0;
}
static int bytes_equal(const unsigned char *p,const unsigned char *want,unsigned int n) {
    unsigned int i; for(i=0;i<n;i++)if(p[i]!=want[i])return 0;return 1;
}
static int validate_main_executable(void){
    const struct macho_header64 *h=(const struct macho_header64*)_dyld_get_image_header(0);
    const char *path=_dyld_get_image_name(0);
    const unsigned char *p;
    unsigned int i,off=0,uuid_ok=0;
    static const unsigned char ma_lea[]={0x48,0x8d,0x35,0x4f,0xa2,0x1f,0x00};
    static const unsigned char str_call[]={0xe8,0xee,0xc0,0x1c,0x00};
    static const unsigned char main_call[]={0xe8,0xff,0x1d,0x00,0x00};
    static const unsigned char post[]={0x4c,0x8b,0xb5,0x88,0xfa,0xff,0xff};
    static const unsigned char final_cmp[]={0x83,0xf8,0xff,0x0f,0x85,0x15,0xfc,0xff,0xff};
    static const unsigned char native_gen[]={0x55,0x48,0x89,0xe5,0x41,0x57};
    static const unsigned char type5[]={0x41,0xc7,0x44,0x24,0x08,0x05,0x00,0x00,0x00};
    static const unsigned char set_next[]={0x55,0x48,0x89,0xe5,0x89,0xb7,0xc8,0x00,0x00,0x00};
    static const unsigned char set_title[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53};
    /* Verified native post-generator assignments for DATA1, DATA2, DATA3. */
    /* Exact post-initialization puts("done") call inside LoadLAN, after
     * DataBank::SetSize, initial PutData, and all console/security setup. */
    static const unsigned char postload_done_lea[]={0x48,0x8d,0x3d,0xbf,0x13,0x1f,0x00};
    static const unsigned char postload_puts_call[]={0xe8,0xe2,0xb6,0x1c,0x00};
    static const unsigned char postload_puts_return[]={0x48,0x8d,0xbd,0x80,0xf8,0xff,0xff};
    static const unsigned char postload_last_file[]={0xe8,0x43,0x33,0xfe,0xff};
    static const unsigned char apply_data1[]={0x8b,0x8d,0x68,0xfa,0xff,0xff,
                                               0x89,0x48,0x28};
    if(!h||h->magic!=MH_MAGIC_64||h->filetype!=MH_EXECUTE||
       h->cputype!=CPU_TYPE_X86_64||h->ncmds==0||h->ncmds>512||
       h->sizeofcmds>65536||!path)return 0;
    {
        const char *last=path,*cursor=path;
        for(;*cursor;cursor++)if(*cursor=='/')last=cursor+1;
        if(!eq(last,"Uplink"))return 0;
    }
    p=(const unsigned char *)(h+1);
    for(i=0;i<h->ncmds;i++) {
        const struct macho_loadcmd *lc;
        if(off>h->sizeofcmds||h->sizeofcmds-off<8)return 0;
        lc=(const struct macho_loadcmd*)(p+off);
        if(lc->cmdsize<8||lc->cmdsize>h->sizeofcmds-off)return 0;
        if(lc->cmd==LC_UUID&&lc->cmdsize>=24) {
            uuid_ok=bytes_equal((const unsigned char*)(lc+1),expected_uuid,16);
            break;
        }
        off+=lc->cmdsize;
    }
    if(!uuid_ok)return 0;
    main_base=(unsigned long)h;
    /* The native DataBank and Data methods used for v0.11 allocation, and
     * native C++ operator new. All are verified before *any* bank writes. */
    static const unsigned char bank_setsize[]={0x55,0x48,0x89,0xe5,0x48,0x83,0xc7,0x38};
    static const unsigned char bank_putat[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56};
    static const unsigned char data_ctor[]={0x55,0x48,0x89,0xe5,0x53,0x50};
    static const unsigned char data_title[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53};
    static const unsigned char native_new_stub[]={0xff,0x25,0xf0,0x82,0x09,0x00};
    static const unsigned char comp_name[]={0x48,0x83,0xc3,0x24};
    static const unsigned char comp_company[]={0x49,0x83,0xc6,0x64};
    static const unsigned char comp_ip[]={0x48,0x81,0xc3,0xa4,0x00,0x00,0x00};
    if(!bytes_equal((const unsigned char*)(main_base+0xe8050),bank_setsize,sizeof(bank_setsize))||
       !bytes_equal((const unsigned char*)(main_base+0xe8290),bank_putat,sizeof(bank_putat))||
       !bytes_equal((const unsigned char*)(main_base+0xe98d0),data_ctor,sizeof(data_ctor))||
       !bytes_equal((const unsigned char*)(main_base+0xe9a20),data_title,sizeof(data_title))||
       !bytes_equal((const unsigned char*)(main_base+0x2d01a2),native_new_stub,sizeof(native_new_stub))||
       !bytes_equal((const unsigned char*)(main_base+0xe1c3b),comp_name,sizeof(comp_name))||
       !bytes_equal((const unsigned char*)(main_base+0xe1c9f),comp_company,sizeof(comp_company))||
       !bytes_equal((const unsigned char*)(main_base+0xe1d0a),comp_ip,sizeof(comp_ip)))return 0;
    /* Exact native FileServerScreenInterface::FileClick/FileDraw sscanf sites.
     * Both callbacks add global baseoffset at 0x37a7c4 immediately after
     * sscanf. The browser interposer substitutes a FULL-BANK row ordinal
     * before the addition, preserving the native row->absolute memory map. */
    static const unsigned char click_format[]={0x48,0x8d,0x35,0xbb,0xf7,0x28,0x00};
    static const unsigned char click_scan[]={0xe8,0x71,0x3e,0x27,0x00};
    static const unsigned char click_base[]={0x8b,0x05,0x3d,0xe0,0x31,0x00};
    static const unsigned char draw_format[]={0x48,0x8d,0x35,0x96,0xf0,0x28,0x00};
    static const unsigned char draw_scan[]={0xe8,0x49,0x37,0x27,0x00};
    static const unsigned char draw_base[]={0x8b,0x05,0x15,0xd9,0x31,0x00};
    static const unsigned char lan_systems[]={0x4c,0x8d,0xb7,0x68,0x02,0x00,0x00};
    static const unsigned char remote_current[]={0x8b,0x73,0x14};
    static const unsigned char game_interface[]={0x48,0x8b,0x47,0x08};
    static const unsigned char remote_interface[]={0x48,0x8b,0x47,0x10};
    static const unsigned char get_remote_screen[]={0x55,0x48,0x89,0xe5,0x48,0x8b,0x7f,0x08};
    static const unsigned char get_screen_comp[]={0x55,0x48,0x89,0xe5,0x53,0x50};
    static const unsigned char ui_game_global[]={0x48,0x8d,0x05,0xec,0xd6,0x31,0x00};
    /* Native ScrollBox::GetScrollBox and SetNumItems, with the native
     * numItems/windowSize/currentIndex fields at +0x50/+0x54/+0x58. */
    static const unsigned char scroll_get[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56};
    static const unsigned char scroll_set[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53};
    static const unsigned char scroll_get_call[]={0xe8,0x6f,0xe8,0x01,0x00};
    static const unsigned char scroll_set_call[]={0xe8,0x89,0xe4,0x01,0x00};
    if(!bytes_equal((const unsigned char*)(main_base+0x5c76f),click_format,sizeof(click_format))||
       !bytes_equal((const unsigned char*)(main_base+0x5c77c),click_scan,sizeof(click_scan))||
       !bytes_equal((const unsigned char*)(main_base+0x5c781),click_base,sizeof(click_base))||
       !bytes_equal((const unsigned char*)(main_base+0x5ce94),draw_format,sizeof(draw_format))||
       !bytes_equal((const unsigned char*)(main_base+0x5cea4),draw_scan,sizeof(draw_scan))||
       !bytes_equal((const unsigned char*)(main_base+0x5cea9),draw_base,sizeof(draw_base))||
       !bytes_equal((const unsigned char*)(main_base+0xef8a8),lan_systems,sizeof(lan_systems))||
       !bytes_equal((const unsigned char*)(main_base+0x715d3),remote_current,sizeof(remote_current))||
       !bytes_equal((const unsigned char*)(main_base+0x14934),game_interface,sizeof(game_interface))||
       !bytes_equal((const unsigned char*)(main_base+0x1d494),remote_interface,sizeof(remote_interface))||
       !bytes_equal((const unsigned char*)(main_base+0x715f0),get_remote_screen,sizeof(get_remote_screen))||
       !bytes_equal((const unsigned char*)(main_base+0xe3de0),get_screen_comp,sizeof(get_screen_comp))||
       !bytes_equal((const unsigned char*)(main_base+0x5ceb5),ui_game_global,sizeof(ui_game_global))||
       !bytes_equal((const unsigned char*)(main_base+0x7c490),scroll_get,sizeof(scroll_get))||
       !bytes_equal((const unsigned char*)(main_base+0x7c0c0),scroll_set,sizeof(scroll_set))||
       !bytes_equal((const unsigned char*)(main_base+0x5dc1c),scroll_get_call,sizeof(scroll_get_call))||
       !bytes_equal((const unsigned char*)(main_base+0x5dc32),scroll_set_call,sizeof(scroll_set_call)))return 0;
    if(!bytes_equal((const unsigned char*)(main_base+0x10453a),ma_lea,sizeof(ma_lea))||
       !bytes_equal((const unsigned char*)(main_base+0x104541),str_call,sizeof(str_call))||
       !bytes_equal((const unsigned char*)(main_base+0x10456c),main_call,sizeof(main_call))||
       !bytes_equal((const unsigned char*)(main_base+0x1047d6),post,sizeof(post))||
       !bytes_equal((const unsigned char*)(main_base+0x1047e7),final_cmp,sizeof(final_cmp))||
       !bytes_equal((const unsigned char*)(main_base+0x106370),native_gen,sizeof(native_gen))||
       !bytes_equal((const unsigned char*)(main_base+0x10675e),type5,sizeof(type5))||
       !bytes_equal((const unsigned char*)(main_base+0xe77f0),set_next,sizeof(set_next))||
       !bytes_equal((const unsigned char*)(main_base+0xe3d20),set_title,sizeof(set_title))||
       !bytes_equal((const unsigned char*)(main_base+0x10442e),apply_data1,sizeof(apply_data1))||
       !bytes_equal((const unsigned char*)(main_base+0x104e80),postload_done_lea,sizeof(postload_done_lea))||
       !bytes_equal((const unsigned char*)(main_base+0x104e87),postload_puts_call,sizeof(postload_puts_call))||
       !bytes_equal((const unsigned char*)(main_base+0x104e8c),postload_puts_return,sizeof(postload_puts_return))||
       !bytes_equal((const unsigned char*)(main_base+0x104d58),postload_last_file,sizeof(postload_last_file)))return 0;
    /* This exact native File_Deleter vtable owns the original Tick at slot 3.
     * Also fingerprint the *post-deletion repacking loop* that made v0.9
     * unsafe: original Tick calls RemoveData at 0x835e9, and then repacks.
     * We deliberately prevent the original Tick from reaching that call. */
    {
        static const unsigned char tick_entry[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56};
        static const unsigned char remove_call[]={0xe8,0xc2,0x56,0x06,0x00};
        static const unsigned char compact_start[]={0x41,0x80,0x7c,0x24,0x40,0x00};
        static const unsigned char get_program[]={0x55,0x48,0x89,0xe5,0x8b,0x7f,0x08};
        static const unsigned char lookup_pid[]={0x55,0x48,0x89,0xe5};
        static const unsigned char ecl_caption[]={0x55,0x48,0x89,0xe5};
        static const unsigned long expected_funcs[10]={
            0x82cc0,0x82cd0,0x83390,0x833a0,0x83830,
            0x83a40,0x83b40,0x83c50,0x82e80,0x82d00};
        unsigned int k;
        const unsigned long *vt=(const unsigned long *)(main_base+0x36cc20ul);
        if(!bytes_equal((const unsigned char*)(main_base+0x833a0),tick_entry,sizeof(tick_entry))||
           !bytes_equal((const unsigned char*)(main_base+0x835e9),remove_call,sizeof(remove_call))||
           !bytes_equal((const unsigned char*)(main_base+0x835ee),compact_start,sizeof(compact_start))||
           !bytes_equal((const unsigned char*)(main_base+0x94bf0),get_program,sizeof(get_program))||
           !bytes_equal((const unsigned char*)(main_base+0xb9400),lookup_pid,sizeof(lookup_pid))||
           !bytes_equal((const unsigned char*)(main_base+0xa61a0),ecl_caption,sizeof(ecl_caption))||
           vt[-2]!=0 || vt[-1]!=(main_base+0x36cc70ul))return 0;
        for(k=0;k<10;k++)if(vt[k]!=main_base+expected_funcs[k])return 0;
    }
    return ufs_bank_machine_signature((const unsigned char*)main_base);
}

/* Disable optimizations that could interfere with frame-chain access or
 * the explicit alteration of this function's saved return-address slot. */
__attribute__((noinline,optnone))
static char *ufs_strstr(const char *haystack,const char *needle) {
    const unsigned long caller=(unsigned long)__builtin_return_address(0);
    if(hook_armed && !in_native_generator && main_base &&
       caller==main_base+0x104546UL &&
       eq(needle,"MAINSERVER") && eq(haystack,"FILESERVER")) {
        unsigned char *self=(unsigned char*)__builtin_frame_address(0);
        /* On this verified site the immediate caller is the frame-pointer-
         * preserving LAN parser. The saved RBP is at *(self+0). */
        unsigned char *parent=(unsigned char *)ufs_load_ulong(self);
        unsigned long *saved_return=(unsigned long *)(self+8);
        typedef int (*Generator)(void *,void *,int,int,int);
        typedef void (*SetNextPage)(void *,int);
        typedef void (*SetTitle)(void *,const char *);
        void *computer,*cluster;
        int x,y,security,data1,index,converted=0;
        if(!parent || parent<=self || (unsigned long)(parent-self)>16384 ||
           *saved_return!=main_base+0x104546UL) {
            logline("v0.11 SPECIAL rejected: unexpected frame chain or return address\n");
            return fallback_strstr(haystack,needle);
        }
        computer=(void *)ufs_load_ulong(parent-0x570);
        cluster=(void *)ufs_load_ulong(parent-0x558);
        x=ufs_load_int(parent-0x564);
        y=ufs_load_int(parent-0x560);
        security=ufs_load_int(parent-0x55c);
        data1=ufs_load_int(parent-0x598);
        if(!computer || !cluster || x<0 || x>2048 || y<0 || y>2048 ||
           security<0 || security>32) {
            logline("v0.11 SPECIAL rejected: implausible native parser parameters\n");
            return fallback_strstr(haystack,needle);
        }
        (void)__sync_add_and_fetch(&match_count,1);
        in_native_generator=1;
        index=((Generator)(main_base+0x106370UL))(computer,cluster,x,y,security);
        in_native_generator=0;
        if(index>=0) {
            converted=ufs_convert_main_to_file(
                computer,cluster,index,(SetNextPage)(main_base+0xe77f0UL),
                (SetTitle)(main_base+0xe3d20UL));
        }
        if(converted) {
            int volume_index=-1,mode_result;
            struct ufs_volume_key key;
            char marker[17];
            const char *marker_label="(none)";
            if(!g_plan_ready || g_postload.completed ||
               g_postload.computer!=(unsigned long)computer ||
               g_volume_plan.cluster_id!=(unsigned long)cluster) {
                ufs_plan_reset(&g_volume_plan,(unsigned long)cluster);
                g_plan_ready=1;
                g_plan_error=0;
                ufs_postload_reset(&g_postload,(unsigned long)computer,(unsigned long)cluster);
                logline("v0.11 new LAN computer/cluster: resetting DATA1 plan\n");
            }
            mode_result=ufs_plan_register(&g_volume_plan,index,data1,&volume_index);
            if(mode_result>=0)ufs_postload_track_node(&g_postload);
            if(mode_result>=0 && ufs_mode_to_key(data1,index,&key)>=0) {
                if(key.mode!='R'){
                    ufs_marker_name(marker,'B',key);
                    marker_label=marker;
                }
                logline("v0.11 DATA1_PLAN node=%d data1=%d mode=%c key=%u volume_slot=%d "
                        "planned_begin=%s total_volumes=%u ROOT_SHARED=%s\n",
                        index,data1,key.mode,key.key,volume_index,marker_label,
                        g_volume_plan.volume_count,key.mode=='R'?"YES":"NO");
            } else {
                g_plan_error=1;
                logline("v0.11 DATA1_PLAN REJECTED index=%d data1=%d "
                        "(no disk changes)\n",index,data1);
            }
            /* The native DataBank is still zero-sized here. Inspect only
             * after LoadLAN's verified final puts("done") below. */
            (void)__sync_add_and_fetch(&converted_count,1);
        }
        else (void)__sync_add_and_fetch(&failed_count,1);
        logline("v0.11 FILESERVER system=(%d,%d) security=%d data1=%d index=%d type7=%s\n",
                x,y,security,data1,index,converted?"YES":"NO (native main fallback)");
        /* Route directly to the parser's existing LOGSERVER common return
         * block. RAX is interpreted as the generated system index and the
         * native parser then assigns the node's data1/data2/data3 fields.
         * This avoids both a second generator call and the warning branch.
         */
        *saved_return=main_base+0x1047d6UL;
        return (char *)(unsigned long)(unsigned int)index;
    }
    return fallback_strstr(haystack,needle);
}

/*
 * Exact-site post-initialization observer.
 * LoadLAN::puts("done") is emitted AFTER native SetSize and all initial
 * DataBank::PutData operations. native UFS2 marker allocation for validated custom LANs
 * runs here AFTER all ordinary files are populated.
 *
 * This is a SECOND independent dyld interpose, not a global hook policy:
 * only the instruction-verified call return address 0x104E8C is eligible.
 * All other puts calls forward to the original libc puts.
 */
__attribute__((noinline,optnone))
static int ufs_puts(const char *line) {
    const unsigned long caller=(unsigned long)__builtin_return_address(0);
    if(hook_armed && main_base && caller==main_base+0x104e8cUL &&
       eq(line,"done") && g_plan_ready && g_postload.node_count>0) {
        unsigned char *self=(unsigned char*)__builtin_frame_address(0);
        const unsigned long parent=ufs_load_ulong(self);
        if(parent>(unsigned long)self && parent-(unsigned long)self<=16384UL) {
            /* The native caller's RBP has a validated Computer* at -0x570.
             * Do not trust any older tracked pointer from another LAN. */
            unsigned long comp=ufs_load_ulong((const void*)(parent-0x570UL));
            if(ufs_postload_admit(&g_postload,comp,caller,main_base+0x104e8cUL)) {
                struct ufs_bank_report report;
                int observed=ufs_bank_inspect((void*)comp,&g_volume_plan,&report);
                if(observed==0) {
                    int range_ok=ufs_plan_ranges(&g_volume_plan,report.bank_gq);
                    logline("v0.11 POSTLOAD_BANK bank_gq=%d file_slots=%d "
                            "file_count=%d used_gq=%d formatted=%d "
                            "marker_count=%d orphan_markers=%d saved_layout=%d "
                            "volumes=%u nodes=%u ROOT_SHARED=%u "
                            "plan_error=%d range_plan=%s projected_gq=%d\n",
                            report.bank_gq,report.file_slots,report.file_count,
                            report.used_gq,report.formatted,report.ufs_marker_count,
                            report.orphan_markers,report.layout_state,
                            g_volume_plan.volume_count,g_volume_plan.fileserver_count,
                            g_volume_plan.root_fileserver_count,g_plan_error,
                            range_ok==0?"PASS":"FAIL",
                            range_ok==0?g_volume_plan.proposed_native_total_gq:-1);
                    if(range_ok==0 && !g_plan_error &&
                       report.layout_state==0 && !report.formatted &&
                       g_postload.node_count==g_volume_plan.fileserver_count &&
                       ufs_load_int((const unsigned char*)comp+0x20)==64 &&
                       ufs_plan_matches_native_lan((void*)comp,&g_volume_plan)) {
                        struct ufs_native_writes native;
                        struct ufs_commit_report commit;
                        int result;
                        native.set_size=(ufs_bank_set_size_fn)(main_base+0xe8050UL);
                        native.put_at=(ufs_bank_put_at_fn)(main_base+0xe8290UL);
                        native.new_data=(ufs_native_new_fn)(main_base+0x2d01a2UL);
                        native.data_ctor=(ufs_data_ctor_fn)(main_base+0xe98d0UL);
                        native.set_title=(ufs_data_title_fn)(main_base+0xe9a20UL);
                        logline("v0.16 ALLOCATE_BEGIN GENERIC original_gq=%d projected_gq=%d "
                                "expected_markers=%u (native root and private operations preserved)\n",
                                report.bank_gq,g_volume_plan.proposed_native_total_gq,
                                2*g_volume_plan.volume_count);
                        result=ufs_commit_partitions((void*)comp,&g_volume_plan,&native,&commit);
                        logline("v0.11 ALLOCATE_RESULT=%s code=%d stage=%d "
                                "before_gq=%d after_gq=%d markers=%d "
                                "root_intact=%d before_files=%d after_files=%d "
                                "before_used=%d after_used=%d\n",
                                result==0?"PASS":(result==1?"ALREADY":"FAIL"),
                                result,commit.stage,commit.before_gq,commit.after_gq,
                                commit.created_markers,commit.root_intact,
                                commit.before_files,commit.after_files,
                                commit.before_used,commit.after_used);
                        if(result==0 && private_population_enabled){
                            unsigned int v;
                            for(v=0;v<g_volume_plan.volume_count;v++){
                                struct ufs_private_seed_report seed;
                                int node=g_volume_plan.volumes[v].first_node;
                                int data1=g_volume_plan.volumes[v].id.mode=='P' ? 1 :
                                      (int)g_volume_plan.volumes[v].id.key;
                                int populated=ufs_populate_private((void*)comp,&g_volume_plan,
                                                                   node,data1,&native,&seed);
                                logline("v0.16 PRIVATE_GENERATION phase=FRESH node=%d "
                                        "result=%d added=%d used=%d marker_stamped=%d\n",
                                        node,populated,seed.files_added,seed.gq_added,
                                        seed.marker_stamped);
                                if(populated<0){
                                    logline("v0.16 WARNING: initial private/group population failed; "
                                            "discard disposable test save\n");
                                    break;
                                }
                            }
                        }
                        if(result!=0)logline("v0.11 WARNING: allocation failed; if stage>0 "
                                             "DISCARD disposable test save; do NOT trust partition layout\n");
                    } else {
                        logline("v0.16 ALLOCATE_RESULT=SKIP (invalid/unpartitioned LAN topology "
                                "or root bank unsuitable; no changes)\n");
                    }
                    logline("v0.16 POSTLOAD_RESULT=COMPLETE "
                            "(validated browser routing and private population enabled)\n");
                }else {
                    logline("v0.11 POSTLOAD_BANK REJECTED error=%d (no bank writes)\n",observed);
                    logline("v0.11 POSTLOAD_RESULT=REVIEW (no bank writes)\n");
                }
            }
        } else logline("v0.11 POSTLOAD_GUARD rejected unexpected frame chain\n");
    }
    /* Dynamic lookup never calls through this interposer again; macOS
     * RTLD_NEXT is (void*)-1. A direct write fallback ensures no recursion
     * even if symbol resolution fails on an unsupported runtime. */
    {
        typedef int (*RealPuts)(const char *);
        static RealPuts original=0;
        if(!original)original=(RealPuts)dlsym((void*)-1,"puts");
        if(original && (void*)original!=(void*)&ufs_puts)return original(line);
        if(line) {
            unsigned long n=strlen(line);
            if(write(1,line,n)<0 || write(1,"\n",1)<0)return -1;
            return 1;
        }
        return -1;
    }
}

/* The native FileServerScreenInterface FileDraw and FileClick methods both
 * parse "fileserverscreen_file %d" through libc sscanf. Their exact return
 * sites are 0x5cea9 (draw) and 0x5c781 (click). A libc interposition can
 * remap the row OR guard clicking, without modifying executable pages and
 * without changing the DataBank. All unrelated sscanf calls use vsscanf.
 */
static int parse_file_button(const char *text) {
    const char *prefix="fileserverscreen_file ";
    int value=0;
    if(!text)return -1;
    while(*prefix){if(*text++!=*prefix++)return -1;}
    if(*text<'0'||*text>'9')return -1;
    while(*text>='0'&&*text<='9'){
        value=value*10+(*text++-'0');
        if(value>14)return -1;
    }
    return !*text?value:-1;
}
/* Direct accessors at verified RVAs avoid any reliance on C++ mangling. */
static void browser_limit_scroll(int visible_count){
    typedef void *(*GetScroll)(const char *);
    typedef void (*SetItems)(void *,int);
    unsigned long sb;
    int n,window,offset;
    if(!main_base || visible_count<=0 || visible_count>20000)return;
    sb=(unsigned long)((GetScroll)(main_base+0x7c490ul))("fileserverscreen_scroll");
    if(sb<0x10000ul || (sb&7ul))return;
    /* GetScrollBox only returns a ScrollBox from its native managed list. */
    n=ufs_load_int((const void*)(sb+0x50));
    window=ufs_load_int((const void*)(sb+0x54));
    offset=ufs_load_int((const void*)(sb+0x58));
    if(n<=0 || n>20000 || window!=15 || offset<0 || offset>20000)return;
    if(n!=visible_count){
        ((SetItems)(main_base+0x7c0c0ul))((void*)sb,visible_count);
        if(browser_last_scroll_items!=visible_count){
            logline("v0.14 SCROLL_LIMIT visible=%d native_before=%d window=15 (scrollbar still drawn)\n",
                    visible_count,n);
            browser_last_scroll_items=visible_count;
        }
    }
}
/* Only perform a mod-owned deletion while Uplink's own File_Deleter is the
 * active mouse target. This does NOT send a target to native FileDeleter.
 * No normal file browser click can delete without this check and both opt-ins. */
static int browser_program_is_targeted(const char *wanted){
    typedef const char *(*GetProgramName)(void*);
    unsigned long game,iface,tm;
    const char *name;
    if(!main_base)return 0;
    game=ufs_load_ulong((const void*)(main_base+0x37a5a8ul));
    if(game<0x10000ul || (game&7))return 0;
    iface=ufs_load_ulong((const void*)(game+0x08));
    if(iface<0x10000ul || (iface&7))return 0;
    tm=ufs_load_ulong((const void*)(iface+0x18));
    if(tm<0x10000ul || (tm&7))return 0;
    /* Native method returns NULL if no program is being targeted. */
    name=((GetProgramName)(main_base+0x94c10ul))((void*)tm);
    return name && eq(name,wanted);
}
static int browser_deleter_is_targeted(void){return browser_program_is_targeted("File_Deleter");}
static int browser_copier_is_targeted(void){return browser_program_is_targeted("File_Copier");}
static int browser_get_current(unsigned long *computer, int *screen) {
    unsigned long game,iface,remote,cs;
    typedef void *(*GetScreen)(void *);
    typedef void *(*GetComputer)(void *);
    if(!main_base||!computer||!screen)return 0;
    game=ufs_load_ulong((const void*)(main_base+0x37a5a8ul));
    if(game<0x10000ul || (game&7))return 0;
    iface=ufs_load_ulong((const void*)(game+0x08));
    if(iface<0x10000ul || (iface&7))return 0;
    remote=ufs_load_ulong((const void*)(iface+0x10));
    if(remote<0x10000ul || (remote&7))return 0;
    *screen=ufs_load_int((const void*)(remote+0x14));
    if(*screen<0 || *screen>8192)return 0;
    cs=(unsigned long)((GetScreen)(main_base+0x715f0ul))((void*)remote);
    if(cs<0x10000ul || (cs&7))return 0;
    *computer=(unsigned long)((GetComputer)(main_base+0xe3de0ul))((void*)cs);
    return *computer>0x10000ul && !(*computer&7);
}
/* The native File_Deleter Tick is used until JUST BEFORE its unsafe
 * terminal RemoveData + repacking branch. This is exact native timing:
 * SvbCycle computes n from real CPU hardware and per-task CPU priority,
 * and SetTarget computes required ticks from the actual file size. */
static int animated_deleter_restore(void *task){
    if(!g_delete_animation.active || g_delete_animation.task!=task)return 0;
    if(ufs_load_ulong(task)==(unsigned long)&g_deleter_vtable_clone[2])
        __builtin_memcpy(task,&(unsigned long){main_base+0x36cc20ul},sizeof(unsigned long));
    g_delete_animation.active=0;
    return 1;
}
static int animated_deleter_connected(void){
    typedef void *(*Getter)(void *);
    typedef int (*Connected)(void *);
    unsigned long game,world,player;
    if(!main_base)return 0;
    game=ufs_load_ulong((const void*)(main_base+0x37a5a8ul));
    if(game<0x10000ul || (game&7ul))return 0;
    world=(unsigned long)((Getter)(main_base+0x14950ul))((void*)game);
    if(world<0x10000ul || (world&7ul))return 0;
    player=(unsigned long)((Getter)(main_base+0x15f920ul))((void*)world);
    if(player<0x10000ul || (player&7ul))return 0;
    return ((Connected)(main_base+0x14ed40ul))((void*)player)!=0;
}
static void animated_deleter_set_done(void *task, int success){
    typedef int (*LookupPid)(void*);
    typedef void* (*GetButton)(const char*);
    typedef void (*Caption)(const char*,const char*,int);
    typedef void (*Dirty)(const char*);
    typedef int (*ResetTarget)(int);
    char name[128];
    int pid=((LookupPid)(main_base+0xb9400ul))(task);
    unsigned long button;
    if(pid<0 || pid>100000)return;
    (void)snprintf(name,sizeof(name),"filedeleter_progress %d",pid);
    button=(unsigned long)((GetButton)(main_base+0xa0a20ul))(name);
    if(button>=0x10000ul && !(button&7ul)){
        /* Exactly the native progress widget's width field. */
        ufs_store_int((void*)(button+0x10ul),120);
        ((Dirty)(main_base+0xa1750ul))(name);
        ((Caption)(main_base+0xa61a0ul))(name,success?"Finished":"Failed",0);
    }
    ufs_store_int((unsigned char*)task+0x3c,2);  /* FILEDELETER_FINISHED */
    if(success)
        ufs_store_int((unsigned char*)task+0x48,
                      ufs_load_int((unsigned char*)task+0x44));
    ((ResetTarget)(main_base+0x836e0ul))(pid);
}
static void ufs_animated_deleter_tick(void *task,int n){
    typedef void (*OriginalTick)(void*,int);
    typedef int (*Visible)(void*);
    const OriginalTick original=(OriginalTick)(main_base+0x833a0ul);
    struct ufs_delete_tick_plan plan;
    struct ufs_safe_delete_report report;
    int status,progress,required,source_index,code;
    unsigned long source,current_comp=0;
    int current_screen=-1;
    if(!g_delete_animation.active || g_delete_animation.task!=task){
        /* Unexpected invocation of our cloned vtable: fail closed. Never
         * invoke native Tick when it could delete from a private bank. */
        logline("v0.14 TICK_BLOCKED unexpected task pointer\n");
        return;
    }
    status=ufs_load_int((unsigned char*)task+0x3c);
    source=ufs_load_ulong((unsigned char*)task+0x30);
    source_index=ufs_load_int((unsigned char*)task+0x38);
    if(status==2){
        /* Native FINISHED branch only resets widget/task; no file removal. */
        original(task,n);
        animated_deleter_restore(task);
        return;
    }
    if(source!=g_delete_animation.computer+UFS_COMPUTER_BANK_OFFSET ||
       source_index!=g_delete_animation.row.first_gq){
        logline("v0.14 TICK_BLOCKED native File_Deleter target mismatch\n");
        return;
    }
    if(status==0){
        /* Native OFF branch starts the same Deleting... animation. */
        original(task,n);
        return;
    }
    if(status!=1 || n<=0 || n>100000000)return;
    /* Native Tick is idle if its own progress UI is not visible. */
    if(!((Visible)(main_base+0x83c50ul))(task))return;
    if(*(unsigned char*)((unsigned char*)task+0x40) &&
       !animated_deleter_connected()){
        /* Match vanilla disconnect cancellation, with n=0 so no removal
         * instruction is reachable. Native Tick may remove/free the task;
         * do not touch its memory again after that call. */
        logline("v0.14 ANIMATION_CANCELLED disconnected; no bank mutation\n");
        g_delete_animation.active=0;
        original(task,0);
        return;
    }
    progress=ufs_load_int((unsigned char*)task+0x48);
    required=ufs_load_int((unsigned char*)task+0x44);
    if(ufs_delete_tick_decide(progress,required,n,&plan)<0){
        logline("v0.14 TICK_BLOCKED invalid native tick counters progress=%d required=%d n=%d\n",
                progress,required,n);
        return;
    }
    if(!plan.finish_now){original(task,plan.vanilla_ticks);return;}
    if(plan.vanilla_ticks>0){
        typedef int (*LookupPid)(void*);
        int oldpid=((LookupPid)(main_base+0xb9400ul))(task);
        if(oldpid<0){g_delete_animation.active=0;return;}
        original(task,plan.vanilla_ticks);
        /* A native task could be removed if the source file changed. Check
         * membership by pointer identity BEFORE dereferencing task memory. */
        if(((LookupPid)(main_base+0xb9400ul))(task)!=oldpid){
            g_delete_animation.active=0;
            logline("v0.14 ANIMATION_CANCELLED native task removed\n");
            return;
        }
        /* The native code must not have reached its terminal branch. */
        if(ufs_load_int((unsigned char*)task+0x3c)!=1)return;
    }
    if(!browser_get_current(&current_comp,&current_screen) ||
       current_comp!=g_delete_animation.computer ||
       current_screen!=g_delete_animation.screen){
        logline("v0.14 TICK_BLOCKED screen changed while deleting; no commit\n");
        return;
    }
    code=ufs_safe_delete_file((void*)g_delete_animation.computer,
                              &g_delete_animation.view,
                              &g_delete_animation.row,&report);
    if(code==0){
        browser_last_scroll_items=-1;
        /* Vanilla Mac 1.6.9 FileDeleter::Tick calls DataBank::RemoveData
         * and, only if remotefile==true, repacks ALL files in that bank.
         * Its native loop at RVAs 0x835ee..0x836a1 would relocate UFS2
         * partition guards. Here the task and its CPU-timed animation have
         * already run unchanged; the deletion has safely committed, so
         * perform the same post-remote-delete packing *within this disk*.
         * No user switch, Defrag tool, or on-browse behavior is added.
         * Repacker post-verifies marker layout and rolls back any failed
         * memory-table edits. If it fails, the deletion remains committed
         * but storage remains fragmented, rather than corrupting a bank. */
        {
            struct ufs_compact_result repack;
            int rc=ufs_compact_native((void*)g_delete_animation.computer,
                                      &g_delete_animation.view,&repack);
            if(rc==0)logline("v0.14 VANILLA_REMOTE_REPACK=PASS node=%d "
                             "mode=%c moved_files=%d files=%d used_gq=%d "
                             "markers_preserved=YES\n",
                             g_delete_animation.view.node_index,
                             g_delete_animation.view.mode,
                             repack.moved_files,repack.files,repack.used_gq);
            else logline("v0.14 VANILLA_REMOTE_REPACK=REJECTED node=%d code=%d "
                         "(deletion committed; no unsafe whole-bank repack)\n",
                         g_delete_animation.view.node_index,rc);
        }
        logline("v0.14 ANIMATED_SAFE_DELETE=PASS screen=%d node=%d "
                "slot=%d gq=[%d,%d) size=%d native_required_ticks=%d "
                "native_SvbCycle=YES markers_preserved=YES\n",
                g_delete_animation.screen,g_delete_animation.view.node_index,
                report.file_slot,report.first_gq,
                report.first_gq+report.length_gq,report.length_gq,required);
    }else{
        logline("v0.14 ANIMATED_SAFE_DELETE=REJECTED screen=%d node=%d "
                "gq=%d code=%d; NO COMMIT\n",
                g_delete_animation.screen,g_delete_animation.view.node_index,
                g_delete_animation.row.first_gq,code);
    }
    animated_deleter_set_done(task,code==0);
}
/* Patch only the selected File_Deleter INSTANCE's virtual Tick pointer.
 * No page protection changes: clone table in writable dylib data, keep the
 * first two Itanium ABI metadata entries, preserve all other virtuals. */
static int animated_deleter_arm(unsigned long computer,int screen,
         const struct ufs_browser_view *view,const struct ufs_browser_row *row){
    typedef void *(*GetProgram)(void*);
    typedef int (*LookupPid)(void*);
    unsigned long game,iface,tm,task,vt;
    const unsigned long *original;
    unsigned int k;
    if(!view || !row || !ufs_deleter_arm_mode_supported(view->mode) ||
       !row->is_file || !browser_deleter_is_targeted() || !main_base)return 0;
    if(g_delete_animation.active){
        int pid=((LookupPid)(main_base+0xb9400ul))(g_delete_animation.task);
        if(pid!=-1)return 0; /* Do not overlap two deletion tasks. */
        g_delete_animation.active=0; /* Old task was destroyed. */
    }
    game=ufs_load_ulong((const void*)(main_base+0x37a5a8ul));
    if(game<0x10000ul || (game&7ul))return 0;
    iface=ufs_load_ulong((const void*)(game+0x08));
    if(iface<0x10000ul || (iface&7ul))return 0;
    tm=ufs_load_ulong((const void*)(iface+0x18));
    if(tm<0x10000ul || (tm&7ul))return 0;
    task=(unsigned long)((GetProgram)(main_base+0x94bf0ul))((void*)tm);
    if(task<0x10000ul || (task&7ul))return 0;
    vt=ufs_load_ulong((const void*)task);
    original=(const unsigned long*)(main_base+0x36cc20ul);
    if(vt!=(unsigned long)original || original[3]!=main_base+0x833a0ul ||
       ufs_load_int((unsigned char*)task+0x3c)!=0)return 0;
    if(!g_deleter_vtable_ready){
        g_deleter_vtable_clone[0]=original[-2];
        g_deleter_vtable_clone[1]=original[-1];
        for(k=0;k<10;k++)g_deleter_vtable_clone[k+2]=original[k];
        g_deleter_vtable_clone[5]=(unsigned long)&ufs_animated_deleter_tick;
        g_deleter_vtable_ready=1;
    }
    g_delete_animation.task=(void*)task;
    g_delete_animation.computer=computer;
    g_delete_animation.screen=screen;
    g_delete_animation.view=*view;
    g_delete_animation.row=*row;
    g_delete_animation.active=1;
    g_delete_animation.reported=0;
    __builtin_memcpy((void*)task,&(unsigned long){(unsigned long)&g_deleter_vtable_clone[2]},8);
    logline("v0.16.1 ANIMATION_ARMED screen=%d node=%d gq=[%d,%d) "
            "file_gq=%d scheduler=native_SvbCycle timing=native_FileDeleter\n",
            screen,view->node_index,row->first_gq,
            row->first_gq+row->length_gq,row->length_gq);
    return 1;
}

__attribute__((noinline,optnone))
static int ufs_sscanf(const char *text,const char *format, ...) {
    unsigned long caller=(unsigned long)__builtin_return_address(0);
    va_list args;
    int *out=0,button_row=-1;
    va_start(args,format);
    if(hook_armed && main_base && format &&
       (caller==main_base+0x5cea9ul || caller==main_base+0x5c781ul) &&
       eq(format,"fileserverscreen_file %d") &&
       (button_row=parse_file_button(text))>=0){
        unsigned long computer=0;
        int screen=-1,baseoffset=0,kind=0;
        struct ufs_browser_view view;
        out=va_arg(args,int*);
        va_end(args);
        if(!out)return 0;
        if(!browser_get_current(&computer,&screen)){
            /* Do not change behavior if the game's screen cannot be found. */
            *out=button_row;return 1;
        }
        {
            struct ufs_volume_plan plan;
            kind=ufs_browser_plan_for_screen((const void*)computer,screen,&view,&plan);
            /* Old saved custom LANs may have TYPE 7 nodes but no UFS2
             * disks. This is a ONE-TIME, explicitly opted-in migration. The
             * native commit function fingerprints the untouched root bank,
             * allocates exact boundary guards and verifies the result. */
            if(kind==2 && saved_migration_enabled &&
               caller==main_base+0x5cea9ul && button_row==0 &&
               !g_delete_animation.active &&
               !browser_copier_is_targeted() && !browser_deleter_is_targeted()){
                struct ufs_native_writes native;
                struct ufs_commit_report commit;
                int result;
                if(migration_last_comp!=computer){
                    migration_last_comp=computer;
                    migration_attempted=0;
                }
                if(!migration_attempted){
                    unsigned int v;
                    migration_attempted=1;
                    native.set_size=(ufs_bank_set_size_fn)(main_base+0xe8050ul);
                    native.put_at=(ufs_bank_put_at_fn)(main_base+0xe8290ul);
                    native.new_data=(ufs_native_new_fn)(main_base+0x2d01a2ul);
                    native.data_ctor=(ufs_data_ctor_fn)(main_base+0xe98d0ul);
                    native.set_title=(ufs_data_title_fn)(main_base+0xe9a20ul);
                    result=ufs_commit_partitions((void*)computer,&plan,&native,&commit);
                    logline("v0.16 SAVED_MIGRATION result=%d stage=%d "
                            "before_gq=%d after_gq=%d volumes=%u markers=%d\n",
                            result,commit.stage,commit.before_gq,commit.after_gq,
                            plan.volume_count,commit.created_markers);
                    if(result==0 && private_population_enabled){
                        for(v=0;v<plan.volume_count;v++){
                            struct ufs_private_seed_report seed;
                            int node=plan.volumes[v].first_node;
                            int data1=plan.volumes[v].id.mode=='P' ? 1 :
                                      (int)plan.volumes[v].id.key;
                            int seeded=ufs_populate_private((void*)computer,&plan,
                                                            node,data1,&native,&seed);
                            logline("v0.16 PRIVATE_GENERATION phase=MIGRATED "
                                    "node=%d mode=%c data1=%d result=%d added=%d\n",
                                    node,plan.volumes[v].id.mode,data1,
                                    seeded,seed.files_added);
                            if(seeded<0){
                                logline("v0.16 MIGRATION POPULATION FAILED: "
                                        "discard disposable save\n");
                                break;
                            }
                        }
                    }
                    if(result!=0 && commit.stage!=0)
                        logline("v0.16 MIGRATION INCOMPLETE: "
                                "discard disposable save immediately\n");
                    kind=ufs_browser_plan_for_screen((const void*)computer,
                                                     screen,&view,&plan);
                    browser_last_scroll_items=-1;
                }
            }
            /* Saved games never re-run the LoadLAN initializer. Reconstruct
             * the physical group plan from actual node DATA1 values; one
             * persistent boundary stamp belongs to ONE physical disk, even
             * when several FILESERVERs share the same DATA1 group. */
            if(kind==1 && private_population_enabled && view.volume_slot>=0 &&
               caller==main_base+0x5cea9ul && button_row==0 &&
               !g_delete_animation.active &&
               !browser_copier_is_targeted() && !browser_deleter_is_targeted()){
                unsigned int bit=(unsigned int)view.volume_slot;
                if(private_seed_comp!=computer){
                    private_seed_comp=computer;
                    private_seed_attempts=0;
                }
                if(bit<64 && !(private_seed_attempts&(1ull<<bit))){
                    struct ufs_native_writes native;
                    struct ufs_private_seed_report seed;
                    int seed_result;
                    private_seed_attempts|=1ull<<bit;
                    native.set_size=(ufs_bank_set_size_fn)(main_base+0xe8050ul);
                    native.put_at=(ufs_bank_put_at_fn)(main_base+0xe8290ul);
                    native.new_data=(ufs_native_new_fn)(main_base+0x2d01a2ul);
                    native.data_ctor=(ufs_data_ctor_fn)(main_base+0xe98d0ul);
                    native.set_title=(ufs_data_title_fn)(main_base+0xe9a20ul);
                    seed_result=ufs_populate_private((void*)computer,&plan,
                                                     view.node_index,view.data1,&native,&seed);
                    logline("v0.16 PRIVATE_GENERATION phase=SAVED node=%d mode=%c "
                            "data1=%d volume=%d result=%d existing=%d added=%d gq=%d "
                            "no_room=%d marker_stamped=%d\n",
                            view.node_index,view.mode,view.data1,view.volume_slot,
                            seed_result,seed.existing_files,seed.files_added,
                            seed.gq_added,seed.skipped_no_room,seed.marker_stamped);
                    if(seed_result<0)
                        logline("v0.16 WARNING: generation failed; "
                                "discard this disposable save\n");
                    kind=ufs_browser_view_for_screen((const void*)computer,screen,&view);
                    browser_last_scroll_items=-1;
                }
            }
        }
        /* Markerless LANs are still native single-bank machines unless the
         * user explicitly opts into conversion. Preserve their original
         * browser behavior instead of hiding unrelated old saves. */
        if(kind==2 && !saved_migration_enabled){
            *out=button_row;
            return 1;
        }
        baseoffset=ufs_load_int((const void*)(main_base+0x37a7c4ul));
        if(baseoffset<0||baseoffset>20000){*out=1000000;return 1;}
        if(screen!=browser_last_screen || computer!=browser_last_comp){
            browser_last_screen=screen;
            browser_last_comp=computer;
            browser_click_reported=0;
            browser_last_scroll_items=-1;
            if(kind==1)
                logline("v0.16 BROWSER_ROUTE screen=%d node=%d mode=%c "
                        "data1=%d range=[%d,%d) root=%d bank=%d "
                        "files=%d markers=%d OPERATIONS=%s\n",
                        screen,view.node_index,view.mode,view.data1,
                        view.begin_gq,view.end_gq,view.root_gq,
                        view.bank_gq,view.total_files,view.marker_count,
                        browser_operations_optin?"OPTED_IN":"BLOCKED");
            else logline("v0.11 BROWSER_ROUTE REJECTED screen=%d "
                         "(invalid UFS2 layout, rows hidden, clicks blocked)\n",screen);
        }
        if(kind==1){
            struct ufs_browser_row row;
            int translated_status=ufs_browser_resolve_row(
                (const void*)computer,&view,button_row+baseoffset,&row);
            if(translated_status==1 && row.native_row>=0 &&
               row.first_gq>=view.begin_gq &&
               row.length_gq>0 &&
               row.first_gq+row.length_gq<=view.end_gq){
                if(caller==main_base+0x5cea9ul){
                    /* Native ScrollBox is still created by Uplink, but
                     * cannot scroll into phantom rows of other partitions. */
                    if(button_row==0)browser_limit_scroll(row.visible_count);
                    *out=row.native_row-baseoffset;
                    return 1;
                }
                if(caller==main_base+0x5c781ul && row.is_file &&
                   ufs_operation_decide(browser_operations_optin,
                                        browser_safe_delete_optin,row.is_file,
                                        browser_deleter_is_targeted(),0,
                                        (char)view.mode)==UFS_OP_TIMED_SAFE_DELETE){
                    if(browser_safe_delete_dryrun){
                        logline("v0.14 SAFE_DELETE=DRYRUN screen=%d node=%d "
                                "gq=[%d,%d) (NO BANK CHANGES)\n",
                                screen,view.node_index,row.first_gq,
                                row.first_gq+row.length_gq);
                        *out=1000000-baseoffset;
                        return 1;
                    }
                    if(animated_deleter_arm(computer,screen,&view,&row)){
                        /* Pass through to native FileClick and SetTarget;
                         * native target setup, movement, animation, CPU
                         * scheduling and progress will run unchanged. */
                        *out=row.native_row-baseoffset;
                        return 1;
                    }
                    logline("v0.16.1 ANIMATION_ARM_REJECTED screen=%d node=%d "
                            "(native target not verified; FILE row blocked)\n",
                            screen,view.node_index);
                    *out=1000000-baseoffset;
                    return 1;
                }
                if(caller==main_base+0x5c781ul && row.is_file &&
                   ufs_operation_decide(browser_operations_optin,
                                        browser_safe_delete_optin,row.is_file,
                                        0,browser_copier_is_targeted(),
                                        (char)view.mode)==UFS_OP_NATIVE_FILE_COPIER_SOURCE){
                    *out=row.native_row-baseoffset;
                    logline("v0.14 DOWNLOAD_SOURCE screen=%d node=%d mode=%c "
                            "gq=[%d,%d) native_row=%d (native File_Copier)\n",
                            screen,view.node_index,view.mode,row.first_gq,
                            row.first_gq+row.length_gq,row.native_row);
                    return 1;
                }
                if(ufs_click_policy_allow(browser_operations_optin,
                       row.is_file,row.first_gq,row.length_gq,
                       view.begin_gq,view.end_gq)){
                    /* Only FREE-SPACE destinations may be selected by native
                     * operations. A separately gated safe-delete path above
                     * handles occupied private file rows without native remove. */
                    *out=row.native_row-baseoffset;
                    logline("v0.14 CLICK_ROUTE screen=%d node=%d mode=%c "
                            "gq=[%d,%d) row=%d native=%d kind=FREE\n",
                            screen,view.node_index,view.mode,row.first_gq,
                            row.first_gq+row.length_gq,button_row+baseoffset,
                            row.native_row);
                    return 1;
                }
                if(browser_operations_optin && row.is_file &&
                   caller==main_base+0x5c781ul){
                    logline("v0.14 FILE_SELECTION_BLOCKED screen=%d "
                            "node=%d gq=%d (no authorized safe-delete target)\n",
                            screen,view.node_index,row.first_gq);
                }
            }
            if(translated_status<0 && !browser_click_reported){
                logline("v0.11 BROWSER_MAP REJECTED screen=%d "
                        "bank invariant failed, display blank\n",screen);
                browser_click_reported=1;
            }
        }
        if(caller==main_base+0x5c781ul && !browser_click_reported){
            logline("v0.14 CLICK_BLOCKED screen=%d "
                    "reason=%s\n",screen,
                    browser_operations_optin?"file selection or invalid row/partition":"read-only default");
            browser_click_reported=1;
        }
        /* Out-of-volume rows are intentionally BLANK, never passed through. */
        *out=1000000-baseoffset;
        return 1;
    }
    {
        int result=vsscanf(text,format,args);
        va_end(args);
        return result;
    }
}

/* dyld's official function interposition table: replacement, replacee. */
__attribute__((used,section("__DATA,__interpose")))
static const struct { void *replacement; void *replacee; } ufs_interpose_strstr={
    (void*)&ufs_strstr,(void*)&strstr
};
__attribute__((used,section("__DATA,__interpose")))
static const struct { void *replacement; void *replacee; } ufs_interpose_puts={
    (void*)&ufs_puts,(void*)&puts
};

__attribute__((used,section("__DATA,__interpose")))
static const struct { void *replacement; void *replacee; } ufs_interpose_sscanf={
    (void*)&ufs_sscanf,(void*)&sscanf
};

__attribute__((constructor))
static void ufs_start(void) {
    const char *home=getenv("HOME");
    char logpath[1900];
    int okay;
    if(home&&*home=='/'&&strlen(home)<1600){
        (void)snprintf(logpath,sizeof(logpath),"%s/Library/Logs/UplinkFileServer-Mac-Diagnostic.log",home);
        log_fd=open(logpath,O_WRONLY_MAC|O_APPEND_MAC|O_CREAT_MAC,0600);
    }
    logline("\n=== Uplink FILESERVER Mac v0.16.1 x86_64 grouped File_Deleter fix ===\n");
    {const char *flag=getenv("UPLINK_UFS09_OPERATIONS");
     browser_operations_optin=flag && eq(flag,"1");}
    {const char *flag=getenv("UPLINK_UFS10_SAFE_DELETE");
     browser_safe_delete_optin=flag && eq(flag,"1");}
    {const char *flag=getenv("UPLINK_UFS10_DRY_RUN");
     browser_safe_delete_dryrun=flag && eq(flag,"1");}
    {const char *flag=getenv("UPLINK_UFS15_GENERATE");
     private_population_enabled=!(flag && eq(flag,"0"));}
    {const char *flag=getenv("UPLINK_UFS16_MIGRATE_SAVED");
     saved_migration_enabled=flag && eq(flag,"1");}
    logline("timestamp=%ld pid=%d\n",time((long*)0),getpid());
    okay=validate_main_executable();
    logline("main_executable=%s\n",_dyld_get_image_name(0));
    logline("UUID_and_machine_code_validation=%s\n",okay?"PASS":"FAIL");
    logline("Interposition enabled for exact MAINSERVER parser site only; no code-page edits\n");
    if(okay){
        hook_armed=1;
        logline("v0.16.1 ARMED -- grouped File_Deleter arm restored; v0.16 partitions retained; "
                "generic private/group FILESERVER file generation=%s (saved migration opt-in=%s)\n",
                private_population_enabled?"ON":"OFF",
                saved_migration_enabled?"ON":"OFF");
        logline("v0.14 OPTIONS root_delete=NATIVE_FILEDELETER_SAFE "
                "download=NATIVE_FILE_COPIER remote_defrag=NOT_IMPLEMENTED "
                "automatic_repack=ON_REMOTE_DELETE_ONLY\n");
        logline("v0.16 UFS2 plan/layout guards ACTIVE; FREE clicks=%s FILE selection=%s "
                "(validated scoped File_Deleter only, no native deletion; dryrun=%s)\n",
                browser_operations_optin?"OPTED_IN":"BLOCKED",
                browser_safe_delete_optin&&browser_operations_optin?"SAFE_DELETE_OPTED_IN":"BLOCKED",
                browser_safe_delete_dryrun?"YES":"NO");
    } else logline("v0.14 DISABLED -- unknown game executable\n");
    /* Keep the descriptor open: FILESERVER is generated only during gameplay.
     * No game files or saves are accessed by the dylib itself. */
}
