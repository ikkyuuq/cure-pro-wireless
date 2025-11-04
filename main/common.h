#ifndef COMMON_H

#define COMMON_H
#include "sdkconfig.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_hid_common.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"

#include "led_strip.h"
#include "led_strip_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "nvs_flash.h"

#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_task_wdt.h"

#endif // COMMON_H
