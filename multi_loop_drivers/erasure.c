#include <stdlib.h>
#include <string.h>

#include <erasurecode.h>

#include "erasure.h"
#include "../logdef.h"
#include "../map/map.h"
#include <glib.h>

int instance_descriptor;
struct ec_args args;
ivdb offset_map;
ivdb file_ids;
ivdb file_sizes;

GMutex mutex;

/*this mutex is required because file size can be used by encode function or
by a layer call.*/
GMutex size_mutex;

/*this mutex is required because file size can be used by encode function or
by a layer call.*/
GMutex get_size_mutex;

/*this mutex is required because file size can be used by encode function or
by a layer call.*/
GMutex file_id_mutex;

uint64_t file_id = 0;

uint64_t get_file_id(char* path) {
    g_mutex_lock(&file_id_mutex);

    int contains = hash_contains_key(&file_ids, path);

    if (contains == 0) {
        file_id += 1;
        g_mutex_unlock(&file_id_mutex);

        return file_id;
    }

    value_db* get_val = NULL;
    hash_get(&file_ids, path, &get_val);
    g_mutex_unlock(&file_id_mutex);

    return get_val->file_size;
}

void get_erasure_block_offset(const char* path, off_t offset, off_t* driver_offset) {
    g_mutex_lock(&mutex);

    uint64_t file_id = get_file_id((char*)path);
    char start_offset[strlen(path) + 50];
    sprintf(start_offset, "%ld:ro:%ld", file_id, offset);

    int contains = hash_contains_key(&offset_map, start_offset);

    if (contains == 0) {
        // DEBUG_MSG("File does not exist yet\n");
        *driver_offset = -1;
    } else {
        value_db* db_start_offset = NULL;
        hash_get(&offset_map, start_offset, &db_start_offset);
        *driver_offset = db_start_offset->file_size;
    }
    g_mutex_unlock(&mutex);
}

void get_erasure_block_size(const char* path, off_t offset, uint64_t* driver_size) {
    g_mutex_lock(&mutex);

    uint64_t file_id = get_file_id((char*)path);
    char stop_offset[strlen(path) + 50];
    sprintf(stop_offset, "%ld:rs:%ld", file_id, offset);

    int contains = hash_contains_key(&offset_map, stop_offset);

    if (contains == 0) {
        *driver_size = -1;
    } else {
        value_db* db_stop_offset;

        hash_get(&offset_map, stop_offset, &db_stop_offset);
        *driver_size = db_stop_offset->file_size;
    }
    g_mutex_unlock(&mutex);
}

void init_erasure(int k, int m) {
    args.k = k;
    args.m = m;
    instance_descriptor = liberasurecode_instance_create(EC_BACKEND_LIBERASURECODE_RS_VAND, &args);

    g_mutex_init(&mutex);
    g_mutex_init(&size_mutex);
    g_mutex_init(&file_id_mutex);
    g_mutex_init(&get_size_mutex);

    init_hash(&offset_map);
    init_hash(&file_ids);
    init_hash(&file_sizes);
}

void erasure_decode(unsigned char* block, unsigned char** magicblocks, int size, int ndevs) {
    uint64_t decoded_size;
    char* decoded_block;

    liberasurecode_decode(instance_descriptor, (char**)magicblocks, ndevs, size, 0, &decoded_block, &decoded_size);

    memcpy(block, decoded_block, (int)decoded_size);

    liberasurecode_decode_cleanup(instance_descriptor, decoded_block);
}

uint64_t erasure_get_file_size(const char* path) {
    g_mutex_lock(&get_size_mutex);

    int contains = hash_contains_key(&file_sizes, (char*)path);

    if (contains == 0) {
        g_mutex_unlock(&get_size_mutex);

        return 0;
    }
    value_db* get_val = NULL;
    hash_get(&file_sizes, (char*)path, &get_val);
    g_mutex_unlock(&get_size_mutex);

    return get_val->file_size;
}

void increment_file_size(const char* path, off_t offset, int block_size) {
    g_mutex_lock(&size_mutex);

    uint64_t current_size = erasure_get_file_size(path);

    if (offset + block_size > current_size) {
        value_db* new_file_size = malloc(sizeof(value_db));
        uint64_t diff = offset + block_size - current_size;
        uint64_t final_size = current_size + diff;
        char* key = malloc(strlen(path) + 1);
        strcpy(key, path);

        new_file_size->file_size = final_size;
        hash_put(&file_sizes, key, new_file_size);
    }
    g_mutex_unlock(&size_mutex);
}

void erasure_encode(const char* path, unsigned char** magicblocks, unsigned char* block, off_t offset, int size,
                    int ndevs) {
    g_mutex_lock(&mutex);

    char** encoded_data;
    char** encoded_parity;
    uint64_t fragment_length = 0;

    char* write_next_block_offset = (char*)malloc(strlen(path) + 50);
    char* read_block_start_offset = (char*)malloc(strlen(path) + 50);
    char* read_block_start_size = (char*)malloc(strlen(path) + 50);

    liberasurecode_encode(instance_descriptor, (const char*)block, size, &encoded_data, &encoded_parity,
                          &fragment_length);

    uint64_t file_id = get_file_id((char*)path);

    char* key = malloc(strlen(path) + 1);
    strcpy(key, path);

    value_db* db_file_id = malloc(sizeof(value_db));

    db_file_id->file_size = file_id;

    hash_put(&file_ids, key, db_file_id);

    // write_next_block_offset
    sprintf(write_next_block_offset, "%ld:wo:%ld", file_id, offset + size);

    // read_block_start_offset
    sprintf(read_block_start_offset, "%ld:ro:%ld", file_id, offset);

    // read_block_start_size
    sprintf(read_block_start_size, "%ld:rs:%ld", file_id, offset);

    // In this layer, the blocks should are always aligned my the align driver.
    // int contains_key = hash_contains_key(value_db, start_offset)
    off_t erasure_offset = offset;

    increment_file_size(path, offset, size);

    // If the block is not going to be written at the beginning of the file.
    if (offset != 0) {
        char* block_offset = (char*)malloc(strlen(path) + 50);
        sprintf(block_offset, "%ld:wo:%ld", file_id, offset);

        value_db* get_val = NULL;
        hash_get(&offset_map, block_offset, &get_val);
        erasure_offset = get_val->file_size;
    }

    value_db* db_write_next_block_offset = malloc(sizeof(value_db));
    db_write_next_block_offset->file_size = erasure_offset + fragment_length;

    value_db* db_read_block_start_offset = malloc(sizeof(value_db));
    db_read_block_start_offset->file_size = erasure_offset;

    value_db* db_read_block_start_size = malloc(sizeof(value_db));
    db_read_block_start_size->file_size = fragment_length;

    hash_put(&offset_map, write_next_block_offset, db_write_next_block_offset);
    hash_put(&offset_map, read_block_start_offset, db_read_block_start_offset);
    hash_put(&offset_map, read_block_start_size, db_read_block_start_size);

    int l;
    for (l = 0; l < ndevs; l++) {
        magicblocks[l] = malloc(fragment_length);
    }

    int i;
    for (i = 0; i < args.k; i++) {
        memcpy(magicblocks[i], encoded_data[i], fragment_length);
    }

    int j;
    for (j = 0; j < args.m; j++) {
        memcpy(magicblocks[i + j], encoded_parity[j], fragment_length);
    }
    liberasurecode_encode_cleanup(instance_descriptor, encoded_data, encoded_parity);
    g_mutex_unlock(&mutex);
}

void erasure_rename(char* from, char* to) {
    g_mutex_lock(&mutex);
    char* to_key = malloc(strlen(to) + 1);
    strcpy(to_key, to);
    move_key(&file_ids, from, to_key);
    move_key(&file_sizes, from, to_key);
    g_mutex_unlock(&mutex);
}

void erasure_create(char* path) {
    g_mutex_lock(&mutex);
    remove_keys(&file_sizes, path);
    remove_keys(&file_ids, path);
    g_mutex_unlock(&mutex);
}