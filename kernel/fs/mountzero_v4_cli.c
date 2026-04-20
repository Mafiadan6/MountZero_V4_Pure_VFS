/*
 * MountZero v4 - Userspace CLI
 *
 * Simple CLI tool for controlling MountZero v4.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/mountzero_v4.h>

#define DEV_PATH "/dev/mountzero"

static int fd = -1;

static int mz4_open(void)
{
    if (fd >= 0) return 0;
    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    return 0;
}

static void mz4_close(void)
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

static void print_usage(const char *prog)
{
    printf("MountZero v4 CLI\n");
    printf("Usage: %s <command> [args]\n\n", prog);
    printf("Commands:\n");
    printf("  version              Show version\n");
    printf("  status               Show status\n");
    printf("  enable               Enable engine\n");
    printf("  disable              Disable engine\n");
    printf("  add <virt> <real>    Add redirect rule\n");
    printf("  del <path>           Delete redirect rule\n");
    printf("  clear                Clear all redirects\n");
    printf("  hide-add <path>      Add hidden path\n");
    printf("  hide-del <path>      Delete hidden path\n");
    printf("  mount-add <path>     Add hidden mount\n");
    printf("  mount-del <path>     Delete hidden mount\n");
    printf("  map-add <path>       Add hidden map\n");
    printf("  map-del <path>       Delete hidden map\n");
    printf("  set-uname <rel> <ver> Set uname spoof\n");
    printf("  reset-uname          Reset uname\n");
    printf("  set-cmdline <cmd>    Set cmdline spoof\n");
    printf("  reset-cmdline        Reset cmdline\n");
    printf("  block-uid <uid>      Block UID\n");
    printf("  unblock-uid <uid>    Unblock UID\n");
    printf("  clear-uids           Clear blocked UIDs\n");
    printf("  avc-add <scon> <tcon> <tclass>  Add AVC spoof\n");
    printf("  avc-del <scon> <tcon> <tclass>  Del AVC spoof\n");
    printf("  avc-clear            Clear AVC spoofs\n");
    printf("  avc-log <0|1>       Enable/disable AVC log spoof\n");
}

int main(int argc, char **argv)
{
    int ret;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "version") == 0) {
        printf("MountZero v4.0.0\n");
        return 0;
    }

    if (mz4_open() != 0) {
        fprintf(stderr, "Failed to open %s\n", DEV_PATH);
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        ret = ioctl(fd, MZ4_IOC_GET_STATUS);
        printf("Enabled: %s\n", ret ? "yes" : "no");
    }
    else if (strcmp(argv[1], "enable") == 0) {
        ret = ioctl(fd, MZ4_IOC_ENABLE);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "disable") == 0) {
        ret = ioctl(fd, MZ4_IOC_DISABLE);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "add") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s add <virt_path> <real_path>\n", argv[0]);
            ret = 1;
        } else {
            struct mz4_rule rule;
            memset(&rule, 0, sizeof(rule));
            strncpy(rule.virtual_path, argv[2], sizeof(rule.virtual_path) - 1);
            strncpy(rule.real_path, argv[3], sizeof(rule.real_path) - 1);
            ret = ioctl(fd, MZ4_IOC_ADD_REDIRECT, &rule);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "del") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s del <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_DEL_REDIRECT, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "clear") == 0) {
        ret = ioctl(fd, MZ4_IOC_CLEAR_REDIRECT);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "hide-add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s hide-add <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_ADD_HIDE_PATH, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "hide-del") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s hide-del <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_DEL_HIDE_PATH, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "mount-add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s mount-add <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_ADD_HIDE_MOUNT, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "mount-del") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s mount-del <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_DEL_HIDE_MOUNT, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "map-add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s map-add <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_ADD_HIDE_MAP, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "map-del") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s map-del <path>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_DEL_HIDE_MAP, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "set-uname") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s set-uname <release> <version>\n", argv[0]);
            ret = 1;
        } else {
            struct mz4_uname uname;
            memset(&uname, 0, sizeof(uname));
            strncpy(uname.release, argv[2], sizeof(uname.release) - 1);
            strncpy(uname.version, argv[3], sizeof(uname.version) - 1);
            ret = ioctl(fd, MZ4_IOC_SET_UNAME, &uname);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "reset-uname") == 0) {
        ret = ioctl(fd, MZ4_IOC_RESET_UNAME);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "set-cmdline") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s set-cmdline <cmdline>\n", argv[0]);
            ret = 1;
        } else {
            ret = ioctl(fd, MZ4_IOC_SET_CMDLINE, argv[2]);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "reset-cmdline") == 0) {
        ret = ioctl(fd, MZ4_IOC_RESET_CMDLINE);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "block-uid") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s block-uid <uid>\n", argv[0]);
            ret = 1;
        } else {
            uid_t uid = atoi(argv[2]);
            ret = ioctl(fd, MZ4_IOC_BLOCK_UID, &uid);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "unblock-uid") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s unblock-uid <uid>\n", argv[0]);
            ret = 1;
        } else {
            uid_t uid = atoi(argv[2]);
            ret = ioctl(fd, MZ4_IOC_UNBLOCK_UID, &uid);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "clear-uids") == 0) {
        ret = ioctl(fd, MZ4_IOC_CLEAR_UIDS);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "avc-add") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s avc-add <scontext> <tcontext> <tclass>\n", argv[0]);
            ret = 1;
        } else {
            struct mz4_avc_spoof spoof;
            memset(&spoof, 0, sizeof(spoof));
            strncpy(spoof.scontext, argv[2], sizeof(spoof.scontext) - 1);
            strncpy(spoof.tcontext, argv[3], sizeof(spoof.tcontext) - 1);
            strncpy(spoof.tclass, argv[4], sizeof(spoof.tclass) - 1);
            ret = ioctl(fd, MZ4_IOC_ADD_AVC_SPOOF, &spoof);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "avc-del") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s avc-del <scontext> <tcontext> <tclass>\n", argv[0]);
            ret = 1;
        } else {
            struct mz4_avc_spoof spoof;
            memset(&spoof, 0, sizeof(spoof));
            strncpy(spoof.scontext, argv[2], sizeof(spoof.scontext) - 1);
            strncpy(spoof.tcontext, argv[3], sizeof(spoof.tcontext) - 1);
            strncpy(spoof.tclass, argv[4], sizeof(spoof.tclass) - 1);
            ret = ioctl(fd, MZ4_IOC_DEL_AVC_SPOOF, &spoof);
            printf("Result: %d\n", ret);
        }
    }
    else if (strcmp(argv[1], "avc-clear") == 0) {
        ret = ioctl(fd, MZ4_IOC_CLEAR_AVC);
        printf("Result: %d\n", ret);
    }
    else if (strcmp(argv[1], "avc-log") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s avc-log <0|1>\n", argv[0]);
            ret = 1;
        } else {
            int enable = atoi(argv[2]);
            ret = ioctl(fd, MZ4_IOC_SET_AVC_LOG, &enable);
            printf("Result: %d\n", ret);
        }
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        ret = 1;
    }

    mz4_close();
    return ret;
}
