/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/


#include "blockalign.h"
#include <errno.h>
#include <stdio.h>

int BLOCKSIZE = 0;

ivdb fsize_cache;

void define_block_size(block_align_config config) {
    BLOCKSIZE = config.block_size;
}

int block_align_create(const char *path, mode_t mode, void *fi, struct fuse_operations nextlayer) {
    DEBUG_MSG("Align create Path %s\n", path);

    ((struct fuse_file_info *)fi)->flags =
        ((((struct fuse_file_info *)fi)->flags & (~O_RDONLY)) & (~O_WRONLY)) | O_RDWR;

    return nextlayer.create(path, mode, fi);
}

int block_align_open(const char *path, void *fi, struct fuse_operations nextlayer) {
    DEBUG_MSG("Align open Path %s\n", path);
    ((struct fuse_file_info *)fi)->flags =
        ((((struct fuse_file_info *)fi)->flags & (~O_RDONLY)) & (~O_WRONLY)) | O_RDWR;

    return nextlayer.open(path, fi);
}

off_t block_align_get_file_size(const char *path, struct fuse_file_info *fi, struct fuse_operations nextlayer) {
    struct stat st;

    if (fi != NULL) {
        nextlayer.fgetattr(path, &st, fi);
        DEBUG_MSG("fi is not null\n");
    } else {
        nextlayer.getattr(path, &st);
        DEBUG_MSG("fi is null\n");
    }

    return st.st_size;
}

int read_block(char *buf, size_t size, uint64_t offset, struct io_info *inf) {
    // read to the buffer by calling the next layer
    return inf->nextlayer.read(inf->path, buf, size, offset, inf->fi);
}

int process_read_block(char *buf, size_t size, uint64_t block_offset, uint64_t block_extra_offset,
                       struct io_info *inf) {
    DEBUG_MSG("Processing read file Path %s, and block offset %llu, extra_offset %llu, size %zu\n", inf->path,
              (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size);

    uint64_t file_size = 0;
    int bytes_to_read;

    if (size + block_extra_offset != BLOCKSIZE) {

        if (file_size == 0) {
            // Get the file size
            file_size = block_align_get_file_size(inf->path, inf->fi, inf->nextlayer);
        }

        bytes_to_read = ((size + block_extra_offset) >= file_size - block_offset) ? size + block_extra_offset
                                                                                  : file_size - block_offset;
        if (bytes_to_read > BLOCKSIZE) {
            bytes_to_read = BLOCKSIZE;
        }
    } else {
        bytes_to_read = BLOCKSIZE;
    }

    // block bytes that must be read

    char aux_buf[bytes_to_read];

    // Read the bytes
    int res = read_block(aux_buf, bytes_to_read, block_offset, inf);

    if (res > 0) {
        DEBUG_MSG("res %d\n", res);
    }

    DEBUG_MSG("Read done  before if file Path %s, and block offset %llu, extra_offset %llu, size %zu, res %d\n",
              inf->path, (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size, res);

    // The request was for reading size bytes at offset block_extra_offset
    // If we read less bytes than the bytes read from the extra_offset return an error.
    if (res - block_extra_offset < 0) {
        ERROR_MSG("res < 0 - Read done file Path %s, and block offset %llu, extra_offset %llu, size %zu, res %d\n",
                  inf->path, (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size,
                  res);
        return -1;
    }

    DEBUG_MSG("Read done before memcpy file Path %s, and block offset %llu, extra_offset %llu, size %zu, res %d\n",
              inf->path, (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size, res);

    int requested_size_read = ((res - block_extra_offset > size)) ? size : res - block_extra_offset;

    // copy the returned bytes to the buffer (excluding the extra_offset)
    memcpy(buf, &aux_buf[block_extra_offset], requested_size_read);

    DEBUG_MSG("Read done file Path %s, and block offset %llu, extra_offset %llu, size %zu, res %d\n", inf->path,
              (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size, res);

    return requested_size_read;
}

int write_block(char *buf, size_t size, uint64_t offset, struct io_info *inf) {
    // Write the buffer to the block by calling the next layer
    return inf->nextlayer.write(inf->path, buf, size, offset, inf->fi);
}

int process_write_block(char *buf, size_t size, uint64_t block_offset, uint64_t block_extra_offset,
                        struct io_info *inf) {
    // Result of write operations
    int res;
    // BUffer with content to be written
    char *write_buf;
    // bytes to write
    int bytes_to_write = 0;

    // auxiliary buff
    char aux_buf[BLOCKSIZE];

    uint64_t file_size = 0;

    DEBUG_MSG("Process Write file Path %s, and block offset %llu, extra_offset %llu, size %zu\n", inf->path,
              (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size);

    // Optimization to avoid calculating file size
    // If the size to write is equal to BLOCKSIZE and extra_offset is zero (It should always be in this case!)
    // Avoid calculating file size and reading back the content already written
    if (size == BLOCKSIZE && block_extra_offset == 0) {
        write_buf = buf;
        bytes_to_write = BLOCKSIZE;

    } else {
        if (file_size == 0) {
            // Get the file size
            file_size = block_align_get_file_size(inf->path, inf->fi, inf->nextlayer);
        }

        // remaining file bytes past the offset to be written
        uint64_t remaining_file_bytes = file_size - block_offset;

        DEBUG_MSG("Remaining file bytes %llu Path %s, and block offset %llu, extra_offset %llu, size %zu\n",
                  (unsigned long long int)remaining_file_bytes, inf->path, (unsigned long long int)block_offset,
                  (unsigned long long int)block_extra_offset, size);

        // If the offset to be written is at the end of file it is an append to the file
        if (block_offset >= file_size) {
            // If this happens fuse is trying to write bytes in an invalid position higher than filesize...
            if (block_offset > file_size || block_extra_offset > 0) {
                // Error this cannot happen
                ERROR_MSG("Append ERROR file Path %s, and block offset %llu, extra_offset %llu, size %zu\n", inf->path,
                          (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size);
                return -1;
            }

            bytes_to_write = size;
            write_buf = buf;

            DEBUG_MSG("Append done file Path %s, and block offset %llu, extra_offset %llu, size %zu\n", inf->path,
                      (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size);
            // Non_Append operation

        } else {
            // optimization to avoid reading the block content if fuse is overwritting the full block
            // E.g. If we are writing to the last block 100 bytes at position 0 and the block only has 10 bytes.
            if (block_extra_offset == 0 && remaining_file_bytes <= size) {
                bytes_to_write = size;
                write_buf = buf;
                DEBUG_MSG(
                    "Optimized Non-Append done file Path %s, and block offset %llu, extra_offset %llu, size %zu\n",
                    inf->path, (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size);

            } else {
                // bytes to read from the block being processed
                int bytes_to_read = (remaining_file_bytes < BLOCKSIZE) ? remaining_file_bytes : BLOCKSIZE;

                // Since the bytes read can be appended with extra bytes we need to check the final amount of bytes to
                // write
                bytes_to_write =
                    (bytes_to_read > size + block_extra_offset) ? bytes_to_read : size + block_extra_offset;

                // read from the next layer the necessary block bytes
                int res = inf->nextlayer.read(inf->path, aux_buf, bytes_to_read, block_offset, inf->fi);
                if (res < bytes_to_read) {
                    ERROR_MSG("Bytes read %s\n", strerror(res));
                    return -1;
                }

                // We read the block and then modify the necessary content.
                // The extra_offset must be accounted to rewrite only the desired bytes.
                memcpy(&aux_buf[block_extra_offset], (char *)buf, size);

                write_buf = aux_buf;
                DEBUG_MSG("Non-Append done file Path %s, and block offset %llu, extra_offset %llu, size %zu\n",
                          inf->path, (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset,
                          size);
            }
        }
    }

    // Finally, write the bytes
    res = write_block(write_buf, bytes_to_write, block_offset, inf);
    if (res < bytes_to_write) {
        DEBUG_MSG(
            "Error bytes_to_write > res file Path %s, and block offset %llu, extra_offset %llu, size %zu, res %d\n",
            inf->path, (unsigned long long int)block_offset, (unsigned long long int)block_extra_offset, size, res);
        return -1;
    }

    return size;
}

int split_into_blocks(int io_type, char *buf, size_t size, uint64_t aligned_offset, uint64_t block_extra_offset,
                      struct io_info *inf) {
    // next block offset to process
    // this is always aligned with the beginning of a block
    uint64_t next_offset_to_process = aligned_offset;

    // extra offset of the block where buffer bytes start to be written/read
    uint64_t next_block_extra_offset = block_extra_offset;

    // buffer bytes already processed
    uint64_t buffer_bytes_processed = 0;

    // While there are still buffer bytes to process
    while (buffer_bytes_processed < size) {
        // check the remaining bytes still to process
        // original request size minus bytes already processed
        uint64_t remaining_size_to_process = size - buffer_bytes_processed;

        // Check if remaining bytes to process fit or not in a single block.
        // We need to have in account if buffer bytes are written in the beggining of the block or not (extra_offset)
        // if the value is higher than BLKSIZE, process the corresponding block (remove the extra_offset bytes that are
        // not going to be read/written) and advance to the next block
        int size_to_process_in_block = (remaining_size_to_process + next_block_extra_offset >= BLOCKSIZE)
                                           ? BLOCKSIZE - next_block_extra_offset
                                           : remaining_size_to_process;

        DEBUG_MSG(
            "Processing file Path %s, and block offset %llu, extra_offset %llu, size %zu, size_to_process %llu, buf "
            "pointer %zu\n",
            inf->path, (unsigned long long int)next_offset_to_process, (unsigned long long int)next_block_extra_offset,
            size, (unsigned long long int)remaining_size_to_process, buffer_bytes_processed);

        int res = 0;
        if (io_type == WRITE) {
            // process from buffer the next bytes by checking the number of bytes alredy processed
            // (buffer_bytes_processed)
            res = process_write_block(&buf[buffer_bytes_processed], size_to_process_in_block, next_offset_to_process,
                                      next_block_extra_offset, inf);
        } else {
            res = process_read_block(&buf[buffer_bytes_processed], size_to_process_in_block, next_offset_to_process,
                                     next_block_extra_offset, inf);
        }
        if (res < 0) {
            DEBUG_MSG("Returning -1 offset %ld with blocksize %lu and res %d\n", next_offset_to_process,
                      size_to_process_in_block, res);
            return res;
        }

        DEBUG_MSG(
            "Processed file Path %s, and block offset %llu, extra_offset %llu, size %zu, size_to_process %llu, buf "
            "pointer %zu\n",
            inf->path, (unsigned long long int)next_offset_to_process, (unsigned long long int)next_block_extra_offset,
            size, (unsigned long long int)remaining_size_to_process, buffer_bytes_processed);

        // The buffer size processed is updated according to thre result of the previous function
        buffer_bytes_processed += res;

        // The next offset to process is increased by the BLKSIZE (offset is always aligned with the BLOCKSIZE)
        next_offset_to_process += BLOCKSIZE;

        // now the writes are aligned for sure so, extra_offset is zero.
        // Only the first block processed may have an extra_offset i.e., not being read/written at the beggining of the
        // block
        next_block_extra_offset = 0;

        // if read/wrote a smaller size than desired return the bytes alreadyprocessed
        if (res < size_to_process_in_block) {
            DEBUG_MSG("Returning error offset %ld with size %lu and res %d size_processed %llu\n",
                      next_offset_to_process, size, res, (unsigned long long int)buffer_bytes_processed);
            // return bytes processed until now
            return buffer_bytes_processed;
        }
    }

    DEBUG_MSG("Returning from align offset %ld with size %lu size_processed %llu\n", next_offset_to_process, size,
              (unsigned long long int)buffer_bytes_processed);
    // All the desired buffer bytes were processed return the amount
    return buffer_bytes_processed;
}

int block_align_read(const char *path, char *buf, size_t size, off_t offset, void *fi,
                     struct fuse_operations nextlayer) {
    DEBUG_MSG("Entering function block_align_read with the following arguments.\n");
    DEBUG_MSG("Path %s,  offset %llu, size %zu\n", path, (unsigned long long int)offset, size);

    // if the offset to write is not aligned with a block, we need to know where the write is positioned in the block
    // (extra_bytes)
    uint64_t block_extra_offset = offset % BLOCKSIZE;

    // the offset where the block starts is the offset to write minus the extra bytes
    uint64_t aligned_offset = offset - block_extra_offset;

    struct io_info inf;
    inf.fi = fi;
    inf.path = path;
    inf.nextlayer = nextlayer;

    return split_into_blocks(READ, buf, size, aligned_offset, block_extra_offset, &inf);
}

int block_align_write(const char *path, const char *buf, size_t size, off_t offset, void *fi,
                      struct fuse_operations nextlayer) {
    DEBUG_MSG("Entering function block_align_write with the following arguments.\n");
    DEBUG_MSG("Path %s,  offset %llu, size %zu\n", path, (unsigned long long int)offset, size);

    // if the offset to write is not aligned with a block, we need to know where the write is positioned in the block
    // (extra_bytes)
    uint64_t block_extra_offset = offset % BLOCKSIZE;

    // the offset where the block starts is the offset to write minus the extra bytes
    uint64_t aligned_offset = offset - block_extra_offset;

    struct io_info inf;
    inf.fi = fi;
    inf.path = path;
    inf.nextlayer = nextlayer;

    return split_into_blocks(WRITE, (char *)buf, size, aligned_offset, block_extra_offset, &inf);
}

int block_align_truncate(const char *path, off_t size, struct fuse_file_info *fi_in, struct fuse_operations nextlayer) {
    struct stat stbuf;
    int res;
    struct fuse_file_info *fi;

    int extra_bytes = size % BLOCKSIZE;
    char buffer[extra_bytes];
    uint64_t block_offset = size / BLOCKSIZE * BLOCKSIZE;

    // Get file size
    if (fi_in == NULL) {
        res = nextlayer.getattr(path, &stbuf);
    } else {
        res = nextlayer.fgetattr(path, &stbuf, fi_in);
    }

    if (res < 0) {
        DEBUG_MSG("Truncate error getting size file %s\n", path);
        return res;
    }

    DEBUG_MSG("size truncate is %lu size is %lu\n", size, stbuf.st_size);

    // TODO in the future this can be optimized since open is not needed for al cases
    if (fi_in != NULL) {
        fi = fi_in;
    } else {
        fi = malloc(sizeof(struct fuse_file_info));
        fi->flags = O_RDWR;
        res = nextlayer.open(path, fi);
        if (res == -1) {
            DEBUG_MSG("Error truncate opening file %s truncate size is %lu\n", path, size);
            free(fi);
            return res;
        }
    }

    // CASE1
    // Size is higher than current file size
    if (size > stbuf.st_size) {
        off_t size_to_write = size - stbuf.st_size;
        off_t size_written = 0;
        int blocksize = 64 * 1024;
        uint64_t offset = stbuf.st_size;
        char buff[blocksize];
        bzero(buff, blocksize);

        while (size_written < size_to_write) {
            int iter_size = (blocksize < (size_to_write - size_written)) ? blocksize : size_to_write - size_written;

            // Fill the file with zeroes
            res = block_align_write(path, buff, iter_size, offset, fi, nextlayer);
            if (res < iter_size) {
                DEBUG_MSG("Failed write %s iter_size is %d offset is %llu\n", path, iter_size,
                          (unsigned long long int)offset);
                if (fi_in == NULL) {
                    free(fi);
                }
                return -1;
            }

            offset += iter_size;
            size_written += iter_size;
        }
    }
    // Size is smaller than current file size
    else {
        // If size is aligned with block there is no problem
        // Otherwise we must re-write the last block since it is written partially

        if (extra_bytes > 0) {
            DEBUG_MSG("read %s extra_bytes is %d offset is %llu\n", path, extra_bytes,
                      (unsigned long long int)block_offset);

            res = block_align_read(path, buffer, extra_bytes, block_offset, fi, nextlayer);
            if (res < extra_bytes) {
                DEBUG_MSG("Failed read %s extra_bytes is %d offset is %llu\n", path, extra_bytes,
                          (unsigned long long int)block_offset);
                if (fi_in == NULL) {
                    free(fi);
                }
                return -1;
            }
        }
    }

    res = nextlayer.ftruncate(path, size, fi);
    if (res < 0) {
        DEBUG_MSG("Failed truncate %s size is %lu\n", path, size);
        if (fi_in == NULL) {
            free(fi);
        }
        return -1;
    }

    if (extra_bytes > 0 && size < stbuf.st_size) {
        struct io_info inf;
        inf.fi = fi;
        inf.path = path;
        inf.nextlayer = nextlayer;

        res = write_block(buffer, extra_bytes, block_offset, &inf);
        if (res < extra_bytes) {
            DEBUG_MSG("Failed write 2 %s iter_size is %d offset is %llu\n", path, extra_bytes,
                      (unsigned long long int)block_offset);
            if (fi_in == NULL) {
                free(fi);
            }
            return -1;
        }
    }

    if (fi_in == NULL) {
        DEBUG_MSG("release new_path %s\n", path);
        res = nextlayer.release(path, fi);
        if (res < 0) {
            DEBUG_MSG("Failed releDase new_path %s\n", path);
            free(fi);
            return -1;
        }

        free(fi);
    }

    return 0;
}
