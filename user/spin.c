#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int iterations = 100000000;
  if(argc > 1)
    iterations = atoi(argv[1]);

  int start = uptime();

  volatile int x = 0;
  for(int i = 0; i < iterations; i++){
    x += i;
  }

  int end = uptime();
  printf("spin: %d iterations in %d ticks\n", iterations, end - start);
  exit(0);
}
