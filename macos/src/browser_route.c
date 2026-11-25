#include "browser_route.h"
#include "active_layout.h"

/* All layouts are for the UUID-fingerprinted x86_64 Mac executable only.
 * Computer::databank +0x128; LanComputer::systems DArray +0x268.
 */
static unsigned long rp(const unsigned char *p, unsigned long off) {
    unsigned long x; __builtin_memcpy(&x,p+off,sizeof(x)); return x;
}
static int ri(const unsigned char *p, unsigned long off) {
    int x; __builtin_memcpy(&x,p+off,sizeof(x)); return x;
}
static int okay_ptr(unsigned long v){return v>0x10000ul && v%8==0;}

int ufs_plan_matches_native_lan(const void *computer,
                                const struct ufs_volume_plan *wanted){
    const unsigned char *c=(const unsigned char*)computer;
    struct ufs_volume_plan real;
    const unsigned long *ptrs;
    const unsigned char *valid;
    int slots,i;
    unsigned int v;
    if(!c||ri(c,0x20)!=64||!ufs_plan_is_valid(wanted))return 0;
    slots=ri(c,0x268+0x10);
    if(slots<1||slots>1024)return 0;
    ptrs=(const unsigned long*)rp(c,0x268+0x20);
    valid=(const unsigned char*)rp(c,0x268+0x28);
    if(!okay_ptr((unsigned long)ptrs)||!okay_ptr((unsigned long)valid))return 0;
    ufs_plan_reset(&real,(unsigned long)ptrs);
    for(i=0;i<slots;i++)if(valid[i]){
        const unsigned char *node;
        if(!okay_ptr(ptrs[i]))return 0;
        node=(const unsigned char*)ptrs[i];
        if(ri(node,0x08)==7){
            int ix;
            if(ufs_plan_register(&real,i,ri(node,0x28),&ix)<0)return 0;
        }
    }
    if(real.volume_count!=wanted->volume_count ||
       real.fileserver_count!=wanted->fileserver_count ||
       real.root_fileserver_count!=wanted->root_fileserver_count)return 0;
    for(v=0;v<real.volume_count;v++){
        const struct ufs_volume *a=&real.volumes[v],*b=&wanted->volumes[v];
        if(a->id.mode!=b->id.mode||a->id.key!=b->id.key||
           a->first_node!=b->first_node||a->members!=b->members)return 0;
    }
    return 1;
}

int ufs_browser_plan_for_screen(const void *computer,int screen_index,
                                struct ufs_browser_view *out,
                                struct ufs_volume_plan *out_plan){
    const unsigned char *c=(const unsigned char*)computer;
    const unsigned char *valid;
    const unsigned long *systems;
    struct ufs_volume_plan plan;
    struct ufs_bank_report report;
    int slots,i,matched=-1,match_type=-1,match_data1=0;
    if(!out || screen_index<0 || screen_index>8192)return 0;
    __builtin_memset(out,0,sizeof(*out));
    if(!c || ri(c,0x20)!=64)return 0; /* Computer::TYPE == LAN */
    slots=ri(c,0x268+0x10);
    if(slots<0||slots>1024)return -1;
    systems=(const unsigned long*)rp(c,0x268+0x20);
    valid=(const unsigned char*)rp(c,0x268+0x28);
    if(slots==0 || !okay_ptr((unsigned long)systems) ||
       !okay_ptr((unsigned long)valid))return -1;
    ufs_plan_reset(&plan,(unsigned long)systems);
    for(i=0;i<slots;i++){
        const unsigned char *system;
        int type,screen,data1;
        if(!valid[i])continue;
        if(!okay_ptr(systems[i]))return -1;
        system=(const unsigned char*)systems[i];
        type=ri(system,0x08);
        /* Native GenerateMainServer creates fssIndex=msIndex+4, but its
         * unmodified type-5 LanComputerSystem leaves dataScreenIndex=-1.
         * The v0.3 converted type-7 nodes explicitly store fssIndex at +0x24.
         */
        screen=(type==5 ? ri(system,0x20)+4 : ri(system,0x24));
        data1=ri(system,0x28);
        if(type==7){
            int volume_index;
            if(ufs_plan_register(&plan,i,data1,&volume_index)<0)return -1;
        }
        if((type==7 || type==5) && screen==screen_index){
            if(matched>=0)return -1; /* ambiguous screen ownership */
            matched=i;match_type=type;match_data1=data1;
        }
    }
    if(matched<0)return 0; /* Not a file browser. */
    if(plan.volume_count==0)return 0; /* Vanilla or root-only LAN. */
    if(!ufs_plan_is_valid(&plan))return -1;
    if(ufs_bank_inspect(c,&plan,&report)!=0)return -1;
    /* A pre-v0.16 saved custom LAN may have no UFS2 markers at all.
     * Expose it ONLY as a candidate for separately opted-in migration. */
    if(report.layout_state==0 && report.ufs_marker_count==0 &&
       report.orphan_markers==0 && !report.formatted &&
       report.bank_gq>0 && ufs_plan_ranges(&plan,report.bank_gq)==0){
        if(out_plan)*out_plan=plan;
        return 2;
    }
    if(report.layout_state!=1 ||
       report.orphan_markers || report.formatted ||
       report.ufs_marker_count!=(int)(2*plan.volume_count) || report.first_marker_gq<1 ||
       ufs_plan_ranges(&plan,report.first_marker_gq)!=0 ||
       plan.proposed_native_total_gq!=report.bank_gq ||
       ufs_verify_active_layout(c,&plan,report.first_marker_gq)!=0)return -1;
    out->node_index=matched;
    out->volume_slot=-1;
    out->screen_index=screen_index;
    out->data1=match_type==5?-1:match_data1;
    out->bank_gq=report.bank_gq;
    out->root_gq=report.first_marker_gq;
    out->total_files=report.file_count;
    out->marker_count=report.ufs_marker_count;
    if(match_type==5 || match_data1<=0){
        out->mode='R';out->begin_gq=0;out->end_gq=report.first_marker_gq;
    }else{
        int found=0;
        for(i=0;i<(int)plan.volume_count;i++){
            struct ufs_volume_key key;
            if(ufs_mode_to_key(match_data1,matched,&key)!=1)return -1;
            if(plan.volumes[i].id.mode==key.mode &&
               plan.volumes[i].id.key==key.key){
                out->mode=key.mode;
                out->volume_slot=i;
                out->begin_gq=plan.volumes[i].begin_marker+1;
                out->end_gq=plan.volumes[i].end_marker;
                found=1;break;
            }
        }
        if(!found)return -1;
    }
    if(out->begin_gq<0 || out->end_gq>out->bank_gq ||
       out->end_gq<=out->begin_gq)return -1;
    if(out_plan)*out_plan=plan;
    return 1;
}

int ufs_browser_view_for_screen(const void *computer,int screen_index,
                                struct ufs_browser_view *out){
    return ufs_browser_plan_for_screen(computer,screen_index,out,0);
}

/* Native FileServerScreenInterface::GetInfoRowDisplayDataBank (RVA 0x5ca30)
 * treats each contiguous file and each contiguous unallocated run as a row.
 * This mapper mirrors THAT function's enumeration (including trailing free
 * space) and returns the original full-bank row ordinal. The caller substitutes
 * it solely in the verified FileDraw sscanf callsite. FileClick is guarded by runtime opt-in.
 */
struct map_work {
    const struct ufs_browser_view *v;
    int wanted, native_cursor, visible, selected, error;
    int first_gq, length_gq, is_file;
};
static void emit(struct map_work *w,int begin,int end,int is_file,int marker){
    int visible;
    if(end<=begin || end> w->v->bank_gq || begin<0){w->error=1;return;}
    visible=!marker && begin>=w->v->begin_gq && end<=w->v->end_gq;
    if(visible){
        if(w->visible==w->wanted){
            w->selected=w->native_cursor;w->first_gq=begin;
            w->length_gq=end-begin;w->is_file=is_file;
        }
        w->visible++;
    }
    w->native_cursor++;
}
int ufs_browser_resolve_row(const void *computer,const struct ufs_browser_view *view,
                        int display_row,struct ufs_browser_row *out){
    const unsigned char *bank;
    const unsigned char *file_valid,*mem_valid,*f;
    const unsigned long *files;
    const int *memory;
    int fs,ms,i,last=-1,rows=0;
    unsigned long last_file=0;
    struct map_work w;
    if(!computer||!view||!out||
       display_row<0||display_row>20000)return -1;
    bank=(const unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    fs=ri(bank,0x18);ms=ri(bank,0x48);
    if(fs<0 || fs>UFS_NATIVE_LIMIT_FILES ||
       ms!=view->bank_gq || ms<1 || ms>UFS_NATIVE_LIMIT_SLOTS)return -1;
    files=(const unsigned long*)rp(bank,0x28);
    file_valid=(const unsigned char*)rp(bank,0x30);
    memory=(const int*)rp(bank,0x58);
    mem_valid=(const unsigned char*)rp(bank,0x60);
    if(!okay_ptr((unsigned long)files)|| !okay_ptr((unsigned long)file_valid)||
       !okay_ptr((unsigned long)memory)|| !okay_ptr((unsigned long)mem_valid))return -1;
    __builtin_memset(&w,0,sizeof(w));
    w.v=view;w.wanted=display_row;w.selected=-1;
    /* Same gate as native UI: if data slots are zero, entire memory is free. */
    if(fs>0){
        for(i=0;i<ms;i++){
            unsigned long current=0;
            int slot,size;
            if(!mem_valid[i])continue;
            slot=memory[i];
            if(slot<0||slot>=fs||!file_valid[slot])return -1;
            current=files[slot];
            if(!okay_ptr(current))return -1;
            if(current==last_file)continue;
            f=(const unsigned char*)current;
            size=ri(f,UFS_MAC_DATA_SIZE_OFFSET);
            if(size<=0 || i>ms-size || (last>=0 && i<last))return -1;
            if(last==-1){
                if(i>0){emit(&w,0,i,0,0);rows++;}
            }else if(i>last){
                emit(&w,last,i,0,0);rows++;
            }
            emit(&w,i,i+size,1,
                f[UFS_MAC_DATA_TITLE_OFFSET]=='U' &&
                f[UFS_MAC_DATA_TITLE_OFFSET+1]=='F' &&
                f[UFS_MAC_DATA_TITLE_OFFSET+2]=='S' &&
                f[UFS_MAC_DATA_TITLE_OFFSET+3]=='2' &&
                f[UFS_MAC_DATA_TITLE_OFFSET+4]=='.');
            rows++;
            last=i+size;last_file=current;
        }
        if(last>=0 && ms>last){emit(&w,last,ms,0,0);rows++;}
    }
    if(rows==0 && ms>0)emit(&w,0,ms,0,0);
    if(w.error)return -1;
    out->native_row=w.selected;
    out->visible_count=w.visible;
    out->first_gq=w.first_gq;
    out->length_gq=w.length_gq;
    out->is_file=w.is_file;
    return w.selected>=0 ? 1 : 0;
}

int ufs_browser_map_row(const void *computer,const struct ufs_browser_view *view,
                        int display_row,int *native_row,int *visible_count){
    struct ufs_browser_row row;
    int result;
    if(!native_row||!visible_count)return -1;
    result=ufs_browser_resolve_row(computer,view,display_row,&row);
    if(result>=0){*native_row=row.native_row;*visible_count=row.visible_count;}
    return result;
}
