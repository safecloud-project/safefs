#ifndef __EC_LAYER_H__
#define __EC_LAYER_H__

#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define _GNU_SOURCE

int init_ec_driver(struct fuse_operations** fuse_operations);
int clean_ec_driver(struct fuse_operations** fuse_operations);

#endif /* __EC_LAYER_H__ */
