/*
 * MountZero v4 - Public Header
 *
 * Simplified VFS-based hiding and uname spoofing.
 * No external dependencies (SUSFS-free).
 *
 * Target: 4.14, 5.x, 6.x kernels
 */

#ifndef __LINUX_MOUNTZERO_V4_H
#define __LINUX_MOUNTZERO_V4_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#define MZ4_IOC_MAGIC 'Z'

/* Core IOCTLs */
#define MZ4_IOC_GET_VERSION    _IOR(MZ4_IOC_MAGIC, 1, int)
#define MZ4_IOC_ENABLE         _IO(MZ4_IOC_MAGIC, 2)
#define MZ4_IOC_DISABLE        _IO(MZ4_IOC_MAGIC, 3)
#define MZ4_IOC_GET_STATUS    _IOR(MZ4_IOC_MAGIC, 4, int)

/* Path redirection */
#define MZ4_IOC_ADD_REDIRECT   _IOW(MZ4_IOC_MAGIC, 10, struct mz4_rule)
#define MZ4_IOC_DEL_REDIRECT   _IOW(MZ4_IOC_MAGIC, 11, char*)
#define MZ4_IOC_LIST_REDIRECT  _IOR(MZ4_IOC_MAGIC, 12, struct mz4_list)
#define MZ4_IOC_CLEAR_REDIRECT _IO(MZ4_IOC_MAGIC, 13)

/* Path hiding (VFS-based inode flagging) */
#define MZ4_IOC_ADD_HIDE_PATH  _IOW(MZ4_IOC_MAGIC, 20, char*)
#define MZ4_IOC_DEL_HIDE_PATH  _IOW(MZ4_IOC_MAGIC, 21, char*)
#define MZ4_IOC_LIST_HIDE      _IOR(MZ4_IOC_MAGIC, 22, struct mz4_list)
#define MZ4_IOC_CLEAR_HIDE     _IO(MZ4_IOC_MAGIC, 23)

/* Mount hiding (VFS-based) */
#define MZ4_IOC_ADD_HIDE_MOUNT _IOW(MZ4_IOC_MAGIC, 30, char*)
#define MZ4_IOC_DEL_HIDE_MOUNT _IOW(MZ4_IOC_MAGIC, 31, char*)
#define MZ4_IOC_SET_HIDE_MOUNTS _IOW(MZ4_IOC_MAGIC, 32, int)
#define MZ4_IOC_LIST_HIDE_MOUNT _IOR(MZ4_IOC_MAGIC, 33, struct mz4_list)
#define MZ4_IOC_CLEAR_HIDE_MOUNT _IO(MZ4_IOC_MAGIC, 34)

/* Maps hiding (for /proc/PID/maps) */
#define MZ4_IOC_ADD_HIDE_MAP   _IOW(MZ4_IOC_MAGIC, 40, char*)
#define MZ4_IOC_DEL_HIDE_MAP   _IOW(MZ4_IOC_MAGIC, 41, char*)
#define MZ4_IOC_LIST_HIDE_MAP  _IOR(MZ4_IOC_MAGIC, 42, struct mz4_list)
#define MZ4_IOC_CLEAR_HIDE_MAP _IO(MZ4_IOC_MAGIC, 43)

/* Uname spoofing */
#define MZ4_IOC_SET_UNAME      _IOW(MZ4_IOC_MAGIC, 50, struct mz4_uname)
#define MZ4_IOC_RESET_UNAME    _IO(MZ4_IOC_MAGIC, 51)
#define MZ4_IOC_GET_UNAME      _IOR(MZ4_IOC_MAGIC, 52, struct mz4_uname_status)

/* Cmdline spoofing */
#define MZ4_IOC_SET_CMDLINE    _IOW(MZ4_IOC_MAGIC, 60, char*)
#define MZ4_IOC_RESET_CMDLINE  _IO(MZ4_IOC_MAGIC, 61)

/* UID blocking */
#define MZ4_IOC_BLOCK_UID      _IOW(MZ4_IOC_MAGIC, 80, uid_t)
#define MZ4_IOC_UNBLOCK_UID    _IOW(MZ4_IOC_MAGIC, 81, uid_t)
#define MZ4_IOC_LIST_UIDS      _IOR(MZ4_IOC_MAGIC, 82, struct mz4_list)
#define MZ4_IOC_CLEAR_UIDS     _IO(MZ4_IOC_MAGIC, 83)

/* AVC log spoofing (SELinux) */
#define MZ4_IOC_ADD_AVC_SPOOF  _IOW(MZ4_IOC_MAGIC, 90, struct mz4_avc_spoof)
#define MZ4_IOC_DEL_AVC_SPOOF  _IOW(MZ4_IOC_MAGIC, 91, struct mz4_avc_spoof)
#define MZ4_IOC_CLEAR_AVC      _IO(MZ4_IOC_MAGIC, 92)
#define MZ4_IOC_SET_AVC_LOG    _IOW(MZ4_IOC_MAGIC, 93, int)

/* Module scanning */
#define MZ4_IOC_SCAN_MODULES   _IO(MZ4_IOC_MAGIC, 100)
#define MZ4_IOC_SCAN_SINGLE    _IOW(MZ4_IOC_MAGIC, 101, struct mz4_module)

/* Diagnostics */
#define MZ4_IOC_DUMP           _IO(MZ4_IOC_MAGIC, 110)

#define MZ4_MAX_PATH_LEN 512
#define MZ4_MAX_LIST_ENTRIES 256

struct mz4_rule {
    char virtual_path[MZ4_MAX_PATH_LEN];
    char real_path[MZ4_MAX_PATH_LEN];
    unsigned int flags;
};

struct mz4_uname {
    char release[64];
    char version[64];
};

struct mz4_uname_status {
    int spoofed;
    char release[64];
    char version[64];
    char stock_release[64];
    char stock_version[64];
};

struct mz4_module {
    char module_id[128];
    char module_path[MZ4_MAX_PATH_LEN];
    int is_custom;
};

struct mz4_avc_spoof {
    char scontext[128];
    char tcontext[128];
    char tclass[64];
};

struct mz4_list {
    char entries[16384];
    int count;
};

#ifdef __KERNEL__

#include <linux/mountzero_v4_def.h>

int mz4_init(void);
void mz4_exit(void);

bool mz4_should_redirect(const char *path);
char *mz4_resolve_path(const char *path);

int mz4_add_redirect(const char *virt, const char *real, unsigned int flags);
int mz4_del_redirect(const char *virt);

int mz4_add_hide_path(const char *path);
int mz4_del_hide_path(const char *path);
int mz4_is_path_hidden(const char *path);
int mz4_clear_hide_paths(void);

int mz4_add_hide_mount(const char *path);
int mz4_del_hide_mount(const char *path);
int mz4_is_mount_hidden(const char *mnt_path);
int mz4_set_hide_all_mounts(bool enable);
bool mz4_get_hide_all_mounts(void);
int mz4_clear_hide_mounts(void);

int mz4_add_hide_map(const char *path);
int mz4_del_hide_map(const char *path);
int mz4_is_map_hidden(const char *path);
int mz4_clear_hide_maps(void);

int mz4_set_uname(const char *release, const char *version);
int mz4_reset_uname(void);
int mz4_get_uname_status(char *buf, size_t len);

int mz4_set_cmdline(const char *cmdline);
int mz4_reset_cmdline(void);
char *mz4_get_fake_cmdline(void);

int mz4_scan_modules(void);
int mz4_scan_single_module(const char *id, const char *path, bool custom);

int mz4_add_hide_inode(struct inode *inode, const char *orig_path);
bool mz4_should_hide_inode(struct inode *inode);

/* UID blocking */
int mz4_block_uid(uid_t uid);
int mz4_unblock_uid(uid_t uid);
bool mz4_is_uid_blocked(uid_t uid);
int mz4_clear_blocked_uids(void);

/* AVC log spoofing */
int mz4_add_avc_spoof(const char *scontext, const char *tcontext, const char *tclass);
int mz4_del_avc_spoof(const char *scontext, const char *tcontext, const char *tclass);
int mz4_should_spoof_avc(const char *scontext, const char *tcontext, const char *tclass);
int mz4_set_avc_log_spoof(bool enable);
bool mz4_get_avc_log_spoof(void);
int mz4_clear_avc_spoof(void);

#endif

#endif
