/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes
*/

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlog.h>

#include "SFSConfig.h"
#include "logdef.h"
#include "utils.h"

struct zlog_category_s *CATEGORY;

#define MAX_CFG_LINE 4096

static struct sds_configuration *CONFIGURATION;

typedef void (*debug_func_t)(const char *format, va_list args);
typedef void (*error_func_t)(const char *format, va_list args);
typedef void (*screen_func_t)(const char *format, va_list args);

static debug_func_t debug = NULL;
static error_func_t error = NULL;
static screen_func_t screen = NULL;

/**
* Check if the directory hosting the file stored at path exists and is a
* directory.
* @param path
* @returns 0 if OK, -1 if not
*/
static int check_directory_existence(char *path) {
    char *directory = dirname(path);
    struct stat s;
    int err = stat(directory, &s);
    if (-1 == err) {
        if (ENOENT == errno) {
            fprintf(stderr, "The logging directory %s does not exist\n", directory);
            return -1;
        } else {
            perror("stat");
            return -1;
        }
    } else {
        if (!S_ISDIR(s.st_mode)) {
            fprintf(stderr, "The logging destination %s is not a directory\n", directory);
            return -1;
        }
    }
    return 0;
}

/**
 * Check if the directories hosting the log files actually exist. If they don't,
 * the program exits with return code EXIT_FAILURE (1).
 * @parm path Path to the zlog configuration file to test
 */
static void check_log_file(char *zlog_config_path) {
    char buffer[MAX_CFG_LINE + 1];
    FILE *conf_file = fopen(zlog_config_path, "r");
    char path[256];
    while (fgets(buffer, MAX_CFG_LINE, conf_file) != NULL) {
        path[0] = '\0';
        sscanf(buffer, "sfuse.DEBUG \"%s\"", path);
        if (strlen(path) > 0) {
            if (check_directory_existence(path) != 0) {
                exit(EXIT_FAILURE);
            }
        }
        path[0] = '\0';
        sscanf(buffer, "sfuse.ERROR \"%s\"", path);
        if (strlen(path) > 0) {
            if (check_directory_existence(path) != 0) {
                exit(EXIT_FAILURE);
            }
        }
    }
    fclose(conf_file);
}

/**
 * An empty function that does not do anything but can be used in replacement of
 * the other logging functions.
 * @param format Format of the message
 * @param args A list of arguments
 */
static void DROP_MSG(const char *format, va_list args) {}

/**
 * A function that logs debug level messages using vzlog.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
static void ACTIVE_DEBUG_MSG(const char *format, va_list args) {
    assert(CATEGORY != NULL);
    vzlog_debug(CATEGORY, format, args);
}

/**
 * A function that logs error level messages using vzlog.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
void ACTIVE_ERROR_MSG(const char *format, va_list args) {
    assert(CATEGORY != NULL);
    vzlog_error(CATEGORY, format, args);
}

/**
 * A function that logs messages on the screen using vprintf.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
void ACTIVE_SCREEN_MSG(const char *format, va_list args) {
    assert(CATEGORY != NULL);
    vprintf(format, args);
}

/**
 * Initializes the logging infrastructure by looking at the various configuration
 * files of the project.
 */
void LOG_INIT() {
    char *config_path = NULL;
    if (file_exists(LOCAL_SDSCONFIG_PATH)) {
        config_path = LOCAL_SDSCONFIG_PATH;
    } else if (file_exists(DEFAULT_SDSCONFIG_PATH)) {
        config_path = DEFAULT_SDSCONFIG_PATH;
    } else {
        fprintf(stderr, "[logdef::LOG_INIT] Could not load any configuration file\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Loading safefs configuration file: %s\n", config_path);

    assert(init_config(config_path, &CONFIGURATION) == 0);

    if (CONFIGURATION->logging_configuration.mode) {
        char *zlog_config_path;
        if (file_exists(LOCAL_ZLOGCONFIG_PATH)) {
            zlog_config_path = LOCAL_ZLOGCONFIG_PATH;
        } else if (file_exists(DEFAULT_ZLOGCONFIG_PATH)) {
            zlog_config_path = DEFAULT_ZLOGCONFIG_PATH;
        } else {
            fprintf(stderr, "[logdef::LOG_INIT] Could not find any configuration file for zlog!\n");
            exit(EXIT_FAILURE);
        }
        fprintf(stdout, "Loading zlog configuration file: %s\n", zlog_config_path);
        int rc = zlog_init(zlog_config_path);
        if (rc) {
            fprintf(stderr, "[logdef::LOG_INIT] Could not load zlog configuration file (rc = %d)\n", rc);
            exit(EXIT_FAILURE);
        }
        check_log_file(zlog_config_path);
        CATEGORY = zlog_get_category("sfuse");
        if (!CATEGORY) {
            fprintf(stderr, "[logdef::LOG_INIT] Could not load category sfuse from zlog.conf\n");
            exit(EXIT_FAILURE);
        }
        debug = ACTIVE_DEBUG_MSG;
        error = ACTIVE_ERROR_MSG;
        screen = ACTIVE_SCREEN_MSG;
        fprintf(stdout, "logging enabled\n");
    } else {
        debug = DROP_MSG;
        error = DROP_MSG;
        screen = DROP_MSG;
        fprintf(stdout, "logging disabled\n");
    }
}

/**
 * Tears down the logging infrastructure
 */
void LOG_EXIT() {
    if (CONFIGURATION->logging_configuration.mode) {
        zlog_fini();
    }
    clean_config(CONFIGURATION);
}

void DEBUG_MSG(const char *format, ...) {
    va_list args;
    va_start(args, format);
    debug(format, args);
    va_end(args);
}

void ERROR_MSG(const char *format, ...) {
    va_list args;
    va_start(args, format);
    error(format, args);
    va_end(args);
}

void SCREEN_MSG(const char *format, ...) {
    va_list args;
    va_start(args, format);
    screen(format, args);
    va_end(args);
}
