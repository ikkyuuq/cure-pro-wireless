#ifndef BATTERY_H
#define BATTERY_H

#include "ble_gap.h"
#include "common.h"

#define BATTERY_READ_INTERVAL_MS                                               \
  30000 // Read battery every 30 seconds (optimized for power)

typedef struct
{
  bool     usb_powered;
  bool     voltage_charging;
  uint16_t battery_voltage_mv;
} battery_power_state_t;

extern battery_power_state_t power_state;

esp_err_t usb_power_init(void);
void      power_task_start(void);

#endif
