#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

char* builtin_commands[] = {"echo", "type", "exit"};

char* find_executable(char* cmd) {
  char *PATH = getenv("PATH");
  char *path = strdup(PATH);
  char *token = strtok(path, ":");

  while (token) {
    DIR *dir = opendir(token);
    if (dir == NULL) {
      token = strtok(NULL, ":");
      continue;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      char* full_path = malloc(strlen(token) + strlen(entry->d_name) + 2);
      if (strcmp(entry->d_name, cmd) != 0) {
        continue;
      }
      sprintf(full_path, "%s/%s", token, entry->d_name);
      if (access(full_path, X_OK) == 0) {
        return full_path;
      }
    }
    token = strtok(NULL, ":");
    closedir(dir);
  }
  free(token);
  free(path);
  free(token);
  return NULL;
}

void echo_command(char *args) {
  int start = 4;
  while (args[start] == ' ') {
    start++;
  }
  printf("%s", args + start);
}

void type_command(char* cmd) {
  if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "type") == 0 || strcmp(cmd, "exit") == 0) {
    printf("%s is a shell builtin\n", cmd);
  } else if (find_executable(cmd) != NULL) {
    printf("%s is %s\n", cmd, find_executable(cmd));
  } else {
    printf("%s: not found\n", cmd);
  }
}

void execute_command(char* cmd, char* args[]) {
  char *executable_path = find_executable(cmd);
  if (executable_path == NULL) {
    printf("%s: command not found\n", cmd);
    return;
  }
  pid_t pid = fork();
  if (pid == 0) {
    execv(executable_path, args);
    exit(1);
  } else {
    waitpid(pid, NULL, 0);
  }
}

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  char cmd[BUFFER_SIZE];
  printf("$ ");

  char args[BUFFER_SIZE];
  while (fgets(args, BUFFER_SIZE, stdin) != NULL) {
    char cmd[BUFFER_SIZE];
    sscanf(args, "%s", cmd);
    if (strcmp(cmd, "exit") == 0) break;
    else if (strcmp(cmd, "echo") == 0) echo_command(args);
    else if (strcmp(cmd, "type") == 0) {
      char arg[BUFFER_SIZE];
      sscanf(args + strlen(cmd), "%s", arg);
      type_command(arg);
    } else if (find_executable(cmd) != NULL) {
      char *args_array[BUFFER_SIZE];
      args_array[0] = cmd;
      int i = 1;
      char *token = strtok(args + strlen(cmd), " \n");
      while (token != NULL) {
        args_array[i++] = token;
        token = strtok(NULL, " \n");
      }
      args_array[i] = NULL;
      execute_command(cmd, args_array);
    } else {
      printf("%s: command not found\n", cmd);
    }
    // if (strcmp(cmd, "exit") == 0) {
    //   break;
    // } else if (strcmp(cmd, "echo") == 0) {
    //   echo_command();
    // } else if (strcmp(cmd, "type") == 0) {
    //   char arg[BUFFER_SIZE];
    //   scanf("%s", arg);
    //   if (strcmp(arg, "echo") == 0 || strcmp(arg, "type") == 0 || strcmp(arg, "exit") == 0) {
    //     printf("%s is a shell builtin\n", arg);
    //   } else if (find_executable(arg) != NULL) {
    //     printf("%s is %s\n", arg, find_executable(arg));
    //   } else {
    //     printf("%s: not found\n", arg);
    //   }
    // } else if (find_executable(cmd) != NULL) {
    //   char *executable_path = find_executable(cmd);
    //   char *args[BUFFER_SIZE];
    //   args[0] = NULL;
    //   pid_t pid = fork();
    //   if (pid == 0) {
    //     execv(executable_path, args);
    //   }
    //   waitpid(pid, NULL, 0);
    // } else {
    //   printf("%s: command not found\n", cmd);
    // }
    printf("$ ");
  }

  return 0;
}
