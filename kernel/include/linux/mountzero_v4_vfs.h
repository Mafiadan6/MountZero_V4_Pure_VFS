/*
 * MountZero v4 - VFS Hooks Header
 *
 * VFS-level hook declarations for path redirection and hiding.
 */

#ifndef __LINUX_MOUNTZERO_V4_VFS_H
#define __LINUX_MOUNTZERO_V4_VFS_H

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>

#ifdef CONFIG_MOUNTZERO_V4

struct filename *mz4_vfs_getname_hook(struct filename *name);
char *mz4_vfs_get_vpath_for_inode(struct inode *inode);
bool mz4_vfs_should_hide_inode(struct inode *inode);
void mz4_vfs_spoof_mmap(struct inode *inode, dev_t *dev, unsigned long *ino);
int mz4_vfs_spoof_stat(struct inode *inode, struct kstat *stat);

#else

static inline struct filename *mz4_vfs_getname_hook(struct filename *name)
{
    return name;
}

static inline char *mz4_vfs_get_vpath_for_inode(struct inode *inode)
{
    return NULL;
}

static inline bool mz4_vfs_should_hide_inode(struct inode *inode)
{
    return false;
}

static inline void mz4_vfs_spoof_mmap(struct inode *inode, dev_t *dev, unsigned long *ino)
{
}

static inline int mz4_vfs_spoof_stat(struct inode *inode, struct kstat *stat)
{
    return 0;
}

#endif

#endif
