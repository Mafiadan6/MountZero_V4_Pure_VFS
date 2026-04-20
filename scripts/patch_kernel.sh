#!/bin/bash
#
# MountZero v4 - Kernel Patch Script
#
# Patches kernel source to integrate MountZero v4.
# No SUSFS dependency required.
#

set -e

KERNEL_SRC="${1:-.}"

echo "MountZero v4 - Kernel Patch Script"
echo "==================================="
echo "Kernel source: $KERNEL_SRC"
echo ""

# Copy headers
echo "[1/5] Copying headers..."
mkdir -p "$KERNEL_SRC/include/linux"
cp kernel/include/linux/mountzero_v4.h "$KERNEL_SRC/include/linux/"
cp kernel/include/linux/mountzero_v4_def.h "$KERNEL_SRC/include/linux/"
cp kernel/include/linux/mountzero_v4_vfs.h "$KERNEL_SRC/include/linux/"

# Copy source files
echo "[2/5] Copying source files..."
cp kernel/fs/mountzero_v4_core.c "$KERNEL_SRC/fs/"
cp kernel/fs/mountzero_v4_vfs.c "$KERNEL_SRC/fs/"

# Update Makefile
echo "[3/5] Updating fs/Makefile..."
if ! grep -q "mountzero_v4" "$KERNEL_SRC/fs/Makefile" 2>/dev/null; then
    echo "obj-\$(CONFIG_MOUNTZERO_V4) += mountzero_v4_core.o mountzero_v4_vfs.o" >> "$KERNEL_SRC/fs/Makefile"
    echo "  Added to fs/Makefile"
else
    echo "  Already patched"
fi

# Update Kconfig
echo "[4/5] Updating fs/Kconfig..."
if ! grep -q "CONFIG_MOUNTZERO_V4" "$KERNEL_SRC/fs/Kconfig" 2>/dev/null; then
    cat >> "$KERNEL_SRC/fs/Kconfig" << 'EOF'

config MOUNTZERO_V4
    bool "MountZero v4 - VFS Hiding System"
    help
      Simplified VFS-based path redirection and hiding system.
      No external dependencies (SUSFS-free).
EOF
    echo "  Added to fs/Kconfig"
else
    echo "  Already patched"
fi

# Create patch for fs/namei.c
echo "[5/5] Creating namei.c hook..."
HOOK_CODE='
#ifdef CONFIG_MOUNTZERO_V4
#include <linux/mountzero_v4_vfs.h>
#endif
'

echo "MountZero v4 patch complete!"
echo ""
echo "To complete integration:"
echo "1. Add 'source \"fs/Kconfig\"' to your Kconfig if needed"
echo "2. Enable CONFIG_MOUNTZERO_V4 in your defconfig"
echo "3. Add VFS hook in fs/namei.c getname_flags() function:"
echo "   #ifdef CONFIG_MOUNTZERO_V4"
echo "       result = mz4_vfs_getname_hook(result);"
echo "   #endif"
echo ""
echo "Example hook location in fs/namei.c:"
echo "  Find: struct filename *getname_flags(const char __user *name, int flags, int *empty)"
echo "  Add hook before: return result;"

# Create the actual patch content
cat > "$KERNEL_SRC/0001-mountzero-v4.patch" << 'PATCHEOF'
--- a/fs/namei.c
+++ b/fs/namei.c
@@ -xxx,6 +xxx,10 @@ struct filename *getname_flags(const char __user *name, int flags, int *empty)
 	}
 #endif
 
+#ifdef CONFIG_MOUNTZERO_V4
+	result = mz4_vfs_getname_hook(result);
+#endif
+
 	return result;
 }
 EXPORT_SYMBOL(getname_flags);
PATCHEOF

echo "Patch file created: 0001-mountzero-v4.patch"
