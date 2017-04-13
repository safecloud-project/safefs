#include "det_symmetric.h"

unsigned char* iv = NULL;
int DET_BLOCKSIZE = 0;

int det_init(char* key, unsigned char* arg_iv, int key_size, block_align_config config) {
    DET_BLOCKSIZE = config.block_size;

    iv = arg_iv;

    int init_sym_val = openssl_init(key, key_size);

    if (init_sym_val < 0) {
        return -1;
    } else {
        return 0;
    }
}

// size here comes without pad
int det_encode(unsigned char* dest, const unsigned char* src, int size, void* ident) {
    DEBUG_MSG("Inside deterministic encoding %d\n", size);

    unsigned char* cypherbuffer = malloc(size + DET_PADSIZE);

    int res = openssl_encode(iv, cypherbuffer, src, size);

    memcpy(dest, cypherbuffer, res);

    free(cypherbuffer);

    DEBUG_MSG("Inside deterministic encoding %d, returning size %d\n", size, res);

    return res;
}

// size here comes with pad
int det_decode(unsigned char* dest, const unsigned char* src, int size, void* ident) {
    unsigned char* plainbuffer = malloc(size);
    int res = openssl_decode(iv, plainbuffer, src, size);
    memcpy(dest, plainbuffer, res);
    free(plainbuffer);

    DEBUG_MSG("Inside deterministic decoding %d, returning res %d\n", size, res);
    return res;
}

int det_clean() { return openssl_clean(); }

off_t det_get_file_size(const char* path, off_t original_size, struct fuse_file_info* fi_in,
                        struct fuse_operations nextlayer) {
    uint64_t nr_complete_blocks = original_size / (DET_BLOCKSIZE + DET_PADSIZE);
    int last_incomplete_block_size = original_size % (DET_BLOCKSIZE + DET_PADSIZE);
    uint64_t last_block_address = original_size - last_incomplete_block_size;
    int last_block_real_size = 0;

    // We have original size but must get the real size of the last block which may be padded
    DEBUG_MSG("Got size %s original size is %lu last block size is %d\n", path, original_size,
              last_incomplete_block_size);

    if (last_incomplete_block_size > 0) {
        char aux_cyphered_buf[last_incomplete_block_size];
        unsigned char aux_plain_buf[last_incomplete_block_size];
        struct fuse_file_info* fi;
        int res;

        if (fi_in != NULL) {
            fi = fi_in;
        } else {
            fi = malloc(sizeof(struct fuse_file_info));
            DEBUG_MSG("before open %s original size is %lu last block size is %d, last_block_address is %llu\n", path,
                      original_size, last_incomplete_block_size, (unsigned long long int)last_block_address);

            // TODO: add -D_GNU_SOURCE for O_LARGEFILE
            fi->flags = O_RDONLY;
            res = nextlayer.open(path, fi);
            if (res == -1) {
                DEBUG_MSG("Failed open %s original size is %lu last block size is %d, last_block_address is %llu\n",
                          path, original_size, last_incomplete_block_size, (unsigned long long int)last_block_address);
                return res;
            }
        }

        DEBUG_MSG("read %s original size is %lu last block size is %d, last_block_address is %llu\n", path,
                  original_size, last_incomplete_block_size, (unsigned long long int)last_block_address);

        // read the block and decode to understand the number of bytes actually written
        res = nextlayer.read(path, aux_cyphered_buf, last_incomplete_block_size, last_block_address, fi);
        if (res < last_incomplete_block_size) {
            DEBUG_MSG("Failed read %s original size is %lu last block size is %d, last_block_address is %llu\n", path,
                      original_size, last_incomplete_block_size, (unsigned long long int)last_block_address);
            return -1;
        }

        if (fi_in == NULL) {
            res = nextlayer.release(path, fi);
            if (res == -1) {
                DEBUG_MSG("Failed close %s original size is %lu last block size is %d, last_block_address is %llu\n",
                          path, original_size, last_incomplete_block_size, (unsigned long long int)last_block_address);
                return -1;
            }
            free(fi);
        }

        DEBUG_MSG("bedore decode read %s original size is %lu last block size is %d, last_block_address is %llu\n",
                  path, original_size, last_incomplete_block_size, (unsigned long long int)last_block_address);

        last_block_real_size =
            det_decode(aux_plain_buf, (unsigned char*)aux_cyphered_buf, last_incomplete_block_size, NULL);
    }

    DEBUG_MSG("size for file %s , last block real size is %d and file real size is %lu.\n", path, last_block_real_size,
              nr_complete_blocks * (DET_BLOCKSIZE) + last_block_real_size);

    return nr_complete_blocks * DET_BLOCKSIZE + last_block_real_size;
}

int det_get_cyphered_block_size(int origin_size) {
    int offset_block_aligned = origin_size / DET_PADSIZE * DET_PADSIZE;

    return offset_block_aligned + DET_PADSIZE;
}

uint64_t det_get_cyphered_block_offset(uint64_t origin_offset) {
    DEBUG_MSG("DET_BLOCKSIZE is  %d.\n", DET_BLOCKSIZE);

    uint64_t blockid = origin_offset / DET_BLOCKSIZE;

    return blockid * (DET_BLOCKSIZE + DET_PADSIZE);
}

off_t det_get_truncate_size(off_t size) {
    uint64_t nr_blocks = size / DET_BLOCKSIZE;
    uint64_t extra_bytes = size % DET_BLOCKSIZE;

    off_t truncate_size = nr_blocks * (DET_BLOCKSIZE + DET_PADSIZE);

    if (extra_bytes > 0) {
        truncate_size += det_get_cyphered_block_size(extra_bytes);
    }

    DEBUG_MSG("truncating file sfuse to %lu\n", truncate_size);
    return truncate_size;
}
