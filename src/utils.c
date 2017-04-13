#include <errno.h>
#include <linux/limits.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

int file_exists(char *path) {
    struct stat s;
    int err = stat(path, &s);
    if (err != 0) {
        return 0;
    }
    /* Check if path points to a regular path */
    if (!S_ISREG(s.st_mode)) {
        return 0;
    }
    return 1;
}

/*
void generate_random_block(unsigned char* str, int size){

        static unsigned char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    int i;
        for(i=0;i<size;i++){
                int charset_pos=rand() % (int)(sizeof(charset) -1);
                str[i]=charset[charset_pos];
        }
}*/

void generate_random_block(unsigned char *str, int size) {
    // Sometimes this function can return an error if not enough sources of randomness was found.
    RAND_pseudo_bytes(str, size);
}

// Replace the path of the file intercepted with fuse with the one of the meta file
// TODO this ROOTPATH is hardcoded for now
int replace_path(char const *path, const char *newpath) {
    strcat((char *)newpath, path);

    return 0;
}

int mkdir_p(const char *path) {
    /* Copied from https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 */
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p;

    errno = 0;

    /* Copy string so its mutable */
    if (len > sizeof(_path) - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(_path, path);

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';
            if (mkdir(_path, S_IRWXU) != 0) {
                if (errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if (mkdir(_path, S_IRWXU) != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}
