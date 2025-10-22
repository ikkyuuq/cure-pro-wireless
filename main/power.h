#ifndef POWER_H
#define POWER_H

#include "common.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define BATTERY_READ_INTERVAL_MS 10000 // Read battery every 10 seconds

typedef struct {
  bool      usb_powered;
  bool      voltage_charging;
  uint16_t  battery_voltage_mv;
} power_state_t;

extern power_state_t power_state;

esp_err_t usb_power_init(void);
uint32_t read_battery_voltage(void);

void power_task_start(void);
void power_task_stop(void);
void power_task(void *pvParameters);

#endif
