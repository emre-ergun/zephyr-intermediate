/*
 * ==============================================================================
 * PROTECTED vs UNPROTECTED COUNTER INCREMENT
 * ==============================================================================
 *
 * 1. When code increases a counter (counter = counter + 1), the computer does
 *    three separate steps: First step is to READ the current value of 'counter'
 *    from memory. Second step is to ADD 1 to that value. And third steps is
 *    to WRITE the new value back into memory.
 *
 * 2. We have two threads running at the same priority with time-slicing
 *    enabled. The scheduler can switch between threads at any time.
 *    Imagine the counter is currently 10:
 *      - Thread 1 reads the counter (value is 10).
 *      - Before Thread 1 can write back 11, the scheduler stops Thread 1
 *        and switches to Thread 2.
 *      - Thread 2 also reads the counter (it is still 10).
 *      - Thread 2 adds 1 and writes 11 back into memory.
 *      - Later, Thread 1 runs again. It still has the old value 10 in its local
 *        temporary variable, adds 1, and writes 11 back into memory.
 *    As a consequence, both threads did an increment, so the counter should
 *    be 12. Instead, the counter is only 11, Thread 1 overwrote Thread 2's
 *    update. One update was completely lost.
 *
 * 3. A mutex works like a lock on a door:
 *      - Before starting the READ, ADD, and WRITE, a thread locks the mutex.
 *      - Only ONE thread can hold the lock at a time.
 *      - If another thread wants to update the counter, it must wait until the
 *        first thread unlocks the mutex.
 *      - This guarantees that READ, ADD, and WRITE finish together without
 *        any other thread interfering.
 *
 * ==============================================================================
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define PRIO 5
#define INCREMENTS 100000

static volatile uint32_t counter;

static K_SEM_DEFINE(done_sem, 0, 2);
static K_MUTEX_DEFINE(counter_mutex);

/* Selected increment function based on CONFIG_PROTECTED */
static inline void counter_increment(void) {
#if IS_ENABLED(CONFIG_PROTECTED)
  k_mutex_lock(&counter_mutex, K_FOREVER);
  uint32_t tmp = counter;
  k_busy_wait(1);
  counter = tmp + 1;
  k_mutex_unlock(&counter_mutex);
#else
  uint32_t tmp = counter;
  k_busy_wait(1);
  counter = tmp + 1;
#endif
}

void t_one_fn(void *p1, void *p2, void *p3) {
  const char *name = k_thread_name_get(k_current_get());

  for (int i = 0; i < INCREMENTS; i++) {
    counter_increment();
  }

  LOG_INF("[%s] finished", name);
  k_sem_give(&done_sem);
}

void t_two_fn(void *p1, void *p2, void *p3) {
  const char *name = k_thread_name_get(k_current_get());

  for (int i = 0; i < INCREMENTS; i++) {
    counter_increment();
  }

  LOG_INF("[%s] finished", name);
  k_sem_give(&done_sem);
}

K_THREAD_DEFINE(t_one, STACK_SIZE, t_one_fn, NULL, NULL, NULL, PRIO, 0, 0);
K_THREAD_DEFINE(t_two, STACK_SIZE, t_two_fn, NULL, NULL, NULL, PRIO, 0, 0);

int main(void) {
  int64_t time = k_uptime_get();
  uint32_t expected = INCREMENTS * 2;

  LOG_INF("=== L2 - TASK 1 ===");
#if IS_ENABLED(CONFIG_PROTECTED)
  LOG_INF("Mode: PROTECTED (CONFIG_PROTECTED=y, Mutex ENABLED)");
#else
  LOG_INF("Mode: UNPROTECTED (CONFIG_PROTECTED=n, Mutex DISABLED)");
#endif
  LOG_INF("Thread 1: Priority %d", PRIO);
  LOG_INF("Thread 2: Priority %d", PRIO);
  LOG_INF("Expected total: %u", expected);

  k_sem_take(&done_sem, K_FOREVER);
  k_sem_take(&done_sem, K_FOREVER);

  LOG_INF("Actual final total: %u", counter);

  if (counter == expected) {
    LOG_INF("Result: SUCCESS (0 lost updates)");
  } else {
    uint32_t lost = expected - counter;
    LOG_ERR("Result: RACE CONDITION CONFIRMED (lost %u updates)", lost);
  }
  LOG_INF("Execution time: %lld ms", k_uptime_delta(&time));

  return 0;
}
