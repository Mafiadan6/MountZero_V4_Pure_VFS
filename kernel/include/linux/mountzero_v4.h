/*
 * MountZero v4 - Public API Header
 *
 * Provides hooks for kernel patches.
 */

#ifndef __LINUX_MOUNTZERO_V4_H
#define __LINUX_MOUNTZERO_V4_H

#include <linux/types.h>

/* Syscall families for path hiding */
#define SYSCALL_FAMILY_ALL_ENOENT   0
#define SYSCALL_FAMILY_MKNOD       1
#define SYSCALL_FAMILY_MKDIRAT      2
#define SYSCALL_FAMILY_RMDIR        3
#define SYSCALL_FAMILY_UNLINKAT     4
#define SYSCALL_FAMILY_SYMLINKAT_NEWNAME 5
#define SYSCALL_FAMILY_LINKAT_OLDNAME 6
#define SYSCALL_FAMILY_LINKAT_NEWNAME 7
#define SYSCALL_FAMILY_RENAMEAT2_OLDNAME 8
#define SYSCALL_FAMILY_RENAMEAT2_NEWNAME 9

#define MZ4_MAX_LEN_PATHNAME 256

/* Path hiding functions - called from kernel hooks */
extern int mz4_sus_path_by_filename(struct filename *fname, int *error, int family);
extern int mz4_sus_path_by_path(const struct path *path, int *error, int family);

/* Kstat spoofing - called from stat syscalls */
extern void mz4_sus_kstat(u64 ino, struct stat *statbuf);

/* Uname spoofing - called from newuname syscall */
extern void mz4_spoof_uname(struct new_utsname *uts);

/* Directory hiding - called from filldir */
extern int mz4_sus_ino_for_filldir64(u64 ino);

/* Mount hiding - called from /proc/mounts */
extern int mz4_sus_mount(struct vfsmount *mnt, struct path *root);

/* Maps hiding - called from /proc/PID/maps */
extern int mz4_sus_maps(unsigned long ino, unsigned long len, 
                       unsigned long *out_ino, dev_t *out_dev,
                       unsigned long *flags, unsigned long *pgoff,
                       struct vm_area_struct *vma, char *out_name);

/* Mount ID reordering */
extern int mz4_uid_need_to_reorder_mnt_id(void);
extern int mz4_get_fake_mnt_id(unsigned int real_id, int *fake_id, int *fake_parent_id);
extern int mz4_add_mnt_id_recorder(struct mnt_namespace *ns);
extern int mz4_remove_mnt_id_recorder(void);
extern void mz4_record_mnt_id(unsigned int mnt_id);

/* Check if inode should be hidden */
extern int mz4_should_hide_by_ino(u64 ino);

/* Enable/disable functions */
extern void mz4_set_enabled(int enable);
extern int mz4_get_enabled(void);

#endif