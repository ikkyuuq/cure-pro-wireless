#ifndef POWER_H
#define POWER_H

#include "common.h"

#define BATTERY_READ_INTERVAL_MS 10000 // Read battery every 10 seconds

typedef struct {
  bool      usb_powered;
  bool      voltage_charging;
  uint16_t  battery_voltage_mv;
} power_state_t;

esp_err_t usb_power_init(void);
void power_task_start(void);

#endif
