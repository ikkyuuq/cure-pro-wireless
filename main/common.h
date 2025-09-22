#ifndef COMMON_H

#define COMMON_H
#include "sdkconfig.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_hid_common.h"
#include "esp_hidd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "nvs_flash.h"
#include "driver/gpio.h"

#endif  // COMMON_H
