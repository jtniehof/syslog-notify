/*Get the size of the pipe buffer*/
#include <limits.h>
#include <stdio.h>

int main() {
  printf("%d\n", PIPE_BUF);
  return 0;
}
