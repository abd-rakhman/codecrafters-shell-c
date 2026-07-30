#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "history.h"

#define BUFFER_SIZE 1024

struct History {
	char **list;
	int size;
	int cursor;
};

History *history_create(void) {
	History *history = malloc(sizeof(History));
	history->list = malloc(BUFFER_SIZE * sizeof(char*));
	history->size = 0;
	history->cursor = 0;
	
	return history;
}

void history_read(History *history, char *path) {
	FILE *file = fopen(path, "r");
	if (file == NULL) {
			return ;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	while ((nread = getline(&line, &len, file)) != -1) {
		if (nread <= 0) break;
		line[strcspn(line, "\n")] = '\0';
		history_add(history, line);
	}
}

void history_write(History *history, char *path, bool append) {
	FILE *file = fopen(path, append ? "a" : "w");
	if (file == NULL) {
			return ;
	}

	for (int i = 0; i < history->size; i++) {
		fprintf(file, "%s\n", history->list[i]);
	}

	fclose(file);
}

char **history_list(History *history) {
	return history->list;
}

void history_print(History *history, int *tail) {
	int start = tail == NULL ? 0 : history->size - *tail;
	for (int i = start; i < history->size; i++) {
		printf("\t%d  %s\n", (i+1), history->list[i]);
	}
}

void history_add(History *history, char *line) {
	history->list[history->size++] = strdup(line);
	history->cursor = history->size;
}

char *history_move_cursor_up(History *history) {
	if (history->cursor > 0) {
		history->cursor--;
		return history->list[history->cursor];
	}
	return NULL;
}

char *history_move_cursor_down(History *history) {
	if (history->cursor+1 < history->size) {
		history->cursor++;
		return history->list[history->cursor];
	}
	return NULL;
}

void history_destroy(History *history) {
	for (int i = 0; i < history->size; i++) {
		free(history->list[i]);
	}
	free(history->list);
	free(history);
}
