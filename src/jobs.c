#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include "jobs.h"

#define BUFFER_SIZE 1024

struct Jobs {
	Job *begin, *end;
	int size;
};


Jobs *jobs_create(void) {
	Jobs *new_jobs = calloc(1, sizeof(Jobs));
	new_jobs->size = 0;
	return new_jobs;
}

static void job_add_after(Job* job, Job* add_after) {
	add_after->right = job; 
	job->left = add_after;
}

static void job_remove(Jobs* jobs, Job* job) {
	if (job->left != NULL) job->left->right = job->right;
	if (job->right != NULL) job->right->left = job->left;
	if (jobs->begin == job) jobs->begin = job->right;
	if (jobs->end == job) jobs->end = job->left;
	free(job->line);
	free(job);
}

static char get_job_marker(Job *job) {
	if (job->right == NULL) return '+';
	else if (job->right->right == NULL) return '-';
	return ' ';
}

void jobs_add(Jobs *jobs, int pid, char *args[]) {
	jobs->size++;
	Job *job = calloc(1, sizeof(Job));
	job->is_running = true;
	job->pid = pid;
	job->line = calloc(1, BUFFER_SIZE);
	for (int i = 0; args[i] != NULL; i++) {
		if (i > 0) strcat(job->line, " ");
		strcat(job->line, args[i]);
	}
	if (jobs->begin == NULL) {
		job->number = 1;
		jobs->begin = job;
		jobs->end = job;
		return ;
	}

	job->number = jobs->end->number + 1;
	job_add_after(job, jobs->end);
	jobs->end = job;
}

void jobs_print(Jobs *jobs) {
	Job* current = jobs->begin;
	while (current != NULL) {
		Job *next = current->right;
		int status;
		pid_t ret = waitpid(current->pid, &status, WNOHANG);

		if (ret == current->pid) // if job has finished
			current->is_running = false;

		printf("[%d]%c  %-24s%s\n", current->number, get_job_marker(current), current->is_running ? "Running" : "Done", current->line);

		if (current->is_running == false) job_remove(jobs, current);
		current = next;
	}
}

void jobs_reap(Jobs *jobs) {
	Job* current = jobs->begin;
	while (current != NULL) {
		Job *next = current->right;
		int status;
		pid_t ret = waitpid(current->pid, &status, WNOHANG);
		if (ret == current->pid) {
			current->is_running = false;
			printf("[%d]%c  %-24s%s\n", current->number, get_job_marker(current), current->is_running ? "Running" : "Done", current->line);
			job_remove(jobs, current);
		}
		current = next;
	}
}
