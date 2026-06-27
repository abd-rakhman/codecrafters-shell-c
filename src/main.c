#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

const char *builtins[] = {"echo", "type", "exit", "pwd", NULL};

static int is_builtin(const char *cmd) {
  for (int i = 0; builtins[i] != NULL; i++) {
    if (strcmp(cmd, builtins[i]) == 0) return 1;
  }
  return 0;
}

static char *find_executable(const char *cmd) {
  char *path_env = getenv("PATH");
  if (path_env == NULL) return NULL;

  char *path = strdup(path_env);
  char *result = NULL;

  for (char *dir = strtok(path, ":"); dir != NULL; dir = strtok(NULL, ":")) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
    if (access(full_path, X_OK) == 0) {
      result = strdup(full_path);
      break;
    }
  }

  free(path);
  return result;
}

static void echo_command(char *args[]) {
  for (int i = 1; args[i] != NULL; i++) {
    printf("%s%s", args[i], args[i + 1] != NULL ? " " : "");
  }
  printf("\n");
}

static void cd_command(const char* path) {
  if (path == NULL || strcmp(path, "~") == 0) {
    path = getenv("HOME");
  }
  if (path != NULL && chdir(path) != 0) {
    printf("cd: %s: No such file or directory\n", path);
  }
}

static void type_command(const char *cmd) {
  if (is_builtin(cmd)) {
    printf("%s is a shell builtin\n", cmd);
    return;
  }
  char *path = find_executable(cmd);
  if (path != NULL) {
    printf("%s is %s\n", cmd, path);
    free(path);
  } else {
    printf("%s: not found\n", cmd);
  }
}

static void pwd_command(void) {
  char cwd[BUFFER_SIZE];

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("pwd");
  }
}

static void execute_command(char *args[]) {
  char *path = find_executable(args[0]);
  if (path == NULL) {
    printf("%s: command not found\n", args[0]);
    return;
  }
  pid_t pid = fork();
  if (pid == 0) {
    execv(path, args);
    perror("execv");
    exit(1);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  } else {
    perror("fork");
  }
  free(path);
}

int main(void) {
  setbuf(stdout, NULL);
  char line[BUFFER_SIZE];

  printf("$ ");
  while (fgets(line, BUFFER_SIZE, stdin) != NULL) {
    line[strcspn(line, "\n")] = '\0';

    int n = 0, start = 0;
    bool is_quote = false;

    char *args[BUFFER_SIZE];
    char *p = line;
    for (; *p; p++) {
      if (*p == '\'') {
        if (is_quote) {
          int end = p - line;
          int length = end - 1 - start;
          args[n] = malloc((length + 1) * sizeof(char));
          memcpy(args[n++], line + start, end - start);
        }
        start = p - line + 1;
        is_quote ^= 1;
      } else if (*p == ' ') {
        if (is_quote) {
          continue;
        } else {
          if (start != p - line) {
            int end = p - line;
            int length = end - start;
            args[n] = malloc((length + 1) * sizeof(char));
            strncpy(args[n++], line + start, end - start);
            printf("%s\n", args[n-1]);
          }
          start = p - line + 1;
        }
      }
    }
    if (*(line + start) != '\0') {
      int end = p - line;
      int length = end - start;
      args[n] = malloc((length + 1) * sizeof(char)); 
      strncpy(args[n++], line + start, end - start);
    }
    args[n] = NULL;

    if (n == 0) {
      printf("$ ");
      continue;
    }
    printf("\n\n\n");

    if (strcmp(args[0], "exit") == 0) break;
    else if (strcmp(args[0], "echo") == 0) echo_command(args);
    else if (strcmp(args[0], "type") == 0) type_command(args[1] ? args[1] : "");
    else if (strcmp(args[0], "pwd") == 0) pwd_command();
    else if (strcmp(args[0], "cd") == 0) cd_command(args[1]);
    else execute_command(args);

    printf("$ ");
  }

  return 0;
}
