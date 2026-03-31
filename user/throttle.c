#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "usage: throttle <active_ticks> <idle_ticks>\n");
    exit(1);
  }

  int active = atoi(argv[1]);
  int idle = atoi(argv[2]);

  if(set_throttle(active, idle) < 0){
    fprintf(2, "throttle: set_throttle(%d, %d) failed\n", active, idle);
    exit(1);
  }

  printf("throttle: set to %d active / %d idle ticks per cycle\n", active, idle);
  exit(0);
}
