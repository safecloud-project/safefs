#ifndef __MULTI_REP_H__
#define __MULTI_REP_H__

#include <sys/types.h>

void rep_decode(unsigned char *block, unsigned char **magicblocks, int size, int ndevs);

void rep_encode(const char *path, unsigned char **magicblocks, unsigned char *block, off_t offset, int size, int ndevs);

#endif /* __MULTI_REP_H__ */
