#ifndef MAP_H
#define MAP_H

typedef struct Map Map;

Map *map_create(void);

void map_add(Map *map, const char* key, const char *value);
char *map_get(Map *map, const char *key);

void map_destroy(Map *map);

#endif

