#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "history.h"

#define BUFFER_SIZE 1024

struct History {
	char **list;
	int size;
};

History *history_access(void) {
	History *history = malloc(sizeof(History));
	history->list = malloc(BUFFER_SIZE * sizeof(char*));
	history->size = 0;
	
	return history;
}

char **history_list(History *history) {
	return history->list;
}

void history_print(History *history) {
	for (int i = 0; i < history->size; i++) {
		printf("\t%d  %s\n", (i+1), history->list[i]);
	}
}

void history_add(History *history, char *line) {
	history->list[history->size++] = strdup(line);
}

void history_destroy(History *history) {
	for (int i = 0; i < history->size; i++) {
		free(history->list[i]);
	}
	free(history->list);
	free(history);
}
