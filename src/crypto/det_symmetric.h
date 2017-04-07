/*

  Symmetric encryption driver
  (c) 2016 2016 INESC TEC.  by R. Pontes (ADD Other authors here)

*/
#ifndef __DET_SYMMETRIC_H__
#define __DET_SYMMETRIC_H__


#include "openssl/symmetric.h"
#include "../logdef.h"
#include "../layers_def.h"
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include "../SDSConfig.h"


 #define DET_PADSIZE 16


int det_init(char* key, unsigned char* arg_iv, int block_size, block_align_config config);

int det_encode(unsigned char* dest, const unsigned char* src, int size, void* ident);

int det_decode(unsigned char* dest, const unsigned char* src, int size, void* ident);

off_t det_get_file_size (const char* path, off_t origin_size, struct fuse_file_info *fi, struct fuse_operations nextlayer);

int det_get_cyphered_block_size(int origin_size);

uint64_t det_get_cyphered_block_offset(uint64_t origin_size);

off_t det_get_truncate_size(off_t size);

int det_clean();


#endif 
