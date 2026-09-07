#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define PRIO 5
#define INCREMENTS 100000

static volatile uint32_t counter;

static K_SEM_DEFINE(done_sem, 0, 2);
static K_MUTEX_DEFINE(counter_mutex);

void t_one_fn(void *p1, void *p2, void *p3) {
  const char *name = k_thread_name_get(k_current_get());

  for (int i = 0; i < INCREMENTS; i++) {
    k_mutex_lock(&counter_mutex, K_FOREVER);
    counter++;
    k_mutex_unlock(&counter_mutex);
  }

  LOG_INF("[%s] finished", name);
  k_sem_give(&done_sem);
}

void t_two_fn(void *p1, void *p2, void *p3) {
  const char *name = k_thread_name_get(k_current_get());

  for (int i = 0; i < INCREMENTS; i++) {
    k_mutex_lock(&counter_mutex, K_FOREVER);
    counter++;
    k_mutex_unlock(&counter_mutex);
  }

  LOG_INF("[%s] finished", name);
  k_sem_give(&done_sem);
}

K_THREAD_DEFINE(t_one, STACK_SIZE, t_one_fn, NULL, NULL, NULL, PRIO, 0, 0);
K_THREAD_DEFINE(t_two, STACK_SIZE, t_two_fn, NULL, NULL, NULL, PRIO, 0, 0);

int main(void) {
  int64_t time = k_uptime_get();

  LOG_INF("=== L2 - TASK 1 ===");
  LOG_INF("Thread 1: Priority %d", PRIO);
  LOG_INF("Thread 2: Priority %d", PRIO);

  k_sem_take(&done_sem, K_FOREVER);
  k_sem_take(&done_sem, K_FOREVER);

  LOG_INF("Actual  final value: %u", counter);

  if (counter == INCREMENTS * 2) {
    LOG_WRN("No race this run");
  } else {
    LOG_ERR("Race condition confirmed: lost %d updates",
            (INCREMENTS * 2) - counter);
  }
  LOG_INF("Execution time: %lld ms", k_uptime_delta(&time));

  return 0;
}
