/*

  Symmetric encryption driver
  (c) 2016 2016 INESC TEC.  by R. Pontes (ADD Other authors here)

*/
#ifndef __OPENSSL_SYMMETRIC_H__
#define __OPENSLL_SYMMETRIC_H__

#include <time.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>
#include "../../logdef.h"

int openssl_init(char* key, int block_size);

int openssl_encode(unsigned char* iv, unsigned char* dest, const unsigned char* src, int size);

int openssl_decode(unsigned char* iv, unsigned char* dest, const unsigned char* src, int size);

int openssl_clean();

unsigned char* openssl_rand_str(int length);

void handleErrors(void);

#endif
