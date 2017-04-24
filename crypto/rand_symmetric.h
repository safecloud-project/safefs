/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/

#ifndef __RANDIV_SYMMETRIC_H__
#define __RANDIV_SYMMETRIC_H__

#include "openssl/symmetric.h"
#include "../logdef.h"
#include "../layers_def.h"
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../SFSConfig.h"

#define RAND_PADSIZE 16

int rand_init(char* key, int block_size, block_align_config config);

int rand_encode(unsigned char* dest, const unsigned char* src, int size, void* ident);

int rand_decode(unsigned char* dest, const unsigned char* src, int size, void* ident);

off_t rand_get_file_size(const char* path, off_t origin_size, struct fuse_file_info* fi,
                         struct fuse_operations nextlayer);

int rand_get_cyphered_block_size(int origin_size);

uint64_t rand_get_cyphered_block_offset(uint64_t origin_size);

int rand_clean();

off_t rand_get_truncate_size(off_t size);

#endif
