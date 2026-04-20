/*
 * MountZero v4 - Hook Functions
 *
 * These functions provide the hooks required by kernel patches.
 * Based on official SUSFS4KSU functionality.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/nsproxy.h>
#include <linux/bitmap.h>
#include <linux/mountzero_v4.h>
#include <linux/mountzero_v4_def.h>

/*
 * SUSFS Compatibility Layer
 * The kernel patches call these functions
 */

/* Called from fs/namei.c - path hiding */
int mz4_sus_path_by_filename(struct filename *fname, int *error, int family)
{
    if (!fname || !mz4_enabled)
        return 0;
    
    /* Check if path should be hidden */
    if (mz4_is_path_hidden(fname->name)) {
        *error = -ENOENT;
        return 1;
    }
    return 0;
}
EXPORT_SYMBOL(mz4_sus_path_by_filename);

/* Called from fs/namei.c - check by path */
int mz4_sus_path_by_path(const struct path *path, int *error, int family)
{
    if (!path || !path->dentry || !mz4_enabled)
        return 0;
    
    /* Check if path should be hidden */
    if (mz4_should_hide_inode(path->dentry->d_inode)) {
        *error = -ENOENT;
        return 1;
    }
    return 0;
}
EXPORT_SYMBOL(mz4_sus_path_by_path);

/* Called from fs/stat.c - kstat spoofing */
void mz4_sus_kstat(u64 ino, struct stat *statbuf)
{
    struct mz4_kstat_entry *entry;
    unsigned long flags;
    u32 hash;

    if (!statbuf || atomic_read(&mz4_enabled) == 0)
        return;

    hash = mz4_hash_string((const char *)(uintptr_t)ino);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_kstat_ht, entry, node, hash) {
        if (entry->hash == hash && entry->fake_ino == ino) {
            statbuf->st_ino = entry->fake_ino;
            statbuf->st_dev = entry->fake_dev;
            statbuf->st_size = entry->fake_size;
            statbuf->st_blocks = entry->fake_blocks;
            statbuf->st_atim = entry->fake_atime;
            statbuf->st_mtim = entry->fake_mtime;
            statbuf->st_ctim = entry->fake_ctime;
            spin_unlock_irqrestore(&mz4_lock, flags);
            return;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
}
EXPORT_SYMBOL(mz4_sus_kstat);

/* Called from kernel/sys.c - uname spoofing */
void mz4_spoof_uname(struct new_utsname *uts)
{
    if (!mz4_uname.enabled)
        return;
    
    if (mz4_uname.release[0])
        strncpy(uts->release, mz4_uname.release, sizeof(uts->release) - 1);
    if (mz4_uname.version[0])
        strncpy(uts->version, mz4_uname.version, sizeof(uts->version) - 1);
}
EXPORT_SYMBOL(mz4_spoof_uname);

/* Called from fs/readdir.c - hide entries */
int mz4_sus_ino_for_filldir64(u64 ino)
{
    /* Check if this inode should be hidden */
    return mz4_should_hide_by_ino(ino);
}
EXPORT_SYMBOL(mz4_sus_ino_for_filldir64);

/* Check if inode should be hidden */
int mz4_should_hide_by_ino(u64 ino)
{
    struct mz4_ino_entry *entry;
    unsigned long flags;
    unsigned long key;
    int hidden = 0;

    if (atomic_read(&mz4_enabled) == 0)
        return 0;

    key = ino;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_ino_ht, entry, node, key) {
        if (entry->real_ino == ino) {
            hidden = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return hidden;
}
EXPORT_SYMBOL(mz4_should_hide_by_ino);

/* Mount hiding functions - called from fs/proc_namespace.c */
int mz4_sus_mount(struct vfsmount *mnt, struct path *root)
{
    if (!mnt || !root || atomic_read(&mz4_enabled) == 0)
        return 0;

    if (mz4_hide_all_mounts)
        return 1;

    return 0;
}
EXPORT_SYMBOL(mz4_sus_mount);

/* Mount ID reordering - called from fs/proc_namespace.c */
bool mz4_uid_need_to_reorder_mnt_id(void)
{
    return mz4_mount_id_remap;
}
EXPORT_SYMBOL(mz4_uid_need_to_reorder_mnt_id);

int mz4_get_fake_mnt_id(unsigned int mnt_id, int *out_mnt_id, int *out_parent_mnt_id)
{
    if (!out_mnt_id || !out_parent_mnt_id)
        return 0;

    *out_mnt_id = mz4_remap_mount_id(mnt_id);
    *out_parent_mnt_id = mz4_remap_mount_id(mnt_id - 1);

    return 1;
}
EXPORT_SYMBOL(mz4_get_fake_mnt_id);

/* Mount ID recorder - tracks mount IDs for reordering */
static struct mnt_namespace *mz4_record_ns;
static unsigned int mz4_recorded_ids[MZ4_MAX_HIDE_MOUNTS];
static int mz4_recorded_count = 0;

int mz4_add_mnt_id_recorder(struct mnt_namespace *ns)
{
    if (!ns)
        return 0;

    mz4_record_ns = ns;
    mz4_recorded_count = 0;
    memset(mz4_recorded_ids, 0, sizeof(mz4_recorded_ids));

    return 0;
}
EXPORT_SYMBOL(mz4_add_mnt_id_recorder);

int mz4_remove_mnt_id_recorder(void)
{
    mz4_record_ns = NULL;
    mz4_recorded_count = 0;

    return 0;
}
EXPORT_SYMBOL(mz4_remove_mnt_id_recorder);

static DECLARE_BITMAP(mz4_mount_id_seen, 65536);

void mz4_record_mnt_id(unsigned int mnt_id)
{
    if (mnt_id >= 65535)
        return;

    if (!test_bit(mnt_id, mz4_mount_id_seen)) {
        set_bit(mnt_id, mz4_mount_id_seen);
        if (mz4_recorded_count < MZ4_MAX_HIDE_MOUNTS) {
            mz4_recorded_ids[mz4_recorded_count++] = mnt_id;
            mz4_add_mount_id_mapping(mnt_id);
        }
    }
}
EXPORT_SYMBOL(mz4_record_mnt_id);

MODULE_LICENSE("GPL v3");
MODULE_AUTHOR("MountZero");
MODULE_DESCRIPTION("MountZero v4 - VFS Hook Functions");