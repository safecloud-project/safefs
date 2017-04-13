/*

Sfuse
  (c) 2016 2016 INESC TEC.  by J. Paulo (ADD Other authors here)

*/
#ifndef __LAYERSDEF_H__
#define __LAYERSDEF_H__

#ifdef __linux__
#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif /* FUSE_USE_VERSION */
#endif /* __linux__ */

#if defined(_POSIX_C_SOURCE)
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
#endif

#include <stdio.h>
#include <fuse.h>
#include <stdlib.h>
#include <sys/stat.h>

// This is used by the sfuse layer drivers
struct key_info {
    const char *path;
    uint64_t offset;
};

struct encode_driver {
    int (*encode)(unsigned char *dest, const unsigned char *src, int size, void *ident);
    int (*decode)(unsigned char *dest, const unsigned char *src, int size, void *ident);
    off_t (*get_file_size)(const char *path, off_t orig_size, struct fuse_file_info *fi,
                           struct fuse_operations nextlayer);
    int (*get_cyphered_block_size)(int orig_size);
    uint64_t (*get_cyphered_block_offset)(uint64_t orig_offset);
    off_t (*get_truncate_size)(off_t size);
    int (*copy_dbkeys)(const char *from, const char *to);
    int (*delete_dbkeys)(const char *path);
};

struct align_driver {
    int (*align_read)(const char *path, char *buf, size_t size, off_t offset, void *fi,
                      struct fuse_operations nextlayer);
    int (*align_write)(const char *path, const char *buf, size_t size, off_t offset, void *fi,
                       struct fuse_operations nextlayer);
    int (*align_create)(const char *path, mode_t mode, void *fi, struct fuse_operations nextlayer);
    int (*align_open)(const char *path, void *fi, struct fuse_operations nextlayer);
    int (*align_truncate)(const char *path, off_t size, struct fuse_file_info *fi, struct fuse_operations nextlayer);
};

struct multi_driver {
    void (*get_driver_offset)(const char *path, off_t offset, off_t *driver_offset);
    void (*get_driver_size)(const char *path, off_t offset, uint64_t *driver_size);
    void (*encode)(const char *path, unsigned char **magicblocks, unsigned char *block, off_t offset, int size,
                   int ndevs);
    void (*decode)(unsigned char *block, unsigned char **magicblocks, int size, int ndevs);
    uint64_t (*get_file_size)(const char *path);
    void (*rename)(char *from, char *to);
    void (*create)(char *path);
    void (*clean)();
};

#endif /*__LAYERSDEF_H__*/
