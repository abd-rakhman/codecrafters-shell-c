#ifndef HISTORY_H
#define HISTORY_H

typedef struct History History;

History *history_access(void);

char **history_list(History *history);
void history_print(History *history);
void history_add(History *history, char *line);

void history_destroy(History *history);

#endif
