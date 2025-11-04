/**
 * @file kb_mgt.c
 * @brief Keyboard Management System
 *
 * Manages HID reports, layer switching, key processing, and split keyboard
 * communication. Organized into four main subsystems:
 * 1. HID Report Management - Building and sending HID reports
 * 2. Layer Management - Layer activation/deactivation logic
 * 3. Key Processor - Key event handling and tap-hold logic
 * 4. Communication - ESP-NOW messaging for split keyboard
 */

#include "kb_mgt.h"
#include "config.h"
#include "espnow.h"
#include "freertos/projdefs.h"
#include "keymap.h"
#include "power_mgmt.h"

static const char       *TAG = "KB_MGT";
static SemaphoreHandle_t sem_hdl;

// =============================================================================
// STATE VARIABLES
// =============================================================================

static kb_mgt_hid_key_report_t      hid_key_report;
static kb_mgt_hid_consumer_report_t hid_consumer_report;
static proc_state_t                 proc_state;

// =============================================================================
// FORWARD DECLARATIONS - HID Management
// =============================================================================

static esp_err_t hid_init(void);
static result_t  hid_add_key_unsafe(uint8_t keycode);
static void      hid_remove_key_unsafe(uint8_t keycode);
static void      hid_set_consumer_unsafe(uint16_t usage);
static void      hid_clear_consumer_unsafe(void);
static void      hid_set_modifier_unsafe(uint8_t modifier);
static void      hid_clear_modifier_unsafe(uint8_t modifier);

// =============================================================================
// FORWARD DECLARATIONS - Layer Management
// =============================================================================

static esp_err_t layer_init(void);
static void      layer_activate_momentary_unsafe(uint8_t layer);
static void      layer_deactivate_momentary_unsafe(uint8_t layer);
static void      layer_toggle_unsafe(uint8_t layer);
static bool      layer_is_momentary_active(uint8_t layer);

// =============================================================================
// FORWARD DECLARATIONS - Key Processor
// =============================================================================

static esp_err_t proc_init(void);

static void proc_handle_press(key_def_t key, uint8_t row, uint8_t col,
                              uint32_t timestamp);
static void proc_handle_release(uint8_t row, uint8_t col, uint32_t timestamp);
static void proc_store_pressed_key(uint8_t row, uint8_t col, key_def_t key);
static key_def_t proc_get_stored_key(uint8_t row, uint8_t col);
static bool      proc_has_stored_key(uint8_t row, uint8_t col);
static void      proc_clear_stored_key(uint8_t row, uint8_t col);

// =============================================================================
// FORWARD DECLARATIONS - Communication
// =============================================================================

static void comm_send_event(kb_comm_event_t event_type, void *data);
static void comm_handle_brief_tap(uint8_t keycode);

// =============================================================================
// PUBLIC API - HID Report Access
// =============================================================================

kb_mgt_hid_key_report_t *kb_mgt_hid_get_current_key_report(void)
{
  return &hid_key_report;
}

kb_mgt_hid_consumer_report_t *kb_mgt_hid_get_current_consumer_report(void)
{
  return &hid_consumer_report;
}

void kb_mgt_hid_clear_report(void)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire keyboard management mutex");
    return;
  }
  memset(&hid_key_report, 0, sizeof(kb_mgt_hid_key_report_t));
  xSemaphoreGive(sem_hdl);
}

#if IS_MASTER
void kb_mgt_hid_send_key_report_unsafe(void)
{
  if (hid_dev)
  {
    esp_hidd_dev_input_set(hid_dev, 0, 1, (uint8_t *)(&hid_key_report),
                           sizeof(kb_mgt_hid_key_report_t));
  }
}
#else
void kb_mgt_hid_send_key_report_unsafe(void)
{
  comm_send_event(KB_COMM_EVENT_TAP, &hid_key_report);
}
#endif

#if IS_MASTER
void kb_mgt_hid_send_consumer_report_unsafe(void)
{
  if (hid_dev)
  {
    ESP_LOGI(TAG, "Sending consumer report: usage=0x%04X",
             hid_consumer_report.usage);
    esp_err_t ret =
        esp_hidd_dev_input_set(hid_dev, 0, 2, (uint8_t *)(&hid_consumer_report),
                               sizeof(kb_mgt_hid_consumer_report_t));
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to send consumer report: %d", ret);
    }
  }
}
#else
void kb_mgt_hid_send_consumer_report_unsafe(void)
{
  comm_send_event(KB_COMM_EVENT_CONSUMER, &hid_consumer_report);
}
#endif

// =============================================================================
// PUBLIC API - Modifier Sync (for split keyboard)
// =============================================================================

void kb_mgt_sync_modifier(uint8_t modifier)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire keyboard management mutex");
    return;
  }
  hid_key_report.modifiers |= modifier;
  xSemaphoreGive(sem_hdl);
  ESP_LOGI(TAG, "Modifier 0x%02x synced", modifier);
}

void kb_mgt_desync_modifier(uint8_t modifier)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire keyboard management mutex");
    return;
  }
  hid_key_report.modifiers &= ~modifier;
  xSemaphoreGive(sem_hdl);
  ESP_LOGI(TAG, "Modifier 0x%02x desynced", modifier);
}

// =============================================================================
// PUBLIC API - Layer Access
// =============================================================================

uint8_t kb_mgt_layer_get_active(void)
{
  // Check for momentary layers first (highest priority)
  for (int i = MAX_LAYERS - 1; i > 0; i--)
  {
    if (proc_state.layer_momentary_active[i])
    {
      return i;
    }
  }
  return proc_state.current_layer;
}

// =============================================================================
// PUBLIC API - Layer Sync (for split keyboard)
// =============================================================================

void kb_mgt_sync_layer(uint8_t layer)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire mutex");
    return;
  }
  layer_activate_momentary_unsafe(layer);
  xSemaphoreGive(sem_hdl);
  ESP_LOGI(TAG, "Layer %d synced (activated)", layer);
}

void kb_mgt_desync_layer(uint8_t layer)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire mutex");
    return;
  }
  layer_deactivate_momentary_unsafe(layer);
  xSemaphoreGive(sem_hdl);
  ESP_LOGI(TAG, "Layer %d desynced (deactivated)", layer);
}

// =============================================================================
// PUBLIC API - Processor Management
// =============================================================================

void kb_mgt_proc_check_tap_timeouts(uint32_t current_time)
{
  for (uint8_t row = 0; row < MATRIX_ROW; row++)
  {
    for (uint8_t col = 0; col < MATRIX_COL; col++)
    {
      if (!proc_state.pressed_key_active[row][col])
        continue;

      key_def_t key = proc_state.pressed_keys[row][col];
      bool      is_tapped = proc_state.key_is_tapped[row][col];

      uint16_t timeout_ms = proc_state.key_tap_timeout[row][col];
      if (timeout_ms == 0)
      {
        timeout_ms = DEFAULT_TIMEOUT_MS;
      }

      bool layer_tap_elapsed =
          (current_time - proc_state.layer_tap_timer[row][col]) >= timeout_ms;
      bool mod_tap_elapsed =
          (current_time - proc_state.mod_tap_timer[row][col]) >= timeout_ms;

      switch (key.type)
      {
      case KEY_TYPE_LAYER_TAP:
        if (layer_tap_elapsed && !is_tapped)
        {
          layer_activate_momentary_unsafe(key.layer_tap.layer);
          proc_state.key_is_tapped[row][col] = true;
          comm_send_event(KB_COMM_EVENT_LAYER_SYNC, &key.layer_tap.layer);
          ESP_LOGD(TAG, "Layer tap timeout (%dms) - activating layer %d",
                   timeout_ms, key.layer_tap.layer);
        }
        break;

      case KEY_TYPE_MOD_TAP:
        if (mod_tap_elapsed && !is_tapped)
        {
          hid_set_modifier_unsafe(key.mod_tap.hold_key);
          proc_state.key_is_tapped[row][col] = true;
          comm_send_event(KB_COMM_EVENT_MOD_SYNC, &key.mod_tap.hold_key);
          ESP_LOGD(TAG, "Mod tap timeout (%dms) - activating modifier 0x%02x",
                   timeout_ms, key.mod_tap.hold_key);
        }
        break;

      default:
        break;
      }
    }
  }
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS
// =============================================================================

// =============================================================================
// SUBSYSTEM 1: HID REPORT MANAGEMENT
// =============================================================================

static esp_err_t hid_init(void)
{
  memset(&hid_key_report, 0, sizeof(kb_mgt_hid_key_report_t));
  memset(&hid_consumer_report, 0, sizeof(kb_mgt_hid_consumer_report_t));

  ESP_LOGI(TAG, "HID management initialized");
  return ESP_OK;
}

static result_t hid_add_key_unsafe(uint8_t keycode)
{
  ESP_LOGD(TAG, "Adding key 0x%02x to HID report", keycode);
  for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++)
  {
    if (hid_key_report.keys[i] == 0)
    {
      hid_key_report.keys[i] = keycode;
      ESP_LOGD(TAG, "Added key 0x%02x to slot %d", keycode, i);
      return SUCCESS;
    }
  }

  ESP_LOGW(TAG, "HID report full, cannot add key 0x%02x", keycode);
  return REPORT_FULL;
}

static void hid_remove_key_unsafe(uint8_t keycode)
{
  ESP_LOGD(TAG, "Removing key 0x%02x from HID report", keycode);
  for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++)
  {
    if (hid_key_report.keys[i] == keycode)
    {
      // Shift remaining keys down
      for (int j = i; j < HID_KEY_SHIFT_LAST_IDX; j++)
      {
        hid_key_report.keys[j] = hid_key_report.keys[j + 1];
      }
      hid_key_report.keys[HID_KEY_SHIFT_LAST_IDX] = 0;

      ESP_LOGD(TAG, "Successfully removed key 0x%02x from slot %d", keycode, i);
      break;
    }
  }
}

static void hid_set_consumer_unsafe(uint16_t usage)
{
  hid_consumer_report.usage = usage;
}

static void hid_clear_consumer_unsafe(void) { hid_consumer_report.usage = 0; }

static void hid_set_modifier_unsafe(uint8_t modifier)
{
  hid_key_report.modifiers |= modifier;
}

static void hid_clear_modifier_unsafe(uint8_t modifier)
{
  hid_key_report.modifiers &= ~modifier;
}

// =============================================================================
// SUBSYSTEM 2: LAYER MANAGEMENT
// =============================================================================

static esp_err_t layer_init(void)
{
  proc_state.current_layer = DEFAULT_LAYER;
  memset(proc_state.layer_momentary_active, false,
         sizeof(proc_state.layer_momentary_active));

  ESP_LOGI(TAG, "Layer management initialized with default layer %d",
           DEFAULT_LAYER);
  return ESP_OK;
}

static void layer_activate_momentary_unsafe(uint8_t layer)
{
  if (layer < MAX_LAYERS)
  {
    proc_state.layer_momentary_active[layer] = true;

    ESP_LOGD(TAG, "Layer %d momentary activated", layer);
  }
}

static void layer_deactivate_momentary_unsafe(uint8_t layer)
{
  if (layer < MAX_LAYERS)
  {
    proc_state.layer_momentary_active[layer] = false;

    ESP_LOGD(TAG, "Layer %d momentary deactivated", layer);
  }
}

static void layer_toggle_unsafe(uint8_t layer)
{
  if (layer < MAX_LAYERS)
  {
    proc_state.current_layer =
        (proc_state.current_layer == layer) ? DEFAULT_LAYER : layer;
#if !IS_MASTER
    comm_send_event(KB_COMM_EVENT_LAYER_SYNC, &proc_state.current_layer);
#endif

    ESP_LOGD(TAG, "Layer toggled to %d", proc_state.current_layer);
  }
}

static bool layer_is_momentary_active(uint8_t layer)
{
  return (layer < MAX_LAYERS) ? proc_state.layer_momentary_active[layer]
                              : false;
}

// =============================================================================
// SUBSYSTEM 3: KEY PROCESSOR
// =============================================================================

static esp_err_t proc_init(void)
{
  memset(&proc_state, 0, sizeof(proc_state_t));
  proc_state.current_layer = DEFAULT_LAYER;

  ESP_LOGI(TAG, "Key processor initialized");
  return ESP_OK;
}

static void proc_handle_press(key_def_t key, uint8_t row, uint8_t col,
                              uint32_t timestamp)
{
  ESP_LOGD(TAG, "Processing key press at [%d:%d], type=%d", row, col, key.type);

  // TAP-PREFERRED: Check for pending tap-hold keys and resolve them as TAP when
  // another key is pressed
  for (uint8_t r = 0; r < MATRIX_ROW; r++)
  {
    for (uint8_t c = 0; c < MATRIX_COL; c++)
    {
      if (!proc_state.pressed_key_active[r][c])
        continue;
      if (r == row && c == col)
        continue;

      uint16_t timeout_ms = proc_state.key_tap_timeout[r][c];
      if (timeout_ms == 0)
        timeout_ms = DEFAULT_TIMEOUT_MS;

      key_def_t held_key = proc_state.pressed_keys[r][c];
      bool      already_resolved = proc_state.key_is_tapped[r][c];

      if (already_resolved)
        continue;

      // LayerTap: send tap key immediately when another key is pressed
      if (held_key.type == KEY_TYPE_LAYER_TAP)
      {
        uint32_t held_time = timestamp - proc_state.layer_tap_timer[r][c];
        if (held_time < timeout_ms)
        {
          hid_add_key_unsafe(held_key.layer_tap.tap_key);
          proc_state.key_is_tapped[r][c] = true;
          ESP_LOGD(TAG, "LayerTap resolved as TAP at [%d:%d]", r, c);
        }
      }

      // ModTap: send tap key immediately when another key is pressed
      if (held_key.type == KEY_TYPE_MOD_TAP)
      {
        uint32_t held_time = timestamp - proc_state.mod_tap_timer[r][c];
        if (held_time < timeout_ms)
        {
          hid_add_key_unsafe(held_key.mod_tap.tap_key);
          proc_state.key_is_tapped[r][c] = true;
          ESP_LOGD(TAG, "ModTap resolved as TAP at [%d:%d]", r, c);
        }
      }
    }
  }

  switch (key.type)
  {
  case KEY_TYPE_NORMAL:
    ESP_LOGD(TAG, "Processing normal key: keycode=0x%02x", key.keycode);
    hid_add_key_unsafe(key.keycode);
    break;

  case KEY_TYPE_CONSUMER:
    hid_set_consumer_unsafe(key.consumer);
    kb_mgt_hid_send_consumer_report_unsafe();
    break;

  case KEY_TYPE_MODIFIER:
    hid_set_modifier_unsafe(key.modifier);
    break;

  case KEY_TYPE_SHIFTED:
    hid_set_modifier_unsafe(HID_MOD_LEFT_SHIFT);
    hid_add_key_unsafe(key.keycode);
    break;

  case KEY_TYPE_LAYER_TAP:
    proc_state.layer_tap_timer[row][col] = timestamp;
    proc_state.key_is_tapped[row][col] = false;
    break;

  case KEY_TYPE_MOD_TAP:
    proc_state.mod_tap_timer[row][col] = timestamp;
    proc_state.key_is_tapped[row][col] = false;
    break;

  case KEY_TYPE_LAYER_MOMENTARY:
    layer_activate_momentary_unsafe(key.layer);
    break;

  case KEY_TYPE_LAYER_TOGGLE:
    layer_toggle_unsafe(key.layer);
    break;

  case KEY_TYPE_TRANSPARENT:
    // Handle transparent key by checking lower layers
    for (int layer = kb_mgt_layer_get_active() - 1; layer >= 0; layer--)
    {
      key_def_t lower_key = keymap_get_key(layer, row, col);
      if (lower_key.type != KEY_TYPE_TRANSPARENT)
      {
        proc_handle_press(lower_key, row, col, timestamp);
        proc_store_pressed_key(row, col, lower_key);
        return;
      }
    }
    break;

  default:
    ESP_LOGW(TAG, "Unknown key type: %d", key.type);
    break;
  }

  // Store the pressed key for release processing
  proc_store_pressed_key(row, col, key);
}

static void proc_handle_release(uint8_t row, uint8_t col, uint32_t timestamp)
{
  if (!proc_has_stored_key(row, col))
  {
    ESP_LOGW(TAG, "No stored key found for release at [%d:%d]", row, col);
    return;
  }

  uint16_t timeout_ms = proc_state.key_tap_timeout[row][col];
  if (timeout_ms == 0)
    timeout_ms = DEFAULT_TIMEOUT_MS;

  key_def_t stored_key = proc_get_stored_key(row, col);

  ESP_LOGD(TAG, "Processing key release at [%d:%d], type=%d", row, col,
           stored_key.type);

  switch (stored_key.type)
  {
  case KEY_TYPE_NORMAL:
    hid_remove_key_unsafe(stored_key.keycode);
    break;

  case KEY_TYPE_CONSUMER:
    hid_clear_consumer_unsafe();
    kb_mgt_hid_send_consumer_report_unsafe(); // Send clear immediately to
    // release the key
    break;

  case KEY_TYPE_MODIFIER:
    hid_clear_modifier_unsafe(stored_key.modifier);
    break;

  case KEY_TYPE_SHIFTED:
    hid_clear_modifier_unsafe(HID_MOD_LEFT_SHIFT);
    hid_remove_key_unsafe(stored_key.keycode);
    break;

  case KEY_TYPE_LAYER_TAP:
  {
    bool is_tapped = proc_state.key_is_tapped[row][col];
    bool layer_is_active =
        layer_is_momentary_active(stored_key.layer_tap.layer);
    uint32_t layer_tap_hold_time =
        timestamp - proc_state.layer_tap_timer[row][col];

    // If tap-preferred sent the tap key, remove it from report
    if (is_tapped && !layer_is_active)
    {
      hid_remove_key_unsafe(stored_key.layer_tap.tap_key);
    }

    // ALWAYS deactivate layer if it was activated (by timeout)
    if (layer_is_active)
    {
      layer_deactivate_momentary_unsafe(stored_key.layer_tap.layer);
      comm_send_event(KB_COMM_EVENT_LAYER_DESYNC, &stored_key.layer_tap.layer);
    }

    // If quick tap without layer activation and wasn't tap-preferred, send
    // brief tap
    if (!is_tapped && !layer_is_active && layer_tap_hold_time < timeout_ms)
    {
      comm_handle_brief_tap(stored_key.layer_tap.tap_key);
    }
    break;
  }

  case KEY_TYPE_MOD_TAP:
  {
    bool     is_tapped = proc_state.key_is_tapped[row][col];
    uint32_t mod_tap_hold_time = timestamp - proc_state.mod_tap_timer[row][col];
    bool     mod_is_active =
        (hid_key_report.modifiers & stored_key.mod_tap.hold_key) != 0;

    // If tap-preferred sent the tap key, remove it from report
    if (is_tapped && !mod_is_active)
    {
      hid_remove_key_unsafe(stored_key.mod_tap.tap_key);
    }

    // ALWAYS clear modifier if it was activated (by timeout)
    if (mod_is_active)
    {
      hid_clear_modifier_unsafe(stored_key.mod_tap.hold_key);
      comm_send_event(KB_COMM_EVENT_MOD_DESYNC, &stored_key.mod_tap.hold_key);
    }

    // If quick tap without modifier activation and wasn't tap-preferred, send
    // brief tap
    if (!is_tapped && !mod_is_active && mod_tap_hold_time < timeout_ms)
    {
      comm_handle_brief_tap(stored_key.mod_tap.tap_key);
    }
    break;
  }

  case KEY_TYPE_LAYER_MOMENTARY:
    layer_deactivate_momentary_unsafe(stored_key.layer);
    break;

  case KEY_TYPE_TRANSPARENT:
    // Handle transparent key release by checking lower layers
    for (int layer = kb_mgt_layer_get_active() - 1; layer >= 0; layer--)
    {
      key_def_t lower_key = keymap_get_key(layer, row, col);
      if (lower_key.type != KEY_TYPE_TRANSPARENT)
      {
        proc_handle_release(row, col, timestamp);
        break;
      }
    }
    break;

  default:
    break;
  }

  proc_clear_stored_key(row, col);
}

static void proc_store_pressed_key(uint8_t row, uint8_t col, key_def_t key)
{
  if (row < MATRIX_ROW && col < MATRIX_COL)
  {
    proc_state.pressed_keys[row][col] = key;
    proc_state.pressed_key_active[row][col] = true;
  }
}

static key_def_t proc_get_stored_key(uint8_t row, uint8_t col)
{
  if (row < MATRIX_ROW && col < MATRIX_COL)
  {
    return proc_state.pressed_keys[row][col];
  }
  key_def_t empty_key = {0};
  return empty_key;
}

static bool proc_has_stored_key(uint8_t row, uint8_t col)
{
  return (row < MATRIX_ROW && col < MATRIX_COL)
             ? proc_state.pressed_key_active[row][col]
             : false;
}

static void proc_clear_stored_key(uint8_t row, uint8_t col)
{
  if (row < MATRIX_ROW && col < MATRIX_COL)
  {
    memset(&proc_state.pressed_keys[row][col], 0, sizeof(key_def_t));
    proc_state.pressed_key_active[row][col] = false;
  }
}

// =============================================================================
// SUBSYSTEM 4: COMMUNICATION (ESP-NOW for Split Keyboard)
// =============================================================================

static void comm_send_event(kb_comm_event_t event_type, void *data)
{
  switch (event_type)
  {
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

static void comm_handle_brief_tap(uint8_t keycode)
{
  hid_add_key_unsafe(keycode);
#if IS_MASTER
  kb_mgt_hid_send_key_report_unsafe();
  hid_remove_key_unsafe(keycode);
  kb_mgt_hid_send_key_report_unsafe();
#else
  comm_send_event(KB_COMM_EVENT_BRIEF_TAP, kb_mgt_hid_get_current_key_report());
  hid_remove_key_unsafe(keycode);
#endif
}

// =============================================================================
// MAIN INITIALIZATION AND PUBLIC API
// =============================================================================

esp_err_t kb_mgt_init(void)
{
  esp_err_t ret = ESP_OK;

  ret |= hid_init();
  ret |= layer_init();
  ret |= proc_init();

  sem_hdl = xSemaphoreCreateMutex();
  if (!sem_hdl)
  {
    ESP_LOGE(TAG, "Failed to create keyboard management mutex");
    ret = ESP_FAIL;
  }

  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG,
             "All keyboard management subsystems initialized successfully");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to initialize keyboard management subsystems");
  }

  return ret;
}

void kb_mgt_process_key_event(key_def_t key, uint8_t row, uint8_t col,
                              bool pressed, uint32_t timestamp)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire keyboard management mutex");
    return;
  }

  if (pressed)
  {
    proc_handle_press(key, row, col, timestamp);
  }
  else
  {
    proc_handle_release(row, col, timestamp);
  }
  xSemaphoreGive(sem_hdl);
}

void kb_mgt_finalize_processing(void)
{
  if (!sem_hdl || xSemaphoreTake(sem_hdl, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to acquire keyboard management mutex");
    return;
  }

  kb_mgt_hid_send_key_report_unsafe();
  xSemaphoreGive(sem_hdl);
}
