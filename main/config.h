#ifndef CONFIG_H

#define CONFIG_H

#include "common.h"

#define IS_MASTER     1

#define MATRIX_ROW    5
#define MATRIX_COL    6
#define MAX_KEYS      (MATRIX_COL * MATRIX_ROW)

#define ROW_PINS      {GPIO_NUM_20, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_15, GPIO_NUM_14}
#define COL_PINS      {GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5}

// Timing Configuration
#define DEBOUNCE_TIME_MS    5
#define TAP_TIMEOUT_MS      200
#define SCAN_INTERVAL_MS    10
#define IDLE_TIMEOUT_MS     30000

// Wireless Configuration
#define ESP_NOW_CHANNEL     1
#define MAX_RETRY_COUNT     3

// Power Management
#define BATTERY_ADC_CHANNEL           GPIO_NUM_6
#define LOW_BATTERY_THRESHOLD_MV      3300
#define CRITICAL_BATTERY_THRESHOLD_MV 3000

// HID Configuration
#define HID_DEVICE_NAME     "CureProWL"
#define HID_MANUFACTURER    "Kppras Keyboards"

#define MATRIX_TASK_STACK_SIZE  2048  // Matrix scaning task
#define HID_TASK_STACK_SIZE     3072  // HID task sending between havles
#define POWER_TASK_STACK_SIZE   1536  // Power task management

#define MATRIX_SCAN_PRIORITY    5
#define HID_PRIORITY            4
#define POWER_PRIORITY          3

// Keyboard Layer Configuration
#define MAX_LAYERS          3
#define DEFAULT_LAYER       0

#endif  // CONFIG_H
