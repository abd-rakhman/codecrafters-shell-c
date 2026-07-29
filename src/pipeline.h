#ifndef PIPELINE_H
#define PIPELINE_H

#include "jobs.h"
#include "compspec.h"
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
void pipeline_execute(Pipeline *pipeline, Compspec *compspecs, Jobs *jobs);

void pipeline_destroy(Pipeline *pipeline);

#endif

