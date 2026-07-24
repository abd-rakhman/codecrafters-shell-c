#include "map.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define BUFFER_SIZE 1024

struct Map {
  int size;
  char **keys;
  char **values;
};

Map *map_create(void) {
  Map *map = malloc(sizeof(Map));
  map->keys = calloc(BUFFER_SIZE, sizeof(char*));
  map->values = calloc(BUFFER_SIZE, sizeof(char*));
  map->size = 0;
  return map;
}

void map_add(Map *map, const char *key, const char *value) {
  // TODO: unique key constraint
  map->keys[map->size] = strdup(key);
  map->values[map->size] = strdup(value);
  map->size++;
}

const char *map_get(Map *map, const char *key) {
  for (int i = 0; i < map->size; i++) {
    if (strcmp(map->keys[i], key) == 0) {
      return map->values[i];
    }
  }
  return NULL;
}

void map_destroy(Map *map) {
  for (int i = 0; i < map->size; i++) {
    free(map->keys[i]);
    free(map->values[i]);
  }
  free(map->keys);
  free(map->values);
  free(map);
}
