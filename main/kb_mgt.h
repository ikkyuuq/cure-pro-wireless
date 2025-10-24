#ifndef KB_MGT_H
#define KB_MGT_H

#include "common.h"
#include "config.h"
#include "keymap.h"

// Forward declaration to avoid circular dependency with espnow.h
struct esp_hidd_dev_s;
typedef struct esp_hidd_dev_s esp_hidd_dev_t;
extern esp_hidd_dev_t        *hid_dev;

// HID Configuration Constants
#define HID_MAX_KEYS_IN_REPORT 6
#define HID_KEY_SHIFT_LAST_IDX 5

// Key processing result types
typedef enum
{
  SUCCESS,
  REPORT_FULL,
  KEY_NOT_FOUND,
  INVALID_PARAM,
  UNKNOWN_ERROR
} result_t;

// Communication event types for ESP-NOW
typedef enum
{
  KB_COMM_EVENT_TAP,
  KB_COMM_EVENT_BRIEF_TAP,
  KB_COMM_EVENT_LAYER_SYNC,
  KB_COMM_EVENT_LAYER_DESYNC,
  KB_COMM_EVENT_MOD_SYNC,
  KB_COMM_EVENT_MOD_DESYNC,
  KB_COMM_EVENT_CONSUMER
} kb_comm_event_t;

// Forward declarations
typedef struct
{
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[HID_MAX_KEYS_IN_REPORT];
} kb_mgt_hid_key_report_t;

typedef struct
{
  uint16_t usage;
} kb_mgt_hid_consumer_report_t;

typedef struct
{
  uint8_t   current_layer;
  uint32_t  layer_tap_timer[MATRIX_ROW][MATRIX_COL];
  uint32_t  mod_tap_timer[MATRIX_ROW][MATRIX_COL];
  uint16_t  key_tap_timeout[MATRIX_ROW][MATRIX_COL];
  key_def_t pressed_keys[MATRIX_ROW][MATRIX_COL];
  bool      key_is_tapped[MATRIX_ROW][MATRIX_COL];
  bool      layer_momentary_active[MAX_LAYERS];
  bool      pressed_key_active[MATRIX_ROW][MATRIX_COL];
} proc_state_t;

// =============================================================================
// HID MANAGEMENT
// =============================================================================

// Get current HID report
kb_mgt_hid_key_report_t *kb_mgt_hid_get_current_key_report(void);

// Get current HID(Consumer) report
kb_mgt_hid_consumer_report_t *kb_mgt_hid_get_current_consumer_report(void);

// Send HID report (only if master)
void kb_mgt_hid_send_key_report_unsafe(void);

// Send HID(Consumer) report (only if master)
void kb_mgt_hid_send_consumer_report_unsafe(void);

// Clear entire HID report
void kb_mgt_hid_clear_report(void);

// Sync modifier from remote half
void kb_mgt_sync_modifier(uint8_t modifier);

// Desync modifier from remote half
void kb_mgt_desync_modifier(uint8_t modifier);

// =============================================================================
// LAYER MANAGEMENT
// =============================================================================

// Get current active layer
uint8_t kb_mgt_layer_get_active(void);

// Sync layer activation from remote half
void kb_mgt_sync_layer(uint8_t layer);

// Desync layer activation from remote half
void kb_mgt_desync_layer(uint8_t layer);

// =============================================================================
// KEY PROCESSOR MANAGEMENT
// =============================================================================

// Check and handle tap timeouts
void kb_mgt_proc_check_tap_timeouts(uint32_t current_time);

// =============================================================================
// MAIN MANAGEMENT INTERFACE
// =============================================================================

// Initialize all keyboard management subsystems
esp_err_t kb_mgt_init(void);

// Process complete key event (combines all subsystems)
void kb_mgt_process_key_event(key_def_t key, uint8_t row, uint8_t col,
                              bool pressed, uint32_t timestamp);

// Send final report after processing events.
void kb_mgt_finalize_processing(void);

#endif // KB_MGT_H
