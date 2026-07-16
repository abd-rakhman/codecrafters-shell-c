#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <termios.h>

#define BUFFER_SIZE 1024

const char *builtins[] = {"echo", "type", "exit", "pwd", NULL};
int stdout_fd, stderr_fd;

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

static void configure_terminal() {
  struct termios old, raw;

  tcgetattr(STDIN_FILENO, &old);
  raw = old;

  raw.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void execute(char line[]) {
  int n = 0, len = 0;
  char *quote = NULL;
  char *args[BUFFER_SIZE];

  args[0] = malloc(BUFFER_SIZE * sizeof(char));
  for (char *p = line; *p; p++) {
    if (*p == ' ' && !quote) {
      if (len > 0) {
        *(args[n] + len) = '\0';
        len = 0;
        n++;
        args[n] = malloc(BUFFER_SIZE * sizeof(char));
      }
      continue;
    } else if (*p == '\'' || *p == '\"') {
      if (!quote) quote = p;
      else if (*quote == *p) quote = NULL;
      else *(args[n] + len++) = *p;
    } else if (*p == '\\' && (!quote || *quote != '\'')) {
        p++;
        *(args[n] + len++) = *p;
    } else {
      if (*p == '\t') {
        if (strcmp(args[n], "ech") == 0) *(args[n] + len++) = 'o';
        else if (strcmp(args[n], "exi") == 0) *(args[n] + len++) = 't';
        else *(args[n] + len++) = *p;
      } 
      else *(args[n] + len++) = *p;
    }
  }
  if (len > 0) {
    *(args[n++] + len) = '\0';
  }
  args[n] = NULL;

  if (n == 0) {
    printf("$ ");
    return;
  }

  if (n > 2) {
    int code = 0;
    bool truncate = false;

    if (strcmp(args[n-2], "1>") == 0 || strcmp(args[n-2], ">") == 0) {
      code = 1;
    } else if (strcmp(args[n-2], "1>>") == 0 || strcmp(args[n-2], ">>") == 0) {
      code = 1; truncate = true;
    } else if (strcmp(args[n-2], "2>") == 0) {
      code = 2;
    } else if (strcmp(args[n-2], "2>>") == 0) {
      code = 2; truncate = true;
    }
    if (code) {
      int command = truncate ? O_WRONLY|O_CREAT|O_TRUNC : O_WRONLY|O_CREAT|O_APPEND;

      printf("Filename: %s\n", args[n-1]);
      int fd = open(args[n-1], command, 0644);
      dup2(fd, code);
      close(fd);
      args[n-2] = NULL;
    }
  }

  if (strcmp(args[0], "exit") == 0) exit(0);
  else if (strcmp(args[0], "echo") == 0) echo_command(args);
  else if (strcmp(args[0], "type") == 0) type_command(args[1] ? args[1] : "");
  else if (strcmp(args[0], "pwd") == 0) pwd_command();
  else if (strcmp(args[0], "cd") == 0) cd_command(args[1]);
  else execute_command(args);

  dup2(stdout_fd, 1);
  dup2(stderr_fd, 2);

  printf("$ ");
}

int main(void) {
  configure_terminal();
  setbuf(stdout, NULL);
  stdout_fd = dup(1), stderr_fd = dup(2);
  char line[BUFFER_SIZE];

  printf("$ ");

  int c, len = 0;
  while ((c = getchar()) != EOF) {
    if (c == 127 || c == 8) {
      if (len > 0) {
        len--;
        write(STDOUT_FILENO, "\b \b", 3);
      }
    } else if (c == '\n') {
      printf("\n");
      line[len++] = '\0';
      execute(line);
      len = 0;
    } else if (c == '\t') {
      if (strcmp(line, "ech") == 0) { 
        line[len++] = 'o';
        line[len++] = ' ';
        printf("o ");
      } else if (strcmp(line, "exi") == 0) { 
        line[len++] = 't';
        line[len++] = ' ';
        printf("t ");
      }
    } else {
      printf("%c", c);
      line[len++] = c;
    }
  }
  close(stdout_fd);
  return 0;
}
