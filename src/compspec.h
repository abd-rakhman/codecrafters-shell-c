#ifndef COMPSPEC_H
#define COMPSPEC_H

typedef struct Compspec Compspec;

Compspec *compspec_create(void);

void compspec_add_path(Compspec *compspec, const char *key, const char *value);
char *compspec_get_path(Compspec *compspec, const char *key);
void compspec_run(Compspec *compspec, char **completions, int *count, const char *command, const char *prefix, const char *before_prefix);

void compspec_destroy(Compspec *compspec);

#endif
