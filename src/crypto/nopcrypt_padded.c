/*

Sfuse
  (c) 2016 2016 INESC TEC.  by J. Paulo (ADD Other authors here)

*/
#include "nopcrypt_padded.h"

#include <string.h>

int NOP_BLOCKSIZE = 0;

void nop_padded_init(block_align_config config) { NOP_BLOCKSIZE = config.block_size; }

// SIze here comes without pad
int nop_encode_padded(unsigned char* dest, const unsigned char* src, int size, void* ident) {
    DEBUG_MSG("Entering nop_encode function.\n");

    // just make a copy
    memcpy(dest, src, size + NOP_PADSIZE);

    DEBUG_MSG("Exiting nop_decode function.\n");

    return size + NOP_PADSIZE;
}

// SIze here comes with pad
int nop_decode_padded(unsigned char* dest, const unsigned char* src, int size, void* ident) {
    DEBUG_MSG("Entering nop_decode function.\n");
    // just make a copy
    memcpy(dest, src, size - NOP_PADSIZE);
    DEBUG_MSG("Exiting nop_decode function.\n");

    return size - NOP_PADSIZE;
}

off_t nop_get_file_size_padded(const char* path, off_t original_size, struct fuse_file_info* fi,
                               struct fuse_operations nextlayer) {
    DEBUG_MSG("Got size %s %lu.\n", path, original_size);

    uint64_t nrblocks = original_size / (NOP_BLOCKSIZE + NOP_PADSIZE);
    if (original_size % (NOP_BLOCKSIZE + NOP_PADSIZE) > 0) {
        nrblocks += 1;
    }

    DEBUG_MSG("size for file %s IS %lu.\n", path, original_size - (nrblocks * NOP_PADSIZE));

    return original_size - (nrblocks * NOP_PADSIZE);
}

int nop_get_cyphered_block_size_padded(int origin_size) { return origin_size + NOP_PADSIZE; }

uint64_t nop_get_cyphered_block_offset_padded(uint64_t origin_offset) {
    DEBUG_MSG("NOP_BLOCKSIZE is  %d.\n", NOP_BLOCKSIZE);

    uint64_t blockid = origin_offset / NOP_BLOCKSIZE;

    return blockid * (NOP_BLOCKSIZE + NOP_PADSIZE);
}

off_t nop_get_truncate_size_padded(off_t size) {
    uint64_t nr_blocks = size / NOP_BLOCKSIZE;
    uint64_t extra_bytes = size % NOP_BLOCKSIZE;

    off_t truncate_size = nr_blocks * (NOP_BLOCKSIZE + NOP_PADSIZE);

    if (extra_bytes > 0) {
        truncate_size += nop_get_cyphered_block_size_padded(extra_bytes);
    }

    DEBUG_MSG("truncating file sfuse to #lu\n", truncate_size);
    return truncate_size;
}
