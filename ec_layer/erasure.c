#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <erasurecode.h>

#include "erasure.h"


/** Number of data blocks produced by erasure coding */
static size_t K;

/** Number of parity blocks produced by erasure coding */
static size_t M;

/** liberasurecode data structures */
static int liberasurecode_instance = -1;
static struct ec_args args;

void erasure_init(size_t k, size_t m) {
	args.k = (int) k;
	args.m = (int) m;

	liberasurecode_instance = liberasurecode_instance_create(EC_BACKEND_LIBERASURECODE_RS_VAND, &args);
	assert(liberasurecode_instance > 0);

	K = k;
	M = m;
}

void erasure_destroy() {
	liberasurecode_instance_destroy(liberasurecode_instance);
}

encoded_file_t * encoded_file_new() {
	encoded_file_t *e = (encoded_file_t *) malloc(sizeof(encoded_file_t));
	e->data_blocks = NULL;
	e->parity_blocks = NULL;
	e->data_blocks_location = NULL;
	e->parity_blocks_location = NULL;
	e->size = 0;
	e->fragment_size = 0;
	return e;
}

void encoded_file_destroy(encoded_file_t *file) {
	if (file != NULL) {
		if (file->data_blocks != NULL) {
			size_t i;
			for (i = 0; i < K; i++) {
				if (file->data_blocks[i] != NULL) {
					free(file->data_blocks[i]);
				}
			}
			free(file->data_blocks);
		}
		if (file->parity_blocks != NULL) {
			size_t i;
			for (i = 0; i < M; i++) {
				if (file->parity_blocks[i] != NULL) {
					free(file->parity_blocks[i]);
				}
			}
			free(file->parity_blocks);
		}
		if (file->data_blocks_location != NULL) {
			size_t i;
			for (i = 0; i < K; i++) {
				if (file->data_blocks_location[i] != NULL) {
					free(file->data_blocks_location[i]);
				}
			}
			free(file->data_blocks_location);
		}

		if (file->parity_blocks_location != NULL) {
			size_t i;
			for (i = 0; i < M; i++) {
				if (file->parity_blocks_location[i] != NULL) {
					free(file->parity_blocks_location[i]);
				}
			}
			free(file->parity_blocks_location);
		}
		free(file);
	}
}

encoded_file_t *encoded_file_copy(const encoded_file_t *file) {
	encoded_file_t *ef = encoded_file_new();
	ef->size = file->size;
	ef->fragment_size = file->fragment_size;
	if (file->data_blocks != NULL) {
		size_t i;
		ef->data_blocks = (char **) malloc(sizeof(char *) * K);
		for (i = 0; i < K; i++) {
			ef->data_blocks[i] = (char *) malloc(sizeof(char) * ef->fragment_size);
			memcpy(ef->data_blocks[i], file->data_blocks[i], ef->fragment_size);
		}
	}
	if (file->parity_blocks != NULL) {
		size_t i;
		ef->parity_blocks = (char **) malloc(sizeof(char *) * M);
		for (i = 0; i < M; i++) {
			ef->parity_blocks[i] = (char *) malloc(sizeof(char) * ef->fragment_size);
			memcpy(ef->parity_blocks[i], file->parity_blocks[i], ef->fragment_size);
		}
	}
	if (file->data_blocks_location != NULL) {
		size_t i;
		ef->data_blocks_location = (char **) malloc(sizeof(char *) * K);
		for (i = 0; i < K; i++) {
			ef->data_blocks_location[i] = strdup(file->data_blocks_location[i]);
		}
	}

	if (file->parity_blocks_location != NULL) {
		size_t i;
		ef->parity_blocks_location = (char **) malloc(sizeof(char *) * K);
		for (i = 0; i < M; i++) {
			ef->parity_blocks_location[i] = strdup(file->parity_blocks_location[i]);
		}
	}
	return ef;
}

encoded_file_t * encode(const char *data, size_t size) {
	encoded_file_t *encoded_data = encoded_file_new();
	char **data_blocks = NULL;
	char **parity_blocks = NULL;
  uint64_t fragment_length = 0;
  liberasurecode_encode(liberasurecode_instance, data, size, &data_blocks, &parity_blocks, &fragment_length);
  size_t i = 0;
  encoded_data->data_blocks = (char **) malloc(sizeof(char *) * K);
  for (i = 0; i < K; i++) {
		encoded_data->data_blocks[i] = (char *) malloc(sizeof(char) * fragment_length);
		memcpy(encoded_data->data_blocks[i], data_blocks[i], fragment_length);
	}
	encoded_data->parity_blocks = (char **) malloc(sizeof(char *) * M);
	for (i = 0; i < M; i++) {
		encoded_data->parity_blocks[i] = (char *) malloc(sizeof(char) * fragment_length);
		memcpy(encoded_data->parity_blocks[i], parity_blocks[i], fragment_length);
	}
	encoded_data->size = size;
	encoded_data->fragment_size = fragment_length;
	liberasurecode_encode_cleanup(liberasurecode_instance, data_blocks, parity_blocks);
	return encoded_data;
}

char * decode(const encoded_file_t *encoded_data) {
	char *buffer;
	uint64_t decoded_data_length;
	assert(encoded_data->data_blocks != NULL);
	size_t i;
	for (i = 0; i < K; i++) {
		assert(encoded_data->data_blocks[i] != NULL);
	}
	assert(encoded_data->parity_blocks != NULL);
	for (i = 0; i < M; i++) {
		assert(encoded_data->parity_blocks[i] != NULL);
	}
	liberasurecode_decode(liberasurecode_instance, encoded_data->data_blocks, K, encoded_data->fragment_size, 0, &buffer, &decoded_data_length);
	char *decoded_data = (char *) malloc(sizeof(char) * decoded_data_length);
	memcpy(decoded_data, buffer, decoded_data_length);
	liberasurecode_decode_cleanup(liberasurecode_instance, buffer);
	return decoded_data;
}


size_t dump_blocks(encoded_file_t *ef, const char *path) {
	assert(ef != NULL);
	assert(path != NULL);
	size_t i = 0, written = 0;
	char filename[PATH_MAX];
	ef->data_blocks_location = (char **) malloc(sizeof(char *) * K);
	for (i = 0; i < K; i++) {
    sprintf(filename, "%s-%05lu.data", path, i);
		ef->data_blocks_location[i] = strdup(filename);
    FILE *out_data = fopen(filename, "w+");
    written += fwrite(ef->data_blocks[i], sizeof(char), ef->fragment_size, out_data);
		fclose(out_data);
		free(ef->data_blocks[i]);
		ef->data_blocks[i] = NULL;
	}
	free(ef->data_blocks);
	ef->data_blocks = NULL;
	ef->parity_blocks_location = (char **) malloc(sizeof(char *) * M);
	for (i = 0; i < M; i++) {
    sprintf(filename, "%s-%05lu.parity", path, i);
    ef->parity_blocks_location[i] = strdup(filename);
    FILE *out_parity = fopen(filename, "w+");
    written += fwrite(ef->parity_blocks[i], sizeof(char), ef->fragment_size, out_parity);
		fclose(out_parity);
		free(ef->parity_blocks[i]);
		ef->parity_blocks[i] = NULL;
	}
	free(ef->parity_blocks);
	ef->parity_blocks = NULL;
	return written;
}

size_t load_blocks(encoded_file_t *ef) {
	//TODO: How to handle blocks that cannot be loaded
	assert(ef != NULL);
	assert(ef->data_blocks_location != NULL);
	assert(ef->parity_blocks_location != NULL);
	size_t i, read = 0;
	ef->data_blocks = (char **) malloc(sizeof(char *) * K);
	for (i = 0; i < K; i++) {
		assert(ef->data_blocks_location[i] != NULL);
		ef->data_blocks[i] = (char *) malloc(sizeof(char) * ef->fragment_size);
		FILE *in_data = fopen(ef->data_blocks_location[i], "r");
		read += fread(ef->data_blocks[i], sizeof(char), ef->fragment_size, in_data);
		fclose(in_data);
	}
	ef->parity_blocks = (char **) malloc(sizeof(char *) * M);
	for (i = 0; i < M; i++) {
		assert(ef->parity_blocks_location[i] != NULL);
		ef->parity_blocks[i] = (char *) malloc(sizeof(char) * ef->fragment_size);
		FILE *in_parity = fopen(ef->parity_blocks_location[i], "r");
		read += fread(ef->parity_blocks[i], sizeof(char), ef->fragment_size, in_parity);
		fclose(in_parity);
	}
	return read;
}
