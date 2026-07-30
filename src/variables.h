#ifndef VARIABLES_H
#define VARIABLES_H

typedef struct Variables Variables;

Variables *variables_create(void);

void variables_declare(Variables *vars, char *key, char *value);
char *variables_get(Variables *vars, char *key);

void variables_destroy(Variables *vars);

#endif
