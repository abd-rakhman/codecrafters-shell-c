#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  char cmd[BUFFER_SIZE];
  printf("$ ");

  while (scanf("%s", cmd) == 1) {
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
    } else {
      printf("%s: command not found\n", cmd);
    }
    printf("$ ");
  }

  return 0;
}
