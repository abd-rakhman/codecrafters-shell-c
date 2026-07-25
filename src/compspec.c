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

void compspec_add_path(Compspec *compspec, const char *key, const char *value) {
  map_add(compspec->map, key, value);
}

char *compspec_get_path(Compspec *compspec, const char *key) {
  return map_get(compspec->map, key);
}

char *compspec_run(Compspec *compspec, const char *command) {
  char *path = compspec_get_path(compspec, command);
  if (path == NULL) {
    return NULL;
  }
  int pipefd[2];
  pipe(pipefd);

  pid_t pid = fork();
  if (pid == 0) {
    close(pipefd[0]); // closing read pipe
    dup2(pipefd[1], STDOUT_FILENO); // redirecting execution to write pipe
    close(pipefd[1]);
    execv(path, NULL); // executing
    perror("execv");
    exit(1);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
    close(pipefd[1]);
  } else {
    perror("fork");
  }

  FILE *stream = fdopen(pipefd[0], "r");
  char *line = NULL;
  size_t len = 0;
  getline(&line, &len, stream);
  size_t n = strlen(line);
  if (n > 0 && line[n - 1] == '\n') {
    line[n - 1] = '\0';
  }
  
  fclose(stream);
  free(path);
  return line;
}


void compspec_destroy(Compspec *compspec) {
  map_destroy(compspec->map);
  free(compspec);
}
