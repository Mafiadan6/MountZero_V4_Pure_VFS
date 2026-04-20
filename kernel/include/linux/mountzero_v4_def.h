/*
 * MountZero v4 - Internal Definitions
 *
 * Simplified structures without SUSFS dependencies.
 */

#ifndef __LINUX_MOUNTZERO_V4_DEF_H
#define __LINUX_MOUNTZERO_V4_DEF_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/uidgid.h>
#include <linux/bitops.h>

#define MZ4_VERSION "4.0.0"

#define MZ4_HASH_BITS 8
#define MZ4_BLOOM_BITS 4096
#define MZ4_BLOOM_MASK (MZ4_BLOOM_BITS - 1)

#define MZ4_MAX_HIDE_PATHS 128
#define MZ4_MAX_HIDE_MOUNTS 64
#define MZ4_MAX_HIDE_MAPS 128
#define MZ4_MAX_REDIRECTS 512
#define MZ4_MAX_BLOCKED_UIDS 64
#define MZ4_MAX_AVC_SPOOFS 64

#define MZ4_FLAG_READ_ONLY    (1 << 0)
#define MZ4_FLAG_PERMANENT    (1 << 1)
#define MZ4_FLAG_DYNAMIC     (1 << 2)

struct mz4_rule {
    char *virtual_path;
    char *real_path;
    unsigned int flags;
    u32 hash;
    u64 real_ino;
    u64 real_dev;
    struct hlist_node node;
    struct hlist_node ino_node;
};

struct mz4_hide_entry {
    char *path;
    u32 hash;
    struct hlist_node node;
};

struct mz4_ino_entry {
    unsigned long key;
    u64 real_ino;
    u64 real_dev;
    char *orig_path;
    struct hlist_node node;
};

struct mz4_uid_entry {
    uid_t uid;
    struct hlist_node node;
};

struct mz4_avc_entry {
    char *scontext;
    char *tcontext;
    char *tclass;
    u32 hash;
    struct hlist_node node;
};

extern atomic_t mz4_enabled;
extern int mz4_debug;
extern spinlock_t mz4_lock;

extern struct hlist_head mz4_redirect_ht[1 << MZ4_HASH_BITS];
extern struct hlist_head mz4_hide_path_ht[1 << MZ4_HASH_BITS];
extern struct hlist_head mz4_hide_mount_ht[1 << MZ4_HASH_BITS];
extern struct hlist_head mz4_hide_map_ht[1 << MZ4_HASH_BITS];
extern struct hlist_head mz4_ino_ht[1 << MZ4_HASH_BITS];
extern struct hlist_head mz4_uid_ht[1 << MZ4_HASH_BITS];
extern struct hlist_head mz4_avc_ht[1 << MZ4_HASH_BITS];

extern atomic_t mz4_rule_count;
extern atomic_t mz4_hide_path_count;
extern atomic_t mz4_hide_mount_count;
extern atomic_t mz4_hide_map_count;
extern atomic_t mz4_uid_count;
extern atomic_t mz4_avc_count;

static inline void mz4_bloom_add(u32 hash);
static inline bool mz4_bloom_test(u32 hash);

#endif
