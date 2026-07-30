#ifndef HISTORY_H
#define HISTORY_H

typedef struct History History;

History *history_create(void);

void history_read(History *history, char *path);
void history_write(History *history, char *path, bool append);

char **history_list(History *history);
void history_print(History *history, int *tail);
void history_add(History *history, char *line);
char *history_move_cursor_up(History *history);
char *history_move_cursor_down(History *history);

void history_destroy(History *history);

#endif
