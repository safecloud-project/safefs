/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

*/

/*
 * Loopback OSXFUSE file system in C. Uses the high-level FUSE API.
 * Based on the fusexmp_fh.c example from the Linux FUSE distribution.
 * Amit Singh <http://osxbook.com>
 */

#include <assert.h>
#include <stdio.h>

#include <libgen.h>
#include <glib.h>

#include "ec_layer.h"
#include "erasure.h"

static GHashTable *FILES = NULL;
static GList *DIRECTORIES = NULL;

/**
 * Returns a file record. Returns NULL if no record of the file is found.
 * @param path Path to the file to load
 * @return The encoded_file_t object
 */
static encoded_file_t *get_encoded_file(const char *path) {
    return (encoded_file_t *)g_hash_table_lookup(FILES, (gconstpointer)path);
}

static int save_encoded_file(const char *path, encoded_file_t *ef) {
    char *key = strdup(path);
    int res = g_hash_table_insert(FILES, key, (gpointer)ef);
    return res;
}

static int ec_getattr(const char *path, struct stat *stbuf) {
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (strcmp(path, "/mnt/fuse_data/") == 0 ||
        g_list_find_custom(DIRECTORIES, (gconstpointer)path, (GCompareFunc)strcmp) != NULL) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }

    // List known directories

    encoded_file_t *ef = get_encoded_file(path);
    if (ef == NULL) {
        return -ENOENT;
    }
    stbuf->st_size = ef->size;
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    ;

    return 0;
}

static int ec_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (strcmp(path, "/mnt/fuse_data/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }

    encoded_file_t *ef = get_encoded_file(path);
    if (ef == NULL) {
        return -ENOENT;
    }
    stbuf->st_size = ef->size;
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;

    return 0;
}
/*
static int
ec_readlink(const char *path, char *buf, size_t size)
{
    int res;


    res = readlink(path, buf, size - 1);
    if (res == -1) {
        return -errno;
    }

    buf[res] = '\0';

    return 0;
}
*/
struct ec_dirp {
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

static int ec_opendir(const char *path, struct fuse_file_info *fi) {
    int res;

    struct ec_dirp *d = malloc(sizeof(struct ec_dirp));
    if (d == NULL) {
        return -ENOMEM;
    }

    d->dp = opendir(path);
    if (d->dp == NULL) {
        res = -errno;
        free(d);
        return res;
    }

    d->offset = 0;
    d->entry = NULL;

    fi->fh = (unsigned long)d;

    return 0;
}

static inline struct ec_dirp *get_dirp(struct fuse_file_info *fi) { return (struct ec_dirp *)(uintptr_t)fi->fh; }

static int is_ancestor_directory(const char *parent, const char *kid) {
    size_t parent_length = strlen(parent);
    size_t kid_length = strlen(kid);
    if (parent_length >= kid_length) {
        return 0;
    }
    if (strncmp(parent, kid, parent_length) == 0) {
        return parent_length;
    }
    return 0;
}

static int is_parent_directory(const char *parent, const char *kid) {
    int end_of_prefix = is_ancestor_directory(parent, kid);
    if (end_of_prefix == 0) {
        return 0;
    }
    char *start_pointer = kid + end_of_prefix + 1;
    char separator = '/';
    char *found;
    if ((found = strchr(start_pointer, separator)) != NULL) {
        size_t position = (size_t)(start_pointer - found);
        if (position != (strlen(start_pointer) != -1)) {
            return 0;
        }
    }
    return 1;
}

static int ec_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    GList *filenames = g_hash_table_get_keys(FILES);
    GList *element;
    for (element = g_list_first(filenames); element != NULL; element = element->next) {
        if (is_parent_directory(path, (char *)element->data)) {
            filler(buf, basename((char *)element->data), NULL, 0);
        }
    }
    for (element = g_list_first(DIRECTORIES); element != NULL; element = element->next) {
        if (is_parent_directory(path, (char *)element->data)) {
            filler(buf, basename((char *)element->data), NULL, 0);
        }
    }

    return 0;
}

static int ec_releasedir(const char *path, struct fuse_file_info *fi) {
    struct ec_dirp *d = get_dirp(fi);

    (void)path;

    closedir(d->dp);
    free(d);

    return 0;
}

static int ec_mknod(const char *path, mode_t mode, dev_t rdev) {
    int res;

    if (S_ISFIFO(mode)) {
        res = mkfifo(path, mode);
    } else {
        res = mknod(path, mode, rdev);
    }

    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int ec_mkdir(const char *path, mode_t mode) {
    int res;

    res = mkdir(path, mode);
    if (res == -1) {
        return -errno;
    }
    char *new_directory = strdup(path);
    DIRECTORIES = g_list_append(DIRECTORIES, (gpointer)new_directory);

    return 0;
}

static int ec_unlink(const char *path) {
    int res;

    res = unlink(path);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int ec_rmdir(const char *path) {
    int res;

    res = rmdir(path);
    if (res == -1) {
        return -errno;
    }

    GList *to_remove = g_list_find_custom(DIRECTORIES, (gconstpointer)path, (GCompareFunc)strcmp);
    DIRECTORIES = g_list_remove(DIRECTORIES, (gconstpointer)to_remove->data);

    return 0;
}
/*
static int
ec_symlink(const char *from, const char *to)
{
    int res;


    res = symlink(from, to);
    if (res == -1) {
        return -errno;
    }

    return 0;
}*/

static int ec_rename(const char *from, const char *to) {
    int res;

    res = rename(from, to);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

/*
static int
ec_link(const char *from, const char *to)
{
    int res;


    res = link(from, to);
    if (res == -1) {
        return -errno;
    }

    return 0;
}*/

static int ec_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char key[PATH_MAX + 1];
    strncpy(key, path, strlen(path));
    if (g_hash_table_contains(FILES, (gpointer)key)) {
        return -1;
    }
    save_encoded_file(key, encoded_file_new());
    return 0;
}

static int ec_open(const char *path, struct fuse_file_info *fi) {
    char key[PATH_MAX + 1];
    strncpy(key, path, strlen(path));
    if (!g_hash_table_contains(FILES, (gpointer)key)) {
        save_encoded_file(key, encoded_file_new());
    }
    return 0;
}

static int ec_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    encoded_file_t *ef = encoded_file_copy(get_encoded_file(path));
    if (ef != NULL) {
        size_t bytes_to_read = size;
        if (ef->size <= offset) {
            /* If the offset is bigger than the data's length, read 0 bytes */
            encoded_file_destroy(ef);
            return 0;
        } else if (ef->size <= (offset + size)) {
            /*
            If the offset + the number of bytes to read is bigger than the data's
            length, adjust the length to read
            */
            size_t bytes_to_read = ef->size - offset;
        }
        load_blocks(ef);
        char *data = decode(ef);
        memcpy(buf, data + offset, bytes_to_read);
        encoded_file_destroy(ef);
        free(data);
        return bytes_to_read;
    }
    return -1;
}

static int ec_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    encoded_file_t *old_ef = get_encoded_file(path);
    encoded_file_t *new_ef;
    if (old_ef == NULL) {
        // The file does not exist
        return -ENOENT;
    }
    if (old_ef->size == 0) {
        // First write to the file
        new_ef = encode(buf, size);
        save_encoded_file(path, new_ef);
        encoded_file_destroy(old_ef);
    } else {
        // Assuming the write is sequential and appending
        load_blocks(old_ef);
        char *decoded = decode(old_ef);
        size_t new_size = 0;
        if ((offset + size) <= old_ef->size) {
            // In case of random write
            new_size = old_ef->size;
        } else {
            // In case of append
            new_size = offset + size;
        }
        char *new_data = (char *)malloc(sizeof(char) * new_size);
        memcpy(new_data, decoded, old_ef->size);
        memcpy(new_data + offset, buf, size);
        new_ef = encode(new_data, new_size);
        save_encoded_file(path, new_ef);
        encoded_file_destroy(old_ef);
        free(decoded);
        free(new_data);
    }
    dump_blocks(new_ef, path);

    return size;
}

static int ec_statfs(const char *path, struct statvfs *stbuf) {
    int res;

    res = statvfs(path, stbuf);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int ec_release(const char *path, struct fuse_file_info *fi) {
    char key[PATH_MAX + 1];
    strncpy(key, path, strlen(path));
    if (!g_hash_table_contains(FILES, (gconstpointer)key)) {
        return -ENOENT;
    }
    return 0;
}

static int ec_truncate(const char *path, off_t size) {
    int res = truncate(path, size);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static int ec_ftruncate(const char *path, off_t size, struct fuse_file_info *fi) {
    int res = ftruncate(fi->fh, size);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static struct fuse_operations ec_oper = {
    .getattr = ec_getattr,
    .fgetattr = ec_fgetattr,
    // .readlink    = ec_readlink,
    .opendir = ec_opendir,
    .readdir = ec_readdir,
    .releasedir = ec_releasedir,
    .mknod = ec_mknod,
    .mkdir = ec_mkdir,
    //.symlink     = ec_symlink,
    .unlink = ec_unlink,
    .rmdir = ec_rmdir,
    .rename = ec_rename,
    // .link        = ec_link,
    .create = ec_create,
    .open = ec_open,
    .read = ec_read,
    .write = ec_write,
    .statfs = ec_statfs,
    .release = ec_release,
    .truncate = ec_truncate,
    .ftruncate = ec_ftruncate,
};

int init_ec_driver(struct fuse_operations **fuse_operations) {
    *fuse_operations = &ec_oper;

    size_t k = 10, m = 5;
    erasure_init(k, m);

    FILES = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)free, (GDestroyNotify)NULL);
    return 0;
}

int clean_ec_driver(struct fuse_operations **fuse_operations) {
    erasure_destroy();
    g_hash_table_destroy(FILES);
    g_list_free(DIRECTORIES);
    return 0;
}
