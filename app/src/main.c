#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define MSG_QUEUE_CAPACITY 16
#define MAX_SAMPLES 20
#define STUCK_THRESHOLD_SAMPLE 5

struct sensor_msg {
  uint32_t seq;
  int16_t temperature;
};

K_MSGQ_DEFINE(sensor_msgq, sizeof(struct sensor_msg), MSG_QUEUE_CAPACITY, 4);

static int consumer_wdt_chan;

static void watchdog_callback(int channel_id, void *user_data) {
  LOG_ERR("=================================================");
  LOG_ERR(" WATCHDOG FIRED! Channel %d failed to feed timer!", channel_id);
  LOG_ERR(" Consumer Thread blocked/stuck. Executing safety reset...");
  LOG_ERR("=================================================");
}

static void producer_thread_fn(void *p1, void *p2, void *p3) {
  struct sensor_msg sample = {.seq = 0, .temperature = 240};

  LOG_INF("[PRODUCER] Starting finite run (%d samples)...", MAX_SAMPLES);

  for (uint32_t i = 0; i < MAX_SAMPLES; i++) {
    sample.seq = i + 1;
    sample.temperature += (sample.seq % 2 == 0) ? 1 : -1;

    int ret = k_msgq_put(&sensor_msgq, &sample, K_NO_WAIT);
    if (ret != 0) {
      LOG_WRN("[PRODUCER] Queue FULL! Unable to enqueue Sample %u", sample.seq);
    } else {
      LOG_INF("[PRODUCER] Enqueued Sample %u", sample.seq);
    }

    k_msleep(100); /* Produce every 100 ms */
  }

  LOG_INF("[PRODUCER] Finished enqueuing all %d samples. Thread exiting.",
          MAX_SAMPLES);
}

K_THREAD_DEFINE(producer_tid, 1024, producer_thread_fn, NULL, NULL, NULL, 5, 0,
                0);

static void consumer_thread_fn(void *p1, void *p2, void *p3) {
  struct sensor_msg msg;

  consumer_wdt_chan = task_wdt_add(1000, watchdog_callback, NULL);
  if (consumer_wdt_chan < 0) {
    LOG_ERR("Failed to install task watchdog channel: %d", consumer_wdt_chan);
    return;
  }

  LOG_INF("[CONSUMER] Installed Task Watchdog channel %d (1000 ms timeout)",
          consumer_wdt_chan);

  for (uint32_t processed = 0; processed < MAX_SAMPLES; processed++) {
    k_msgq_get(&sensor_msgq, &msg, K_FOREVER);
    LOG_INF("  --> [CONSUMER] Processed Sample %u", msg.seq);

    if (msg.seq >= STUCK_THRESHOLD_SAMPLE) {
      LOG_ERR("!!! [CONSUMER] Simulating STUCK THREAD (Entering 5s delay "
              "without feeding WDT) !!!");
      k_msleep(5000); /* Long delay > 1000ms watchdog timeout */
    }

    task_wdt_feed(consumer_wdt_chan);
  }

  LOG_INF("[CONSUMER] Thread completed all %d samples. Exiting.", MAX_SAMPLES);
}

K_THREAD_DEFINE(consumer_tid, 1024, consumer_thread_fn, NULL, NULL, NULL, 5, 0,
                0);

static void health_check_thread_fn(void *p1, void *p2, void *p3) {
  const uint32_t warn_capacity =
      (MSG_QUEUE_CAPACITY * 75) / 100; /* 75% = 12 items */
  const uint32_t max_checks = 20;

  for (uint32_t check = 0; check < max_checks; check++) {
    uint32_t used_count = k_msgq_num_used_get(&sensor_msgq);

    if (used_count >= warn_capacity) {
      LOG_WRN("[HEALTH MONITOR] WARNING: Queue capacity at %u/%d items (>= "
              "75%% fill level)!",
              used_count, MSG_QUEUE_CAPACITY);
    } else {
      LOG_INF("[HEALTH MONITOR] Queue level normal: %u/%d items", used_count,
              MSG_QUEUE_CAPACITY);
    }

    k_msleep(250); /* Health-check period */
  }

  LOG_INF(
      "[HEALTH MONITOR] Health monitoring period completed. Thread exiting.");
}

K_THREAD_DEFINE(health_check_tid, 1024, health_check_thread_fn, NULL, NULL,
                NULL, 7, 0, 0);

int main(void) {
  LOG_INF("=== L5 Homework - Reliablity Under Pressure ===");

  int ret = task_wdt_init(NULL);
  if (ret != 0) {
    LOG_ERR("[MAIN] Task Watchdog initialization failed: %d", ret);
  } else {
    LOG_INF("[MAIN] Watchdog Initialized");
  }

  return 0;
}
