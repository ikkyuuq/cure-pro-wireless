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

esp_err_t matrix_init(void);
void matrix_scan_task(void *pvParameters);

#endif
