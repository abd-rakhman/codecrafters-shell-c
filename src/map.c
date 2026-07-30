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
	for (int i = 0; i < map->size; i++) {
		if (strcmp(map->keys[i], key) == 0) {
			free(map->values[i]);
			map->values[i] = strdup(value);
			return;
		}
	}
	map->keys[map->size] = strdup(key);
	map->values[map->size] = strdup(value);
	map->size++;
}

char *map_get(Map *map, const char *key) {
	for (int i = 0; i < map->size; i++) {
		if (strcmp(map->keys[i], key) == 0) {
			return map->values[i];
		}
	}
	return NULL;
}

static void swap(char **a, char **b) {
	char *temp = *a;
	*a = *b;
	*b = temp;
}

void map_remove(Map *map, const char *key) {
	int pos = -1;
	for (int i = 0; i < map->size; i++) {
		if (strcmp(map->keys[i], key) == 0) {
			pos = i;
			break;
		}
	}
	if (pos == -1) {
		return ;
	}
	if (pos != map->size - 1) {
		swap(&map->values[pos], &map->values[map->size-1]);
		swap(&map->keys[map->size-1], &map->keys[pos]);
	}
	free(map->keys[map->size-1]);
	free(map->values[map->size-1]);
	map->size--;
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
