#include "ufs_compact_native.h"
#include "native_bank.h"
#define CP __builtin_memcpy
static int i32(const unsigned char *b,int off){int x;CP(&x,b+off,4);return x;}
static unsigned long p64(const unsigned char *b,int off){unsigned long x;CP(&x,b+off,8);return x;}
static int sane(unsigned long x){return x>0x10000ul && !(x&7ul);}
struct context {void *computer;struct ufs_browser_view view;int slots;unsigned long *files;unsigned char *file_valid;};
static int get_size(void *p,int slot){
    struct context *c=(struct context*)p;
    const unsigned char *f;
    int size;
    if(slot<0||slot>=c->slots||!c->file_valid[slot]||!sane(c->files[slot]))return -1;
    f=(const unsigned char*)c->files[slot];
    if(f[UFS_MAC_DATA_TITLE_OFFSET]=='U' && f[UFS_MAC_DATA_TITLE_OFFSET+1]=='F' &&
       f[UFS_MAC_DATA_TITLE_OFFSET+2]=='S' && f[UFS_MAC_DATA_TITLE_OFFSET+3]=='2' &&
       f[UFS_MAC_DATA_TITLE_OFFSET+4]=='.')return -1;
    size=i32(f,UFS_MAC_DATA_SIZE_OFFSET);
    return size>0 ? size : -1;
}
static int verify(void *p){
    struct context *c=(struct context*)p;
    struct ufs_browser_view fresh;
    return ufs_browser_view_for_screen(c->computer,c->view.screen_index,&fresh)==1 &&
       fresh.mode==c->view.mode && fresh.node_index==c->view.node_index &&
       fresh.begin_gq==c->view.begin_gq && fresh.end_gq==c->view.end_gq &&
       fresh.marker_count==c->view.marker_count && fresh.bank_gq==c->view.bank_gq;
}
int ufs_compact_native(void *computer,const struct ufs_browser_view *view,
                       struct ufs_compact_result *out){
    unsigned char *bank,*mv;int *m;struct context c;
    struct ufs_browser_view fresh;
    int slots,bank_size;
    if(!computer||!view||!out||view->marker_count<2 || (view->marker_count&1))return -1;
    if(ufs_browser_view_for_screen(computer,view->screen_index,&fresh)!=1 ||
       fresh.mode!=view->mode || fresh.node_index!=view->node_index ||
       fresh.begin_gq!=view->begin_gq || fresh.end_gq!=view->end_gq ||
       fresh.bank_gq!=view->bank_gq)return -2;
    bank=(unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    slots=i32(bank,0x18);bank_size=i32(bank,0x48);
    if(slots<1||slots>UFS_NATIVE_LIMIT_FILES||bank_size!=view->bank_gq ||
       bank_size<=0||bank_size>UFS_NATIVE_LIMIT_SLOTS)return -3;
    c.computer=computer;c.view=*view;c.slots=slots;
    c.files=(unsigned long*)p64(bank,0x28);c.file_valid=(unsigned char*)p64(bank,0x30);
    m=(int*)p64(bank,0x58);mv=(unsigned char*)p64(bank,0x60);
    if(!sane((unsigned long)c.files)||!sane((unsigned long)c.file_valid)||
       !sane((unsigned long)m)||!sane((unsigned long)mv))return -4;
    return ufs_compact_range(bank_size,view->begin_gq,view->end_gq,slots,
                             m,mv,get_size,&c,verify,out);
}
