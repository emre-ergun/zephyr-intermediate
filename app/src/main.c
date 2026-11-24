#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_COOP (-1)
#define PRIO_LOW 7
#define PRIO_MED 5
#define PRIO_HIG 3

void coop_fn(void *p1, void *p2, void *p3) {
  LOG_INF("[COOP] starting - will run 5 steps without yielding");

  for (int i = 0; i < 5; i++) {
    k_busy_wait(400000);
    LOG_INF("[COOP] step %d/5 - still holding CPU tick=%u", i + 1,
            k_uptime_get_32());
  }

  /*
   * Note on k_yield():
   * In Zephyr, k_yield() yields CPU execution only to ready threads of
   * equal or higher priority (numerically <= current priority).
   * Since t_coop runs at priority -1 (cooperative), and threads main (prio 0),
   * t_high (prio 3), t_med (prio 5), and t_low (prio 7) all have lower priority,
   * k_yield() does not allow them to run. It returns immediately, and
   * lower-priority preemptible threads only progress after coop_fn returns.
   */
  LOG_INF("[COOP] calling k_yield() - yielding from prio -1 only allows ready threads of equal/higher priority to run");
  k_yield();
  LOG_INF("[COOP] done - exiting coop_fn so lower priority threads can now run.");
}

/*
 * Execution Frequency based on sleep periods:
 *    - t_high (prio 3): sleeps 100 ms -> runs ~10 times / sec.
 *    - t_med  (prio 5): sleeps 200 ms -> runs ~5 times / sec.
 *    - t_low  (prio 7): sleeps 300 ms -> runs ~3.33 times / sec.
 *    Over any 600 ms period (least common multiple of 100, 200, 300 ms):
 *      - t_high logs 6 times
 *      - t_med  logs 3 times
 *      - t_low  logs 2 times
 */
/* 
*** Booting Zephyr OS build v4.4.0 ***
[00:00:00.000,000] <inf> demo: [COOP] starting - will run 5 steps without yielding
[00:00:00.400,000] <inf> demo: [COOP] step 1/5 - still holding CPU tick=400
[00:00:00.800,000] <inf> demo: [COOP] step 2/5 - still holding CPU tick=800
[00:00:01.200,000] <inf> demo: [COOP] step 3/5 - still holding CPU tick=1200
[00:00:01.600,000] <inf> demo: [COOP] step 4/5 - still holding CPU tick=1600
[00:00:02.000,000] <inf> demo: [COOP] step 5/5 - still holding CPU tick=2000
[00:00:02.000,000] <inf> demo: [COOP] calling k_yield() - yielding from prio -1 only allows ready threads of equal/higher priority to run
[00:00:02.000,000] <inf> demo: [COOP] done - exiting coop_fn so lower priority threads can now run.
[00:00:02.000,000] <inf> demo: === L1 - TASK 1 ===
[00:00:02.000,000] <inf> demo: Thread COOP: Priority -1, busy wait then yield
[00:00:02.000,000] <inf> demo: Thread LOW: Priority 7, sleeps 300ms
[00:00:02.000,000] <inf> demo: Thread MEDIUM: Priority 5, sleeps 200ms
[00:00:02.000,000] <inf> demo: Thread HIGH: Priority 3, sleeps 100ms
[00:00:02.000,000] <inf> demo: T_HIGH running - CPU tick=2000
[00:00:02.000,000] <inf> demo: T_MED running - CPU tick=2000
[00:00:02.000,000] <inf> demo: T_LOW running - CPU tick=2000
[00:00:02.110,000] <inf> demo: T_HIGH running - CPU tick=2110
[00:00:02.210,000] <inf> demo: T_MED running - CPU tick=2210
[00:00:02.220,000] <inf> demo: T_HIGH running - CPU tick=2220
[00:00:02.310,000] <inf> demo: T_LOW running - CPU tick=2310
[00:00:02.330,000] <inf> demo: T_HIGH running - CPU tick=2330
[00:00:02.420,000] <inf> demo: T_MED running - CPU tick=2420
[00:00:02.440,000] <inf> demo: T_HIGH running - CPU tick=2440
[00:00:02.550,000] <inf> demo: T_HIGH running - CPU tick=2550
*/
/*
 * Simultaneous Readiness & Priority:
 *    When multiple threads wake up or become ready at the exact same tick
 *    Zephyr's priority-based preemptive scheduler picks the ready thread with the
 *    highest priority as you see at the oupput above:
 *    at tick = 2000 all three preemptive tasks were ready to run. They were executed
 *    based on their priorities.
 */
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
