/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/


#include "sfuse.h"
#include "timestamps/timestamps.h"

// struct with original operations from mounted filesystem
static struct fuse_operations *originalfs_oper;
// struct with sfuse operations
static struct fuse_operations sfuse_oper;

// struct with encoding algorithms
static struct encode_driver enc_driver;

GSList *sfuse_write_list = NULL, *sfuse_read_list = NULL;

static int sfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // struct timespec tstart={0,0}, tend={0,0};
    // clock_gettime(CLOCK_MONOTONIC, &tstart);
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    DEBUG_MSG("(sfuse.c) - Going to read from the file-system.\n");
    DEBUG_MSG("Going to read offset %ld with size %lu\n", offset, size);

    int cblock_size = enc_driver.get_cyphered_block_size(size);
    uint64_t cblock_offset = enc_driver.get_cyphered_block_offset(offset);

    DEBUG_MSG("Going to read path %s cblock_offset %ld with cblock_size %lu\n", path, cblock_offset, cblock_size);

    char aux_cyphered_buf[cblock_size];

    int res = originalfs_oper->read(path, aux_cyphered_buf, cblock_size, cblock_offset, fi);
    if (res <= 0) {
        return res;
    }

    DEBUG_MSG("READ path %s cblock_offset %ld with cblock_size %lu\n", path, cblock_offset, cblock_size);

    struct key_info info;
    info.path = path;
    info.offset = cblock_offset;

    res = enc_driver.decode((unsigned char *)buf, (unsigned char *)aux_cyphered_buf, res, &info);
    DEBUG_MSG("Read path %s cblock_offset %ld with cblock_size %lu return size%d\n", path, cblock_offset, cblock_size,
              res);

    gettimeofday(&tend, NULL);

    // clock_gettime(CLOCK_MONOTONIC, &tend);
    store(&sfuse_read_list, tstart, tend);

    return res;
}

static int sfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // struct timespec tstart={0,0}, tend={0,0};
    // clock_gettime(CLOCK_MONOTONIC, &tstart);
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    DEBUG_MSG("(sfuse.c) - Going to write to the file-system.\n");
    DEBUG_MSG("Going to write path %s offset %ld with size %lu\n", path, offset, size);

    int res;

    int cblock_size = enc_driver.get_cyphered_block_size(size);
    uint64_t cblock_offset = enc_driver.get_cyphered_block_offset(offset);

    DEBUG_MSG("Going to write path %s cblock_offset %ld with cblock_size %lu\n", path, cblock_offset, cblock_size);

    char aux_cyphered_buf[cblock_size];

    struct key_info info;
    info.path = path;
    info.offset = cblock_offset;

    res = enc_driver.encode((unsigned char *)aux_cyphered_buf, (unsigned char *)buf, size, &info);
    if (res < cblock_size) {
        DEBUG_MSG("RES < cblock for encode Going to write path %s cblock_offset %ld with cblock_size %lu\n", path,
                  cblock_offset, cblock_size);

        return -1;
    }

    res = originalfs_oper->write(path, aux_cyphered_buf, cblock_size, cblock_offset, fi);
    if (res < 0) {
        return res;
    }
    if (res < cblock_size) {
        return -1;
    }
    // clock_gettime(CLOCK_MONOTONIC, &tend);
    gettimeofday(&tend, NULL);

    store(&sfuse_write_list, tstart, tend);

    return size;
}

static int sfuse_getattr(const char *path, struct stat *stbuf) {
    int res;

    DEBUG_MSG("(sfuse.c) - Going to gettattr to the file-system.\n");
    DEBUG_MSG("path %s\n", path);

    res = originalfs_oper->getattr(path, stbuf);

    DEBUG_MSG("path checking if is reg file%s\n", path);

    if (res >= 0 && S_ISREG(stbuf->st_mode)) {
        DEBUG_MSG("path is reg file%s\n", path);

        stbuf->st_size = enc_driver.get_file_size(path, stbuf->st_size, NULL, *originalfs_oper);

        if (stbuf->st_size < 0) {
            return -1;
        }
    }

    return res;
}

static int sfuse_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res;

    (void)path;

    DEBUG_MSG("(sfuse.c) - Going to fgettattr to the file-system.\n");
    DEBUG_MSG("path %s\n", path);

    res = originalfs_oper->fgetattr(path, stbuf, fi);

    DEBUG_MSG("res is %d\n", res);

    if (res == 0 && S_ISREG(stbuf->st_mode)) {
        DEBUG_MSG("path is reg file%s\n", path);

        stbuf->st_size = enc_driver.get_file_size(path, stbuf->st_size, fi, *originalfs_oper);
        if (stbuf->st_size < 0) {
            return -1;
        }
    }

    return res;
}

static int sfuse_truncate(const char *path, off_t size) {
    DEBUG_MSG("sfuse truncate path %s %lu\n", path, size);
    // Check if it is working
    off_t truncate_size = enc_driver.get_truncate_size(size);

    // int res=originalfs_oper->truncate(path,size);
    int res = originalfs_oper->truncate(path, truncate_size);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int sfuse_ftruncate(const char *path, off_t size, struct fuse_file_info *fi) {
    DEBUG_MSG("ftruncate path %s %lu\n", path, size);

    off_t truncate_size = enc_driver.get_truncate_size(size);

    int res = originalfs_oper->ftruncate(path, truncate_size, fi);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

// TODO: IVS should also be deleted for unlinked (removed) files.

int init_sfuse_driver(struct fuse_operations **originop, configuration data) {
    originalfs_oper = *originop;

    // TODO:
    // Maybe this could be initialized with the sfuse_oper struct
    sfuse_oper.init = originalfs_oper->init;
    sfuse_oper.destroy = originalfs_oper->destroy;
    sfuse_oper.getattr = sfuse_getattr;
    sfuse_oper.fgetattr = sfuse_fgetattr;
    sfuse_oper.access = originalfs_oper->access;
    sfuse_oper.readlink = originalfs_oper->readlink;
    sfuse_oper.opendir = originalfs_oper->opendir;
    sfuse_oper.readdir = originalfs_oper->readdir;
    sfuse_oper.releasedir = originalfs_oper->releasedir;
    sfuse_oper.mknod = originalfs_oper->mknod;
    sfuse_oper.mkdir = originalfs_oper->mkdir;
    sfuse_oper.symlink = originalfs_oper->symlink;
    sfuse_oper.unlink = originalfs_oper->unlink;
    sfuse_oper.rmdir = originalfs_oper->rmdir;
    sfuse_oper.rename = originalfs_oper->rename;
    sfuse_oper.link = originalfs_oper->link;
    sfuse_oper.create = originalfs_oper->create;
    sfuse_oper.open = originalfs_oper->open;
    sfuse_oper.read = sfuse_read;
    sfuse_oper.write = sfuse_write;
    sfuse_oper.statfs = originalfs_oper->statfs;
    sfuse_oper.flush = originalfs_oper->flush;
    sfuse_oper.release = originalfs_oper->release;
    sfuse_oper.fsync = originalfs_oper->fsync;
    sfuse_oper.truncate = sfuse_truncate;
    sfuse_oper.ftruncate = sfuse_ftruncate;
    sfuse_oper.chown = originalfs_oper->chown;
    sfuse_oper.chmod = originalfs_oper->chmod;
    // sfuse_oper.setxattr    = originalfs_oper->setxattr;
    // sfuse_oper.getxattr    = originalfs_oper->getxattr;
    // sfuse_oper.listxattr   = originalfs_oper->listxattr;
    // sfuse_oper.removexattr = originalfs_oper->removexattr;

    switch (data.enc_config.mode) {
        case STANDARD:
            rand_init(data.enc_config.key, 16, data.block_config);
            enc_driver.encode = rand_encode;
            enc_driver.decode = rand_decode;
            enc_driver.get_file_size = rand_get_file_size;
            enc_driver.get_cyphered_block_size = rand_get_cyphered_block_size;
            enc_driver.get_cyphered_block_offset = rand_get_cyphered_block_offset;
            enc_driver.get_truncate_size = rand_get_truncate_size;
            break;
        case DETERMINISTIC:
            det_init(data.enc_config.key, (unsigned char *)data.enc_config.iv, 16, data.block_config);
            enc_driver.encode = det_encode;
            enc_driver.decode = det_decode;
            enc_driver.get_file_size = det_get_file_size;
            enc_driver.get_cyphered_block_size = det_get_cyphered_block_size;
            enc_driver.get_cyphered_block_offset = det_get_cyphered_block_offset;
            enc_driver.get_truncate_size = det_get_truncate_size;
            break;
        case NOPCRYPT:
            enc_driver.encode = nop_encode;
            enc_driver.decode = nop_decode;
            enc_driver.get_file_size = nop_get_file_size;
            enc_driver.get_cyphered_block_size = nop_get_cyphered_block_size;
            enc_driver.get_cyphered_block_offset = nop_get_cyphered_block_offset;
            enc_driver.get_truncate_size = nop_get_truncate_size;
            break;
        case NOPCRYPT_PAD:
            nop_padded_init(data.block_config);
            enc_driver.encode = nop_encode_padded;
            enc_driver.decode = nop_decode_padded;
            enc_driver.get_file_size = nop_get_file_size_padded;
            enc_driver.get_cyphered_block_size = nop_get_cyphered_block_size_padded;
            enc_driver.get_cyphered_block_offset = nop_get_cyphered_block_offset_padded;
            enc_driver.get_truncate_size = nop_get_truncate_size_padded;
            break;
        default:
            return -1;
    }

    // Copy original filesystem opers to a struct
    // TODO: try to avoid originalfs_oper being a global variable
    *originop = &sfuse_oper;

    return 0;
}

int clean_sfuse_driver(configuration data) {
    // DEBUG_MSG("Going to clean sfuse drivers\n");
    print_latencies(sfuse_write_list, "sfuse", "write");
    print_latencies(sfuse_read_list, "sfuse", "read");

    switch (data.enc_config.mode) {
        case STANDARD:
            return rand_clean();
        case DETERMINISTIC:
            return det_clean();
        default:
            return 0;
    }
}
