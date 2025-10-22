#include "kb_mgt.h"
#include "config.h"
#include "espnow.h"
#include "handler.h"
#include "keymap.h"

static const char *TAG = "KB_MGT";

static kb_mgt_hid_key_report_t            current_hid_report;
static kb_mgt_hid_consumer_report_t   current_hid_consumer_report;
static kb_mgt_processor_state_t       processor_state;

esp_err_t kb_mgt_hid_init(void) {
  memset(&current_hid_report, 0, sizeof(kb_mgt_hid_key_report_t));
  memset(&current_hid_consumer_report, 0, sizeof(kb_mgt_hid_consumer_report_t));

  ESP_LOGI(TAG, "HID management initialized");
  return ESP_OK;
}

kb_mgt_hid_key_report_t* kb_mgt_hid_get_current_report(void) {
  return &current_hid_report;
}

kb_mgt_hid_consumer_report_t* kb_mgt_hid_get_current_consumer_report(void) {
  return &current_hid_consumer_report;
}

kb_mgt_result_t kb_mgt_hid_add_key(uint8_t keycode) {
  for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++) {
    if (current_hid_report.keys[i] == 0) {
      current_hid_report.keys[i] = keycode;
      return KB_MGT_RESULT_SUCCESS;
    }
  }

  ESP_LOGW(TAG, "HID report full, cannot add key 0x%02x", keycode);
  return KB_MGT_RESULT_REPORT_FULL;
}

void kb_mgt_hid_remove_key(uint8_t keycode) {
  for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++) {
    if (current_hid_report.keys[i] == keycode) {
      // Shift remaining keys down
      for (int j = i; j < HID_KEY_SHIFT_LAST_IDX; j++) {
        current_hid_report.keys[j] = current_hid_report.keys[j + 1];
      }
      current_hid_report.keys[HID_KEY_SHIFT_LAST_IDX] = 0;

      ESP_LOGD(TAG, "Removed key 0x%02x from HID report", keycode);
      break;
    }
  }
}

void kb_mgt_hid_set_consumer(uint16_t usage) {
  current_hid_consumer_report.usage = usage;
}

void kb_mgt_hid_clear_consumer(void) {
  current_hid_consumer_report.usage = 0;
}

void kb_mgt_hid_set_modifier(uint8_t modifier) {
  current_hid_report.modifiers |= modifier;
}

void kb_mgt_hid_clear_modifier(uint8_t modifier) {
  current_hid_report.modifiers &= ~modifier;
}

#if IS_MASTER
void kb_mgt_hid_send_consumer_report(void) {
  if (hid_dev) {
    ESP_LOGI(TAG, "Sending consumer report: usage=0x%04X", current_hid_consumer_report.usage);
    esp_err_t ret = esp_hidd_dev_input_set(hid_dev, 0, 2, (uint8_t *)&current_hid_consumer_report, sizeof(kb_mgt_hid_consumer_report_t));
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to send consumer report: %d", ret);
    }
  }
}
#else
void kb_mgt_hid_send_consumer_report(void) {
  // For slave, this would send via ESP-NOW to master
  kb_mgt_comm_send_event(KB_COMM_EVENT_CONSUMER, &current_hid_consumer_report);
}
#endif

#if IS_MASTER
void kb_mgt_hid_send_report(void) {
  if (hid_dev) {
    esp_hidd_dev_input_set(hid_dev, 0, 1, (uint8_t *)&current_hid_report, sizeof(kb_mgt_hid_key_report_t));
  }
}
#else
void kb_mgt_hid_send_report(void) {
  // For slave, this would send via ESP-NOW to master
  kb_mgt_comm_send_event(KB_COMM_EVENT_TAP, &current_hid_report);
}
#endif

void kb_mgt_hid_clear_report(void) {
  memset(&current_hid_report, 0, sizeof(kb_mgt_hid_key_report_t));
}

void kb_mgt_sync_modifier(uint8_t modifier) {
  current_hid_report.modifiers |= modifier;

  ESP_LOGI(TAG, "Modifier 0x%02x synced", modifier);
}

void kb_mgt_desync_modifier(uint8_t modifier) {
  current_hid_report.modifiers &= ~modifier;

  ESP_LOGI(TAG, "Modifier 0x%02x desynced", modifier);
}

esp_err_t kb_mgt_layer_init(void) {
  processor_state.current_layer = DEFAULT_LAYER;
  memset(processor_state.layer_momentary_active, false, sizeof(processor_state.layer_momentary_active));

  ESP_LOGI(TAG, "Layer management initialized with default layer %d", DEFAULT_LAYER);
  return ESP_OK;
}

uint8_t kb_mgt_layer_get_active(void) {
  // Check for momentary layers first (highest priority)
  for (int i = MAX_LAYERS - 1; i > 0; i--) {
    if (processor_state.layer_momentary_active[i]) {
      return i;
    }
  }
  return processor_state.current_layer;
}

void kb_mgt_layer_set_base(uint8_t layer) {
  if (layer < MAX_LAYERS) {
    processor_state.current_layer = layer;

    ESP_LOGD(TAG, "Base layer set to %d", layer);
  }
}

void kb_mgt_layer_activate_momentary(uint8_t layer) {
  if (layer < MAX_LAYERS) {
    processor_state.layer_momentary_active[layer] = true;

    ESP_LOGD(TAG, "Layer %d momentary activated", layer);
  }
}

void kb_mgt_layer_deactivate_momentary(uint8_t layer) {
  if (layer < MAX_LAYERS) {
    processor_state.layer_momentary_active[layer] = false;

    ESP_LOGD(TAG, "Layer %d momentary deactivated", layer);
  }
}

void kb_mgt_layer_toggle(uint8_t layer) {
  if (layer < MAX_LAYERS) {
    processor_state.current_layer = (processor_state.current_layer == layer) ? DEFAULT_LAYER : layer;
#if !IS_MASTER
    kb_mgt_comm_send_event(KB_COMM_EVENT_LAYER_SYNC, &processor_state.current_layer);
#endif

    ESP_LOGD(TAG, "Layer toggled to %d", processor_state.current_layer);
  }
}

bool kb_mgt_layer_is_momentary_active(uint8_t layer) {
  return (layer < MAX_LAYERS) ? processor_state.layer_momentary_active[layer] : false;
}

void kb_mgt_sync_layer(uint8_t layer) {
  kb_mgt_layer_activate_momentary(layer);

  ESP_LOGI(TAG, "Layer %d synced (activated)", layer);
}

void kb_mgt_desync_layer(uint8_t layer) {
  kb_mgt_layer_deactivate_momentary(layer);

  ESP_LOGI(TAG, "Layer %d desynced (deactivated)", layer);
}

esp_err_t kb_mgt_processor_init(void) {
  memset(&processor_state, 0, sizeof(kb_mgt_processor_state_t));
  processor_state.current_layer = DEFAULT_LAYER;

  ESP_LOGI(TAG, "Key processor initialized");
  return ESP_OK;
}

kb_mgt_processor_state_t* kb_mgt_processor_get_state(void) {
  return &processor_state;
}

void kb_mgt_processor_handle_press(key_definition_t key, uint8_t row, uint8_t col, uint32_t timestamp) {
  ESP_LOGD(TAG, "Processing key press at [%d:%d], type=%d", row, col, key.type);

  // TAP-PREFERRED: Check for pending tap-hold keys and resolve them as TAP when another key is pressed
  for (uint8_t r = 0; r < MATRIX_ROW; r++) {
    for (uint8_t c = 0; c < MATRIX_COL; c++) {
      if (!processor_state.pressed_key_active[r][c]) continue;
      if (r == row && c == col) continue;

      uint16_t timeout_ms = processor_state.key_tap_timeout[r][c];
      if (timeout_ms == 0) timeout_ms = DEFAULT_TIMEOUT_MS;

      key_definition_t held_key = processor_state.pressed_keys[r][c];
      bool already_resolved = processor_state.key_is_tapped[r][c];

      if (already_resolved) continue;

      // LayerTap: send tap key immediately when another key is pressed
      if (held_key.type == KEY_TYPE_LAYER_TAP) {
        uint32_t held_time = timestamp - processor_state.layer_tap_timer[r][c];
        if (held_time < timeout_ms) {
          kb_mgt_hid_add_key(held_key.layer_tap.tap_key);
          processor_state.key_is_tapped[r][c] = true;
          ESP_LOGD(TAG, "LayerTap resolved as TAP at [%d:%d]", r, c);
        }
      }

      // ModTap: send tap key immediately when another key is pressed
      if (held_key.type == KEY_TYPE_MOD_TAP) {
        uint32_t held_time = timestamp - processor_state.mod_tap_timer[r][c];
        if (held_time < timeout_ms) {
          kb_mgt_hid_add_key(held_key.mod_tap.tap_key);
          processor_state.key_is_tapped[r][c] = true;
          ESP_LOGD(TAG, "ModTap resolved as TAP at [%d:%d]", r, c);
        }
      }
    }
  }

  switch (key.type) {
    case KEY_TYPE_NORMAL:
      kb_mgt_hid_add_key(key.keycode);
      break;

    case KEY_TYPE_CONSUMER:
      kb_mgt_hid_set_consumer(key.consumer);
      kb_mgt_hid_send_consumer_report();
      break;

    case KEY_TYPE_MODIFIER:
      kb_mgt_hid_set_modifier(key.modifier);
      break;

    case KEY_TYPE_SHIFTED:
      kb_mgt_hid_set_modifier(HID_MOD_LEFT_SHIFT);
      kb_mgt_hid_add_key(key.keycode);
      break;

    case KEY_TYPE_LAYER_TAP:
      processor_state.layer_tap_timer[row][col] = timestamp;
      processor_state.key_is_tapped[row][col] = false;
      break;

    case KEY_TYPE_MOD_TAP:
      processor_state.mod_tap_timer[row][col] = timestamp;
      processor_state.key_is_tapped[row][col] = false;
      break;

    case KEY_TYPE_LAYER_MOMENTARY:
      kb_mgt_layer_activate_momentary(key.layer);
      break;

    case KEY_TYPE_LAYER_TOGGLE:
      kb_mgt_layer_toggle(key.layer);
      break;

    case KEY_TYPE_TRANSPARENT:
      // Handle transparent key by checking lower layers
      for (int layer = kb_mgt_layer_get_active() - 1; layer >= 0; layer--) {
        key_definition_t lower_key = keymap_get_key(layer, row, col);
        if (lower_key.type != KEY_TYPE_TRANSPARENT) {
          kb_mgt_processor_handle_press(lower_key, row, col, timestamp);
          kb_mgt_processor_store_pressed_key(row, col, lower_key);
          return;
        }
      }
      break;

    default:
      ESP_LOGW(TAG, "Unknown key type: %d", key.type);
      break;
  }

  // Store the pressed key for release processing
  kb_mgt_processor_store_pressed_key(row, col, key);
}

void kb_mgt_processor_handle_release(uint8_t row, uint8_t col, uint32_t timestamp) {
  if (!kb_mgt_processor_has_stored_key(row, col)) {
    ESP_LOGW(TAG, "No stored key found for release at [%d:%d]", row, col);
    return;
  }

  uint16_t timeout_ms = processor_state.key_tap_timeout[row][col];
  if (timeout_ms == 0) timeout_ms = DEFAULT_TIMEOUT_MS;

  key_definition_t stored_key = kb_mgt_processor_get_stored_key(row, col);

  ESP_LOGD(TAG, "Processing key release at [%d:%d], type=%d", row, col, stored_key.type);

  switch (stored_key.type) {
    case KEY_TYPE_NORMAL:
      kb_mgt_hid_remove_key(stored_key.keycode);
      break;

    case KEY_TYPE_CONSUMER:
      kb_mgt_hid_clear_consumer();
      kb_mgt_hid_send_consumer_report();  // Send clear immediately to release the key
      break;

    case KEY_TYPE_MODIFIER:
      kb_mgt_hid_clear_modifier(stored_key.modifier);
      break;

    case KEY_TYPE_SHIFTED:
      kb_mgt_hid_clear_modifier(HID_MOD_LEFT_SHIFT);
      kb_mgt_hid_remove_key(stored_key.keycode);
      break;

    case KEY_TYPE_LAYER_TAP: {
      bool is_tapped = processor_state.key_is_tapped[row][col];
      bool layer_is_active = kb_mgt_layer_is_momentary_active(stored_key.layer_tap.layer);
      uint32_t layer_tap_hold_time = timestamp - processor_state.layer_tap_timer[row][col];

      // If tap-preferred sent the tap key, remove it from report
      if (is_tapped && !layer_is_active) {
        kb_mgt_hid_remove_key(stored_key.layer_tap.tap_key);
      }

      // ALWAYS deactivate layer if it was activated (by timeout)
      if (layer_is_active) {
        kb_mgt_layer_deactivate_momentary(stored_key.layer_tap.layer);
        kb_mgt_comm_send_event(KB_COMM_EVENT_LAYER_DESYNC, &stored_key.layer_tap.layer);
      }

      // If quick tap without layer activation and wasn't tap-preferred, send brief tap
      if (!is_tapped && !layer_is_active && layer_tap_hold_time < timeout_ms) {
        kb_mgt_comm_handle_brief_tap(stored_key.layer_tap.tap_key);
      }
      break;
    }

    case KEY_TYPE_MOD_TAP: {
      bool is_tapped = processor_state.key_is_tapped[row][col];
      uint32_t mod_tap_hold_time = timestamp - processor_state.mod_tap_timer[row][col];
      bool mod_is_active = (current_hid_report.modifiers & stored_key.mod_tap.hold_key) != 0;

      // If tap-preferred sent the tap key, remove it from report
      if (is_tapped && !mod_is_active) {
        kb_mgt_hid_remove_key(stored_key.mod_tap.tap_key);
      }

      // ALWAYS clear modifier if it was activated (by timeout)
      if (mod_is_active) {
        kb_mgt_hid_clear_modifier(stored_key.mod_tap.hold_key);
        kb_mgt_comm_send_event(KB_COMM_EVENT_MOD_DESYNC, &stored_key.mod_tap.hold_key);
      }

      // If quick tap without modifier activation and wasn't tap-preferred, send brief tap
      if (!is_tapped && !mod_is_active && mod_tap_hold_time < timeout_ms) {
        kb_mgt_comm_handle_brief_tap(stored_key.mod_tap.tap_key);
      }
      break;
    }

    case KEY_TYPE_LAYER_MOMENTARY:
      kb_mgt_layer_deactivate_momentary(stored_key.layer);
      break;

    case KEY_TYPE_TRANSPARENT:
      // Handle transparent key release by checking lower layers
      for (int layer = kb_mgt_layer_get_active() - 1; layer >= 0; layer--) {
        key_definition_t lower_key = keymap_get_key(layer, row, col);
        if (lower_key.type != KEY_TYPE_TRANSPARENT) {
          kb_mgt_processor_handle_release(row, col, timestamp);
          break;
        }
      }
      break;

    default:
      break;
  }

  kb_mgt_processor_clear_stored_key(row, col);
}

void kb_mgt_processor_check_tap_timeouts(uint32_t current_time) {
  for (uint8_t row = 0; row < MATRIX_ROW; row++) {
    for (uint8_t col = 0; col < MATRIX_COL; col++) {
      if (!processor_state.pressed_key_active[row][col]) continue;

      key_definition_t key = processor_state.pressed_keys[row][col];
      bool is_tapped = processor_state.key_is_tapped[row][col];

      // Get the per-key timeout (fallback to default if 0)
      uint16_t timeout_ms = processor_state.key_tap_timeout[row][col];
      if (timeout_ms == 0) {
        timeout_ms = DEFAULT_TIMEOUT_MS;
      }

      bool layer_tap_elapsed = (current_time - processor_state.layer_tap_timer[row][col]) >= timeout_ms;
      bool mod_tap_elapsed = (current_time - processor_state.mod_tap_timer[row][col]) >= timeout_ms;

      switch (key.type) {
        case KEY_TYPE_LAYER_TAP:
          if (layer_tap_elapsed && !is_tapped) {
            kb_mgt_layer_activate_momentary(key.layer_tap.layer);
            processor_state.key_is_tapped[row][col] = true;
            kb_mgt_comm_send_event(KB_COMM_EVENT_LAYER_SYNC, &key.layer_tap.layer);

            ESP_LOGD(TAG, "Layer tap timeout (%dms) - activating layer %d", timeout_ms, key.layer_tap.layer);
          }
          break;

        case KEY_TYPE_MOD_TAP:
          if (mod_tap_elapsed && !is_tapped) {
            kb_mgt_hid_set_modifier(key.mod_tap.hold_key);
            processor_state.key_is_tapped[row][col] = true;
            kb_mgt_comm_send_event(KB_COMM_EVENT_MOD_SYNC, &key.mod_tap.hold_key);

            ESP_LOGD(TAG, "Mod tap timeout (%dms) - activating modifier 0x%02x", timeout_ms, key.mod_tap.hold_key);
          }
          break;

        default:
          break;
      }
    }
  }
}

void kb_mgt_processor_store_pressed_key(uint8_t row, uint8_t col, key_definition_t key) {
  if (row < MATRIX_ROW && col < MATRIX_COL) {
    processor_state.pressed_keys[row][col] = key;
    processor_state.pressed_key_active[row][col] = true;
  }
}

key_definition_t kb_mgt_processor_get_stored_key(uint8_t row, uint8_t col) {
  if (row < MATRIX_ROW && col < MATRIX_COL) {
    return processor_state.pressed_keys[row][col];
  }
  key_definition_t empty_key = {0};
  return empty_key;
}

bool kb_mgt_processor_has_stored_key(uint8_t row, uint8_t col) {
  return (row < MATRIX_ROW && col < MATRIX_COL) ? processor_state.pressed_key_active[row][col] : false;
}

void kb_mgt_processor_clear_stored_key(uint8_t row, uint8_t col) {
  if (row < MATRIX_ROW && col < MATRIX_COL) {
    memset(&processor_state.pressed_keys[row][col], 0, sizeof(key_definition_t));
    processor_state.pressed_key_active[row][col] = false;
  }
}

esp_err_t kb_mgt_comm_init(void) {
  ESP_LOGI(TAG, "Communication management initialized");
  return ESP_OK;
}

void kb_mgt_comm_send_event(kb_comm_event_t event_type, void* data) {
  switch (event_type) {
    case KB_COMM_EVENT_TAP:
#if IS_MASTER
    send_to_espnow(MASTER, TAP, data);
#else
    send_to_espnow(SLAVE, TAP, data);
#endif
    break;

    case KB_COMM_EVENT_BRIEF_TAP:
#if IS_MASTER
    send_to_espnow(MASTER, BRIEF_TAP, data);
#else
    send_to_espnow(SLAVE, BRIEF_TAP, data);
#endif
    break;

    case KB_COMM_EVENT_LAYER_SYNC:
#if IS_MASTER
    send_to_espnow(MASTER, LAYER_SYNC, data);
#else
    send_to_espnow(SLAVE, LAYER_SYNC, data);
#endif
    break;

    case KB_COMM_EVENT_LAYER_DESYNC:
#if IS_MASTER
    send_to_espnow(MASTER, LAYER_DESYNC, data);
#else
    send_to_espnow(SLAVE, LAYER_DESYNC, data);
#endif
    break;

    case KB_COMM_EVENT_MOD_SYNC:
#if IS_MASTER
    send_to_espnow(MASTER, MOD_SYNC, data);
#else
    send_to_espnow(SLAVE, MOD_SYNC, data);
#endif
    break;

    case KB_COMM_EVENT_MOD_DESYNC:
#if IS_MASTER
    send_to_espnow(MASTER, MOD_DESYNC, data);
#else
    send_to_espnow(SLAVE, MOD_DESYNC, data);
#endif
    break;

    case KB_COMM_EVENT_CONSUMER:
#if IS_MASTER
    send_to_espnow(MASTER, CONSUMER, data);
#else
    send_to_espnow(SLAVE, CONSUMER, data);
#endif
    break;
  }
}

void kb_mgt_comm_handle_brief_tap(uint8_t keycode) {
  kb_mgt_hid_add_key(keycode);
#if IS_MASTER
  kb_mgt_hid_send_report();
  kb_mgt_hid_remove_key(keycode);
  kb_mgt_hid_send_report();
#else
  kb_mgt_comm_send_event(KB_COMM_EVENT_BRIEF_TAP, kb_mgt_hid_get_current_report());
  kb_mgt_hid_remove_key(keycode);
#endif
}

esp_err_t kb_mgt_init(void) {
  esp_err_t ret = ESP_OK;

  ret |= kb_mgt_hid_init();
  ret |= kb_mgt_layer_init();
  ret |= kb_mgt_processor_init();
  ret |= kb_mgt_comm_init();

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "All keyboard management subsystems initialized successfully");
  } else {
    ESP_LOGE(TAG, "Failed to initialize keyboard management subsystems");
  }

  return ret;
}

void kb_mgt_process_key_event(key_definition_t key, uint8_t row, uint8_t col, bool pressed, uint32_t timestamp) {
  if (pressed) {
    kb_mgt_processor_handle_press(key, row, col, timestamp);
  } else {
    kb_mgt_processor_handle_release(row, col, timestamp);
  }
}

void kb_mgt_finalize_processing(void) {
  kb_mgt_hid_send_report();
}
