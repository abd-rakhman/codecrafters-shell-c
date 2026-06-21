#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  char cmd[40];

  printf("$ ");

  scanf("%s", cmd);
  printf("%s: command not found\n", cmd);

  return 0;
}
