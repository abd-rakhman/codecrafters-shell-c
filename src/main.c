#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  char cmd[40];
  printf("$ ");

  while (scanf("%s", cmd) == 1) {
    printf("%s: command not found\n", cmd);
    printf("$ ");
  }

  return 0;
}
