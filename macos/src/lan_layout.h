/*
 * Verified x86_64 Uplink 1.6.9 native structures used by v0.3.
 * All offsets are relative to actual native objects, not the file format.
 */
#ifndef UFS_LAN_LAYOUT_H
#define UFS_LAN_LAYOUT_H

typedef unsigned long ufs_uptr;

static unsigned long ufs_load_ulong(const void *p) {
    unsigned long v;
    __builtin_memcpy(&v,p,sizeof(v));
    return v;
}
static int ufs_load_int(const void *p) {
    int v;
    __builtin_memcpy(&v,p,sizeof(v));
    return v;
}
static void ufs_store_int(void *p, int value) {
    __builtin_memcpy(p,&value,sizeof(value));
}

/*
 * LanCluster::systems DArray: capacity at +0x10, allocation count +0x18,
 * ptr array +0x20 and validity array +0x28.
 * Computer::screens DArray: capacity +0x108, pointer array +0x118,
 * validity array +0x120.
 * LanComputerSystem: TYPE +0x08, screenIndex +0x20,
 * dataScreenIndex +0x24, data1-3 +0x28/+0x2c/+0x30.
 */
static int ufs_convert_main_to_file(void *computer, void *cluster,
                                     int index,
                                     void (*set_next_page)(void *,int),
                                     void (*set_main_title)(void *,const char *)) {
    unsigned char *co = (unsigned char *)computer;
    unsigned char *cl = (unsigned char *)cluster;
    unsigned long sys_cap,screen_cap,sys_ptrs,screen_ptrs,sys_valid,screen_valid;
    unsigned char *system,*password,*file;
    int msg_index,pass_index,file_index;
    if (!co || !cl || !set_next_page || !set_main_title || index<0 || index>2048) return 0;

    sys_cap=ufs_load_ulong(cl+0x10);
    if ((unsigned long)index>=sys_cap || sys_cap>4096) return 0;
    sys_ptrs=ufs_load_ulong(cl+0x20);
    sys_valid=ufs_load_ulong(cl+0x28);
    if (!sys_ptrs || !sys_valid || !((unsigned char *)sys_valid)[index]) return 0;
    system=(unsigned char *)ufs_load_ulong((void *)(sys_ptrs+(unsigned long)index*8));
    if (!system || ufs_load_int(system+0x08)!=5) return 0;
    msg_index=ufs_load_int(system+0x20);
    if (msg_index<0 || msg_index>4090) return 0;
    pass_index=msg_index+1;
    file_index=msg_index+4;

    screen_cap=ufs_load_ulong(co+0x108);
    if ((unsigned long)file_index>=screen_cap || screen_cap>8192) return 0;
    screen_ptrs=ufs_load_ulong(co+0x118);
    screen_valid=ufs_load_ulong(co+0x120);
    if (!screen_ptrs || !screen_valid ||
        !((unsigned char*)screen_valid)[pass_index] ||
        !((unsigned char*)screen_valid)[file_index]) return 0;
    password=(unsigned char *)ufs_load_ulong((void *)(screen_ptrs+(unsigned long)pass_index*8));
    file=(unsigned char *)ufs_load_ulong((void *)(screen_ptrs+(unsigned long)file_index*8));
    if (!password || !file) return 0;

    /* Native PasswordScreen::SetNextPage: RVA 0xe77f0, writes +0xc8. */
    set_next_page(password,file_index);
    set_main_title(password,"File Server");
    /* v0.4 cosmetic correction: the native FileServerScreen was still titled
     * "Main Server" after the v0.3 conversion. This setter is common to all
     * native ComputerScreen subclasses and was already verified in v0.3. */
    set_main_title(file,"File Server");
    ufs_store_int(system+0x08,7);
    ufs_store_int(system+0x20,pass_index);
    ufs_store_int(system+0x24,file_index);
    return 1;
}
#endif
