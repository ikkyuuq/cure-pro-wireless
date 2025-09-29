#include "kb_matrix.h"
#include "config.h"
#include "freertos/projdefs.h"
#include "kb_mgt.h"

TaskHandle_t matrix_task_hdl = NULL;

const gpio_num_t row_pins[MATRIX_ROW] = ROW_PINS;
const gpio_num_t col_pins[MATRIX_COL] = COL_PINS;

static const char *TAG = "MATRIX";
static matrix_state_t matrix_state;

esp_err_t matrix_init(void) {
  esp_err_t ret = ESP_OK;

  for (int i = 0; i < MATRIX_ROW; i++) {
    gpio_config_t row_config = {
      .pin_bit_mask = (1ULL << row_pins[i]),
      .mode = GPIO_MODE_OUTPUT,
      .intr_type = GPIO_INTR_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_ENABLE
    };

    ret |= gpio_config(&row_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "failed to setup gpio config for rows");
      return ret;
    }
    gpio_set_level(row_pins[i], 1); // Default high
    gpio_set_drive_capability(row_pins[i], GPIO_DRIVE_CAP_3);
  }

  for (int i = 0; i < MATRIX_COL; i++) {
    gpio_config_t col_config = {
        .pin_bit_mask = (1ULL << col_pins[i]),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ret |= gpio_config(&col_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "failed to setup gpio config for cols");
      return ret;
    }
  }

  // Initialize matrix state and keyboard management
  memset(&matrix_state, 0, sizeof(matrix_state_t));
  ret |= kb_mgt_init();

  return ret;
}

void matrix_set_row(uint8_t row, bool state) {
  if (row < MATRIX_ROW) gpio_set_level(row_pins[row], state ? 1 : 0);
}

bool matrix_read_col(uint8_t col) {
  return col < MATRIX_COL ? !gpio_get_level(col_pins[col]) : false; // Inverted due to pull-up
}

void reset_and_track_key_state(bool key_state, uint8_t row, uint8_t col, uint32_t timestamp) {
  matrix_state.raw_state[row][col] = key_state;
  matrix_state.debounce_time[row][col] = timestamp;
}

bool matrix_scan(key_event_t *event, uint8_t *event_count) {
  *event_count = 0;
  bool detected_changes = false;
  uint32_t current_time = esp_timer_get_time() / 1000;

  for (uint8_t row = 0; row < MATRIX_ROW; row++) {
    // Set the current row low, all others high
    for (uint8_t r = 0; r < MATRIX_ROW; r++) { matrix_set_row(r, r != row); }

    esp_rom_delay_us(GPIO_SETTLE_US);

    for (uint8_t col = 0; col < MATRIX_COL; col++) {
      bool key_state = matrix_read_col(col);
      bool raw_state = matrix_state.raw_state[row][col];
      bool current_state = matrix_state.current_state[row][col];

      if (key_state != raw_state) reset_and_track_key_state(key_state, row, col, current_time);
      
      bool debounce_elapsed = (current_time - matrix_state.debounce_time[row][col]) >= DEBOUNCE_TIME_MS;
      bool state_actually_changes = debounce_elapsed && (current_state != raw_state);
      if (state_actually_changes) {
        matrix_state.previous_state[row][col] = current_state;
        matrix_state.current_state[row][col] = raw_state;

        if (*event_count < MAX_KEYS) {
          event[*event_count].col = col;
          event[*event_count].row = row;
          event[*event_count].pressed = raw_state;
          event[*event_count].timestamp = current_time;

          (*event_count)++;
          detected_changes = true;

          ESP_LOGI(TAG, "Key %s at [%d:%d] -> %s",
                   raw_state ? "pressed" : "released",
                   row, col,
                   keymap_key_to_string(keymap_get_key(kb_mgt_layer_get_active(), row, col)));
          }
      }

      esp_rom_delay_us(GPIO_SETTLE_US);
    }

    esp_rom_delay_us(ROW_DELAY_US);
  }

  // Set all rows high when done scanning
  for (uint8_t r = 0; r < MATRIX_ROW; r++) {
    matrix_set_row(r, true);
  }

  vTaskDelay(1);

  return detected_changes;
}

void process_key_event(key_event_t *events, uint8_t *event_count) {
  uint32_t current_time = esp_timer_get_time() / 1000;

  kb_mgt_processor_check_tap_timeouts(current_time);

  for (int i = 0; i < *event_count; i++) {
    key_definition_t key = keymap_get_key(kb_mgt_layer_get_active(), events[i].row, events[i].col);
    bool pressed = events[i].pressed;

    kb_mgt_process_key_event(key, events[i].row, events[i].col, pressed, current_time);
  }

  kb_mgt_finalize_processing();
}

void matrix_scan_start(void) {
  if (!matrix_task_hdl) {
    xTaskCreate(matrix_scan_task, "matrix_scan", MATRIX_TASK_STACK_SIZE, NULL, MATRIX_SCAN_PRIORITY, &matrix_task_hdl);
  }
}

void matrix_scan_stop(void) {
  if (matrix_task_hdl) {
    vTaskDelete(matrix_task_hdl);
    matrix_task_hdl = NULL;
  }
}

void matrix_scan_task(void *pvParameters) {
  key_event_t events[MAX_KEYS];
  uint8_t event_count;

  while (1) {
    if (matrix_scan(events, &event_count)) {
      ESP_LOGD(TAG, "*** KEY EVENT DETECTED: %d events ***", event_count);
      process_key_event(events, &event_count);
    }

    uint32_t current_time = esp_timer_get_time() / 1000;
    kb_mgt_processor_check_tap_timeouts(current_time);

    vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
  }
}
