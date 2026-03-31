#include "kernel/types.h"
#include "user/user.h"

static int
run_spin(int iters)
{
  int start = uptime();
  volatile int a = 1, b = 2, c = 3;
  for(int i = 1; i <= iters; i++){
    a = (a * 1664525) + 1013904223;
    b = b / (i % 31 + 1) + a;
    c = c / (i % 37 + 1) + b;
    a ^= (c >> 5);
  }
  return uptime() - start;
}

int
main(int argc, char *argv[])
{
  int iters = 30000000;
  if(argc > 1)
    iters = atoi(argv[1]);

  printf("\n=== CPU Throttle Demo ===\n");
  printf("Workload: %d iterations per trial\n\n", iters);

  set_throttle(100, 0);
  int t_100 = run_spin(iters);
  printf("  100/0   (no throttle): %d ticks\n", t_100);

  set_throttle(70, 30);
  int t_70 = run_spin(iters);
  printf("   70/30  (70%% active):  %d ticks\n", t_70);

  set_throttle(50, 50);
  int t_50 = run_spin(iters);
  printf("   50/50  (50%% active):  %d ticks\n", t_50);

  set_throttle(30, 70);
  int t_30 = run_spin(iters);
  printf("   30/70  (30%% active):  %d ticks\n", t_30);

  set_throttle(100, 0);

  printf("\n--- Slowdown vs baseline ---\n");
  if(t_100 > 0){
    printf("  70/30:  %dx\n", t_70 / t_100);
    printf("  50/50:  %dx\n", t_50 / t_100);
    printf("  30/70:  %dx\n", t_30 / t_100);
  } else {
    printf("  (baseline too fast to measure -- increase iterations)\n");
  }

  printf("\n=== Demo complete ===\n");
  exit(0);
}
