#include "sensor_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define MAX_SAMPLES 10

static void fast_display_cb(const struct zbus_channel *chan) {
  const struct sensor_msg *msg = zbus_chan_const_msg(chan);

  LOG_INF("[DISPLAY - FAST] Seq: %u | Temp: %d.%d C", msg->seq,
          msg->temperature / 10, msg->temperature % 10);
}

ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_msg, NULL, NULL,
                 ZBUS_OBSERVERS(fast_display_listener, slow_logger_sub),
                 ZBUS_MSG_INIT(.seq = 0, .temperature = 0));

ZBUS_LISTENER_DEFINE(fast_display_listener, fast_display_cb);
ZBUS_SUBSCRIBER_DEFINE(slow_logger_sub, 10);

static void publisher_thread_fn(void *p1, void *p2, void *p3) {
  struct sensor_msg sample = {
      .seq = 0, .temperature = 240 /* 24.0 °C */
  };

  LOG_INF("Starting publisher stream (%d samples total)...", MAX_SAMPLES);

  for (uint32_t i = 0; i < MAX_SAMPLES; i++) {
    sample.seq = i + 1;
    sample.temperature += (sample.seq % 2 == 0) ? 1 : -1;

    int ret = zbus_chan_pub(&sensor_chan, &sample, K_MSEC(10));
    if (ret != 0) {
      LOG_WRN("Failed to publish sample %u (err: %d)", sample.seq, ret);
    }

    k_msleep(100);
  }

  LOG_INF("Publisher thread completed all %d samples.", MAX_SAMPLES);
}

static void slow_logger_thread_fn(void *p1, void *p2, void *p3) {
  const struct zbus_channel *chan;
  struct sensor_msg msg;

  while (1) {
    if (zbus_sub_wait(&slow_logger_sub, &chan, K_FOREVER) == 0) {
      if (chan == &sensor_chan) {
        zbus_chan_read(chan, &msg, K_NO_WAIT);

        LOG_INF("[LOGGER - SLOW] Logging Seq %u to storage...", msg.seq);

        k_msleep(300);
      }
    }
  }
}

K_THREAD_DEFINE(publisher_tid, 1024, publisher_thread_fn, NULL, NULL, NULL, 5,
                0, 0);
K_THREAD_DEFINE(slow_logger_tid, 1024, slow_logger_thread_fn, NULL, NULL, NULL,
                5, 0, 0);

int main(void) {
  LOG_INF("=== L4 Homework - Event Driven System with Zbus");
  return 0;
}
