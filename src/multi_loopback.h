#ifndef __MULTI_LOOPBACK_LINUX_H__
#define __MULTI_LOOPBACK_LINUX_H__

#define FUSE_USE_VERSION 26

#define _GNU_SOURCE

#include <fuse.h>
#include "logdef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include "utils.h"
#include "SDSConfig.h"
#include "layers_def.h"
#include "multi_loop_drivers/xor.h"
#include "multi_loop_drivers/rep.h"
#include "multi_loop_drivers/erasure.h"
#include <glib.h>

#define REP 0
#define XOR 1
#define ERASURE 2

#define READ_OP 0
#define WRITE_OP 1
#define RELEASE_OP 2
#define RENAME_OP 4
#define FLUSH_OP 5
#define FSYNC_OP 6
#define TRUNCATE_OP 7
#define FTRUNCATE_OP 8
#define OPENDIR 9
#define MKNOD_OP 10
#define MKDIR_OP 11
#define UNLINK_OP 12
#define RMDIR_OP 13
#define CREATE_OP 14
#define OPEN_OP 15
#define OPENDIR_OP 16
#define RELEASEDIR_OP 17
#define SYMLINK_OP 18
#define LINK_OP 19
#define CHMOD_OP 20
#define CHOWN_OP 21

struct loopback_dirp {
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

struct mpath_aux {
    struct loopback_dirp *ldp;
    unsigned long *devs_fd;
};

struct op_info {
    int op_type;
    int op_res;
    off_t fd;
    char *path;
    char *frompath;
    char *topath;
    char *buf;
    uid_t uid;
    gid_t gid;
    off_t size;
    uint64_t magicblocksize;
    off_t magicblockoffset;
    off_t offset;
    mode_t mode;
    dev_t rdev;
    int flags;
    struct loopback_dirp *d;
    pthread_mutex_t *lock;
    pthread_cond_t *cond;
    int *ops_done;
    int op_error;
};

int init_multi_loopback_driver(struct fuse_operations **fuse_operations, configuration data);
int clean_multi_loopback_driver(configuration data);

#endif
