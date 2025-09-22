#include "kb_matrix.h"
#include "config.h"
#include "hid_gatt_svr_svc.h"
#include "keymap.h"

const gpio_num_t row_pins[MATRIX_ROW] = ROW_PINS;
const gpio_num_t col_pins[MATRIX_COL] = COL_PINS;

static const char *TAG = "MATRIX";
static matrix_state_t matrix_state;
static key_processor_t key_processor;
static hid_keyboard_report_t current_report = {0};

esp_err_t matrix_init(void) {
  esp_err_t ret = ESP_OK;

  ESP_LOGI(TAG, "Initializing matrix with %d rows and %d cols", MATRIX_ROW, MATRIX_COL);

  for (int i = 0; i < MATRIX_ROW; i++) {
    ESP_LOGI(TAG, "Setting up row %d on GPIO %d", i, row_pins[i]);
    gpio_config_t row_config = {.pin_bit_mask = (1ULL << row_pins[i]),
                                .mode = GPIO_MODE_OUTPUT,
                                .intr_type = GPIO_INTR_DISABLE,
                                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                .pull_up_en = GPIO_PULLUP_ENABLE};

    ret |= gpio_config(&row_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "failed to setup gpio config for rows");
      return ret;
    }
    gpio_set_level(row_pins[i], 1); // Default high
    gpio_set_drive_capability(row_pins[i], GPIO_DRIVE_CAP_3);
  }

  for (int i = 0; i < MATRIX_COL; i++) {
    ESP_LOGI(TAG, "Setting up col %d on GPIO %d", i, col_pins[i]);
    gpio_config_t col_config = {
        .pin_bit_mask = (1ULL << col_pins[i]),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ret |= gpio_config(&col_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "failed to setup gpio config for rows");
      return ret;
    }
  }

  // Initialize matrix state and key processor
  memset(&matrix_state, 0, sizeof(matrix_state_t));
  memset(&key_processor, 0, sizeof(key_processor_t));
  return ret;
}

void matrix_set_row(uint8_t row, bool state) {
  if (row < MATRIX_ROW) {
    gpio_set_level(row_pins[row], state ? 1 : 0);
  }
}

bool matrix_read_col(uint8_t col) {
  if (col < MATRIX_COL) {
    return !gpio_get_level(col_pins[col]); // Inverted due to pull-up
  }
  return false;
}

bool matrix_scan(key_event_t *event, uint8_t *event_count) {
  bool detected_changes = false;
  *event_count = 0;
  uint32_t current_time = esp_timer_get_time() / 1000;

  for (uint8_t row = 0; row < MATRIX_ROW; row++) {
    // Set the current row low, all others high
    for (uint8_t r = 0; r < MATRIX_ROW; r++) {
      matrix_set_row(r, r != row);
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    for (uint8_t col = 0; col < MATRIX_COL; col++) {
      bool current_state = matrix_read_col(col);

      if (current_state != matrix_state.raw_state[row][col]) {
        matrix_state.raw_state[row][col] = current_state;
        matrix_state.debounce_time[row][col] = current_time;
      }

      bool debounce_elapsed = (current_time - matrix_state.debounce_time[row][col]) >= DEBOUNCE_TIME_MS;
      if (debounce_elapsed) {
        if (matrix_state.current_state[row][col] != matrix_state.raw_state[row][col]) {
          matrix_state.previous_state[row][col] = matrix_state.current_state[row][col];
          matrix_state.current_state[row][col] = matrix_state.raw_state[row][col];

          if (*event_count < MAX_KEYS) {
            event[*event_count].col = col;
            event[*event_count].row = row;
            event[*event_count].pressed = current_state;
            event[*event_count].timestamp = current_time;
            (*event_count)++;
            detected_changes = true;

            ESP_LOGI(TAG, "Key %s at [%d:%d]", current_state ? "pressed" : "released", row, col);
          }
        }
      }

      vTaskDelay(pdMS_TO_TICKS(1));
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  // Set all rows high when done scanning
  for (uint8_t r = 0; r < MATRIX_ROW; r++) {
    matrix_set_row(r, true);
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  return detected_changes;
}

// Function to send HID report
static void send_hid_report(void) {
  if (hid_dev) {
    esp_hidd_dev_input_set(hid_dev, 0, 1, (uint8_t *)&current_report,
                           sizeof(hid_keyboard_report_t));
    ESP_LOGD(TAG,
             "HID Report: mod=0x%02x "
             "keys=[0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x]",
             current_report.modifiers, current_report.keys[0],
             current_report.keys[1], current_report.keys[2],
             current_report.keys[3], current_report.keys[4],
             current_report.keys[5]);
  }
}

// Function to add a key to the HID report
static bool add_key_to_report(uint8_t keycode) {
  for (int i = 0; i < 6; i++) {
    if (current_report.keys[i] == 0) {
      current_report.keys[i] = keycode;
      return true;
    }
  }
  ESP_LOGW(TAG, "HID report full, cannot add key 0x%02x", keycode);
  return false; // Report is full
}

// Function to remove a key from the HID report
static void remove_key_from_report(uint8_t keycode) {
  for (int i = 0; i < 6; i++) {
    if (current_report.keys[i] == keycode) {
      // Shift remaining keys down
      for (int j = i; j < 5; j++) {
        current_report.keys[j] = current_report.keys[j + 1];
      }
      current_report.keys[5] = 0;
      ESP_LOGI(TAG, "Removed key 0x%02x from HID report", keycode);
      break;
    }
  }
}

// Function to get the active layer
static uint8_t get_active_layer(void) {
  // Check for momentary layers first (highest priority)
  for (int i = MAX_LAYERS - 1; i > 0; i--) {
    if (key_processor.layer_momentary_active[i]) {
      return i;
    }
  }
  return key_processor.current_layer;
}

// Function to handle different key types
static void process_key_press(key_definition_t key, uint8_t row, uint8_t col, uint32_t timestamp) {
  ESP_LOGD(TAG, "Processing key press at [%d:%d], type=%d", row, col, key.type);

  switch (key.type) {
  case KEY_TYPE_NORMAL:
    add_key_to_report(key.keycode);
    break;

  case KEY_TYPE_MODIFIER:
    current_report.modifiers |= key.modifier;
    break;

  case KEY_TYPE_SHIFTED:
    current_report.modifiers |= HID_MOD_LEFT_SHIFT;
    add_key_to_report(key.keycode);
    break;

  case KEY_TYPE_LAYER_TAP:
    // Start timer for layer tap
    key_processor.layer_tap_timer[row][col] = timestamp;
    key_processor.key_is_tapped[row][col] = false;
    break;

  case KEY_TYPE_MOD_TAP:
    // Start timer for mod tap
    key_processor.mod_tap_timer[row][col] = timestamp;
    key_processor.key_is_tapped[row][col] = false;
    break;

  case KEY_TYPE_LAYER_MOMENTARY:
    key_processor.layer_momentary_active[key.layer] = true;
    ESP_LOGI(TAG, "Layer %d momentary activated", key.layer);
    break;

  case KEY_TYPE_LAYER_TOGGLE:
    key_processor.current_layer = (key_processor.current_layer == key.layer) ? 0 : key.layer;
    ESP_LOGI(TAG, "Layer toggled to %d", key_processor.current_layer);
    break;

  case KEY_TYPE_CONSUMER:
    ESP_LOGI(TAG, "Consumer key: 0x%04x", key.consumer);
    break;

  case KEY_TYPE_TRANSPARENT:
    for (int layer = get_active_layer() - 1; layer >= 0; layer--) {
      key_definition_t lower_key = keymap_get_key(layer, row, col);
      if (lower_key.type != KEY_TYPE_TRANSPARENT) {
        process_key_press(lower_key, row, col, timestamp);
        break;
      }
    }
    break;

  default:
    ESP_LOGW(TAG, "Unknown key type: %d", key.type);
    break;
  }

  // Store the pressed key for release processing
  key_processor.pressed_keys[row][col] = key;
  key_processor.pressed_key_active[row][col] = true;
}

static void process_key_release(key_definition_t key, uint8_t row, uint8_t col, uint32_t timestamp) {
  ESP_LOGD(TAG, "Processing key release at [%d:%d], type=%d", row, col,
           key.type);

  switch (key.type) {
  case KEY_TYPE_NORMAL:
    remove_key_from_report(key.keycode);
    break;

  case KEY_TYPE_MODIFIER:
    current_report.modifiers &= ~key.modifier;
    break;

  case KEY_TYPE_SHIFTED:
    current_report.modifiers &= ~HID_MOD_LEFT_SHIFT;
    remove_key_from_report(key.keycode);
    break;

  case KEY_TYPE_LAYER_TAP: {
    uint32_t hold_time = timestamp - key_processor.layer_tap_timer[row][col];
    if (hold_time < TAP_TIMEOUT_MS && !key_processor.key_is_tapped[row][col]) {
      // It was a tap - send the tap key
      add_key_to_report(key.layer_tap.tap_key);
      send_hid_report();
      vTaskDelay(pdMS_TO_TICKS(50));
      remove_key_from_report(key.layer_tap.tap_key);
      ESP_LOGI(TAG, "Layer tap key sent: 0x%02x", key.layer_tap.tap_key);
    } else {
      // It was a hold - deactivate layer
      key_processor.layer_momentary_active[key.layer_tap.layer] = false;
      ESP_LOGI(TAG, "Layer %d deactivated", key.layer_tap.layer);
    }
    break;
  }

  case KEY_TYPE_MOD_TAP: {
    uint32_t hold_time = timestamp - key_processor.mod_tap_timer[row][col];
    if (hold_time < TAP_TIMEOUT_MS && !key_processor.key_is_tapped[row][col]) {
      // It was a tap - send the tap key
      add_key_to_report(key.mod_tap.tap_key);
      send_hid_report();
      vTaskDelay(pdMS_TO_TICKS(50)); // Brief press
      remove_key_from_report(key.mod_tap.tap_key);
      ESP_LOGI(TAG, "Mod tap key sent: 0x%02x", key.mod_tap.tap_key);
    } else {
      // It was a hold - remove modifier
      current_report.modifiers &= ~key.mod_tap.hold_key;
      ESP_LOGI(TAG, "Modifier 0x%02x released", key.mod_tap.hold_key);
    }
    break;
  }

  case KEY_TYPE_LAYER_MOMENTARY:
    key_processor.layer_momentary_active[key.layer] = false;
    ESP_LOGI(TAG, "Layer %d momentary deactivated", key.layer);
    break;

  case KEY_TYPE_TRANSPARENT:
    for (int layer = get_active_layer() - 1; layer >= 0; layer--) {
      key_definition_t lower_key = keymap_get_key(layer, row, col);
      if (lower_key.type != KEY_TYPE_TRANSPARENT) {
        process_key_release(lower_key, row, col, timestamp);
        break;
      }
    }
    break;

  default:
    break;
  }

  // Clear the stored key
  memset(&key_processor.pressed_keys[row][col], 0, sizeof(key_definition_t));
  key_processor.pressed_key_active[row][col] = false;
}

// Function to check for tap timeouts
static void check_tap_timeouts(uint32_t current_time) {
  for (uint8_t row = 0; row < MATRIX_ROW; row++) {
    for (uint8_t col = 0; col < MATRIX_COL; col++) {
      key_definition_t key = key_processor.pressed_keys[row][col];

      switch (key.type) {
        case KEY_TYPE_LAYER_TAP: 
          bool layer_tap_timer_elapsed = (current_time - key_processor.layer_tap_timer[row][col]) >= TAP_TIMEOUT_MS;
          if (layer_tap_timer_elapsed && !key_processor.key_is_tapped[row][col]) {
              key_processor.layer_momentary_active[key.layer_tap.layer] = true;
              key_processor.key_is_tapped[row][col] = true;
              ESP_LOGI(TAG, "Layer tap timeout - activating layer %d", key.layer_tap.layer);
          }
          break;
        case KEY_TYPE_MOD_TAP:
          bool mod_tap_timer_elapsed = (current_time - key_processor.mod_tap_timer[row][col]) >= TAP_TIMEOUT_MS;
          if (mod_tap_timer_elapsed && !key_processor.key_is_tapped[row][col]) {
            current_report.modifiers |= key.mod_tap.hold_key;
            key_processor.key_is_tapped[row][col] = true;
            ESP_LOGI(TAG, "Mod tap timeout - activating modifier 0x%02x", key.mod_tap.hold_key);
          }
          break;
        default:
          break;
      }

    }
  }
}

void hid_process_key_event(key_event_t *events, uint8_t *event_count) {
  uint32_t current_time = esp_timer_get_time() / 1000;

  check_tap_timeouts(current_time);

  for (int i = 0; i < *event_count; i++) {
    uint8_t active_layer = get_active_layer();
    key_definition_t key = keymap_get_key(active_layer, events[i].row, events[i].col);

    ESP_LOGI(TAG, "Key [%d:%d] %s -> L%d", events[i].row, events[i].col,
             events[i].pressed ? "PRESS" : "REL", active_layer);

    if (events[i].pressed) {
      process_key_press(key, events[i].row, events[i].col, current_time);
    } else {
      // Use the stored key from when it was pressed
      if (key_processor.pressed_key_active[events[i].row][events[i].col]) {
        key_definition_t stored_key = key_processor.pressed_keys[events[i].row][events[i].col];
        ESP_LOGI(TAG, "Processing key release for stored key type %d", stored_key.type);
        process_key_release(stored_key, events[i].row, events[i].col, current_time);
      } else {
        ESP_LOGW(TAG, "No stored key found for release at [%d:%d]", events[i].row, events[i].col);
      }
    }
  }

  send_hid_report();
}

void matrix_scan_task(void *pvParameters) {
  key_event_t events[MAX_KEYS];
  uint8_t event_count;

  while (1) {
    if (matrix_scan(events, &event_count)) {
      ESP_LOGI(TAG, "*** KEY EVENT DETECTED: %d events ***", event_count);
      hid_process_key_event(events, &event_count);
    }

    vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
  }
}
