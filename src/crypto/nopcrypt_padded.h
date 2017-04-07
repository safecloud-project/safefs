#ifndef __NOCRYPT_PADDED_H__
#define __NOCRYPT_PADDED_H__

#include "../logdef.h"
#include "../layers_def.h"
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include "../SDSConfig.h"


#define NOP_PADSIZE 16

int nop_encode_padded (unsigned char* dest, const unsigned char* src, int size,void* ident);

int nop_decode_padded (unsigned char* dest, const unsigned char* src, int size,void* ident);

off_t nop_get_file_size_padded (const char* path, off_t origin_size,struct fuse_file_info *fi, struct fuse_operations nextlayer);

int nop_get_cyphered_block_size_padded(int origin_size);

uint64_t nop_get_cyphered_block_offset_padded(uint64_t origin_size);

void nop_padded_init(block_align_config config);

off_t nop_get_truncate_size_padded(off_t size);


#endif /* __NOCRYPT_PADDED_H__ */
