#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include "trie.h"
#include "compspec.h"

#define BUFFER_SIZE 1024

const char *builtins[] = {"echo", "type", "exit", "pwd", "complete", NULL};
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
      executables[*count] = malloc(BUFFER_SIZE);
      strcpy(executables[*count], entry->d_name);
      *count = *count + 1;
    }
    closedir(dir);
  }

  free(path);
}

static int compare(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void list_path_completions(const char *dir_path, const char *prefix, int *count,
                                  char **completions) {
  *count = 0;

  DIR *dir = opendir(dir_path);
  if (dir == NULL) {
    return;
  }

  size_t prefix_len = strlen(prefix);
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    if (strncmp(name, prefix, prefix_len) != 0) continue;
    bool is_dir = (entry->d_type == DT_DIR);
    const char* rest = name + prefix_len;
    size_t n = strlen(rest);

    completions[*count] = malloc(n + 2);
    memcpy(completions[*count], rest, n);
    if (is_dir) {
      completions[*count][n] = '/';
      completions[*count][n+1] = '\0';
    } else {
      completions[*count][n] = '\0';
    }
    (*count)++;
  }

  qsort(completions, *count, sizeof(*completions), compare);

  closedir(dir);
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

static void complete_command(Compspec *compspecs, char *args[], int argc) {
  if (strcmp(args[1], "-p") == 0) {
    const char *cmd = args[2];
    if (cmd == NULL) {
      printf("complete: your function is not complete\n");
      return ;
    }
    const char *path = compspec_get_path(compspecs, cmd);
    if (path == NULL) {
      printf("complete: %s: no completion specification\n", cmd);
    } else {
      printf("complete -C '%s' %s\n", path, cmd);
    }
  } else if (strcmp(args[1], "-C") == 0) {
    const char *path = args[2];
    const char *cmd = args[3];
    if (path == NULL || cmd == NULL) {
      printf("complete: your function is not complete\n");
      return ;
    }
    compspec_add_path(compspecs, cmd, path);
  } else {
    printf("complete: incorrect format\n");
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

void parse_line(const char *line, int line_len, int *n, char **args) {
  int tok_len = 0;
  const char *quote = NULL;

  args[0] = malloc(BUFFER_SIZE);
  for (int i = 0; i < line_len; i++) {
    char ch = line[i];
    if (ch == ' ' && !quote) {
      if (tok_len > 0) {
        args[*n][tok_len] = '\0';
        tok_len = 0;
        (*n)++;
        args[*n] = malloc(BUFFER_SIZE);
      }
      continue;
    } else if (ch == '\'' || ch == '\"') {
      if (!quote) quote = &line[i];
      else if (*quote == ch) quote = NULL;
      else args[*n][tok_len++] = ch;
    } else if (ch == '\\' && (!quote || *quote != '\'')) {
      i++;
      if (i >= line_len) break;
      args[*n][tok_len++] = line[i];
    } else {
      args[*n][tok_len++] = ch;
    }
  }
  if (tok_len > 0) {
    args[*n][tok_len] = '\0';
    (*n)++;
  }
  args[*n] = NULL;
}

void execute(Compspec *compspecs, char line[], int line_len) {
  int n = 0;
  char *args[BUFFER_SIZE];
  
  parse_line(line, line_len, &n, args);

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
  else if (strcmp(args[0], "complete") == 0) complete_command(compspecs, args, n);
  else execute_command(args);

  dup2(stdout_fd, 1);
  dup2(stderr_fd, 2);

  printf("$ ");
}

static void handle_backspace(char *line, int *len) {
  if (*len > 0) {
    (*len)--;
    line[*len] = '\0';
    write(STDOUT_FILENO, "\b \b", 3);
  }
}

static void handle_enter(Compspec *compspecs, char *line, int *len) {
  printf("\n");
  execute(compspecs, line, *len);
  *len = 0;
  line[0] = '\0';
}

static void handle_char(char *line, int *len, int c) {
  line[(*len)++] = c;
  line[*len] = '\0';
  putchar(c);
}

static void free_str_array(char **args, int argc) {
  for (int i = 0; i < argc; i++) free(args[i]);
  free(args);
}

/* Owns the returned dir string; *filename points into token (do not free). */
static char *split_path_token(const char *token, const char **filename) {
  const char *last_slash = strrchr(token, '/');
  if (last_slash == NULL) {
    *filename = token;
    return strdup(".");
  }
  size_t dir_len = (size_t)(last_slash - token);
  *filename = last_slash + 1;
  if (dir_len == 0) return strdup("/");
  return strndup(token, dir_len);
}

static void append_to_line(char *line, int *len, const char *s) {
  for (; *s; s++) {
    line[(*len)++] = *s;
    putchar(*s);
  }
  line[*len] = '\0';
}

static size_t common_prefix_len(char **completions, int count) {
  size_t n = strlen(completions[0]);
  for (int i = 1; i < count; i++) {
    size_t len = strlen(completions[i]);
    if (len < n) n = len;
    for (size_t j = 0; j < n; j++) {
      if (completions[i][j] != completions[0][j]) {
        n = j;
        break;
      }
    }
    if (n == 0) break;
  }
  return n;
}

static void apply_completions(char *line, int *len, const char *word, int *tab_count,
                              int count, char **completions) {
  if (count == 0) {
    printf("\a");
    *tab_count = 0;
    return;
  }

  if (count == 1) {
    append_to_line(line, len, completions[0]);
    bool is_dir = *len > 0 && line[*len - 1] == '/';
    if (!is_dir) {
      line[(*len)++] = ' ';
      putchar(' ');
    }
    *tab_count = 0;
    return;
  }

  size_t shared = common_prefix_len(completions, count);
  if (shared == 0) {
    if (*tab_count >= 2) {
      const char *base = strrchr(word, '/');
      base = base ? base + 1 : word;

      printf("\n");
      for (int i = 0; i < count; i++) {
        if (i > 0) printf("  ");
        printf("%s%s", base, completions[i]);
      }
      printf("\n$ %.*s", *len, line);
      *tab_count = 0;
    } else {
      printf("\a");
    }
    return;
  }

  for (size_t j = 0; j < shared; j++) {
    line[(*len)++] = completions[0][j];
    putchar(completions[0][j]);
  }
  line[*len] = '\0';
  *tab_count = 0;
}

static void handle_tab(Compspec *compspec, char *line, int *len, int *tab_count, Trie *trie) {
  char **args = malloc(BUFFER_SIZE * sizeof(char *));
  int argc = 0;
  parse_line(line, *len, &argc, args);

  bool trailing_space = *len > 0 && line[*len - 1] == ' ';
  if (argc == 1 && !trailing_space) {
    TrieResult *result = trie_autocomplete(trie, args[0]);
    apply_completions(line, len, args[0], tab_count, result->count, result->results);
    trie_result_destroy(result);
    free_str_array(args, argc);
    return;
  }

  if (argc == 1) {
    if (compspec_get_path(compspec, args[0]) != NULL) {
      char *completion = compspec_run(compspec, args[0]);
      if (completion == NULL) {
        printf("\a");
        return ;
      }
      append_to_line(line, len, completion);
      line[(*len)++] = ' ';
      putchar(' ');
      free(completion);
      return ;
    }
  }

  const char *token = trailing_space ? "" : (argc > 0 ? args[argc - 1] : "");
  const char *filename;
  char *dir_path = split_path_token(token, &filename);
  char **completions = malloc(BUFFER_SIZE * sizeof(char *));
  int count = 0;
  list_path_completions(dir_path, filename, &count, completions);
  apply_completions(line, len, token, tab_count, count, completions);

  free_str_array(completions, count);
  free(dir_path);
  free_str_array(args, argc);
}

static Trie *build_executables_trie(void) {
  Trie *trie = trie_create();
  for (int i = 0; builtins[i] != NULL; i++) trie_add(trie, builtins[i]);

  char **executables = malloc(256 * BUFFER_SIZE * sizeof(char *));
  int *executables_count = malloc(sizeof(int));
  *executables_count = 0;
  find_all_executables(executables_count, executables);
  for (int i = 0; i < *executables_count; i++) {
    trie_add(trie, executables[i]);
  }
  return trie;
}

static void run_repl(Trie *trie, Compspec *compspecs) {
  char line[BUFFER_SIZE];
  int len = 0;
  int tab_count = 0;
  int c;

  line[0] = '\0';

  while ((c = getchar()) != EOF) {
    if (c == 127 || c == 8) {
      handle_backspace(line, &len);
      tab_count = 0;
    } else if (c == '\n') {
      handle_enter(compspecs, line, &len);
      tab_count = 0;
    } else if (c == '\t') {
      tab_count++;
      handle_tab(compspecs, line, &len, &tab_count, trie);
    } else {
      handle_char(line, &len, c);
      tab_count = 0;
    }
  }
}

int main(void) {
  configure_terminal();
  setbuf(stdout, NULL);
  stdout_fd = dup(1), stderr_fd = dup(2);
  Trie* trie = build_executables_trie();
  Compspec* compspecs = compspec_create();

  printf("$ ");
  run_repl(trie, compspecs);

  trie_destroy(trie);
  close(stdout_fd);
  return 0;
}
