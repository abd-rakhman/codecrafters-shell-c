#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
  char *original_path = getenv("PATH");

  setbuf(stdout, NULL);
  char cmd[BUFFER_SIZE];
  printf("$ ");

  while (scanf("%s", cmd) == 1) {
    char *path = strdup(original_path);
    if (strcmp(cmd, "exit") == 0) {
      break;
    } else if (strcmp(cmd, "echo") == 0) {
      char args[BUFFER_SIZE];
      fgets(args, BUFFER_SIZE, stdin);
      int start = 0;
      while (args[start] == ' ') {
        start++;
      }
      printf("%s", args + start);
    } else if (strcmp(cmd, "type") == 0) {
      char arg[BUFFER_SIZE];
      scanf("%s", arg);
      if (strcmp(arg, "echo") == 0 || strcmp(arg, "type") == 0 || strcmp(arg, "exit") == 0) {
        printf("%s is a shell builtin\n$ ", arg);
        continue;
      }
      char *token = strtok(path, ":");
      bool found = false;

      while (token) {
        DIR *dir = opendir(token);
        if (dir == NULL) {
          token = strtok(NULL, ":");
          continue;
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
          char* full_path = malloc(strlen(token) + strlen(entry->d_name) + 2);
          sprintf(full_path, "%s/%s", token, entry->d_name);
          if (access(full_path, X_OK) == 0 && strcmp(entry->d_name, arg) == 0) {
            printf("%s is %s/%s\n$ ", arg, token, entry->d_name);
            found = true;
            break;
          }
        }
        if (found) {
          break;
        }

        token = strtok(NULL, ":");
      }
      if (found) {
        continue;
      }
      printf("%s: not found\n", arg);
    } else {
      printf("%s: command not found\n", cmd);
    }
    printf("$ ");
  }

  return 0;
}
