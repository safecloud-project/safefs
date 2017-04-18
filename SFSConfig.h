/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/


#ifndef __SFSConfig_H__
#define __SFSConfig_H__

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include "logdef.h"
#include "inih/ini.h"

#define MULTI_LOOPBACK 0
#define SFUSE 1
#define BLOCK_ALIGN 2
#define NOPFUSE 3

// Default configuration files location
#define LOCAL_SDSCONFIG_PATH "default.ini"
#define DEFAULT_SDSCONFIG_PATH "/etc/safefs/default.ini"

typedef struct multi_loop_configuration {
    GSList* loop_paths;
    char* root_path;
    int mode;
    int ndevs;
} m_loop_conf;

typedef struct encode_configuration {
    char* key;
    char* iv;
    int key_size;
    int mode;
} enc_config;

typedef struct block_align_configuration {
    int block_size;
    int mode;
} block_align_config;

typedef struct log_configuration { int mode; } log_config;

typedef struct sds_configuration {
    enc_config enc_config;
    m_loop_conf m_loop_config;
    block_align_config block_config;
    GSList* layers;
    log_config logging_configuration;
} configuration;

int init_config(char* configuration_file_path, configuration** config);

int clean_config(configuration* config);

#endif /* __SFSConfig_H__ */
