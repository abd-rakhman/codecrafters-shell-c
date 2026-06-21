#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  char cmd[40];
  printf("$ ");

  while (scanf("%s", cmd) == 1) {
    if (strcmp(cmd, "exit") == 0) {
      break;
    }
    printf("%s: command not found\n", cmd);
    printf("$ ");
  }

  return 0;
}
