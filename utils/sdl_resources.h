/* resources_common.h */
#ifndef RESOURCES_COMMON_H
#define RESOURCES_COMMON_H

typedef struct {
    char *name;
    char *value;
} ResourceEntry;

typedef struct {
    ResourceEntry *entries;
    int count;
} ResourceManager;

ResourceManager *resource_manager_init(int argc, char **argv, const XrmOptionDescRec *options, const char * const *defaults);
void resource_manager_free(ResourceManager *rm);
Bool resource_get_boolean(ResourceManager *rm, const char *name, Bool default_value);
float resource_get_float(ResourceManager *rm, const char *name, float default_value);
int resource_get_int(ResourceManager *rm, const char *name, int default_value);
char *resource_get_string(ResourceManager *rm, const char *name, const char *default_value);

#endif
