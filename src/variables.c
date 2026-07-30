#include <stdlib.h>
#include "map.h"
#include "variables.h"

struct Variables {
	Map *map;
};

Variables *variables_create(void) {
	Variables *vars = malloc(sizeof(Variables));
	vars->map = map_create();

	return vars;
}

void variables_declare(Variables *vars, char *key, char *value) {
	map_add(vars->map, key, value);
}

char *variables_get(Variables *vars, char *key) {
	return map_get(vars->map, key);
}

void variables_destroy(Variables *vars) {
	map_destroy(vars->map);
	free(vars);
}
