/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/
#include "nopfuse.h"
#include "timestamps/timestamps.h"

// struct with original operations from mounted filesystem
static struct fuse_operations *originalfs_oper;
// struct with nop operations
static struct fuse_operations nop_oper;

GSList *nop_write_list = NULL, *nop_read_list = NULL;

static int nopfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    int res = originalfs_oper->read(path, buf, size, offset, fi);

    gettimeofday(&tend, NULL);
    store(&nop_read_list, tstart, tend);

    return res;
}

static int nopfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    int res = originalfs_oper->write(path, buf, size, offset, fi);

    gettimeofday(&tend, NULL);
    store(&nop_write_list, tstart, tend);

    return res;
}

int init_nop_layer(struct fuse_operations **originop, configuration data) {
    originalfs_oper = *originop;

    nop_oper.init = originalfs_oper->init;
    nop_oper.destroy = originalfs_oper->destroy;
    nop_oper.getattr = originalfs_oper->getattr;
    nop_oper.fgetattr = originalfs_oper->fgetattr;
    nop_oper.access = originalfs_oper->access;
    nop_oper.readlink = originalfs_oper->readlink;
    nop_oper.opendir = originalfs_oper->opendir;
    nop_oper.readdir = originalfs_oper->readdir;
    nop_oper.releasedir = originalfs_oper->releasedir;
    nop_oper.mknod = originalfs_oper->mknod;
    nop_oper.mkdir = originalfs_oper->mkdir;
    nop_oper.symlink = originalfs_oper->symlink;
    nop_oper.unlink = originalfs_oper->unlink;
    nop_oper.rmdir = originalfs_oper->rmdir;
    nop_oper.rename = originalfs_oper->rename;
    nop_oper.link = originalfs_oper->link;
    nop_oper.create = originalfs_oper->create;
    nop_oper.open = originalfs_oper->open;
    nop_oper.read = nopfuse_read;
    nop_oper.write = nopfuse_write;
    nop_oper.statfs = originalfs_oper->statfs;
    nop_oper.flush = originalfs_oper->flush;
    nop_oper.release = originalfs_oper->release;
    nop_oper.fsync = originalfs_oper->fsync;
    nop_oper.truncate = originalfs_oper->truncate;
    nop_oper.ftruncate = originalfs_oper->ftruncate;
    nop_oper.chown = originalfs_oper->chown;
    nop_oper.chmod = originalfs_oper->chmod;
    // nop_oper.setxattr    = originalfs_oper->setxattr;
    // nop_oper.getxattr    = originalfs_oper->getxattr;
    // nop_oper.listxattr   = originalfs_oper->listxattr;
    // nop_oper.removexattr = originalfs_oper->removexattr;

    // Copy original filesystem opers to a struct
    // TODO: try to avoid originalfs_oper being a global variable
    *originop = &nop_oper;

    return 0;
}

int clean_nop_layer(configuration data) {
    // DEBUG_MSG("Going to clean nop drivers\n");
    print_latencies(nop_write_list, "nop", "write");
    print_latencies(nop_read_list, "nop", "read");
    return 0;
}
