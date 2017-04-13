#ifndef __ERASURE_H__
#define __ERASURE_H__

#include <sys/types.h>

void init_erasure(int k, int m);

uint64_t erasure_get_file_size(const char* path);

void get_erasure_block_offset(const char* path, off_t offset, off_t* erasure_size);

void get_erasure_block_size(const char* path, off_t offset, uint64_t* erasue_size);

void erasure_encode(const char* path, unsigned char** magicblocks, unsigned char* block, off_t offset, int size,
                    int ndevs);
/**
 * Decode blocks of erasure coded data.
 * @param block Destination for the decoded block
 * @param magicblocks Encoded blocks
 * @param size Size of the blocks
 * @param ndevs The number of blocks to write to
 */
void erasure_decode(unsigned char* block, unsigned char** magicblocks, int size, int ndevs);

void erasure_rename(char* from, char* to);

void erasure_create(char* path);

#endif /* __ERASURE_H__ */
