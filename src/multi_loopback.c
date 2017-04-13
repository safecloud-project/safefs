/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

*/

/*
 * Loopback OSXFUSE file system in C. Uses the high-level FUSE API.
 * Based on the fusexmp_fh.c example from the Linux FUSE distribution.
 * Amit Singh <http://osxbook.com>
 */

// TODO: error numbers should be catech in threads_func with -errorn and kept in struct inf.

#include <assert.h>

#include "multi_loopback.h"
#include "utils.h"
#include "timestamps/timestamps.h"

int NDEVS;
// struct with encode algorithms
char **devices_path;
GThreadPool *thread_pool;

#define MAX_THREADS 10

static struct multi_driver m_driver;
int DRIVER;

GSList *multi_write_list = NULL, *multi_read_list = NULL;

void threads_func(gpointer data, gpointer user_data) {
    struct op_info *inf = (struct op_info *)data;

    switch (inf->op_type) {
        case READ_OP:
            if (DRIVER == ERASURE) {
                DEBUG_MSG("1-Reading CONTENT off %lld and size %lld\n", inf->magicblockoffset, inf->magicblocksize);
                inf->op_res = pread(inf->fd, inf->buf, inf->magicblocksize, inf->magicblockoffset);
                DEBUG_MSG("Content actually read was %ld\n", inf->op_res);
                if (inf->op_res > 0) {
                    inf->op_res = inf->size;
                }
            } else {
                DEBUG_MSG("2-Reading CONTENT off %lld and size %lld\n", inf->offset, inf->size);
                inf->op_res = pread(inf->fd, inf->buf, inf->size, inf->offset);
            }
            break;
        case WRITE_OP:
            if (DRIVER == ERASURE) {
                DEBUG_MSG("1-Writing CONTENT off %lld and size %lld\n", inf->magicblockoffset, inf->magicblocksize);
                pwrite(inf->fd, inf->buf, inf->magicblocksize, inf->magicblockoffset);
                inf->op_res = inf->size;
            } else {
                DEBUG_MSG("2-Writing CONTENT off %lld and size %lld\n", inf->offset, inf->size);
                int res = pwrite(inf->fd, inf->buf, inf->size, inf->offset);
                DEBUG_MSG("result is %d\n", res);
                inf->op_res = res;
            }
            break;
        case FLUSH_OP:
            inf->op_res = close(dup(inf->fd));
            break;
        case RELEASE_OP:
            inf->op_res = close(inf->fd);
            break;
        case FSYNC_OP:
            inf->op_res = fsync(inf->fd);
            break;
        case TRUNCATE_OP:
            inf->op_res = truncate(inf->path, inf->size);
            break;
        case FTRUNCATE_OP:
            inf->op_res = ftruncate(inf->fd, inf->size);
            break;
        case MKNOD_OP:
            if (S_ISFIFO(inf->mode)) {
                inf->op_res = mkfifo(inf->path, inf->mode);
            } else {
                inf->op_res = mknod(inf->path, inf->mode, inf->rdev);
            }
            break;
        case MKDIR_OP:
            inf->op_res = mkdir(inf->path, inf->mode);
            break;
        case UNLINK_OP:
            inf->op_res = unlink(inf->path);
            break;
        case RMDIR_OP:
            inf->op_res = rmdir(inf->path);
            break;
        case CREATE_OP:
            inf->op_res = open(inf->path, inf->flags, inf->mode);
            break;
        case OPEN_OP:
            inf->op_res = open(inf->path, inf->flags);
            break;
        case OPENDIR_OP:
            inf->d->dp = opendir(inf->path);
            if (inf->d->dp == NULL) {
                inf->op_res = -errno;
            } else {
                inf->op_res = 0;
                inf->d->offset = 0;
                inf->d->entry = NULL;
            }
            break;
        case RELEASEDIR_OP:
            inf->op_res = closedir(inf->d->dp);
            break;
        case SYMLINK_OP:
            inf->op_res = symlink(inf->frompath, inf->topath);
            break;
        case RENAME_OP:
            inf->op_res = rename(inf->frompath, inf->topath);
            break;
        case LINK_OP:
            inf->op_res = link(inf->frompath, inf->topath);
            break;
        case CHMOD_OP:
            inf->op_res = chmod(inf->path, inf->mode);
            break;
        case CHOWN_OP:
            inf->op_res = lchown(inf->path, inf->uid, inf->gid);
            break;
        default:
            ERROR_MSG("OP_TYPE Unknown\n");
            break;
    }

    if (inf->op_res == -1) {
        inf->op_error = -errno;
    }

    pthread_mutex_lock(inf->lock);

    *inf->ops_done = *inf->ops_done + 1;

    pthread_cond_signal(inf->cond);

    pthread_mutex_unlock(inf->lock);
}

int wait_for_all_requests(struct op_info *inf, pthread_mutex_t *op_lock, pthread_cond_t *wait_ops) {
    DEBUG_MSG("waitrequests\n");

    int res;
    int i = 0;

    pthread_mutex_lock(op_lock);
    while (*inf[0].ops_done < NDEVS) {
        pthread_cond_wait(wait_ops, op_lock);
    }
    pthread_mutex_unlock(op_lock);

    for (i = 0; i < NDEVS; i++) {
        if (inf[i].op_res == -1) {
            inf[0].op_error = inf[i].op_error;
            return -1;
        }

        if (i > 0 && (inf[i].op_type == READ_OP || inf[i].op_type == WRITE_OP)) {
            if (res != inf[i].op_res) {
                inf[0].op_error = inf[i].op_error;
                return -1;
            }
        }

        if (i == 0) {
            res = inf[i].op_res;
        }
    }

    return res;
}

static int loopback_getattr(const char *path, struct stat *stbuf) {
    int res;

    DEBUG_MSG("getattr\n");

    // Check only the status from one pen since this is replicated in all
    char newpath[PATHSIZE];
    strcpy(newpath, devices_path[0]);
    replace_path(path, newpath);

    res = lstat(newpath, stbuf);

    if (res == -1) {
        return -errno;
    }
    if (DRIVER == ERASURE) {
        uint64_t size = m_driver.get_file_size(path);
        DEBUG_MSG("Size found is %lld\n", size);
        stbuf->st_size = size;
    }
    return 0;
}

static int loopback_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res;

    DEBUG_MSG("fgetattr\n");

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;

    // Check only the status from one pen since this is replicated in all
    res = fstat(mp->devs_fd[0], stbuf);
    if (res == -1) {
        return -errno;
    }

    if (DRIVER == ERASURE) {
        uint64_t size = m_driver.get_file_size(path);
        DEBUG_MSG("Size found is %lld\n", size);
        stbuf->st_size = size;
    }

    return 0;
}

static int loopback_readlink(const char *path, char *buf, size_t size) {
    int res;

    DEBUG_MSG("readlink\n");

    char newpath[PATHSIZE];
    int psize;
    char auxbuf[PATHSIZE];
    strcpy(newpath, devices_path[0]);
    replace_path(path, newpath);

    res = readlink(newpath, auxbuf, size - 1);
    if (res == -1) {
        return -errno;
    }

    psize = strlen(devices_path[0]);
    strcpy(buf, &auxbuf[psize]);

    buf[res - psize] = '\0';

    return 0;
}

static int loopback_opendir(const char *path, struct fuse_file_info *fi) {
    int res;

    DEBUG_MSG("opendir\n");

    struct mpath_aux *mp = malloc(sizeof(struct mpath_aux));
    mp->ldp = malloc(sizeof(struct loopback_dirp) * NDEVS);
    int i;

    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    // create folder in each device
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = OPENDIR_OP;
        inf[i].d = &(mp->ldp[i]);
        if (inf[i].d == NULL) {
            free(mp->ldp);
            free(mp);
            return -ENOMEM;
        }

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        free(mp->ldp);
        free(mp);
        return inf[0].op_error;
    }

    fi->fh = (unsigned long)mp;

    return 0;
}

static int loopback_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                            struct fuse_file_info *fi) {
    DEBUG_MSG("readdir\n");

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;
    struct loopback_dirp *d = &(mp->ldp[0]);

    if (offset != d->offset) {
        seekdir(d->dp, offset);
        d->entry = NULL;
        d->offset = offset;
    }

    while (1) {
        struct stat st;
        off_t nextoff;

        if (!d->entry) {
            d->entry = readdir(d->dp);
            if (!d->entry) {
                break;
            }
        }

        memset(&st, 0, sizeof(st));
        st.st_ino = d->entry->d_ino;
        st.st_mode = d->entry->d_type << 12;
        nextoff = telldir(d->dp);
        if (filler(buf, d->entry->d_name, &st, nextoff)) {
            break;
        }
        d->entry = NULL;
        d->offset = nextoff;
    }
    return 0;
}

static int loopback_releasedir(const char *path, struct fuse_file_info *fi) {
    DEBUG_MSG("releasedir\n");
    int res;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;

    int i;
    // create folder in each device
    for (i = 0; i < NDEVS; i++) {
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = RELEASEDIR_OP;
        inf[i].d = &(mp->ldp[i]);

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    free(mp->ldp);
    free(mp);

    if (res == -1) {
        DEBUG_MSG("releasedirerror path %s %d\n", path, inf[0].op_error);
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_mknod(const char *path, mode_t mode, dev_t rdev) {
    int res = 0;

    DEBUG_MSG("mknod\n");

    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    // mknod in each device
    int i;
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = MKNOD_OP;
        inf[i].mode = mode;
        inf[i].rdev = rdev;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_mkdir(const char *path, mode_t mode) {
    int res;

    DEBUG_MSG("mkdir\n");
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    // create folder in each device
    int i;
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = MKDIR_OP;
        inf[i].mode = mode;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_unlink(const char *path) {
    int res = 0;

    DEBUG_MSG("unlink path %s\n", path);
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    // remove file in each device
    int i;
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = UNLINK_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        DEBUG_MSG("unlink path error %s\n", path, inf[0].op_error);
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_rmdir(const char *path) {
    int res = 0;
    int i;
    DEBUG_MSG("rmdir\n");
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    // remove folder in each device
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = RMDIR_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_symlink(const char *from, const char *to) {
    DEBUG_MSG("symlink\n");

    int res;
    int i;

    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newfrompath[NDEVS];
    char *devices_newtopath[NDEVS];

    for (i = 0; i < NDEVS; i++) {
        devices_newfrompath[i] = malloc(PATHSIZE);
        strcpy(devices_newfrompath[i], devices_path[i]);
        replace_path(from, devices_newfrompath[i]);

        devices_newtopath[i] = malloc(PATHSIZE);
        strcpy(devices_newtopath[i], devices_path[i]);
        replace_path(to, devices_newtopath[i]);

        inf[i].frompath = devices_newfrompath[i];
        inf[i].topath = devices_newtopath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = SYMLINK_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newfrompath[i]);
        free(devices_newtopath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }
    return 0;
}

static int loopback_rename(const char *from, const char *to) {
    int res = 0;

    DEBUG_MSG("rename\n");
    int i;

    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newfrompath[NDEVS];
    char *devices_newtopath[NDEVS];

    for (i = 0; i < NDEVS; i++) {
        devices_newfrompath[i] = malloc(PATHSIZE);
        strcpy(devices_newfrompath[i], devices_path[i]);
        replace_path(from, devices_newfrompath[i]);

        devices_newtopath[i] = malloc(PATHSIZE);
        strcpy(devices_newtopath[i], devices_path[i]);
        replace_path(to, devices_newtopath[i]);

        inf[i].frompath = devices_newfrompath[i];
        inf[i].topath = devices_newtopath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = RENAME_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);
    DEBUG_MSG("Exit wait\n");
    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newfrompath[i]);
        free(devices_newtopath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }
    if (DRIVER == ERASURE) {
        DEBUG_MSG("Going to rename\n");
        m_driver.rename((char *)from, (char *)to);
    }

    return 0;
}

static int loopback_link(const char *from, const char *to) {
    DEBUG_MSG("link\n");

    int res;
    int i;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newfrompath[NDEVS];
    char *devices_newtopath[NDEVS];

    for (i = 0; i < NDEVS; i++) {
        devices_newfrompath[i] = malloc(PATHSIZE);
        strcpy(devices_newfrompath[i], devices_path[i]);
        replace_path(from, devices_newfrompath[i]);

        devices_newtopath[i] = malloc(PATHSIZE);
        strcpy(devices_newtopath[i], devices_path[i]);
        replace_path(to, devices_newtopath[i]);

        inf[i].frompath = devices_newfrompath[i];
        inf[i].topath = devices_newtopath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = LINK_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newfrompath[i]);
        free(devices_newtopath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    DEBUG_MSG("create\n");
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    int res;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    struct mpath_aux *mp = malloc(sizeof(struct mpath_aux));
    mp->devs_fd = malloc(sizeof(unsigned long) * NDEVS);

    char *devices_newpath[NDEVS];

    int i;
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].flags = fi->flags;
        inf[i].mode = mode;
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = CREATE_OP;

        DEBUG_MSG("sending create op %s\n", inf[i].path);
        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        DEBUG_MSG("create error\n", inf[0].op_error);
        free(mp->devs_fd);
        free(mp);
        return inf[0].op_error;
    }

    for (i = 0; i < NDEVS; i++) {
        mp->devs_fd[i] = inf[i].op_res;
    }

    fi->fh = (unsigned long)mp;
    if (DRIVER == ERASURE) {
        DEBUG_MSG("GOING TO REMOVE keys if exist\n");
        m_driver.create((char *)path);
    }
    return 0;
}

static int loopback_open(const char *path, struct fuse_file_info *fi) {
    DEBUG_MSG("open\n");

    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;
    int res;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    struct mpath_aux *mp = malloc(sizeof(struct mpath_aux));
    mp->devs_fd = malloc(sizeof(unsigned long) * NDEVS);

    char *devices_newpath[NDEVS];

    int i;
    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].flags = fi->flags;
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = OPEN_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        DEBUG_MSG("open returned error %d\n", inf[0].op_error);
        free(mp->devs_fd);
        free(mp);
        return inf[0].op_error;
    }

    for (i = 0; i < NDEVS; i++) {
        mp->devs_fd[i] = inf[i].op_res;
    }
    fi->fh = (unsigned long)mp;

    return 0;
}

static int loopback_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // struct timespec tstart={0,0}, tend={0,0};
    // clock_gettime(CLOCK_MONOTONIC, &tstart);
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    DEBUG_MSG("read size %lld and offset %lld\n", size, offset);
    int res = 0;
    int i;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    unsigned char *magicblocks[NDEVS];
    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    off_t driver_offset = 0;
    uint64_t driver_size = 0;
    if (DRIVER == ERASURE) {
        DEBUG_MSG("Going to get driver_offset and drive-size for offset %lld\n", offset);
        m_driver.get_driver_offset(path, offset, &driver_offset);
        m_driver.get_driver_size(path, offset, &driver_size);
        DEBUG_MSG("Driver-offset is %lld\n", driver_offset);
        DEBUG_MSG("Driver-size is %lld\n", driver_size);
    }
    for (i = 0; i < NDEVS; i++) {
        // TODO: newpath is only here for debug purposes
        char newpath[PATHSIZE];

        if (DRIVER != ERASURE) {
            magicblocks[i] = malloc(size);
        } else {
            magicblocks[i] = malloc(driver_size);
        }
        strcpy(newpath, devices_path[i]);
        replace_path(path, newpath);

        DEBUG_MSG("Reading CONTENT at %s off %lld and size %lld\n", newpath, offset, size);

        inf[i].fd = mp->devs_fd[i];
        inf[i].buf = (char *)magicblocks[i];
        inf[i].size = size;
        inf[i].offset = offset;
        inf[i].magicblocksize = -1;
        inf[i].magicblockoffset = -1;

        if (DRIVER == ERASURE) {
            inf[i].magicblocksize = driver_size;
            inf[i].magicblockoffset = driver_offset;
        }

        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = READ_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    if (res > 0) {
        int decode_size = size;

        if (DRIVER == ERASURE) {
            decode_size = driver_size;
        }
        DEBUG_MSG("Call result is %d\n", res);
        m_driver.decode((unsigned char *)buf, magicblocks, decode_size, NDEVS);
    }

    for (i = 0; i < NDEVS; i++) {
        free(magicblocks[i]);
    }

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    // clock_gettime(CLOCK_MONOTONIC, &tend);
    gettimeofday(&tend, NULL);
    store(&multi_read_list, tstart, tend);

    DEBUG_MSG("Exiting read  operation %d\n", res);

    return res;
}

static int loopback_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // struct timespec tstart={0,0}, tend={0,0};
    // clock_gettime(CLOCK_MONOTONIC, &tstart);
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    DEBUG_MSG("write\n");
    int res;
    int i = 0;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;
    unsigned char *magicblocks[NDEVS];
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    if (DRIVER != ERASURE) {
        for (i = 0; i < NDEVS; i++) {
            magicblocks[i] = malloc(size);
        }
    }

    unsigned char block[size];
    memcpy(block, buf, size);

    m_driver.encode(path, magicblocks, block, offset, size, NDEVS);

    off_t magicblockoffset = -1;
    uint64_t magicblocksize = -1;
    if (DRIVER == ERASURE) {
        get_erasure_block_offset(path, offset, &magicblockoffset);
        get_erasure_block_size(path, offset, &magicblocksize);
    }
    // this must be assync in the future
    // Use a thread pool
    for (i = 0; i < NDEVS; i++) {
        // TODO: newpath is only here for debug purposes
        char newpath[PATHSIZE];
        strcpy(newpath, devices_path[i]);
        replace_path(path, newpath);

        inf[i].fd = mp->devs_fd[i];
        inf[i].buf = (char *)magicblocks[i];
        inf[i].size = size;
        inf[i].offset = offset;
        inf[i].magicblockoffset = magicblockoffset;
        inf[i].magicblocksize = magicblocksize;
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = WRITE_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);

        if (DRIVER == ERASURE) {
            DEBUG_MSG("WRITING CONTENT at %s off %lld and size %lld\n", newpath, inf[i].magicblockoffset,
                      inf[i].magicblocksize);
        } else {
            DEBUG_MSG("WRITING CONTENT at %s off %lld and size %lld\n", newpath, offset, size);
        }
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);
    DEBUG_MSG("DOne waiting\n");

    for (i = 0; i < NDEVS; i++) {
        free(magicblocks[i]);
    }
    DEBUG_MSG("devs blocks free\n");

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);
    DEBUG_MSG("Exiting write with res %d\n", res);

    // clock_gettime(CLOCK_MONOTONIC, &tend);
    gettimeofday(&tend, NULL);
    store(&multi_write_list, tstart, tend);

    return res;
}

static int loopback_statfs(const char *path, struct statvfs *stbuf) {
    int res;
    DEBUG_MSG("statfs\n");

    // Check only first device since all are replicates
    char newpath[PATHSIZE];
    strcpy(newpath, devices_path[0]);
    replace_path(path, newpath);

    res = statvfs(newpath, stbuf);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int loopback_flush(const char *path, struct fuse_file_info *fi) {
    int res;
    DEBUG_MSG("flush\n");
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;

    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    (void)path;

    // this must be assync in the future
    // Use a thread pool
    // call flush in all devices
    int i;
    for (i = 0; i < NDEVS; i++) {
        inf[i].fd = mp->devs_fd[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = FLUSH_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    DEBUG_MSG("release\n");

    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;

    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    int i, res;
    // call release in all devices
    for (i = 0; i < NDEVS; i++) {
        inf[i].fd = mp->devs_fd[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = RELEASE_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);
    free(mp->devs_fd);
    free(mp);

    if (res == -1) {
        DEBUG_MSG("release error %d\n", inf[0].op_error);
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    int res;
    DEBUG_MSG("fsync\n");
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    (void)path;

    (void)isdatasync;
    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;

    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    // this must be assync in the future
    // Use a thread pool
    // call flush in all devices
    int i;
    for (i = 0; i < NDEVS; i++) {
        inf[i].fd = mp->devs_fd[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = FSYNC_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_truncate(const char *path, off_t size) {
    DEBUG_MSG("truncate\n");
    int i, res;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].size = size;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = TRUNCATE_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_ftruncate(const char *path, off_t size, struct fuse_file_info *fi) {
    DEBUG_MSG("ftruncate\n");
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct mpath_aux *mp = (struct mpath_aux *)(uintptr_t)fi->fh;
    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    int i, res;
    for (i = 0; i < NDEVS; i++) {
        inf[i].fd = mp->devs_fd[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = FTRUNCATE_OP;
        inf[i].size = size;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_chmod(const char *path, mode_t mode) {
    DEBUG_MSG("chmod\n");

    int i, res;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].mode = mode;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = CHMOD_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

static int loopback_chown(const char *path, uid_t uid, gid_t gid) {
    DEBUG_MSG("chown\n");

    int i, res;
    pthread_mutex_t op_lock;
    pthread_cond_t wait_ops;
    int ops_done = 0;

    struct op_info inf[NDEVS];

    // Initialize mutex and condition variable
    pthread_mutex_init(&op_lock, 0);
    pthread_cond_init(&wait_ops, 0);

    char *devices_newpath[NDEVS];

    for (i = 0; i < NDEVS; i++) {
        devices_newpath[i] = malloc(PATHSIZE);
        strcpy(devices_newpath[i], devices_path[i]);
        replace_path(path, devices_newpath[i]);

        inf[i].path = devices_newpath[i];
        inf[i].cond = &wait_ops;
        inf[i].lock = &op_lock;
        inf[i].uid = uid;
        inf[i].gid = gid;
        inf[i].ops_done = &ops_done;
        inf[i].op_type = CHOWN_OP;

        g_thread_pool_push(thread_pool, &inf[i], NULL);
    }

    res = wait_for_all_requests(inf, &op_lock, &wait_ops);

    pthread_mutex_destroy(&op_lock);
    pthread_cond_destroy(&wait_ops);

    for (i = 0; i < NDEVS; i++) {
        free(devices_newpath[i]);
    }

    if (res == -1) {
        return inf[0].op_error;
    }

    return 0;
}

// BASIC - necessary for writing, reading and listing one pdf inside the folder
// SECOND - necessary to copy files via GUI
// THIRD

static struct fuse_operations loopback_oper = {
    .getattr = loopback_getattr,    // BASIC //needed to have the mount point visible
    .fgetattr = loopback_fgetattr,  // THIRD
    .chown = loopback_chown,
    .chmod = loopback_chmod,
    /*  .access      = loopback_access, */
    .readlink = loopback_readlink,
    .opendir = loopback_opendir,        // BASIC
    .readdir = loopback_readdir,        // BASIC
    .releasedir = loopback_releasedir,  // FORTH folder support
    .mknod = loopback_mknod,
    .mkdir = loopback_mkdir,  // FORTH
    .symlink = loopback_symlink,
    .unlink = loopback_unlink,  // Second //needed to remove files
    .rmdir = loopback_rmdir,    // FORTH
    .rename = loopback_rename,  // FIFTH
    .link = loopback_link,
    .create = loopback_create,    // BASIC //needed to create files in the directory
    .open = loopback_open,        // BASIC
    .read = loopback_read,        // BASIC
    .write = loopback_write,      // BASIC
    .statfs = loopback_statfs,    // SECOND
    .flush = loopback_flush,      // FIFTH
    .release = loopback_release,  // FIFTH
    .fsync = loopback_fsync,      // FIFTH
    .truncate = loopback_truncate,
    .ftruncate = loopback_ftruncate,

};

int init_paths(configuration config) {
    GSList *current = config.m_loop_config.loop_paths;
    DEBUG_MSG("Current list pointer is %p\n", current);
    int ndevs = config.m_loop_config.ndevs;
    devices_path = malloc(ndevs * sizeof(char **));
    int i = 0;

    do {
        DEBUG_MSG("Going through the loop at %d\n", i);

        char *path = (char *)current->data;
        DEBUG_MSG("Current path is %s\n", path);
        // Check that the paths exist and if not create them
        assert(mkdir_p(path) == 0);
        devices_path[i] = path;
        int path_size = strlen(devices_path[i]);

        if (devices_path[i][path_size - 1] != '/') {
            devices_path[i][path_size] = '/';
            devices_path[i][path_size + 1] = '\0';
        }

        DEBUG_MSG("modified path is %s\n", path);

        i += 1;

        current = current->next;

    } while (current != NULL);

    // Init thread pool
    thread_pool = g_thread_pool_new((GFunc)threads_func, NULL, MAX_THREADS, FALSE, NULL);

    return 0;
}

int init_multi_loopback_driver(struct fuse_operations **fuse_operations, configuration data) {
    DEBUG_MSG("Starting multi_loopback driver %d\n");

    DEBUG_MSG("Going to setup paths\n");

    init_paths(data);
    // DEBUG_MSG("path 1 is %s\n", devices_path[0]);
    // DEBUG_MSG("path 2 is %s\n", devices_path[1]);

    DEBUG_MSG("Going to copy root_path\n");

    // strcpy(ROOTPATH, data.m_loop_config.root_path);
    // DEBUG_MSG("rootpath is %s\n", ROOTPATH);

    DRIVER = data.m_loop_config.mode;
    DEBUG_MSG("Driver is %d\n", DRIVER);

    switch (DRIVER) {
        case XOR:
            m_driver.encode = encode_xor;
            m_driver.decode = decode_xor;
            break;
        case REP:
            m_driver.encode = rep_encode;
            m_driver.decode = rep_decode;
            break;
        case ERASURE:
            init_erasure(2, 1);
            m_driver.encode = erasure_encode;
            m_driver.decode = erasure_decode;
            m_driver.get_driver_offset = get_erasure_block_offset;
            m_driver.get_driver_size = get_erasure_block_size;
            m_driver.get_file_size = erasure_get_file_size;
            m_driver.rename = erasure_rename;
            m_driver.create = erasure_create;
            break;
        default:
            return 1;
    }

    NDEVS = data.m_loop_config.ndevs;

    *fuse_operations = &loopback_oper;
    DEBUG_MSG("Going to return setup driver");

    return 0;
}

int clean_multi_loopback_driver(configuration data) {
    // DEBUG_MSG("Going to clean multi_loopback drivers\n");
    print_latencies(multi_write_list, "multi", "write");
    print_latencies(multi_read_list, "multi", "read");
    return 0;
}
