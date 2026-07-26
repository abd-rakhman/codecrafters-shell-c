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

char *compspec_run(Compspec *compspec, const char *command, const char *prefix, const char *word_before_prefix) {
  char *path = compspec_get_path(compspec, command);
  if (path == NULL) {
    return NULL;
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
    return NULL;
  }

  FILE *stream = fdopen(pipefd[0], "r");
  char *line = NULL;
  size_t len = 0;
  if (getline(&line, &len, stream) < 0) {
    free(line);
    line = NULL;
  } else {
    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\n') {
      line[n - 1] = '\0';
    }
  }

  fclose(stream);
  return line;
}


void compspec_destroy(Compspec *compspec) {
  map_destroy(compspec->map);
  free(compspec);
}
