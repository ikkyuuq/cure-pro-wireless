#ifndef INDICATOR_H

#define INDICATOR_H

#include "common.h"
#include "config.h"

// LED Connection States
typedef enum
{
  CONN_STATE_CONNECTED, // Green - BLE connected (master) or ESP-NOW active
  // (slave)
  CONN_STATE_WAITING, // Blue blinking - Waiting for connection/heartbeat
  // (disconnected)
  CONN_STATE_SLEEPING // Off - Power saving mode
} conn_state_t;

// LED Battery States
typedef enum
{
  BATT_STATE_GOOD,     // Green - Battery > LOW_BATTERY_THRESHOLD_MV
  BATT_STATE_LOW,      // Yellow - Battery between CRITICAL and LOW threshold
  BATT_STATE_CRITICAL, // Red blinking - Battery < CRITICAL_BATTERY_THRESHOLD_MV
  BATT_STATE_CHARGING  // Blue - Charging (if applicable)
} batt_state_t;

typedef struct
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} color_t;

// Timing constants
#define BLINK_INTERVAL_MS 500 // 0.5 second blink interval

// Color definitions
#define COLOR_OFF    {0, 0, 0}
#define COLOR_RED    {255, 0, 0}
#define COLOR_GREEN  {0, 255, 0}
#define COLOR_BLUE   {0, 0, 255}
#define COLOR_YELLOW {255, 255, 0}
#define COLOR_ORANGE                                                           \
  {255, 165, 0} // Orange for low battery (distinct from power saving)
#define COLOR_PURPLE     {128, 0, 128}
#define COLOR_CYAN       {0, 255, 255}
#define COLOR_MAGENTA    {255, 0, 255} // Magenta for active mode
#define COLOR_WHITE      {255, 255, 255}
#define COLOR_DIM_BLUE   {0, 0, 128}
#define COLOR_DIM_GREEN  {0, 128, 0}
#define COLOR_DIM_YELLOW {64, 64, 0} // Dim yellow for power saving (distinct)

// LED Power States
typedef enum
{
  POWER_STATE_ACTIVE,    // Green/Purple - Active typing mode
  POWER_STATE_NORMAL,    // Blue - Normal mode
  POWER_STATE_EFFICIENT, // Yellow - Power saving mode
  POWER_STATE_DEEP       // Off - Deep power saving mode
} power_state_t;

esp_err_t     indicator_init(void);
conn_state_t  indicator_get_conn_state(void);
batt_state_t  indicator_get_batt_state(void);
power_state_t indicator_get_power_state(void);
void          indicator_set_conn_state(conn_state_t state);
void          indicator_set_batt_state(batt_state_t state);
void          indicator_set_power_state(power_state_t state);

// Power-aware LED combination functions
void indicator_update_combined_state(conn_state_t  conn_state,
                                     batt_state_t  batt_state,
                                     power_state_t power_state);

#endif
