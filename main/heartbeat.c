#include "heartbeat.h"
#include "config.h"
#include "indicator.h"

static const char *TAG = "HEARTBEAT";

TaskHandle_t heartbeat_task_hdl = NULL;

heartbeat_state_t heartbeat_state = {0};

void heartbeat_start(void) {
  if (!heartbeat_task_hdl) {
    xTaskCreate(heartbeat_task, "heartbeat_task", HEARTBEAT_TASK_STACK_SIZE, NULL, HEARTBEAT_PRIORITY, &heartbeat_task_hdl);
  }
}

void heartbeat_stop(void) {
  if (heartbeat_task_hdl) {
    vTaskDelete(heartbeat_task_hdl);
    heartbeat_task_hdl = NULL;
  }
}

void heartbeat_task(void *pvParameters) {

  ESP_LOGI(TAG, "Heartbeat task started");

  while (1) {
    uint32_t current_time = esp_timer_get_time() / 1000;

    if (current_time - heartbeat_state.last_req_time >= HEARTBEAT_INTERVAL_MS) {
      heartbeat_state.received = false;
      heartbeat_state.last_req_time = current_time;
      send_to_espnow(SLAVE, REQ_HEARTBEAT, NULL);
    }

    if (heartbeat_state.received) {
      if (current_conn_state != CONN_STATE_CONNECTED) {
        indicator_set_conn_state(CONN_STATE_CONNECTED);
      }
    } else {
      // Check if it sent a heartbeat request recently
      if (heartbeat_state.last_req_time > 0) {
        uint32_t time_since_req = current_time - heartbeat_state.last_req_time;

        // Immediate waiting state if no response yet (after 1 second wait for stable tranmission) and still connected
        if (time_since_req > HEARTBEAT_STABLE_MS && current_conn_state == CONN_STATE_CONNECTED) {
          indicator_set_conn_state(CONN_STATE_WAITING);
          ESP_LOGI(TAG, "Master not responding - slave should show blue blinking");
        }

        // After 10 seconds of no response, go to sleep mode
        if (time_since_req > (HEARTBEAT_TIMEOUT_MS + HEARTBEAT_STABLE_MS) && current_conn_state == CONN_STATE_WAITING) {
          indicator_set_conn_state(CONN_STATE_SLEEPING);
          ESP_LOGI(TAG, "Master not responding - slave should show off");
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void update_heartbeat(void) {
  heartbeat_state.received = true;
  heartbeat_state.last_req_time = esp_timer_get_time() / 1000;
}
