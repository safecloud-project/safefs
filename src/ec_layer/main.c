#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "ec_layer.h"
#include "erasure.h"

int main(int argc, char* argv[]) {
    struct fuse_operations* operations;
    int res = init_ec_driver(&operations);

    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    int i;
    for (i = 0; i < argc; i++) {
        fuse_opt_add_arg(&args, argv[i]);
    }

    fuse_main(args.argc, args.argv, operations, NULL);
    res = clean_ec_driver(&operations);
    return res;
}
