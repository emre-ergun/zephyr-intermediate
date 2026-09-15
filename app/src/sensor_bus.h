#ifndef SENSOR_BUS_H_
#define SENSOR_BUS_H_

#include <zephyr/zbus/zbus.h>

struct sensor_msg {
  uint32_t seq;
  int16_t temperature;
};

ZBUS_CHAN_DECLARE(sensor_chan);

#endif /* SENSOR_BUS_H_ */
