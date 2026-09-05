#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_COOP (-1)
#define PRIO_LOW 7
#define PRIO_MED 5
#define PRIO_HIG 3

void coop_fn(void *p1, void *p2, void *p3) {
  LOG_INF("[COOP] starting - will run 3 steps without yielding");

  for (int i = 0; i < 3; i++) {
    k_busy_wait(400000);
    LOG_INF("[COOP] step %d/3 - still holding CPU tick=%u", i + 1,
            k_uptime_get_32());
  }

  LOG_INF("[COOP] yielding now, LOW, MEDIUM and HIGH can run");
  k_yield();
  LOG_INF("[COOP] done.");
}

void t_low_fn(void *p1, void *p2, void *p3) {
  while (1) {
    LOG_INF("T_LOW running - CPU tick=%u", k_uptime_get_32());
    k_msleep(300);
  }
}

void t_med_fn(void *p1, void *p2, void *p3) {
  while (1) {
    LOG_INF("T_MED running - CPU tick=%u", k_uptime_get_32());
    k_msleep(200);
  }
}

void t_high_fn(void *p1, void *p2, void *p3) {
  while (1) {
    LOG_INF("T_HIGH running - CPU tick=%u", k_uptime_get_32());
    k_msleep(100);
  }
}

K_THREAD_DEFINE(t_coop, STACK_SIZE, coop_fn, NULL, NULL, NULL, PRIO_COOP, 0, 0);
K_THREAD_DEFINE(t_low, STACK_SIZE, t_low_fn, NULL, NULL, NULL, PRIO_LOW, 0, 0);
K_THREAD_DEFINE(t_med, STACK_SIZE, t_med_fn, NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(t_high, STACK_SIZE, t_high_fn, NULL, NULL, NULL, PRIO_HIG, 0,
                0);

int main(void) {
  LOG_INF("=== L1 - TASK 1 ===");
  LOG_INF("Thread COOP: Priority %d, busy wait then yield", PRIO_COOP);
  LOG_INF("Thread LOW: Priority %d, sleeps 300ms", PRIO_LOW);
  LOG_INF("Thread MEDIUM: Priority %d, sleeps 200ms", PRIO_MED);
  LOG_INF("Thread HIGH: Priority %d, sleeps 100ms", PRIO_HIG);

  return 0;
}
