/*
 * MountZero v4 - VFS Hooks
 *
 * VFS-level hooks for path redirection and hiding.
 * Simple, lightweight implementation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mountzero_v4.h>
#include <linux/mountzero_v4_def.h>
#include <linux/mountzero_v4_vfs.h>

struct filename *mz4_vfs_getname_hook(struct filename *name)
{
    char *real_path;
    struct filename *new_name;

    if (!name || name->name[0] != '/')
        return name;

    if (atomic_read(&mz4_enabled) == 0)
        return name;

    real_path = mz4_resolve_path(name->name);
    if (!real_path)
        return name;

    new_name = getname_kernel(real_path);
    kfree(real_path);

    if (IS_ERR(new_name))
        return name;

    putname(name);
    return new_name;
}
EXPORT_SYMBOL(mz4_vfs_getname_hook);

char *mz4_vfs_get_vpath_for_inode(struct inode *inode)
{
    struct mz4_rule *rule;
    unsigned long key;
    unsigned long flags;
    char *copy = NULL;

    if (!inode || !inode->i_sb)
        return NULL;

    if (atomic_read(&mz4_enabled) == 0)
        return NULL;

    key = inode->i_ino ^ inode->i_sb->s_dev;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_ino_ht, rule, ino_node, key) {
        if (rule->real_ino == inode->i_ino &&
            rule->real_dev == inode->i_sb->s_dev) {
            copy = kstrdup(rule->virtual_path, GFP_ATOMIC);
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return copy;
}
EXPORT_SYMBOL(mz4_vfs_get_vpath_for_inode);

bool mz4_vfs_should_hide_inode(struct inode *inode)
{
    struct mz4_rule *rule;
    unsigned long key;
    unsigned long flags;

    if (!inode || !inode->i_sb)
        return false;

    if (atomic_read(&mz4_enabled) == 0)
        return false;

    key = inode->i_ino ^ inode->i_sb->s_dev;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_ino_ht, rule, ino_node, key) {
        if (rule->real_ino == inode->i_ino &&
            rule->real_dev == inode->i_sb->s_dev) {
            spin_unlock_irqrestore(&mz4_lock, flags);
            return true;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return false;
}
EXPORT_SYMBOL(mz4_vfs_should_hide_inode);

void mz4_vfs_spoof_mmap(struct inode *inode, dev_t *dev, unsigned long *ino)
{
    struct mz4_rule *rule;
    unsigned long key;
    unsigned long flags;

    if (!inode || !inode->i_sb)
        return;

    if (atomic_read(&mz4_enabled) == 0)
        return;

    key = inode->i_ino ^ inode->i_sb->s_dev;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_ino_ht, rule, ino_node, key) {
        if (rule->real_ino == inode->i_ino &&
            rule->real_dev == inode->i_sb->s_dev) {
            *dev = inode->i_sb->s_dev;
            *ino = inode->i_ino + 1;
            spin_unlock_irqrestore(&mz4_lock, flags);
            return;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
}
EXPORT_SYMBOL(mz4_vfs_spoof_mmap);

int mz4_vfs_spoof_stat(struct inode *inode, struct kstat *stat)
{
    struct mz4_rule *rule;
    unsigned long key;
    unsigned long flags;
    int spoofed = 0;

    if (!inode || !inode->i_sb || !stat)
        return 0;

    if (atomic_read(&mz4_enabled) == 0)
        return 0;

    key = inode->i_ino ^ inode->i_sb->s_dev;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_ino_ht, rule, ino_node, key) {
        if (rule->real_ino == inode->i_ino &&
            rule->real_dev == inode->i_sb->s_dev) {
            stat->ino = inode->i_ino + 1;
            spoofed = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return spoofed;
}
EXPORT_SYMBOL(mz4_vfs_spoof_stat);

static int __init mz4_vfs_init(void)
{
    pr_info("MZ4: VFS hooks initialized\n");
    return 0;
}

static void __exit mz4_vfs_exit(void)
{
    pr_info("MZ4: VFS hooks unloaded\n");
}

module_init(mz4_vfs_init);
module_exit(mz4_vfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mastermind");
MODULE_DESCRIPTION("MountZero v4 VFS hooks");
MODULE_VERSION(MZ4_VERSION);
