/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/


#ifndef __BLOCKALIGN_H__
#define __BLOCKALIGN_H__

#include "../layers_def.h"
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../logdef.h"
#include "../SDSConfig.h"
#include "../ivtable/ivtable.h"

#define READ 0
#define WRITE 1

int block_align_read(const char *path, char *buf, size_t size, off_t offset, void *fi, struct fuse_operations);
int block_align_write(const char *path, const char *buf, size_t size, off_t offset, void *fi, struct fuse_operations);
int block_align_create(const char *path, mode_t mode, void *fi, struct fuse_operations nextlayer);
int block_align_open(const char *path, void *fi, struct fuse_operations nextlayer);
int block_align_truncate(const char *path, off_t size, struct fuse_file_info *fi, struct fuse_operations nextlayer);

struct io_info {
    const char *path;
    void *fi;
    struct fuse_operations nextlayer;
};

void define_block_size(block_align_config config);

#endif /* __BLOCKALIGN_H__ */
