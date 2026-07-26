#include "compspec.h"
#include "map.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

struct Compspec {
  Map *map;
};

Compspec *compspec_create(void) {
  Compspec *compspec = calloc(1, sizeof(Compspec));
  compspec->map = map_create();
  return compspec;
}

void compspec_add_path(Compspec *compspec, const char *command, const char *path) {
  map_add(compspec->map, command, path);
}

char *compspec_get_path(Compspec *compspec, const char *command) {
  return map_get(compspec->map, command);
}

void compspec_remove_path(Compspec *compspec, const char *command) {
  map_remove(compspec->map, command);
}

void compspec_run(Compspec *compspec, char **completions, int *count, const char *command, const char *prefix, const char *word_before_prefix) {
  char *path = compspec_get_path(compspec, command);
  if (path == NULL) {
    return ;
  }
  int pipefd[2];
  pipe(pipefd);

  pid_t pid = fork();
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    char *argv[] = {
      path,
      (char *)command,
      (char *)prefix,
      (char *)word_before_prefix,
      NULL,
    };
    execv(path, argv);
    perror("execv");
    exit(1);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
    close(pipefd[1]);
  } else {
    perror("fork");
    return ;
  }

  FILE *stream = fdopen(pipefd[0], "r");
  char *line = NULL;
  size_t len = 0;
  while (getline(&line, &len, stream) != -1) {
    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\n') {
      line[n - 1] = '\0';
    }

    int prefix_len = strlen(prefix);
    char *suffix = line + prefix_len;
    completions[*count] = strdup(suffix);
    (*count)++;
    line = NULL;
    len = 0;
  }

  free(line);
  fclose(stream);
}


void compspec_destroy(Compspec *compspec) {
  map_destroy(compspec->map);
  free(compspec);
}
