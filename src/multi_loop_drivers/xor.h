#ifndef __XOR_H__
#define __XOR_H__

#include <sys/types.h>

void decode_xor(unsigned char *block, unsigned char **magicblocks, int size, int ndevs);

void encode_xor(const char *path, unsigned char **magicblocks, unsigned char *block, off_t offset, int size, int ndevs);

#endif /* __XOR_H__ */
