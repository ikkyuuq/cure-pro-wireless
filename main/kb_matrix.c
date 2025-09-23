#include "kb_matrix.h"
#include "config.h"
#include "espnow.h"
#include "handler.h"
#include "keymap.h"

TaskHandle_t matrix_task_hdl = NULL;

const gpio_num_t row_pins[MATRIX_ROW] = ROW_PINS;
const gpio_num_t col_pins[MATRIX_COL] = COL_PINS;

#if DEV
static const char *TAG = "MATRIX";
#endif
static matrix_state_t matrix_state;
static key_processor_t key_processor;
static hid_keyboard_report_t current_report;
void sync_active_layer(uint8_t layer);
void desync_active_layer(uint8_t layer);
void sync_modifier(hid_keyboard_report_t *report);
void desync_modifier(hid_keyboard_report_t *report);

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
#if DEV
      ESP_LOGE(TAG, "failed to setup gpio config for rows");
#endif
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
#if DEV
      ESP_LOGE(TAG, "failed to setup gpio config for cols");
#endif
      return ret;
    }
  }

  // Initialize matrix_state, key_processor, and current_report to an empty
  memset(&matrix_state, 0, sizeof(matrix_state_t));
  memset(&key_processor, 0, sizeof(key_processor_t));
  memset(&current_report, 0, sizeof(hid_keyboard_report_t));

#if DEV
  ESP_LOGI(TAG, "All states initialized to zero - any stuck modifiers cleared");
#endif
  return ret;
}

void matrix_set_row(uint8_t row, bool state) {
  if (row < MATRIX_ROW) gpio_set_level(row_pins[row], state ? 1 : 0);
}

bool matrix_read_col(uint8_t col) {
  return col < MATRIX_COL ? !gpio_get_level(col_pins[col]) : false; // Inverted due to pull-up
}

bool matrix_scan(key_event_t *event, uint8_t *event_count) {
  *event_count = 0;
  bool detected_changes = false;
  uint32_t current_time = esp_timer_get_time() / 1000;

  for (uint8_t row = 0; row < MATRIX_ROW; row++) {
    // Set the current row low, all others high
    for (uint8_t r = 0; r < MATRIX_ROW; r++) { matrix_set_row(r, r != row); }

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

#if DEV
            ESP_LOGI(TAG, "Key %s at [%d:%d] -> %s",
                     current_state ? "pressed" : "released",
                     row, col,
                     keymap_key_to_string(keymap_get_key(get_active_layer(), row, col)));
#endif
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

#if IS_MASTER
// Function to send HID report
void send_hid_report(hid_keyboard_report_t *report) {
  if (hid_dev) {
    esp_hidd_dev_input_set(hid_dev, 0, 1, (uint8_t *)report, sizeof(hid_keyboard_report_t));
  }
}
#endif // IS_MASTER

// Function to add a key to the HID report
static bool add_key_to_report(uint8_t keycode) {
  for (int i = 0; i < 6; i++) {
    if (current_report.keys[i] == 0) {
      current_report.keys[i] = keycode;
      return true;
    }
  }
#if DEV
  ESP_LOGW(TAG, "HID report full, cannot add key 0x%02x", keycode);
#endif
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
#if DEV
      ESP_LOGD(TAG, "Removed key 0x%02x from HID report", keycode);
#endif
      break;
    }
  }
}

// Function to get the active layer
uint8_t get_active_layer(void) {
  // Check for momentary layers first (highest priority)
  for (int i = MAX_LAYERS - 1; i > 0; i--) {
    if (key_processor.layer_momentary_active[i]) {
      return i;
    }
  }
  return key_processor.current_layer;
}

// Function to handle different key types
static void process_key_press(key_definition_t key, uint8_t row, uint8_t col,
                              uint32_t timestamp) {
#if DEV
  ESP_LOGD(TAG, "Processing key press at [%d:%d], type=%d", row, col, key.type);
#endif

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
#if DEV
    ESP_LOGD(TAG, "Layer %d momentary activated", key.layer);
#endif
    break;

  case KEY_TYPE_LAYER_TOGGLE:
    key_processor.current_layer = (key_processor.current_layer == key.layer) ? 0 : key.layer;
#if DEV
    ESP_LOGD(TAG, "Layer toggled to %d", key_processor.current_layer);
#endif
    break;

  case KEY_TYPE_CONSUMER:
#if DEV
    ESP_LOGD(TAG, "Consumer key: 0x%04x", key.consumer);
#endif
    break;

  case KEY_TYPE_TRANSPARENT:
    for (int layer = get_active_layer() - 1; layer >= 0; layer--) {
      ESP_LOGI(TAG, "current layer: %d, must be layer: %d", get_active_layer(), layer);

      key_definition_t lower_key = keymap_get_key(layer, row, col);

      if (lower_key.type != KEY_TYPE_TRANSPARENT) {
        process_key_press(lower_key, row, col, timestamp);
        ESP_LOGI(TAG, "key def: %d", lower_key.type);
        key_processor.pressed_keys[row][col] = lower_key;
        key_processor.pressed_key_active[row][col] = true;
        return;
      }
    }
    break;

  default:
#if DEV
    ESP_LOGW(TAG, "Unknown key type: %d", key.type);
#endif
    break;
  }

  // Store the pressed key for release processing
  key_processor.pressed_keys[row][col] = key;
  key_processor.pressed_key_active[row][col] = true;
}

static void process_key_release(key_definition_t key, uint8_t row, uint8_t col, uint32_t timestamp) {
#if DEV
  ESP_LOGD(TAG, "Processing key release at [%d:%d], type=%d", row, col,
           key.type);
#endif

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
    bool is_tapped = key_processor.key_is_tapped[row][col];

    if (hold_time < TAP_TIMEOUT_MS && !is_tapped) {
      add_key_to_report(key.layer_tap.tap_key);
#if IS_MASTER
      send_hid_report(&current_report);
      remove_key_from_report(key.layer_tap.tap_key);
      send_hid_report(&current_report);
#else
      espnow_requied_data_t data = {0};
      data.report = &current_report;
      send_to_espnow(LEFT_SIDE, BRIEF_TAP, &data);
      remove_key_from_report(key.layer_tap.tap_key);
#endif
#if DEV
      ESP_LOGD(TAG, "Layer tap key sent: 0x%02x", key.layer_tap.tap_key);
#endif
    } else {
      key_processor.layer_momentary_active[key.layer_tap.layer] = false;
      espnow_requied_data_t data = {0};
      data.layer = key.layer_tap.layer;
#if IS_MASTER
      send_to_espnow(RIGHT_SIDE, LAYER_DESYNC, &data);
#else
      send_to_espnow(LEFT_SIDE, LAYER_DESYNC, &data);
#endif
#if DEV
      ESP_LOGD(TAG, "Layer %d deactivated", key.layer_tap.layer);
#endif
    }
    break;
  }

  case KEY_TYPE_MOD_TAP: {
    uint32_t hold_time = timestamp - key_processor.mod_tap_timer[row][col];
    if (hold_time < TAP_TIMEOUT_MS && !key_processor.key_is_tapped[row][col]) {
      add_key_to_report(key.mod_tap.tap_key);
#if IS_MASTER
      send_hid_report(&current_report);
      remove_key_from_report(key.mod_tap.tap_key);
      send_hid_report(&current_report);
#else
      espnow_requied_data_t data = {0};
      data.report = &current_report;
      send_to_espnow(LEFT_SIDE, BRIEF_TAP, &data);
      remove_key_from_report(key.mod_tap.tap_key);
#endif
#if DEV
      ESP_LOGD(TAG, "Mod tap key sent: 0x%02x", key.mod_tap.tap_key);
#endif
    } else {
      espnow_requied_data_t data = {0};
      hid_keyboard_report_t desync_report = {0};
      desync_report.modifiers = key.mod_tap.hold_key;
      data.report = &desync_report;
      current_report.modifiers &= ~key.mod_tap.hold_key;
#if IS_MASTER
      send_to_espnow(RIGHT_SIDE, MOD_DESYNC, &data);
#else
      send_to_espnow(LEFT_SIDE, MOD_DESYNC, &data);
#endif

#if DEV
      ESP_LOGD(TAG, "Modifier 0x%02x released", key.mod_tap.hold_key);
#endif
    }
    break;
  }

  case KEY_TYPE_LAYER_MOMENTARY:
    key_processor.layer_momentary_active[key.layer] = false;
#if DEV
    ESP_LOGD(TAG, "Layer %d momentary deactivated", key.layer);
#endif
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

  // Clear the stored key at that position
  memset(&key_processor.pressed_keys[row][col], 0, sizeof(key_definition_t));
  key_processor.pressed_key_active[row][col] = false;
}

// Function to check for tap timeouts
static void check_tap_timeouts(uint32_t current_time) {
  for (uint8_t row = 0; row < MATRIX_ROW; row++) {
    for (uint8_t col = 0; col < MATRIX_COL; col++) {
      key_definition_t key = key_processor.pressed_keys[row][col];
      bool layer_tap_timer_elapsed = (current_time - key_processor.layer_tap_timer[row][col]) >= TAP_TIMEOUT_MS;
      bool mod_tap_timer_elapsed = (current_time - key_processor.mod_tap_timer[row][col]) >= TAP_TIMEOUT_MS;
      bool is_tapped = key_processor.key_is_tapped[row][col];

      switch (key.type) {
      case KEY_TYPE_LAYER_TAP:
        if (layer_tap_timer_elapsed && !is_tapped) {
          key_processor.layer_momentary_active[key.layer_tap.layer] = true;
          key_processor.key_is_tapped[row][col] = true;
          espnow_requied_data_t data = {0};
          data.layer = key.layer_tap.layer;
#if IS_MASTER
          send_to_espnow(RIGHT_SIDE, LAYER_SYNC, &data);
#else
          send_to_espnow(LEFT_SIDE, LAYER_SYNC, &data);
#endif
#if DEV
          ESP_LOGD(TAG, "Layer tap timeout - activating layer %d", key.layer_tap.layer);
#endif
        }
        break;
      case KEY_TYPE_MOD_TAP:
        if (mod_tap_timer_elapsed && !is_tapped) {
          current_report.modifiers |= key.mod_tap.hold_key;
          key_processor.key_is_tapped[row][col] = true;
          espnow_requied_data_t data = {0};
          data.report = &current_report;
#if IS_MASTER
          send_to_espnow(RIGHT_SIDE, MOD_SYNC, &data);
#else
          send_to_espnow(LEFT_SIDE, MOD_SYNC, &data);
#endif
#if DEV
          ESP_LOGD(TAG, "Mod tap timeout - activating modifier 0x%02x", key.mod_tap.hold_key);
#endif
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

#if DEV
    ESP_LOGD(TAG, "Key [%d:%d] %s -> L%d",
             events[i].row, events[i].col,
             events[i].pressed ? "PRESS" : "REL",
             active_layer);
#endif

    if (events[i].pressed) {
      process_key_press(key, events[i].row, events[i].col, current_time);
    } 
    else {
      // Use the stored key from when it was pressed
      bool is_key_marked_as_active = key_processor.pressed_key_active[events[i].row][events[i].col];
      if (is_key_marked_as_active) {
        key_definition_t stored_key = key_processor.pressed_keys[events[i].row][events[i].col];
#if DEV
        ESP_LOGD(TAG, "Processing key release for stored key type %d", stored_key.type);
#endif
        process_key_release(stored_key, events[i].row, events[i].col, current_time);
      } else {
#if DEV
        ESP_LOGW(TAG, "No stored key found for release at [%d:%d]", events[i].row, events[i].col);
#endif
      }
    }
  }

#if IS_MASTER
  send_hid_report(&current_report);
#else
  espnow_requied_data_t data = {0};
  data.report = &current_report;
  send_to_espnow(LEFT_SIDE, TAP, &data);
#endif
}

void matrix_scan_task(void *pvParameters) {
  key_event_t events[MAX_KEYS];
  uint8_t event_count;

  while (1) {
    if (matrix_scan(events, &event_count)) {
#if DEV
      ESP_LOGD(TAG, "*** KEY EVENT DETECTED: %d events ***", event_count);
#endif
      hid_process_key_event(events, &event_count);
    }

    uint32_t current_time = esp_timer_get_time() / 1000;
    check_tap_timeouts(current_time);

    vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
  }
}

void sync_modifier(hid_keyboard_report_t *report) {
  current_report.modifiers |= report->modifiers;
#if DEV
  ESP_LOGI(TAG, "Modifier synced");
#endif
}

void desync_modifier(hid_keyboard_report_t *report) {
  current_report.modifiers &= ~report->modifiers;
#if DEV
  ESP_LOGI(TAG, "Modifier desynced");
#endif
}

// Function to set the active layer for layer sync
void sync_active_layer(uint8_t layer) {
  key_processor.layer_momentary_active[layer] = true;
#if DEV
  ESP_LOGI(TAG, "Layer %d synced (activated)", layer);
#endif
}

// Function to unset the active layer for layer sync
void desync_active_layer(uint8_t layer) {
  key_processor.layer_momentary_active[layer] = false;
#if DEV
  ESP_LOGI(TAG, "Layer %d desynced (deactivated)", layer);
#endif
}
