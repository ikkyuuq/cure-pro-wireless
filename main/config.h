#ifndef CONFIG_H

#define CONFIG_H

#include "common.h"

#define IS_MASTER 0

#define MATRIX_ROW 5
#define MATRIX_COL 6
#define MAX_KEYS   (MATRIX_COL * MATRIX_ROW)

#define ROW_PINS                                                               \
  {GPIO_NUM_20, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_15, GPIO_NUM_14}
#define COL_PINS                                                               \
  {GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5}
#define BATT_LED_GPIO GPIO_NUM_7
#define CONN_LED_GPIO GPIO_NUM_8

// Sleep/Wake-up Configuration - Using column pins 0-5 for wake-up
#define WAKEUP_PINS                                                            \
  {GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5}
#define WAKEUP_PINS_COUNT 6

// Ultra Low Latency Configuration
#define DEBOUNCE_TIME_MS   4   // Minimal debounce == Less latecy
#define DEFAULT_TIMEOUT_MS 120 // Optimized for quick typing (reduced from 150ms)
#define SCAN_INTERVAL_MS   1   // aggressive polling == Less latency

// Optimized GPIO timing for speed
#define GPIO_SETTLE_US 5 // Minimal stable GPIO settling
#define ROW_DELAY_US   2 // Minimal row completion delay

// Wireless Configuration
#define ESP_NOW_CHANNEL 1
#define MAX_RETRY_COUNT 3

// Power Management
#define BATT_ADC_CHAN             ADC_CHANNEL_6
#define BATT_ADC_ATTEN            ADC_ATTEN_DB_12
#define BATT_BIT_WIDTH            ADC_BITWIDTH_12
#define BATT_VOLTAGE_DIVIDER      261
#define BATT_VOLTAGE_NOMINAL_MV   3300
#define BATT_VOLTAGE_CRITICAL_MV  3000
#define BATT_VOLTAGE_THRESHOLD_MV 4200

// HID Configuration
#define HID_DEVICE_NAME  "CureProWL"
#define HID_MANUFACTURER "Kppras"

#define MATRIX_TASK_STACK_SIZE    4096 // Matrix scaning task
#define ESPNOW_TASK_STACK_SIZE    4096 // ESPNOW task sending between havles
#define POWER_TASK_STACK_SIZE     2048 // Power task management
#define HEARTBEAT_TASK_STACK_SIZE 2048 // Heartbeat task
#define INDICATOR_TASK_STACK_SIZE 4096 // Power task management

#define MATRIX_SCAN_PRIORITY 7
#define ESPNOW_PRIORITY      4
#define POWER_PRIORITY       3
#define HEARTBEAT_PRIORITY   2 // Heartbeat task
#define INDICATOR_PRIORITY   1 // Connection and battery indicator

// Keyboard Layer Configuration
#define MAX_LAYERS    3
#define DEFAULT_LAYER 0

#if IS_MASTER
#define ESPNOW_PEER_ADDR {0xFC, 0x01, 0x2C, 0xF5, 0xF2, 0x10}
#else
#define ESPNOW_PEER_ADDR {0xFC, 0x01, 0x2C, 0xF5, 0xEE, 0x8C}
#endif

#define ESP_WIFI_CHANNEL   1
#define ESP_NOW_CHANNEL    1
#define ESP_NOW_QUEUE_SIZE 6

#endif // CONFIG_H
