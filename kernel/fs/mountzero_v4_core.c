/*
 * MountZero v4 - Core Driver
 *
 * Simplified VFS-based path redirection, hiding, and uname spoofing.
 * NO external dependencies (SUSFS-free).
 *
 * Works with KernelSU/ReSukiSU on 4.14, 5.x, 6.x kernels.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/cred.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/file.h>
#include <linux/bitmap.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/utsname.h>
#include <linux/mountzero_v4.h>
#include <linux/mountzero_v4_def.h>

atomic_t mz4_enabled = ATOMIC_INIT(0);
EXPORT_SYMBOL(mz4_enabled);

int mz4_debug = 0;
EXPORT_SYMBOL(mz4_debug);

spinlock_t mz4_lock;
EXPORT_SYMBOL(mz4_lock);

struct hlist_head mz4_redirect_ht[1 << MZ4_HASH_BITS];
struct hlist_head mz4_hide_path_ht[1 << MZ4_HASH_BITS];
struct hlist_head mz4_hide_mount_ht[1 << MZ4_HASH_BITS];
struct hlist_head mz4_hide_map_ht[1 << MZ4_HASH_BITS];
struct hlist_head mz4_ino_ht[1 << MZ4_HASH_BITS];
struct hlist_head mz4_uid_ht[1 << MZ4_HASH_BITS];
struct hlist_head mz4_avc_ht[1 << MZ4_HASH_BITS];

atomic_t mz4_rule_count = ATOMIC_INIT(0);
atomic_t mz4_hide_path_count = ATOMIC_INIT(0);
atomic_t mz4_hide_mount_count = ATOMIC_INIT(0);
atomic_t mz4_hide_map_count = ATOMIC_INIT(0);
atomic_t mz4_uid_count = ATOMIC_INIT(0);
atomic_t mz4_avc_count = ATOMIC_INIT(0);

static DECLARE_BITMAP(mz4_bloom, MZ4_BLOOM_BITS);
static bool mz4_hide_all_mounts = false;

static char mz4_stock_release[__NEW_UTS_LEN + 1] = {0};
static char mz4_stock_version[__NEW_UTS_LEN + 1] = {0};
static char mz4_spoofed_release[__NEW_UTS_LEN + 1] = {0};
static char mz4_spoofed_version[__NEW_UTS_LEN + 1] = {0};
static bool mz4_stock_saved = false;
static DEFINE_MUTEX(mz4_uname_mutex);

static char *mz4_fake_cmdline = NULL;
static bool mz4_cmdline_set = false;
static DEFINE_MUTEX(mz4_cmdline_mutex);

static bool mz4_avc_log_spoof_enabled = false;
static DEFINE_MUTEX(mz4_avc_mutex);

static struct kobject *mz4_kobj;

static ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%s\n", MZ4_VERSION);
}

static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "enabled=%d\nredirects=%d\nhide_paths=%d\nhide_mounts=%d\nhide_maps=%d\n",
                      atomic_read(&mz4_enabled),
                      atomic_read(&mz4_rule_count),
                      atomic_read(&mz4_hide_path_count),
                      atomic_read(&mz4_hide_mount_count),
                      atomic_read(&mz4_hide_map_count));
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%d\n", mz4_debug);
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr,
                            const char *buf, size_t count)
{
    int level;
    if (kstrtoint(buf, 10, &level) == 0) {
        mz4_debug = clamp(level, 0, 2);
    }
    return count;
}

static struct kobj_attribute mz4_version_attr = __ATTR_RO(version);
static struct kobj_attribute mz4_status_attr = __ATTR_RO(status);
static struct kobj_attribute mz4_debug_attr = __ATTR_RW(debug);

static struct attribute *mz4_attrs[] = {
    &mz4_version_attr.attr,
    &mz4_status_attr.attr,
    &mz4_debug_attr.attr,
    NULL,
};

static struct attribute_group mz4_attr_group = {
    .attrs = mz4_attrs,
};

static inline u32 mz4_hash_string(const char *str)
{
    u32 hash = 0;
    if (!str) return 0;
    while (*str) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static inline void mz4_bloom_add(u32 hash)
{
    set_bit(hash & MZ4_BLOOM_MASK, mz4_bloom);
    set_bit((hash >> 8) & MZ4_BLOOM_MASK, mz4_bloom);
    set_bit((hash >> 16) & MZ4_BLOOM_MASK, mz4_bloom);
}

static inline bool mz4_bloom_test(u32 hash)
{
    return test_bit(hash & MZ4_BLOOM_MASK, mz4_bloom) &&
           test_bit((hash >> 8) & MZ4_BLOOM_MASK, mz4_bloom) &&
           test_bit((hash >> 16) & MZ4_BLOOM_MASK, mz4_bloom);
}

static void mz4_save_stock_uname(void)
{
    down_read(&uts_sem);
    strscpy(mz4_stock_release, utsname()->release, sizeof(mz4_stock_release));
    strscpy(mz4_stock_version, utsname()->version, sizeof(mz4_stock_version));
    up_read(&uts_sem);
    mz4_stock_saved = true;
    pr_info("MZ4: Stock uname saved: %s %s\n", mz4_stock_release, mz4_stock_version);
}

bool mz4_should_redirect(const char *path)
{
    struct mz4_rule *rule;
    u32 hash;
    unsigned long flags;

    if (atomic_read(&mz4_enabled) == 0 || !path)
        return false;

    hash = mz4_hash_string(path);
    if (!mz4_bloom_test(hash))
        return false;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_redirect_ht, rule, node, hash) {
        if (rule->hash == hash && strcmp(rule->virtual_path, path) == 0) {
            spin_unlock_irqrestore(&mz4_lock, flags);
            return true;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return false;
}
EXPORT_SYMBOL(mz4_should_redirect);

char *mz4_resolve_path(const char *path)
{
    struct mz4_rule *rule;
    u32 hash;
    unsigned long flags;
    char *resolved = NULL;

    if (!path) return NULL;

    hash = mz4_hash_string(path);
    if (!mz4_bloom_test(hash))
        return NULL;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_redirect_ht, rule, node, hash) {
        if (rule->hash == hash && strcmp(rule->virtual_path, path) == 0) {
            resolved = kstrdup(rule->real_path, GFP_ATOMIC);
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return resolved;
}
EXPORT_SYMBOL(mz4_resolve_path);

int mz4_add_redirect(const char *virt, const char *real, unsigned int flags)
{
    struct mz4_rule *rule;
    u32 hash;
    unsigned long irq_flags;

    if (!virt || !real) return -EINVAL;

    rule = kzalloc(sizeof(*rule), GFP_KERNEL);
    if (!rule) return -ENOMEM;

    rule->virtual_path = kstrdup(virt, GFP_KERNEL);
    rule->real_path = kstrdup(real, GFP_KERNEL);
    if (!rule->virtual_path || !rule->real_path) {
        kfree(rule->virtual_path);
        kfree(rule->real_path);
        kfree(rule);
        return -ENOMEM;
    }

    rule->flags = flags;
    hash = mz4_hash_string(virt);
    rule->hash = hash;

    {
        struct path p;
        if (kern_path(real, LOOKUP_FOLLOW, &p) == 0) {
            struct inode *inode = d_backing_inode(p.dentry);
            if (inode) {
                rule->real_ino = inode->i_ino;
                rule->real_dev = inode->i_sb->s_dev;
                spin_lock_irqsave(&mz4_lock, irq_flags);
                hash_add(mz4_ino_ht, &rule->ino_node, inode->i_ino ^ inode->i_sb->s_dev);
                spin_unlock_irqrestore(&mz4_lock, irq_flags);
            }
            path_put(&p);
        }
    }

    spin_lock_irqsave(&mz4_lock, irq_flags);
    hash_add(mz4_redirect_ht, &rule->node, hash);
    mz4_bloom_add(hash);
    atomic_inc(&mz4_rule_count);
    spin_unlock_irqrestore(&mz4_lock, irq_flags);

    if (mz4_debug > 0)
        pr_info("MZ4: Redirect added: %s -> %s\n", virt, real);
    return 0;
}
EXPORT_SYMBOL(mz4_add_redirect);

int mz4_del_redirect(const char *virt)
{
    struct mz4_rule *rule;
    u32 hash;
    unsigned long flags;
    int found = 0;

    if (!virt) return -EINVAL;
    hash = mz4_hash_string(virt);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_redirect_ht, rule, node, hash) {
        if (rule->hash == hash && strcmp(rule->virtual_path, virt) == 0) {
            hash_del(&rule->node);
            if (!hlist_unhashed(&rule->ino_node))
                hash_del(&rule->ino_node);
            kfree(rule->virtual_path);
            kfree(rule->real_path);
            kfree(rule);
            atomic_dec(&mz4_rule_count);
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(mz4_del_redirect);

int mz4_add_hide_path(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;

    if (!path) return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return -ENOMEM;

    entry->path = kstrdup(path, GFP_KERNEL);
    if (!entry->path) {
        kfree(entry);
        return -ENOMEM;
    }

    hash = mz4_hash_string(path);
    entry->hash = hash;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_add(mz4_hide_path_ht, &entry->node, hash);
    atomic_inc(&mz4_hide_path_count);
    spin_unlock_irqrestore(&mz4_lock, flags);

    if (mz4_debug > 0)
        pr_info("MZ4: Hide path added: %s\n", path);
    return 0;
}
EXPORT_SYMBOL(mz4_add_hide_path);

int mz4_del_hide_path(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;
    int found = 0;

    if (!path) return -EINVAL;
    hash = mz4_hash_string(path);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_hide_path_ht, entry, node, hash) {
        if (entry->hash == hash && strcmp(entry->path, path) == 0) {
            hash_del(&entry->node);
            kfree(entry->path);
            kfree(entry);
            atomic_dec(&mz4_hide_path_count);
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(mz4_del_hide_path);

int mz4_is_path_hidden(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;
    int hidden = 0;

    if (!path) return 0;
    hash = mz4_hash_string(path);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_hide_path_ht, entry, node, hash) {
        if (entry->hash == hash && strcmp(entry->path, path) == 0) {
            hidden = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return hidden;
}
EXPORT_SYMBOL(mz4_is_path_hidden);

int mz4_clear_hide_paths(void)
{
    struct mz4_hide_entry *entry;
    struct hlist_node *tmp;
    unsigned long flags;
    int bkt;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_safe(mz4_hide_path_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->path);
        kfree(entry);
    }
    atomic_set(&mz4_hide_path_count, 0);
    spin_unlock_irqrestore(&mz4_lock, flags);
    return 0;
}
EXPORT_SYMBOL(mz4_clear_hide_paths);

int mz4_add_hide_mount(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;

    if (!path) return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return -ENOMEM;

    entry->path = kstrdup(path, GFP_KERNEL);
    if (!entry->path) {
        kfree(entry);
        return -ENOMEM;
    }

    hash = mz4_hash_string(path);
    entry->hash = hash;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_add(mz4_hide_mount_ht, &entry->node, hash);
    atomic_inc(&mz4_hide_mount_count);
    spin_unlock_irqrestore(&mz4_lock, flags);

    if (mz4_debug > 0)
        pr_info("MZ4: Hide mount added: %s\n", path);
    return 0;
}
EXPORT_SYMBOL(mz4_add_hide_mount);

int mz4_del_hide_mount(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;
    int found = 0;

    if (!path) return -EINVAL;
    hash = mz4_hash_string(path);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_hide_mount_ht, entry, node, hash) {
        if (entry->hash == hash && strcmp(entry->path, path) == 0) {
            hash_del(&entry->node);
            kfree(entry->path);
            kfree(entry);
            atomic_dec(&mz4_hide_mount_count);
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(mz4_del_hide_mount);

int mz4_is_mount_hidden(const char *mnt_path)
{
    struct mz4_hide_entry *entry;
    unsigned long flags;

    if (!mnt_path) return 0;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_hide_mount_ht, entry, node, mz4_hash_string(mnt_path)) {
        if (strstr(mnt_path, entry->path)) {
            spin_unlock_irqrestore(&mz4_lock, flags);
            return 1;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return 0;
}
EXPORT_SYMBOL(mz4_is_mount_hidden);

int mz4_set_hide_all_mounts(bool enable)
{
    mz4_hide_all_mounts = enable;
    pr_info("MZ4: Hide all mounts: %s\n", enable ? "enabled" : "disabled");
    return 0;
}
EXPORT_SYMBOL(mz4_set_hide_all_mounts);

bool mz4_get_hide_all_mounts(void)
{
    return mz4_hide_all_mounts;
}
EXPORT_SYMBOL(mz4_get_hide_all_mounts);

int mz4_clear_hide_mounts(void)
{
    struct mz4_hide_entry *entry;
    struct hlist_node *tmp;
    unsigned long flags;
    int bkt;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_safe(mz4_hide_mount_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->path);
        kfree(entry);
    }
    atomic_set(&mz4_hide_mount_count, 0);
    spin_unlock_irqrestore(&mz4_lock, flags);
    return 0;
}
EXPORT_SYMBOL(mz4_clear_hide_mounts);

int mz4_add_hide_map(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;

    if (!path) return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return -ENOMEM;

    entry->path = kstrdup(path, GFP_KERNEL);
    if (!entry->path) {
        kfree(entry);
        return -ENOMEM;
    }

    hash = mz4_hash_string(path);
    entry->hash = hash;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_add(mz4_hide_map_ht, &entry->node, hash);
    atomic_inc(&mz4_hide_map_count);
    spin_unlock_irqrestore(&mz4_lock, flags);

    if (mz4_debug > 0)
        pr_info("MZ4: Hide map added: %s\n", path);
    return 0;
}
EXPORT_SYMBOL(mz4_add_hide_map);

int mz4_del_hide_map(const char *path)
{
    struct mz4_hide_entry *entry;
    u32 hash;
    unsigned long flags;
    int found = 0;

    if (!path) return -EINVAL;
    hash = mz4_hash_string(path);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_hide_map_ht, entry, node, hash) {
        if (entry->hash == hash && strcmp(entry->path, path) == 0) {
            hash_del(&entry->node);
            kfree(entry->path);
            kfree(entry);
            atomic_dec(&mz4_hide_map_count);
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(mz4_del_hide_map);

int mz4_is_map_hidden(const char *path)
{
    struct mz4_hide_entry *entry;
    unsigned long flags;

    if (!path) return 0;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_hide_map_ht, entry, node, mz4_hash_string(path)) {
        if (strstr(path, entry->path)) {
            spin_unlock_irqrestore(&mz4_lock, flags);
            return 1;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);
    return 0;
}
EXPORT_SYMBOL(mz4_is_map_hidden);

int mz4_clear_hide_maps(void)
{
    struct mz4_hide_entry *entry;
    struct hlist_node *tmp;
    unsigned long flags;
    int bkt;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_safe(mz4_hide_map_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->path);
        kfree(entry);
    }
    atomic_set(&mz4_hide_map_count, 0);
    spin_unlock_irqrestore(&mz4_lock, flags);
    return 0;
}
EXPORT_SYMBOL(mz4_clear_hide_maps);

int mz4_add_hide_inode(struct inode *inode, const char *orig_path)
{
    struct mz4_ino_entry *entry;
    unsigned long key;
    unsigned long flags;

    if (!inode) return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return -ENOMEM;

    entry->key = inode->i_ino ^ (inode->i_sb ? inode->i_sb->s_dev : 0);
    entry->real_ino = inode->i_ino;
    entry->real_dev = inode->i_sb ? inode->i_sb->s_dev : 0;
    entry->orig_path = kstrdup(orig_path, GFP_KERNEL);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_add(mz4_ino_ht, &entry->node, entry->key);
    spin_unlock_irqrestore(&mz4_lock, flags);

    return 0;
}
EXPORT_SYMBOL(mz4_add_hide_inode);

bool mz4_should_hide_inode(struct inode *inode)
{
    struct mz4_ino_entry *entry;
    unsigned long key;
    unsigned long flags;
    bool hidden = false;

    if (!inode) return false;

    key = inode->i_ino ^ (inode->i_sb ? inode->i_sb->s_dev : 0);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_ino_ht, entry, node, key) {
        if (entry->key == key) {
            hidden = true;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return hidden;
}
EXPORT_SYMBOL(mz4_should_hide_inode);

/* UID Blocking */
int mz4_block_uid(uid_t uid)
{
    struct mz4_uid_entry *entry;
    unsigned long flags;

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return -ENOMEM;

    entry->uid = uid;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_add(mz4_uid_ht, &entry->node, uid);
    atomic_inc(&mz4_uid_count);
    spin_unlock_irqrestore(&mz4_lock, flags);

    pr_info("MZ4: UID blocked: %u\n", uid);
    return 0;
}
EXPORT_SYMBOL(mz4_block_uid);

int mz4_unblock_uid(uid_t uid)
{
    struct mz4_uid_entry *entry;
    unsigned long flags;
    int found = 0;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_uid_ht, entry, node, uid) {
        if (entry->uid == uid) {
            hash_del(&entry->node);
            kfree(entry);
            atomic_dec(&mz4_uid_count);
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(mz4_unblock_uid);

bool mz4_is_uid_blocked(uid_t uid)
{
    struct mz4_uid_entry *entry;
    unsigned long flags;
    bool blocked = false;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_uid_ht, entry, node, uid) {
        if (entry->uid == uid) {
            blocked = true;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return blocked;
}
EXPORT_SYMBOL(mz4_is_uid_blocked);

int mz4_clear_blocked_uids(void)
{
    struct mz4_uid_entry *entry;
    struct hlist_node *tmp;
    unsigned long flags;
    int bkt;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_safe(mz4_uid_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry);
    }
    atomic_set(&mz4_uid_count, 0);
    spin_unlock_irqrestore(&mz4_lock, flags);

    return 0;
}
EXPORT_SYMBOL(mz4_clear_blocked_uids);

/* AVC Log Spoofing */
static u32 mz4_avc_hash(const char *s, const char *t, const char *c)
{
    u32 hash = 0;
    if (s) while (*s) { hash += *s++; hash += (hash << 10); hash ^= (hash >> 6); }
    if (t) while (*t) { hash += *t++; hash += (hash << 10); hash ^= (hash >> 6); }
    if (c) while (*c) { hash += *c++; hash += (hash << 10); hash ^= (hash >> 6); }
    hash += (hash << 3); hash ^= (hash >> 11); hash += (hash << 15);
    return hash;
}

int mz4_add_avc_spoof(const char *scontext, const char *tcontext, const char *tclass)
{
    struct mz4_avc_entry *entry;
    u32 hash;
    unsigned long flags;

    if (!scontext || !tcontext || !tclass) return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) return -ENOMEM;

    entry->scontext = kstrdup(scontext, GFP_KERNEL);
    entry->tcontext = kstrdup(tcontext, GFP_KERNEL);
    entry->tclass = kstrdup(tclass, GFP_KERNEL);

    if (!entry->scontext || !entry->tcontext || !entry->tclass) {
        kfree(entry->scontext);
        kfree(entry->tcontext);
        kfree(entry->tclass);
        kfree(entry);
        return -ENOMEM;
    }

    hash = mz4_avc_hash(scontext, tcontext, tclass);
    entry->hash = hash;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_add(mz4_avc_ht, &entry->node, hash);
    atomic_inc(&mz4_avc_count);
    spin_unlock_irqrestore(&mz4_lock, flags);

    pr_info("MZ4: AVC spoof added: %s %s %s\n", scontext, tcontext, tclass);
    return 0;
}
EXPORT_SYMBOL(mz4_add_avc_spoof);

int mz4_del_avc_spoof(const char *scontext, const char *tcontext, const char *tclass)
{
    struct mz4_avc_entry *entry;
    u32 hash;
    unsigned long flags;
    int found = 0;

    hash = mz4_avc_hash(scontext, tcontext, tclass);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_avc_ht, entry, node, hash) {
        if (entry->hash == hash &&
            strcmp(entry->scontext, scontext) == 0 &&
            strcmp(entry->tcontext, tcontext) == 0 &&
            strcmp(entry->tclass, tclass) == 0) {
            hash_del(&entry->node);
            kfree(entry->scontext);
            kfree(entry->tcontext);
            kfree(entry->tclass);
            kfree(entry);
            atomic_dec(&mz4_avc_count);
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return found ? 0 : -ENOENT;
}
EXPORT_SYMBOL(mz4_del_avc_spoof);

int mz4_should_spoof_avc(const char *scontext, const char *tcontext, const char *tclass)
{
    struct mz4_avc_entry *entry;
    u32 hash;
    unsigned long flags;
    int should_spoof = 0;

    if (!scontext || !tcontext || !tclass) return 0;
    if (!mz4_avc_log_spoof_enabled) return 0;

    hash = mz4_avc_hash(scontext, tcontext, tclass);

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_possible(mz4_avc_ht, entry, node, hash) {
        if (entry->hash == hash &&
            strcmp(entry->scontext, scontext) == 0 &&
            strcmp(entry->tcontext, tcontext) == 0 &&
            strcmp(entry->tclass, tclass) == 0) {
            should_spoof = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&mz4_lock, flags);

    return should_spoof;
}
EXPORT_SYMBOL(mz4_should_spoof_avc);

int mz4_set_avc_log_spoof(bool enable)
{
    mz4_avc_log_spoof_enabled = enable;
    pr_info("MZ4: AVC log spoofing: %s\n", enable ? "enabled" : "disabled");
    return 0;
}
EXPORT_SYMBOL(mz4_set_avc_log_spoof);

bool mz4_get_avc_log_spoof(void)
{
    return mz4_avc_log_spoof_enabled;
}
EXPORT_SYMBOL(mz4_get_avc_log_spoof);

int mz4_clear_avc_spoof(void)
{
    struct mz4_avc_entry *entry;
    struct hlist_node *tmp;
    unsigned long flags;
    int bkt;

    spin_lock_irqsave(&mz4_lock, flags);
    hash_for_each_safe(mz4_avc_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->scontext);
        kfree(entry->tcontext);
        kfree(entry->tclass);
        kfree(entry);
    }
    atomic_set(&mz4_avc_count, 0);
    spin_unlock_irqrestore(&mz4_lock, flags);

    return 0;
}
EXPORT_SYMBOL(mz4_clear_avc_spoof);

int mz4_set_uname(const char *release, const char *version)
{
    if (!release || !version) return -EINVAL;

    mutex_lock(&mz4_uname_mutex);

    if (!mz4_stock_saved)
        mz4_save_stock_uname();

    strscpy(mz4_spoofed_release, release, sizeof(mz4_spoofed_release));
    strscpy(mz4_spoofed_version, version, sizeof(mz4_spoofed_version));

    down_write(&uts_sem);
    strscpy(utsname()->release, release, sizeof(utsname()->release));
    strscpy(utsname()->version, version, sizeof(utsname()->version));
    up_write(&uts_sem);

    mutex_unlock(&mz4_uname_mutex);

    pr_info("MZ4: Uname spoofed: %s %s\n", release, version);
    return 0;
}
EXPORT_SYMBOL(mz4_set_uname);

int mz4_reset_uname(void)
{
    mutex_lock(&mz4_uname_mutex);

    if (!mz4_stock_saved) {
        mutex_unlock(&mz4_uname_mutex);
        return -ENODATA;
    }

    mz4_spoofed_release[0] = '\0';
    mz4_spoofed_version[0] = '\0';

    down_write(&uts_sem);
    strscpy(utsname()->release, mz4_stock_release, sizeof(utsname()->release));
    strscpy(utsname()->version, mz4_stock_version, sizeof(utsname()->version));
    up_write(&uts_sem);

    mutex_unlock(&mz4_uname_mutex);

    pr_info("MZ4: Uname reset to stock\n");
    return 0;
}
EXPORT_SYMBOL(mz4_reset_uname);

int mz4_get_uname_status(char *buf, size_t len)
{
    int ret;

    mutex_lock(&mz4_uname_mutex);

    if (mz4_spoofed_release[0] != '\0') {
        ret = snprintf(buf, len,
            "spoofed=1\nrelease=%s\nversion=%s\nstock_release=%s\nstock_version=%s\n",
            mz4_spoofed_release, mz4_spoofed_version,
            mz4_stock_release, mz4_stock_version);
    } else {
        ret = snprintf(buf, len,
            "spoofed=0\nrelease=%s\nversion=%s\n",
            mz4_stock_release, mz4_stock_version);
    }

    mutex_unlock(&mz4_uname_mutex);
    return ret;
}
EXPORT_SYMBOL(mz4_get_uname_status);

int mz4_set_cmdline(const char *cmdline)
{
    mutex_lock(&mz4_cmdline_mutex);

    if (mz4_fake_cmdline) {
        kfree(mz4_fake_cmdline);
        mz4_fake_cmdline = NULL;
    }

    if (!cmdline || strlen(cmdline) == 0) {
        mutex_unlock(&mz4_cmdline_mutex);
        return -EINVAL;
    }

    mz4_fake_cmdline = kstrdup(cmdline, GFP_KERNEL);
    if (!mz4_fake_cmdline) {
        mutex_unlock(&mz4_cmdline_mutex);
        return -ENOMEM;
    }

    mz4_cmdline_set = true;
    mutex_unlock(&mz4_cmdline_mutex);

    pr_info("MZ4: Cmdline spoofed\n");
    return 0;
}
EXPORT_SYMBOL(mz4_set_cmdline);

int mz4_reset_cmdline(void)
{
    mutex_lock(&mz4_cmdline_mutex);

    if (mz4_fake_cmdline) {
        kfree(mz4_fake_cmdline);
        mz4_fake_cmdline = NULL;
    }
    mz4_cmdline_set = false;

    mutex_unlock(&mz4_cmdline_mutex);

    pr_info("MZ4: Cmdline reset\n");
    return 0;
}
EXPORT_SYMBOL(mz4_reset_cmdline);

char *mz4_get_fake_cmdline(void)
{
    if (mz4_cmdline_set && mz4_fake_cmdline)
        return mz4_fake_cmdline;
    return NULL;
}
EXPORT_SYMBOL(mz4_get_fake_cmdline);

static long mz4_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    if (_IOC_TYPE(cmd) != MZ4_IOC_MAGIC)
        return -ENOTTY;

    if (cmd != MZ4_IOC_GET_VERSION && cmd != MZ4_IOC_GET_STATUS && cmd != MZ4_IOC_GET_UNAME) {
        if (!capable(CAP_SYS_ADMIN))
            return -EPERM;
    }

    switch (cmd) {
    case MZ4_IOC_GET_VERSION:
        return 0;

    case MZ4_IOC_ENABLE:
        atomic_set(&mz4_enabled, 1);
        pr_info("MZ4: Enabled\n");
        return 0;

    case MZ4_IOC_DISABLE:
        atomic_set(&mz4_enabled, 0);
        pr_info("MZ4: Disabled\n");
        return 0;

    case MZ4_IOC_GET_STATUS:
        return atomic_read(&mz4_enabled);

    case MZ4_IOC_ADD_REDIRECT: {
        struct mz4_rule rule;
        if (copy_from_user(&rule, (void __user *)arg, sizeof(rule)))
            return -EFAULT;
        rule.virtual_path[sizeof(rule.virtual_path) - 1] = '\0';
        rule.real_path[sizeof(rule.real_path) - 1] = '\0';
        return mz4_add_redirect(rule.virtual_path, rule.real_path, rule.flags);
    }

    case MZ4_IOC_DEL_REDIRECT: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_del_redirect(path);
    }

    case MZ4_IOC_CLEAR_REDIRECT: {
        struct mz4_rule *rule;
        struct hlist_node *tmp;
        int bkt;
        unsigned long flags;

        atomic_set(&mz4_enabled, 0);
        synchronize_rcu();

        spin_lock_irqsave(&mz4_lock, flags);
        hash_for_each_safe(mz4_redirect_ht, bkt, tmp, rule, node) {
            hash_del(&rule->node);
            if (!hlist_unhashed(&rule->ino_node))
                hash_del(&rule->ino_node);
            kfree(rule->virtual_path);
            kfree(rule->real_path);
            kfree(rule);
        }
        bitmap_zero(mz4_bloom, MZ4_BLOOM_BITS);
        atomic_set(&mz4_rule_count, 0);
        spin_unlock_irqrestore(&mz4_lock, flags);

        atomic_set(&mz4_enabled, 1);
        pr_info("MZ4: All redirects cleared\n");
        return 0;
    }

    case MZ4_IOC_ADD_HIDE_PATH: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_add_hide_path(path);
    }

    case MZ4_IOC_DEL_HIDE_PATH: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_del_hide_path(path);
    }

    case MZ4_IOC_CLEAR_HIDE:
        return mz4_clear_hide_paths();

    case MZ4_IOC_ADD_HIDE_MOUNT: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_add_hide_mount(path);
    }

    case MZ4_IOC_DEL_HIDE_MOUNT: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_del_hide_mount(path);
    }

    case MZ4_IOC_SET_HIDE_MOUNTS: {
        int enable;
        if (get_user(enable, (int __user *)arg))
            return -EFAULT;
        return mz4_set_hide_all_mounts(enable != 0);
    }

    case MZ4_IOC_CLEAR_HIDE_MOUNT:
        return mz4_clear_hide_mounts();

    case MZ4_IOC_ADD_HIDE_MAP: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_add_hide_map(path);
    }

    case MZ4_IOC_DEL_HIDE_MAP: {
        char path[MZ4_MAX_PATH_LEN];
        if (copy_from_user(path, (void __user *)arg, sizeof(path)))
            return -EFAULT;
        path[sizeof(path) - 1] = '\0';
        return mz4_del_hide_map(path);
    }

    case MZ4_IOC_CLEAR_HIDE_MAP:
        return mz4_clear_hide_maps();

    case MZ4_IOC_SET_UNAME: {
        struct mz4_uname uname;
        if (copy_from_user(&uname, (void __user *)arg, sizeof(uname)))
            return -EFAULT;
        uname.release[sizeof(uname.release) - 1] = '\0';
        uname.version[sizeof(uname.version) - 1] = '\0';
        return mz4_set_uname(uname.release, uname.version);
    }

    case MZ4_IOC_RESET_UNAME:
        return mz4_reset_uname();

    case MZ4_IOC_GET_UNAME: {
        struct mz4_uname_status status;
        char buf[256];
        int len;

        memset(&status, 0, sizeof(status));
        len = mz4_get_uname_status(buf, sizeof(buf));

        if (len > 0) {
            char *p = buf;
            while (p && *p) {
                if (strncmp(p, "spoofed=", 8) == 0)
                    status.spoofed = simple_strtol(p + 8, NULL, 10);
                else if (strncmp(p, "release=", 8) == 0)
                    strncpy(status.release, p + 8, sizeof(status.release) - 1);
                else if (strncmp(p, "version=", 8) == 0)
                    strncpy(status.version, p + 8, sizeof(status.version) - 1);
                else if (strncmp(p, "stock_release=", 14) == 0)
                    strncpy(status.stock_release, p + 14, sizeof(status.stock_release) - 1);
                else if (strncmp(p, "stock_version=", 14) == 0)
                    strncpy(status.stock_version, p + 14, sizeof(status.stock_version) - 1);
                p = strchr(p, '\n');
                if (p) p++;
            }
        }

        if (copy_to_user((void __user *)arg, &status, sizeof(status)))
            return -EFAULT;
        return 0;
    }

    case MZ4_IOC_SET_CMDLINE: {
        char cmdline[1024];
        if (copy_from_user(cmdline, (void __user *)arg, sizeof(cmdline)))
            return -EFAULT;
        cmdline[sizeof(cmdline) - 1] = '\0';
        return mz4_set_cmdline(cmdline);
    }

    case MZ4_IOC_RESET_CMDLINE:
        return mz4_reset_cmdline();

    case MZ4_IOC_BLOCK_UID: {
        uid_t uid;
        if (get_user(uid, (uid_t __user *)arg))
            return -EFAULT;
        return mz4_block_uid(uid);
    }

    case MZ4_IOC_UNBLOCK_UID: {
        uid_t uid;
        if (get_user(uid, (uid_t __user *)arg))
            return -EFAULT;
        return mz4_unblock_uid(uid);
    }

    case MZ4_IOC_CLEAR_UIDS:
        return mz4_clear_blocked_uids();

    case MZ4_IOC_ADD_AVC_SPOOF: {
        struct mz4_avc_spoof spoof;
        if (copy_from_user(&spoof, (void __user *)arg, sizeof(spoof)))
            return -EFAULT;
        spoof.scontext[sizeof(spoof.scontext) - 1] = '\0';
        spoof.tcontext[sizeof(spoof.tcontext) - 1] = '\0';
        spoof.tclass[sizeof(spoof.tclass) - 1] = '\0';
        return mz4_add_avc_spoof(spoof.scontext, spoof.tcontext, spoof.tclass);
    }

    case MZ4_IOC_DEL_AVC_SPOOF: {
        struct mz4_avc_spoof spoof;
        if (copy_from_user(&spoof, (void __user *)arg, sizeof(spoof)))
            return -EFAULT;
        spoof.scontext[sizeof(spoof.scontext) - 1] = '\0';
        spoof.tcontext[sizeof(spoof.tcontext) - 1] = '\0';
        spoof.tclass[sizeof(spoof.tclass) - 1] = '\0';
        return mz4_del_avc_spoof(spoof.scontext, spoof.tcontext, spoof.tclass);
    }

    case MZ4_IOC_CLEAR_AVC:
        return mz4_clear_avc_spoof();

    case MZ4_IOC_SET_AVC_LOG: {
        int enable;
        if (get_user(enable, (int __user *)arg))
            return -EFAULT;
        return mz4_set_avc_log_spoof(enable != 0);
    }

    default:
        return -EINVAL;
    }

    return ret;
}

static int mz4_open(struct inode *inode, struct file *file)
{
    if (!uid_eq(current_euid(), GLOBAL_ROOT_UID)) {
        pr_warn("MZ4: Permission denied\n");
        return -EPERM;
    }
    return 0;
}

static const struct file_operations mz4_fops = {
    .owner = THIS_MODULE,
    .open = mz4_open,
    .unlocked_ioctl = mz4_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = mz4_ioctl,
#endif
};

static struct miscdevice mz4_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mountzero",
    .fops = &mz4_fops,
};

int mz4_init(void)
{
    int ret;

    pr_info("MZ4: MountZero v%s initializing\n", MZ4_VERSION);

    spin_lock_init(&mz4_lock);

    hash_init(mz4_redirect_ht);
    hash_init(mz4_hide_path_ht);
    hash_init(mz4_hide_mount_ht);
    hash_init(mz4_hide_map_ht);
    hash_init(mz4_ino_ht);
    hash_init(mz4_uid_ht);
    hash_init(mz4_avc_ht);

    bitmap_zero(mz4_bloom, MZ4_BLOOM_BITS);

    ret = misc_register(&mz4_misc);
    if (ret) {
        pr_err("MZ4: Failed to register misc device: %d\n", ret);
        return ret;
    }

    ret = sysfs_create_group(kernel_kobj, &mz4_attr_group);
    if (ret)
        pr_warn("MZ4: Failed to create sysfs: %d\n", ret);

    mz4_save_stock_uname();

    atomic_set(&mz4_enabled, 1);

    pr_info("MZ4: Device at /dev/mountzero\n");
    pr_info("MZ4: Sysfs at /sys/kernel/mountzero/\n");

    return 0;
}

void mz4_exit(void)
{
    struct mz4_rule *rule;
    struct mz4_hide_entry *entry;
    struct mz4_ino_entry *ino_entry;
    struct mz4_uid_entry *uid_entry;
    struct mz4_avc_entry *avc_entry;
    struct hlist_node *tmp;
    int bkt;
    unsigned long flags;

    spin_lock_irqsave(&mz4_lock, flags);

    hash_for_each_safe(mz4_redirect_ht, bkt, tmp, rule, node) {
        hash_del(&rule->node);
        kfree(rule->virtual_path);
        kfree(rule->real_path);
        kfree(rule);
    }

    hash_for_each_safe(mz4_hide_path_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->path);
        kfree(entry);
    }

    hash_for_each_safe(mz4_hide_mount_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->path);
        kfree(entry);
    }

    hash_for_each_safe(mz4_hide_map_ht, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry->path);
        kfree(entry);
    }

    hash_for_each_safe(mz4_ino_ht, bkt, tmp, ino_entry, node) {
        hash_del(&ino_entry->node);
        kfree(ino_entry->orig_path);
        kfree(ino_entry);
    }

    hash_for_each_safe(mz4_uid_ht, bkt, tmp, uid_entry, node) {
        hash_del(&uid_entry->node);
        kfree(uid_entry);
    }

    hash_for_each_safe(mz4_avc_ht, bkt, tmp, avc_entry, node) {
        hash_del(&avc_entry->node);
        kfree(avc_entry->scontext);
        kfree(avc_entry->tcontext);
        kfree(avc_entry->tclass);
        kfree(avc_entry);
    }

    spin_unlock_irqrestore(&mz4_lock, flags);

    if (mz4_fake_cmdline)
        kfree(mz4_fake_cmdline);

    sysfs_remove_group(kernel_kobj, &mz4_attr_group);
    misc_deregister(&mz4_misc);

    pr_info("MZ4: Unloaded\n");
}

module_init(mz4_init);
module_exit(mz4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mastermind");
MODULE_DESCRIPTION("MountZero v4 - Simplified VFS hiding without SUSFS");
MODULE_VERSION(MZ4_VERSION);
