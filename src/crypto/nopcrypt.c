/*

Sfuse
  (c) 2016 2016 INESC TEC.  by J. Paulo (ADD Other authors here)

*/
#include "nopcrypt.h"

#include <string.h>

int nop_encode(unsigned char* dest, const unsigned char* src, int size, void* ident) {
    DEBUG_MSG("Entering nop_encode function.\n");
    // just make a copy
    memcpy(dest, src, size);

    DEBUG_MSG("Exiting nop_decode function.\n");

    return size;
}

int nop_decode(unsigned char* dest, const unsigned char* src, int size, void* ident) {
    DEBUG_MSG("Entering nop_decode function.\n");
    // just make a copy
    memcpy(dest, src, size);
    DEBUG_MSG("Exiting nop_decode function.\n");

    return size;
}

off_t nop_get_file_size(const char* path, off_t origin_size, struct fuse_file_info* fi,
                        struct fuse_operations nextlayer) {
    return origin_size;
}

int nop_get_cyphered_block_size(int origin_size) { return origin_size; }

uint64_t nop_get_cyphered_block_offset(uint64_t origin_offset) { return origin_offset; }

off_t nop_get_truncate_size(off_t size) { return size; }
