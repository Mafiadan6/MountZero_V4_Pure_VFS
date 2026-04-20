# MountZero v4

<p align="center">
  <strong>VFS-level path hiding and uname spoofing - SUSFS-free</strong>
</p>

<p align="center">
  <a href="https://t.me/mountzerozvfs">
    <img src="https://img.shields.io/badge/Telegram-@mountzerozvfs-2CA5E0?style=for-the-badge&logo=telegram" alt="Telegram">
  </a>
  <img src="https://img.shields.io/badge/License-GPL--3.0-green?style=for-the-badge" alt="License">
</p>

---

## ⚠️ Development Status

**This version is in development and is NOT stable.**

- May cause bootloops or reboots after some time
- Testing in progress
- Use MountZero v2 (requires SUSFS) for production

---

## Overview

MountZero v4 is a **simplified VFS hiding system** that eliminates SUSFS completely. It provides:

- **VFS Path Redirection** - transparent file redirection at VFS layer
- **Path Hiding** - hide files/directories from non-root processes
- **Mount Hiding** - hide mounts from /proc/mounts
- **Maps Hiding** - hide entries from /proc/PID/maps
- **UID Blocking** - block specific UIDs from hiding system
- **Uname Spoofing** - spoof kernel version string
- **Cmdline Spoofing** - spoof kernel cmdline
- **AVC Log Spoofing** - hide SELinux AVC denials

**NO external dependencies** - works with any kernel that has KernelSU or ReSukiSU.

---

## Credits

This project was inspired by the incredible work from:

- **Super-Builders** (https://github.com/Enginex0/Super-Builders) - GKI kernel patches with SUSFS + ZeroMount
- **SUSFS** by simonpunk - the original hiding framework
- **KernelSU** - root framework
- **ZeroMount** - VFS architecture inspiration

Thank you to the community!

---

## Architecture

```
┌─────────────────────────────────────────┐
│    Kernel (/dev/mountzero)               │
│  ├─ Hash table path redirection         │
│  ├─ Bloom filter for fast lookups       │
│  ├─ Inode-based hiding                  │
│  ├─ UID blocking                       │
│  ├─ AVC log spoofing (SELinux)         │
│  ├─ Uname spoofing (direct)            │
│  └─ Cmdline spoofing                   │
└─────────────────────────────────────────┘
```

---

## Quick Start

### 1. Patch Kernel

```bash
cd MountZero_v4
./scripts/patch_kernel.sh /path/to/kernel/source
```

### 2. Enable in defconfig

```text
CONFIG_MOUNTZERO_V4=y
```

### 3. Add VFS Hook

In `fs/namei.c`, find `getname_flags()` and add:

```c
#ifdef CONFIG_MOUNTZERO_V4
#include <linux/mountzero_v4_vfs.h>
#endif

struct filename *getname_flags(...) {
    /* ... existing code ... */
    
#ifdef CONFIG_MOUNTZERO_V4
    result = mz4_vfs_getname_hook(result);
#endif

    return result;
}
```

### 4. Build & Flash

```bash
make -j$(nproc)
fastboot flash boot boot.img
```

---

## CLI Usage

```bash
# Basic
mz4ctl version              # Show version
mz4ctl status               # Show engine status
mz4ctl enable               # Enable engine
mz4ctl disable              # Disable engine

# Redirects
mz4ctl add /system/bin/su /data/adb/ksu/bin/su
mz4ctl del /system/bin/su
mz4ctl clear

# Path hiding
mz4ctl hide-add /data/adb/ksu
mz4ctl hide-del /data/adb/ksu

# Mount hiding
mz4ctl mount-add /data/adb/modules
mz4ctl mount-del /data/adb/modules

# Maps hiding
mz4ctl map-add /data/adb/ksu/zygisk.so
mz4ctl map-del /data/adb/ksu/zygisk.so

# Uname spoofing
mz4ctl set-uname '5.14.113' '#1 SMP PREEMPT Mon Oct 6 16:50:48 UTC 2025'
mz4ctl reset-uname

# Cmdline spoofing
mz4ctl set-cmdline "androidboot.bootdevice=..."
mz4ctl reset-cmdline

# UID blocking
mz4ctl block-uid 0
mz4ctl unblock-uid 0
mz4ctl clear-uids

# AVC log spoofing (SELinux)
mz4ctl avc-add "u:r:su:s0" "u:r:kernel:s0" "file"
mz4ctl avc-del "u:r:su:s0" "u:r:kernel:s0" "file"
mz4ctl avc-clear
mz4ctl avc-log 1
```

---

## Kernel Config Options

| Option | Description |
|--------|-------------|
| `CONFIG_MOUNTZERO_V4` | Enable MountZero v4 driver |

No other dependencies required!

---

## File Structure

```
MountZero_v4/
├── README.md
├── LICENSE                 # GPL v3.0
├── kernel/
│   ├── fs/
│   │   ├── mountzero_v4_core.c    # Core driver
│   │   ├── mountzero_v4_vfs.c     # VFS hooks
│   │   └── mountzero_v4_cli.c     # Userspace CLI
│   └── include/linux/
│       ├── mountzero_v4.h         # Public API
│       ├── mountzero_v4_def.h     # Internal definitions
│       └── mountzero_v4_vfs.h     # VFS declarations
└── scripts/
    └── patch_kernel.sh            # Kernel patcher
```

---

## Comparison: v2 vs v4

| Feature | v2 (MountZero) | v4 (MountZero_v4) |
|---------|-----------------|---------------------|
| SUSFS dependency | Required | **None** |
| Complexity | High | **Low** |
| Integration | Difficult | **Simple** |
| Path hiding | Via SUSFS bridge | **Built-in** |
| Mount hiding | Via SUSFS bridge | **Built-in** |
| Maps hiding | Via SUSFS bridge | **Built-in** |
| UID blocking | Via SUSFS bridge | **Built-in** |
| AVC spoofing | Via SUSFS bridge | **Built-in** |
| Uname spoofing | Via SUSFS bridge | **Built-in** |
| Lines of code | ~2500 | **~1400** |

---

## ⚠️ Warning - Not for Production

This version is in development. Known issues:
- May cause unexpected reboots
- Not fully tested
- Use at your own risk

For production, use MountZero v2 (requires SUSFS).

---

## License

GPL v3.0 - See LICENSE file

---

## Author

**Mastermind** - [@mafiadan6](https://github.com/mafiadan6)

---

## Support

Telegram: [@mountzerozvfs](https://t.me/mountzerozvfs)