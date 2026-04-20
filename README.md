# MountZero v4

<p align="center">
  <strong>Complete VFS Hiding System - SUSFS-Free</strong><br>
  Built-in kernel hiding for KernelSU/APatch
</p>

<p align="center">
  <a href="https://t.me/mountzerozvfs">
    <img src="https://img.shields.io/badge/Telegram-@mountzerozvfs-2CA5E0?style=for-the-badge&logo=telegram" alt="Telegram">
  </a>
  <img src="https://img.shields.io/badge/Version-v4.0.0-green?style=for-the-badge" alt="Version">
  <img src="https://img.shields.io/badge/License-GPL--3.0-green?style=for-the-badge" alt="License">
</p>

---

## What is MountZero v4?

MountZero v4 is a **complete VFS hiding system** that provides full hiding functionality **without SUSFS dependency**. It's based on the official SUSFS4KSU implementation but rebuilt as a standalone module.

### Key Features

| Feature | Description |
|---------|-------------|
| **Path Hiding** | Hide files/directories from all processes |
| **Mount Hiding** | Hide mounts from /proc/mounts |
| **Maps Hiding** | Hide entries from /proc/PID/maps |
| **Mount ID Remap** | Reorder mount IDs to prevent detection |
| **UID Blocking** | Block specific UIDs from hiding system |
| **Uname Spoofing** | Spoof kernel version string |
| **Cmdline Spoofing** | Spoof kernel cmdline |
| **AVC Log Spoofing** | Hide SELinux AVC denials |
| **Kstat Spoofing** | Fake file metadata in stat calls |

### Why v4?

- ✅ **No SUSFS dependency** - Completely standalone
- ✅ **Easy integration** - Apply patches and build
- ✅ **Full hiding** - All features built-in
- ✅ **Actively maintained** - Based on SUSFS4KSU

---

## Quick Start (KernelSU Module)

### Installation

1. **Download** the latest release zip
2. **Install** via KernelSU Manager (Modules tab)
3. **Reboot** your device

### Usage

```bash
# Via Terminal
mz4 enable                          # Enable hiding
mz4 add-path /data/adb/ksu         # Hide a path
mz4 add-mount /data/adb/modules    # Hide a mount
mz4 hide-mounts 1                 # Hide all mounts
mz4 set-uname "4.19.123" "#1 SMP" # Spoof uname

# Via WebUI
# Access http://127.0.0.1:PORT/mountzero_v4
```

---

## Quick Start (Build into Kernel)

### 1. Copy Source Files

```bash
# Kernel source directory
KERNEL_PATH=/path/to/kernel/source

# Copy MountZero v4 source
cp MountZero_v4/kernel/fs/*.c $KERNEL_PATH/fs/
cp -r MountZero_v4/kernel/include/linux/*.h $KERNEL_PATH/include/linux/
```

### 2. Apply Patches

```bash
cd $KERNEL_PATH

# Choose your kernel version
# Kernel 4.14
patch -p1 < MountZero_v4/kernel_patches/50_add_mountzero_v4_in_kernel-4.14.patch
patch -p1 < MountZero_v4/kernel_patches/51_add_mountzero_v4_mount_hiding.patch

# Kernel 4.19
patch -p1 < MountZero_v4/kernel_patches/50_add_mountzero_v4_in_kernel-4.19.patch
patch -p1 < MountZero_v4/kernel_patches/51_add_mountzero_v4_mount_hiding-4.19.patch
```

### 3. Enable in Kernel Config

```bash
# Add to your defconfig
echo 'CONFIG_MOUNTZERO_V4=y' >> arch/arm64/configs/your_defconfig
echo 'CONFIG_MOUNTZERO_V4_SUS_PATH=y' >> arch/arm64/configs/your_defconfig
echo 'CONFIG_MOUNTZERO_V4_SUS_MOUNT=y' >> arch/arm64/configs/your_defconfig
echo 'CONFIG_MOUNTZERO_V4_SUS_MAPS=y' >> arch/arm64/configs/your_defconfig
echo 'CONFIG_MOUNTZERO_V4_SPOOF_UNAME=y' >> arch/arm64/configs/your_defconfig
```

### 4. Build

```bash
# Build kernel
make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

# Flash
fastboot flash boot boot.img
```

---

## CLI Commands

### Basic

| Command | Description |
|---------|-------------|
| `mz4 version` | Show version |
| `mz4 status` | Show engine status (1=enabled, 0=disabled) |
| `mz4 enable` | Enable hiding engine |
| `mz4 disable` | Disable hiding engine |

### Path Hiding

| Command | Description |
|---------|-------------|
| `mz4 add-path <path>` | Add path to hide |
| `mz4 del-path <path>` | Remove path from hide list |
| `mz4 list-paths` | List all hidden paths |
| `mz4 clear-path` | Clear all hidden paths |

### Mount Hiding

| Command | Description |
|---------|-------------|
| `mz4 add-mount <path>` | Add mount to hide |
| `mz4 del-mount <path>` | Remove mount from hide list |
| `mz4 list-mounts` | List all hidden mounts |
| `mz4 hide-mounts 1` | Hide all mounts |
| `mz4 hide-mounts 0` | Show all mounts |
| `mz4 remap 1` | Enable mount ID remapping |
| `mz4 clear-mount` | Clear all hidden mounts |

### Maps Hiding

| Command | Description |
|---------|-------------|
| `mz4 add-map <path>` | Add path to /proc/PID/maps hide |
| `mz4 del-map <path>` | Remove from maps hide |
| `mz4 list-maps` | List all hidden maps |
| `mz4 clear-map` | Clear all hidden maps |

### Uname Spoofing

| Command | Description |
|---------|-------------|
| `mz4 set-uname <release> <version>` | Set spoofed uname |
| `mz4 get-uname` | Show current uname settings |
| `mz4 reset-uname` | Reset to stock uname |

### Cmdline Spoofing

| Command | Description |
|---------|-------------|
| `mz4 set-cmdline <text>` | Set spoofed cmdline |
| `mz4 reset-cmdline` | Reset to stock cmdline |

### UID Blocking

| Command | Description |
|---------|-------------|
| `mz4 block-uid <uid>` | Block UID from hiding |
| `mz4 unblock-uid <uid>` | Unblock UID |
| `mz4 clear-uid` | Clear all blocked UIDs |

### AVC Spoofing

| Command | Description |
|---------|-------------|
| `mz4 avc-log 1` | Enable AVC log spoofing |
| `mz4 avc-log 0` | Disable AVC log spoofing |
| `mz4 clear-avc` | Clear AVC rules |

---

## Configuration File

Create `/data/adb/mountzero_v4/config.toml`:

```toml
[general]
enabled = true

[susfs]
path_hide = true
maps_hide = true
mount_hide = true
mount_id_reorder = true

[selinux]
avc_log = false
```

---

## Supported Kernels

| Kernel | Main Patch | Extra Patches |
|--------|-----------|---------------|
| 4.14 | ✅ Ready | ✅ Ready |
| 4.19 | ✅ Ready | ✅ Ready |
| 5.4 | ✅ Ready | ✅ Ready |
| 5.10 | ✅ Ready | ✅ Ready |
| 5.15 | ✅ Ready | ✅ Ready |
| 6.1 | ✅ Ready | ✅ Ready |

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│           MountZero v4 Module                   │
├─────────────────────────────────────────────────┤
│  Userspace:                                     │
│  ├── mz4 CLI (ARM64 binary)                    │
│  ├── WebUI (JavaScript)                        │
│  └── config.toml                                │
├─────────────────────────────────────────────────┤
│  Kernel:                                        │
│  ├── fs/namei.c    → Path hiding               │
│  ├── fs/stat.c     → Kstat spoofing            │
│  ├── fs/proc_namespace.c → Mount hiding        │
│  ├── fs/proc_task_mmu.c → Maps hiding          │
│  ├── fs/readdir.c  → Directory hiding         │
│  └── kernel/sys.c  → Uname/cmdline spoof       │
├─────────────────────────────────────────────────┤
│  Core:                                          │
│  ├── Hash tables for rules                      │
│  ├── Mount ID remapping                         │
│  ├── Inode flag system                         │
│  └── AVC log spoofing                          │
└─────────────────────────────────────────────────┘
```

---

## File Structure

```
MountZero_v4/
├── README.md                    # This file
├── LICENSE                      # GPL v3.0
│
├── kernel/                      # Kernel source
│   ├── fs/
│   │   ├── mountzero_v4_core.c   # Core driver
│   │   ├── mountzero_v4_vfs.c    # VFS hooks
│   │   ├── mountzero_v4_hooks.c  # Hook functions
│   │   └── mountzero_v4_cli.c    # CLI tool
│   ├── include/linux/
│   │   ├── mountzero_v4.h        # Public API
│   │   ├── mountzero_v4_def.h    # Definitions
│   │   └── mountzero_v4_vfs.h   # VFS internals
│   ├── Kconfig                   # Kernel config
│   └── Makefile                  # Build file
│
├── kernel_patches/              # Kernel patches
│   ├── 50_add_mountzero_v4_in_kernel-4.14.patch
│   ├── 50_add_mountzero_v4_in_kernel-4.19.patch
│   ├── 50_add_mountzero_v4_in_kernel-5.4.patch
│   ├── 50_add_mountzero_v4_in_kernel-5.10.patch
│   ├── 50_add_mountzero_v4_in_kernel-5.15.patch
│   ├── 50_add_mountzero_v4_in_kernel-6.1.patch
│   └── *_*_mount_hiding.patch
│   └── *_*_maps_hiding.patch
│
└── userspace/                   # KernelSU module
    ├── module.prop              # Module metadata
    ├── service.sh               # Service script
    ├── post-fs-data.sh          # Post-fs-data script
    ├── hiding.sh                # Hiding functions
    ├── config.sh                # Config loader
    ├── bridge.sh               # External config bridge
    ├── bin/
    │   ├── mz4                 # ARM64 CLI binary
    │   └── mzctl → mz4         # Alias
    ├── system/bin/
    │   └── mzctl               # Installed here
    └── webroot/
        ├── index.html          # WebUI
        ├── script.js           # JavaScript
        └── styles.css          # Styles
```

---

## Credits

MountZero v4 is based on **ZeroMount** by Super-Builders:

- **ZeroMount** - https://github.com/Enginex0/Super-Builders
- **SUSFS4KSU** by simonpunk - https://gitlab.com/simonpunk/susfs4ksu
- **KernelSU** - Root framework
- **Community testers**

Thank you to all the original developers!

---

## License

GPL v3.0 - See LICENSE file

---

## Support

Telegram: [@mountzerozvfs](https://t.me/mountzerozvfs)

---

## Version

**v4.0.0** - First release
