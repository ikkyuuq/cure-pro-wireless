/**
 * @file kb_matrix.c
 * @brief Keyboard Matrix Scanner
 *
 * Implements matrix scanning for mechanical keyboard switch detection.
 * Handles GPIO configuration, debouncing, and key event generation.
 *
 * Key responsibilities:
 * - Matrix GPIO initialization (rows as outputs, columns as inputs)
 * - Continuous matrix scanning with configurable interval
 * - Key state debouncing to filter electrical noise
 * - Key event generation and routing to keyboard management
 * - Support for both master and slave keyboard halves
 */

#include "kb_matrix.h"
#include "config.h"
#include "freertos/projdefs.h"
#include "kb_mgt.h"
#include "utils.h"
#include <stdint.h>

static const char *TAG = "MATRIX";

// =============================================================================
// STATE VARIABLES
// =============================================================================

static TaskHandle_t   task_hdl = NULL;
static matrix_state_t state;

// GPIO pin mappings
const gpio_num_t row_pins[MATRIX_ROW] = ROW_PINS;
const gpio_num_t col_pins[MATRIX_COL] = COL_PINS;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool       scan(key_event_t *event, uint8_t *event_count);
static void       set_row(uint8_t row, bool state);
static bool       read_col(uint8_t col);
static bool       read_raw_state(uint8_t row, uint8_t col);
static bool       read_current_state(uint8_t row, uint8_t col);
static bool       read_previous_state(uint8_t row, uint8_t col);
static mt_state_t read_state(uint8_t row, uint8_t col);
static void reset_and_track_key_state(bool key_state, uint8_t row, uint8_t col,
                                      uint32_t timestamp);
static void process_key_event(key_event_t *events, uint8_t *event_count);

// =============================================================================
// PUBLIC API - INITIALIZATION
// =============================================================================

esp_err_t matrix_init(void)
{
  esp_err_t ret = ESP_OK;

  // Configure row pins (outputs)
  for (int i = 0; i < MATRIX_ROW; i++)
  {
    gpio_config_t row_config = {.pin_bit_mask = (1ULL << row_pins[i]),
                                .mode = GPIO_MODE_OUTPUT,
                                .intr_type = GPIO_INTR_DISABLE,
                                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                .pull_up_en = GPIO_PULLUP_ENABLE};

    ret |= gpio_config(&row_config);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to setup GPIO config for rows");
      return ret;
    }
    gpio_set_level(row_pins[i], 1); // Default high
    gpio_set_drive_capability(row_pins[i], GPIO_DRIVE_CAP_3);
  }

  // Configure column pins (inputs with pull-ups)
  for (int i = 0; i < MATRIX_COL; i++)
  {
    gpio_config_t col_config = {
        .pin_bit_mask = (1ULL << col_pins[i]),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    ret |= gpio_config(&col_config);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to setup GPIO config for columns");
      return ret;
    }
  }

  // Initialize matrix state and keyboard management
  memset(&state, 0, sizeof(matrix_state_t));
  ret |= kb_mgt_init();

  ESP_LOGI(TAG, "Matrix scanner initialized");
  return ret;
}

// =============================================================================
// PUBLIC API - TASK CONTROL
// =============================================================================

void matrix_scan_start(void)
{
  task_hdl_init(&task_hdl, matrix_scan_task, "matrix_scan",
                MATRIX_SCAN_PRIORITY, MATRIX_TASK_STACK_SIZE, NULL);
  ESP_LOGI(TAG, "Matrix scanning started");
}

void matrix_scan_stop(void)
{
  task_hdl_cleanup(task_hdl);
  ESP_LOGI(TAG, "Matrix scanning stopped");
}

void matrix_scan_task(void *pvParameters)
{
  key_event_t events[MAX_KEYS];
  uint8_t     event_count;

  ESP_LOGI(TAG, "Matrix scan task started");

  while (1)
  {
    if (scan(events, &event_count))
    {
      ESP_LOGD(TAG, "*** KEY EVENT DETECTED: %d events ***", event_count);
      process_key_event(events, &event_count);
    }

    kb_mgt_proc_check_tap_timeouts(get_current_time_ms());
    vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
  }
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - GPIO CONTROL
// =============================================================================

static void set_row(uint8_t row, bool state)
{
  if (row < MATRIX_ROW)
  {
    gpio_set_level(row_pins[row], state ? 1 : 0);
  }
}

static bool read_col(uint8_t col)
{
  // Inverted due to pull-up resistors
  return col < MATRIX_COL ? !gpio_get_level(col_pins[col]) : false;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - STATE READERS
// =============================================================================

static bool read_raw_state(uint8_t row, uint8_t col)
{
  return (row < MATRIX_ROW && col < MATRIX_COL) ? state.raw[row][col] : false;
}

static bool read_current_state(uint8_t row, uint8_t col)
{
  return (row < MATRIX_ROW && col < MATRIX_COL) ? state.current[row][col]
                                                : false;
}

static bool read_previous_state(uint8_t row, uint8_t col)
{
  return (row < MATRIX_ROW && col < MATRIX_COL) ? state.previous[row][col]
                                                : false;
}

static mt_state_t read_state(uint8_t row, uint8_t col)
{
  bool raw = read_raw_state(row, col);
  bool current = read_current_state(row, col);
  bool previous = read_previous_state(row, col);
  bool pressed = read_col(col);

  mt_state_t state = {raw, current, previous, pressed};
  return state;
}

static void reset_and_track_key_state(bool key_state, uint8_t row, uint8_t col,
                                      uint32_t timestamp)
{
  state.raw[row][col] = key_state;
  state.debounce_time[row][col] = timestamp;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - MATRIX SCANNING
// =============================================================================

static bool scan(key_event_t *event, uint8_t *event_count)
{
  *event_count = 0;
  bool detected_changes = false;

  for (uint8_t row = 0; row < MATRIX_ROW; row++)
  {
    // Set the current row low, all others high
    for (uint8_t r = 0; r < MATRIX_ROW; r++)
    {
      set_row(r, r != row);
    }

    esp_rom_delay_us(GPIO_SETTLE_US);

    for (uint8_t col = 0; col < MATRIX_COL; col++)
    {
      mt_state_t mt_state = read_state(row, col);

      // Track state changes
      if (mt_state.pressed != mt_state.raw)
      {
        reset_and_track_key_state(mt_state.pressed, row, col,
                                  get_current_time_ms());
      }

      // Debounce check
      bool debounce_elapsed =
          (get_current_time_ms() - state.debounce_time[row][col]) >=
          DEBOUNCE_TIME_MS;
      bool state_actually_changes =
          debounce_elapsed && (mt_state.current != mt_state.raw);

      if (state_actually_changes)
      {
        state.previous[row][col] = mt_state.current;
        state.current[row][col] = mt_state.raw;

        if (*event_count < MAX_KEYS)
        {
          event[*event_count].col = col;
          event[*event_count].row = row;
          event[*event_count].pressed = mt_state.raw;
          event[*event_count].timestamp = get_current_time_ms();

          (*event_count)++;
          detected_changes = true;
        }
      }

      esp_rom_delay_us(GPIO_SETTLE_US);
    }

    esp_rom_delay_us(ROW_DELAY_US);
  }

  // Set all rows high when done scanning
  for (uint8_t r = 0; r < MATRIX_ROW; r++)
  {
    set_row(r, true);
  }

  return detected_changes;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - EVENT PROCESSING
// =============================================================================

static void process_key_event(key_event_t *events, uint8_t *event_count)
{
  kb_mgt_proc_check_tap_timeouts(get_current_time_ms());

  for (int i = 0; i < *event_count; i++)
  {
    // Mirror column mapping for slave half
#if !IS_MASTER
    key_def_t key = keymap_get_key(kb_mgt_layer_get_active(), events[i].row,
                                   MATRIX_COL - 1 - events[i].col);
#else
    key_def_t key =
        keymap_get_key(kb_mgt_layer_get_active(), events[i].row, events[i].col);
#endif

    kb_mgt_process_key_event(key, events[i].row, events[i].col,
                             events[i].pressed, get_current_time_ms());
  }

  kb_mgt_finalize_processing();
}
