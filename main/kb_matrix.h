#ifndef KB_MATRIX_H
#define KB_MATRIX_H

#include "common.h"
#include "config.h"
#include "keymap.h"

extern esp_hidd_dev_t *hid_dev;

typedef struct {
  uint8_t   row;
  uint8_t   col;
  bool      pressed;
  uint32_t  timestamp;
} key_event_t;

typedef struct {
  bool      raw_state[MATRIX_ROW][MATRIX_COL];
  bool      current_state[MATRIX_ROW][MATRIX_COL];
  bool      previous_state[MATRIX_ROW][MATRIX_COL];
  uint32_t  debounce_time[MATRIX_ROW][MATRIX_COL];
} matrix_state_t;

typedef struct {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
} hid_keyboard_report_t;

typedef struct {
  uint8_t           current_layer;
  uint8_t           active_modifiers;
  uint32_t          layer_tap_timer[MATRIX_ROW][MATRIX_COL];
  uint32_t          mod_tap_timer[MATRIX_ROW][MATRIX_COL];
  key_definition_t  pressed_keys[MATRIX_ROW][MATRIX_COL];
  bool              key_is_tapped[MATRIX_ROW][MATRIX_COL];
  bool              layer_momentary_active[MAX_LAYERS];
  bool              pressed_key_active[MATRIX_ROW][MATRIX_COL];
} key_processor_t;

esp_err_t matrix_init(void);
void matrix_scan_task(void *pvParameters);
bool matrix_scan(key_event_t *event, uint8_t *event_count);
void matrix_set_row(uint8_t row, bool state);
bool matrix_read_col(uint8_t col);
void hid_process_key_event(key_event_t *events, uint8_t *event_count);

#endif
