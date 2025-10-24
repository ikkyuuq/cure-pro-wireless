/**
 * @file heartbeat.c
 * @brief Heartbeat Monitor for Split Keyboard
 *
 * Monitors connection health between master and slave keyboard halves.
 * Sends periodic heartbeat requests and tracks responses to detect
 * disconnections.
 *
 * Key responsibilities:
 * - Periodic heartbeat request transmission (slave to master)
 * - Response tracking and timeout detection
 * - Connection state management based on heartbeat health
 * - Indicator state updates for connection status
 */

#include "heartbeat.h"
#include "config.h"
#include "freertos/projdefs.h"
#include "indicator.h"
#include "utils.h"

static const char *TAG = "HEARTBEAT";

// =============================================================================
// STATE VARIABLES
// =============================================================================

static TaskHandle_t      task_hdl = NULL;
static heartbeat_state_t state = {.received = false,
                                            .last_req_time = 0};

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void task(void *pvParameters);

// =============================================================================
// PUBLIC API - TASK CONTROL
// =============================================================================

void heartbeat_start(void)
{
  task_hdl_init(&task_hdl, task, "heartbeat_task", HEARTBEAT_PRIORITY,
                HEARTBEAT_TASK_STACK_SIZE, NULL);
  ESP_LOGI(TAG, "Heartbeat monitoring started");
}

void heartbeat_stop(void)
{
  task_hdl_cleanup(task_hdl);
  ESP_LOGI(TAG, "Heartbeat monitoring stopped");
}

void update_heartbeat(void)
{
  state.received = true;
  state.last_req_time = get_current_time_ms();
  ESP_LOGD(TAG, "Heartbeat response received");
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - HEARTBEAT TASK
// =============================================================================

static void task(void *pvParameters)
{
  ESP_LOGI(TAG, "Heartbeat task started");

  while (1)
  {
    // Send periodic heartbeat requests
    if (get_current_time_ms() - state.last_req_time >=
        HEARTBEAT_INTERVAL_MS)
    {
      state.received = false;
      state.last_req_time = get_current_time_ms();
      send_to_espnow(SLAVE, REQ_HEARTBEAT, NULL);
      ESP_LOGD(TAG, "Heartbeat request sent");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to take heartbeat mutex");
    }

    // Monitor heartbeat response status
    if (state.received)
    {
      // Heartbeat response received - maintain connected state
      if (indicator_get_conn_state() != CONN_STATE_CONNECTED)
      {
        indicator_set_conn_state(CONN_STATE_CONNECTED);
      }
    }
    else
    {
      // No heartbeat response - check timeout conditions
      if (state.last_req_time > 0)
      {
        uint32_t time_since_req =
            get_current_time_ms() - state.last_req_time;

        // Transition to waiting state after stable transmission period
        if (time_since_req > HEARTBEAT_STABLE_MS &&
            indicator_get_conn_state() == CONN_STATE_CONNECTED)
        {
          indicator_set_conn_state(CONN_STATE_WAITING);
          ESP_LOGI(TAG, "Master not responding - entering waiting state");
        }

        // Transition to sleeping state after full timeout
        if (time_since_req > (HEARTBEAT_TIMEOUT_MS + HEARTBEAT_STABLE_MS) &&
            indicator_get_conn_state() == CONN_STATE_WAITING)
        {
          indicator_set_conn_state(CONN_STATE_SLEEPING);
          ESP_LOGI(TAG, "Master timeout - entering sleep state");
          // TODO: Implement deep sleep mode
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
