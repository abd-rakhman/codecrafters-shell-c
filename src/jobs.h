#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>

typedef struct Job {
	int number;
	int pid;
	char *line;
	bool is_running;
	struct Job *right, *left;
} Job;

typedef struct Jobs Jobs;

Jobs *jobs_create(void);

void jobs_add(Jobs *jobs, int pid, char *args[]);
void jobs_print(Jobs *jobs);
void jobs_reap(Jobs *jobs);

void jobs_destroy(Jobs *jobs);

#endif
