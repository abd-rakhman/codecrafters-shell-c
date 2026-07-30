#ifndef PIPELINE_H
#define PIPELINE_H

#include "history.h"
#include "jobs.h"
#include "compspec.h"
#include "variables.h"
#include <stdbool.h>

extern const char *builtins[];

typedef struct {
	char **argv;
	int argc;
	int fd[3];
} Command;

typedef struct Pipeline Pipeline;

Pipeline* pipeline_create(char **argv, int argc);
bool pipeline_empty(Pipeline *pipeline);
void pipeline_execute(Pipeline *pipeline, History *history, Compspec *compspecs, Jobs *jobs, Variables *variables);

void pipeline_destroy(Pipeline *pipeline);

#endif

