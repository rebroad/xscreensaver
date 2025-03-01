/* resources_common.c */
#include "resources_common.h"
#include <string.h>
#include <stdlib.h>

ResourceManager *resource_manager_init(int argc, char **argv, const XrmOptionDescRec *options, const char * const *defaults) {
    ResourceManager *rm = calloc(1, sizeof(ResourceManager));
    rm->entries = calloc(argc + 100, sizeof(ResourceEntry)); // Oversize for defaults
    
    // Parse command-line options
    int entry_count = 0;
    for (int i = 1; i < argc && options; i++) {
        for (int j = 0; options[j].option; j++) {
            if (strcmp(argv[i], options[j].option) == 0) {
                rm->entries[entry_count].name = strdup(options[j].specifier + 1); // Skip '.'
                if (options[j].argKind == XrmoptionNoArg) {
                    rm->entries[entry_count].value = strdup("True");
                } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                    rm->entries[entry_count].value = strdup(argv[++i]);
                }
                entry_count++;
                break;
            }
        }
    }
    
    // Add defaults
    for (int i = 0; defaults && defaults[i]; i++) {
        char *name = strdup(defaults[i]);
        char *value = strchr(name, ':');
        if (value) {
            *value++ = 0;
            while (*value == ' ' || *value == '\t') value++;
            rm->entries[entry_count].name = name;
            rm->entries[entry_count].value = strdup(value);
            entry_count++;
        } else {
            free(name);
        }
    }
    
    rm->count = entry_count;
    return rm;
}

void resource_manager_free(ResourceManager *rm) {
    for (int i = 0; i < rm->count; i++) {
        free(rm->entries[i].name);
        free(rm->entries[i].value);
    }
    free(rm->entries);
    free(rm);
}

static const char *find_value(ResourceManager *rm, const char *name) {
    for (int i = 0; i < rm->count; i++) {
        if (strcmp(rm->entries[i].name, name) == 0) return rm->entries[i].value;
    }
    return NULL;
}

Bool resource_get_boolean(ResourceManager *rm, const char *name, Bool default_value) {
    const char *val = find_value(rm, name);
    if (!val) return default_value;
    return (!strcmp(val, "True") || !strcmp(val, "on") || !strcmp(val, "yes"));
}

float resource_get_float(ResourceManager *rm, const char *name, float default_value) {
    const char *val = find_value(rm, name);
    return val ? atof(val) : default_value;
}

int resource_get_int(ResourceManager *rm, const char *name, int default_value) {
    const char *val = find_value(rm, name);
    return val ? atoi(val) : default_value;
}

char *resource_get_string(ResourceManager *rm, const char *name, const char *default_value) {
    const char *val = find_value(rm, name);
    return val ? strdup(val) : (default_value ? strdup(default_value) : NULL);
}
