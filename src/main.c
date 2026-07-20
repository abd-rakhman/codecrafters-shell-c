#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include "trie.h"

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

static void find_all_executables(int *count, char **executables) {
  char *path_env = getenv("PATH");
  if (path_env == NULL) return ;

  char *path = strdup(path_env);

  for (char *dir_path = strtok(path, ":"); dir_path != NULL; dir_path = strtok(NULL, ":")) {
    if (dir_path[0] == '\0') continue;

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        continue;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_type != DT_REG && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN) continue;
      executables[*count] = malloc(BUFFER_SIZE * sizeof(char));
      strcpy(executables[*count], entry->d_name);
      *count = *count + 1;
    }
    closedir(dir);
  }

  free(path);
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

static void configure_terminal(void) {
  struct termios old, raw;

  tcgetattr(STDIN_FILENO, &old);
  raw = old;

  raw.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void apply_trailing_redirect(char *args[], int n) {
  if (n <= 2) return;

  int fd_target = 0;
  bool append = false;

  if (strcmp(args[n - 2], "1>") == 0 || strcmp(args[n - 2], ">") == 0) {
    fd_target = 1;
  } else if (strcmp(args[n - 2], "1>>") == 0 || strcmp(args[n - 2], ">>") == 0) {
    fd_target = 1;
    append = true;
  } else if (strcmp(args[n - 2], "2>") == 0) {
    fd_target = 2;
  } else if (strcmp(args[n - 2], "2>>") == 0) {
    fd_target = 2;
    append = true;
  }

  if (!fd_target) return;

  int open_flags = append ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC);
  int fd = open(args[n - 1], open_flags, 0644);
  dup2(fd, fd_target);
  close(fd);
  args[n - 2] = NULL;
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
      *(args[n] + len++) = *p;
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

  apply_trailing_redirect(args, n);

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
static void reverse(char *str) {
  size_t left = 0;
  size_t right = strlen(str);

  if (right == 0) return;
  right--;

  while (left < right) {
    char tmp = str[left];
    str[left] = str[right];
    str[right] = tmp;
    left++;
    right--;
  }
}

static void current_word(const char *line, int len, char *word_out) {
  int word_pos = 0;
  for (int pos = len - 1; pos >= 0 && line[pos] != ' '; pos--) {
    word_out[word_pos++] = line[pos];
  }
  word_out[word_pos] = '\0';
  reverse(word_out);
}

static void handle_backspace(int *len) {
  if (*len > 0) {
    (*len)--;
    write(STDOUT_FILENO, "\b \b", 3);
  }
}

static void handle_enter(char *line, int *len) {
  printf("\n");
  line[(*len)++] = '\0';
  execute(line);
  *len = 0;
}

static void handle_char(char *line, int *len, int c) {
  printf("%c", c);
  line[(*len)++] = c;
}

static void handle_tab(char *line, int *len, bool *tabbed, Trie *trie) {
  char word[BUFFER_SIZE];
  current_word(line, *len, word);

  TrieResult *result = trie_autocomplete(trie, word);

  if (*tabbed) {
    printf("\n");
    for (int i = 0; i < result->count; i++) {
      if (i > 0) printf("  ");
      printf("%s%s", word, result->results[i]);
    }
    printf("\n$ %s", line);
    *tabbed = false;
  } else if (result->count == 1) {
    for (char *p = result->results[0]; *p; p++) {
      line[(*len)++] = *p;
      printf("%c", *p);
    }
    line[(*len)++] = ' ';
    printf(" ");
    *tabbed = false;
  } else if (result->count == 0) {
    printf("\a");
  } else {
    int prefix_len = strlen(result->results[0]);
    for (int i = 1; i < result->count; i++) {
      char *str = result->results[i];
      if (strlen(str) < prefix_len) prefix_len = strlen(str);
      for (int j = 0; j < prefix_len; j++) {
        if (str[j] != result->results[0][j]) {
          prefix_len = j;
          break;
        }
      }
      if (prefix_len == 0) break;
    }
    if (prefix_len == 0) {
      *tabbed = true;
      printf("\a");
    } else {
      for (int j = 0; j < prefix_len; j++) {
         line[(*len)++] = result->results[0][j];
         printf("%c", result->results[0][j]);
      }
    }
  }

  trie_result_destroy(result);
}

static Trie *build_executables_trie(void) {
  Trie *trie = trie_create();
  for (int i = 0; builtins[i] != NULL; i++) trie_add(trie, builtins[i]);
  // trie_add(trie, "xyz_hello_back");
  // trie_add(trie, "xyz_hello_front");

  char **executables = malloc(256 * BUFFER_SIZE * sizeof(char *));
  int *executables_count = malloc(sizeof(int));
  *executables_count = 0;
  find_all_executables(executables_count, executables);
  for (int i = 0; i < *executables_count; i++) {
    trie_add(trie, executables[i]);
  }
  return trie;
}

static void run_repl(Trie *trie) {
  char line[BUFFER_SIZE];
  int len = 0;
  bool tabbed = false;
  int c;

  while ((c = getchar()) != EOF) {
    if (c == 127 || c == 8) {
      handle_backspace(&len);
      tabbed = false;
    } else if (c == '\n') {
      handle_enter(line, &len);
      tabbed = false;
    } else if (c == '\t') {
      handle_tab(line, &len, &tabbed, trie);
    } else {
      handle_char(line, &len, c);
      tabbed = false;
    }
  }
}

int main(void) {
  configure_terminal();
  setbuf(stdout, NULL);
  stdout_fd = dup(1), stderr_fd = dup(2);
  Trie* trie = build_executables_trie();

  printf("$ ");
  run_repl(trie);

  trie_destroy(trie);
  close(stdout_fd);
  return 0;
}
