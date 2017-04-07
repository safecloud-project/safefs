#include <stdio.h>
#include <stdlib.h>

#include "SDSFuse.h"
#include "utils.h"

int compose_layers(struct fuse_operations** operations, configuration config)
{
	DEBUG_MSG("Going to compose layers\n");

	GSList* current = config.layers;
	do{

		//DEBUG_MSG("Going to read layer number %p\n", current);

		int layer = GPOINTER_TO_INT(current->data);
		DEBUG_MSG("Going to init driver %d\n", layer);

		switch(layer){
			case BLOCK_ALIGN:
				init_align_driver(operations, config);
				break;
			case SFUSE:
				init_sfuse_driver(operations, config);
				break;
			case MULTI_LOOPBACK:
				init_multi_loopback_driver(operations, config);
				break;
			case NOPFUSE:
				init_nop_layer(operations, config);
				break;
			default:
				return 1;
		}
		current = current->next;

	}while(current != NULL);

	return 0;
}

int clean_layers(configuration config){
	DEBUG_MSG("Going to clean layers\n");

	GSList* current = config.layers;
	do{
		int layer = GPOINTER_TO_INT(current->data);
		DEBUG_MSG("Going to clean drivers %d\n", layer);

		switch(layer){
			case BLOCK_ALIGN:
				clean_align_driver(config);
				break;
			case SFUSE:
				clean_sfuse_driver(config);
				break;
			case MULTI_LOOPBACK:
				clean_multi_loopback_driver(config);
				break;
			case NOPFUSE:
				clean_nop_layer(config);
				break;
			default:
				return 1;
		}
		current = current->next;

	}while(current != NULL);
	return 0;
}

int main(int argc, char *argv[]) {
	char* local_file_path = LOCAL_SDSCONFIG_PATH;
	char* default_file_path = DEFAULT_SDSCONFIG_PATH;
	int res;
	configuration* config = NULL;
	LOG_INIT();
	DEBUG_MSG("Trying to load local configuration file %s\n", local_file_path);
	if (file_exists(local_file_path)) {
		res = init_config(local_file_path, &config);
		DEBUG_MSG("Local configuration file successfully loaded\n");
	} else if (file_exists(default_file_path)) {
		DEBUG_MSG("Could not find local configuration file (%s)\n", local_file_path);
		DEBUG_MSG("Trying to load default configuration file (%s)\n", default_file_path);

		res = init_config(default_file_path, &config);
	} else {
		DEBUG_MSG("Could not find default configuration file (%s)\n", default_file_path);
		DEBUG_MSG("Will now exit\n");
		exit(EXIT_FAILURE);
	}
		
	struct fuse_operations* operations;
	DEBUG_MSG("Configuration structure is setup\n");

	compose_layers(&operations, *config);

	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    int i;
    for (i=0;   i < argc;    i++) {
       fuse_opt_add_arg(&args, argv[i]);
    }
    fuse_opt_add_arg(&args, "-omodules=subdir,subdir=/");

	fuse_main(args.argc, args.argv, operations, NULL);
	DEBUG_MSG("Going to clean layers\n");

	clean_layers(*config);
	res = clean_config(config);
    LOG_EXIT();
	return res;
}
