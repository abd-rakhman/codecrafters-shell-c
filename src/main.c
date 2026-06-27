#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

const char *builtins[] = {"echo", "type", "exit", NULL};

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

static void pwd_command() {
  char cwd[1024];

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
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
    line[strcspn(line, "\n")] = '\0';   // strip trailing newline

    char *args[BUFFER_SIZE];
    int n = 0;
    for (char *tok = strtok(line, " "); tok != NULL; tok = strtok(NULL, " ")) {
      args[n++] = tok;
    }
    args[n] = NULL;

    if (n == 0) {
      printf("$ ");
      continue;
    }


    if (strcmp(args[0], "exit") == 0) break;
    else if (strcmp(args[0], "echo") == 0) echo_command(args);
    else if (strcmp(args[0], "type") == 0) type_command(args[1] ? args[1] : "");
    else execute_command(args);

    printf("$ ");
  }

  return 0;
}
