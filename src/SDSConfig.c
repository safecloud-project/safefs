#include "SDSConfig.h"

int handle_section_layers(configuration* config, const char* name, const char* value) {
    if (strcmp(name, "block_align") == 0) {
        config->layers = g_slist_append(config->layers, GINT_TO_POINTER(BLOCK_ALIGN));
    } else if (strcmp(name, "sfuse") == 0) {
        config->layers = g_slist_append(config->layers, GINT_TO_POINTER(SFUSE));
    } else if (strcmp(name, "multi_loop") == 0) {
        config->layers = g_slist_append(config->layers, GINT_TO_POINTER(MULTI_LOOPBACK));
    } else if (strcmp(name, "nopfuse") == 0) {
        config->layers = g_slist_append(config->layers, GINT_TO_POINTER(NOPFUSE));
    } else {
        return 0;
    }
    return 1;
}

int handle_section_block_align(configuration* config, const char* name, const char* value) {
    if (strcmp(name, "block_size") == 0) {
        (config->block_config).block_size = atoi(value);
    } else if (strcmp(name, "mode") == 0) {
        (config->block_config).mode = atoi(value);
    } else {
        return 0;
    }

    return 1;
}

int handle_section_sfuse(configuration* config, const char* name, const char* value) {
    if (strcmp(name, "key") == 0) {
        (config->enc_config).key = strdup(value);
    } else if (strcmp(name, "iv") == 0) {
        (config->enc_config).iv = strdup(value);
    } else if (strcmp(name, "key_size") == 0) {
        (config->enc_config).key_size = atoi(value);
    } else if (strcmp(name, "mode") == 0) {
        (config->enc_config).mode = atoi(value);
    } else {
        return 0;
    }

    return 1;
}

int handle_section_multi_loop(configuration* config, const char* name, const char* value) {
    if (strcmp(name, "mode") == 0) {
        (config->m_loop_config).mode = atoi(value);
    } else if (strcmp(name, "ndevs") == 0) {
        (config->m_loop_config).ndevs = atoi(value);

    } else if (strstr(name, "path") != NULL) {
        // TODO: FREE these strings
        int path_size = strlen(value);
        char* alloc_val = malloc(path_size * sizeof(char*) + 1);
        strcpy(alloc_val, (char*)value);

        (config->m_loop_config).loop_paths = g_slist_append((config->m_loop_config).loop_paths, alloc_val);
    } else if (strcmp(name, "root") == 0) {
        int path_size = strlen(value);
        char* alloc_val = malloc(path_size * sizeof(char*) + 1);
        strcpy(alloc_val, (char*)value);
        (config->m_loop_config).root_path = alloc_val;
    } else {
        return 0;
    }

    return 1;
}

int handle_section_log(configuration* config, const char* name, const char* value) {
    if (strcmp(name, "mode") == 0) {
        config->logging_configuration.mode = atoi(value);
    }
    return 1;
}

int handler(void* config, const char* section, const char* name, const char* value) {
    if (strcmp(section, "layers") == 0) {
        return handle_section_layers(config, name, value);
    } else if (strcmp(section, "block_align") == 0) {
        return handle_section_block_align(config, name, value);
    } else if (strcmp(section, "sfuse") == 0) {
        return handle_section_sfuse(config, name, value);
    } else if (strcmp(section, "log") == 0) {
        return handle_section_log(config, name, value);
    } else if (strcmp(section, "multi_loop") == 0) {
        return handle_section_multi_loop(config, name, value);
    } else {
        return 0;
    }
    return 1;
}

int init_config(char* configuration_file_path, configuration** config) {
    configuration* pconfig = malloc(sizeof(struct sds_configuration));
    // This pointers are allocated when the first element is inserted.
    pconfig->layers = NULL;
    (pconfig->m_loop_config).loop_paths = NULL;

    if (ini_parse(configuration_file_path, handler, pconfig) < 0) {
        DEBUG_MSG("Configuration could not be loaded.\n");
        return 1;
    }
    GSList* current = pconfig->m_loop_config.loop_paths;
    *config = pconfig;
    return 0;
}

int clean_config(configuration* config) {
    g_slist_free(config->layers);
    g_slist_free((config->m_loop_config).loop_paths);

    free((config->enc_config).key);
    free((config->enc_config).iv);
    free(config);
}
