#ifndef KB_MGT_H
#define KB_MGT_H

#include "common.h"
#include "config.h"
#include "keymap.h"

// Forward declaration to avoid circular dependency with espnow.h
struct esp_hidd_dev_s;
typedef struct esp_hidd_dev_s esp_hidd_dev_t;
extern esp_hidd_dev_t *hid_dev;

// HID Configuration Constants
#define HID_MAX_KEYS_IN_REPORT  6
#define HID_KEY_SHIFT_LAST_IDX  5

// Key processing result types
typedef enum {
    KB_MGT_RESULT_SUCCESS,
    KB_MGT_RESULT_REPORT_FULL,
    KB_MGT_RESULT_KEY_NOT_FOUND,
    KB_MGT_RESULT_INVALID_PARAM,
    KB_MGT_RESULT_ERROR
} kb_mgt_result_t;

// Communication event types for ESP-NOW
typedef enum {
    KB_COMM_EVENT_TAP,
    KB_COMM_EVENT_BRIEF_TAP,
    KB_COMM_EVENT_LAYER_SYNC,
    KB_COMM_EVENT_LAYER_DESYNC,
    KB_COMM_EVENT_MOD_SYNC,
    KB_COMM_EVENT_MOD_DESYNC
} kb_comm_event_t;

// Forward declarations
typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[HID_MAX_KEYS_IN_REPORT];
} kb_mgt_hid_report_t;

typedef struct {
    uint8_t           current_layer;
    uint32_t          layer_tap_timer[MATRIX_ROW][MATRIX_COL];
    uint32_t          mod_tap_timer[MATRIX_ROW][MATRIX_COL];
    key_definition_t  pressed_keys[MATRIX_ROW][MATRIX_COL];
    bool              key_is_tapped[MATRIX_ROW][MATRIX_COL];
    bool              layer_momentary_active[MAX_LAYERS];
    bool              pressed_key_active[MATRIX_ROW][MATRIX_COL];
} kb_mgt_processor_state_t;

// =============================================================================
// HID MANAGEMENT
// =============================================================================

// Initialize HID management
esp_err_t kb_mgt_hid_init(void);

// Get current HID report
kb_mgt_hid_report_t* kb_mgt_hid_get_current_report(void);

// Add key to HID report
kb_mgt_result_t kb_mgt_hid_add_key(uint8_t keycode);

// Remove key from HID report
void kb_mgt_hid_remove_key(uint8_t keycode);

// Set modifier in HID report
void kb_mgt_hid_set_modifier(uint8_t modifier);

// Clear modifier from HID report
void kb_mgt_hid_clear_modifier(uint8_t modifier);

// Send HID report (only if master)
void kb_mgt_hid_send_report(void);

// Clear entire HID report
void kb_mgt_hid_clear_report(void);

// Sync modifier from remote half
void kb_mgt_sync_modifier(uint8_t modifier);

// Desync modifier from remote half
void kb_mgt_desync_modifier(uint8_t modifier);

// =============================================================================
// LAYER MANAGEMENT
// =============================================================================

// Initialize layer management
esp_err_t kb_mgt_layer_init(void);

// Get current active layer
uint8_t kb_mgt_layer_get_active(void);

// Set base layer
void kb_mgt_layer_set_base(uint8_t layer);

// Activate momentary layer
void kb_mgt_layer_activate_momentary(uint8_t layer);

// Deactivate momentary layer
void kb_mgt_layer_deactivate_momentary(uint8_t layer);

// Toggle layer
void kb_mgt_layer_toggle(uint8_t layer);

// Check if layer is momentarily active
bool kb_mgt_layer_is_momentary_active(uint8_t layer);

// Sync layer activation from remote half
void kb_mgt_sync_layer(uint8_t layer);

// Desync layer activation from remote half
void kb_mgt_desync_layer(uint8_t layer);

// =============================================================================
// KEY PROCESSOR MANAGEMENT
// =============================================================================

// Initialize key processor
esp_err_t kb_mgt_processor_init(void);

// Get processor state (for access to timers, etc.)
kb_mgt_processor_state_t* kb_mgt_processor_get_state(void);

// Process a key press event
void kb_mgt_processor_handle_press(key_definition_t key, uint8_t row, uint8_t col, uint32_t timestamp);

// Process a key release event
void kb_mgt_processor_handle_release(uint8_t row, uint8_t col, uint32_t timestamp);

// Check and handle tap timeouts
void kb_mgt_processor_check_tap_timeouts(uint32_t current_time);

// Store pressed key for release processing
void kb_mgt_processor_store_pressed_key(uint8_t row, uint8_t col, key_definition_t key);

// Get stored pressed key
key_definition_t kb_mgt_processor_get_stored_key(uint8_t row, uint8_t col);

// Check if key position has stored key
bool kb_mgt_processor_has_stored_key(uint8_t row, uint8_t col);

// Clear stored key
void kb_mgt_processor_clear_stored_key(uint8_t row, uint8_t col);

// =============================================================================
// COMMUNICATION MANAGEMENT
// =============================================================================

// Initialize communication management
esp_err_t kb_mgt_comm_init(void);

// Send communication event to other half
void kb_mgt_comm_send_event(kb_comm_event_t event_type, void* data);

// Handle brief tap (tap and release immediately)
void kb_mgt_comm_handle_brief_tap(uint8_t keycode);

// =============================================================================
// MAIN MANAGEMENT INTERFACE
// =============================================================================

// Initialize all keyboard management subsystems
esp_err_t kb_mgt_init(void);

// Process complete key event (combines all subsystems)
void kb_mgt_process_key_event(key_definition_t key, uint8_t row, uint8_t col, bool pressed, uint32_t timestamp);

// Send final report after processing events.
void kb_mgt_finalize_processing(void);

#endif // KB_MGT_H
