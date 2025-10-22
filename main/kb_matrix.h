#ifndef KB_MATRIX_H
#define KB_MATRIX_H

#include "common.h"
#include "config.h"
#include "keymap.h"
#include <stdbool.h>
#include <stdint.h>

extern esp_hidd_dev_t *hid_dev;

typedef struct {
  uint8_t   row;
  uint8_t   col;
  bool      pressed;
  uint32_t  timestamp;
} key_event_t;

typedef struct {
  bool      raw[MATRIX_ROW][MATRIX_COL];
  bool      current[MATRIX_ROW][MATRIX_COL];
  bool      previous[MATRIX_ROW][MATRIX_COL];
  uint32_t  debounce_time[MATRIX_ROW][MATRIX_COL];
} matrix_state_t;

typedef struct {
  bool      raw;
  bool      current;
  bool      previous;
  bool      pressed;
} mt_state_t;

esp_err_t matrix_init(void);
void matrix_scan_task(void *pvParameters);
void matrix_scan_start(void);
void matrix_scan_stop(void);

#endif
