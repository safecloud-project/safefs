#ifndef __ALIGNFUSE_H__
#define __ALIGNFUSE_H__

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

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <sys/param.h>

#include "layers_def.h"
#include "align/nopalign.h"
#include "align/blockalign.h"
#include "logdef.h"

#define NOP 0
#define BLOCK 1

int init_align_driver(struct fuse_operations** originop, configuration config);
int clean_align_driver(configuration config);

#endif /* __ALIGNFUSE_H__ */
