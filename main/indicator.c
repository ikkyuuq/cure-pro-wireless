/**
 * @file indicator.c
 * @brief LED Indicator System for Keyboard Status
 *
 * Manages RGB LED indicators for connection and battery status visualization.
 * Provides visual feedback through solid colors and blinking patterns.
 *
 * Key responsibilities:
 * - Connection status indication (green=connected, blue blinking=waiting,
 * off=sleeping)
 * - Battery status indication (green=good, yellow=low, red blinking=critical,
 * blue=charging)
 * - LED strip initialization (RMT for connection, SPI for battery)
 * - Blinking pattern management
 */

#include "indicator.h"
#include "config.h"
#include "utils.h"

static const char *TAG = "INDICATOR";

// =============================================================================
// STATE VARIABLES
// =============================================================================

static SemaphoreHandle_t indicator_sem = NULL;
static TaskHandle_t      task_hdl = NULL;

// LED strip handles
static led_strip_handle_t batt_indicator_hdl = NULL;
static led_strip_handle_t conn_indicator_hdl = NULL;

// Blinking state
static bool    conn_blink_active = false;
static bool    batt_blink_active = false;
static color_t conn_blink_color = COLOR_OFF;
static color_t batt_blink_color = COLOR_OFF;

// Connection and Battery state
static conn_state_t current_conn_state = CONN_STATE_WAITING;
static batt_state_t current_batt_state = BATT_STATE_GOOD;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void set_color(color_t color, led_strip_handle_t hdl);
static void start_blinking(led_strip_handle_t hdl, color_t color);
static void stop_blinking(led_strip_handle_t hdl);
static void task(void *pvParameters);

// =============================================================================
// PUBLIC API - INITIALIZATION
// =============================================================================

esp_err_t indicator_init(void)
{
  esp_err_t ret = ESP_OK;

  indicator_sem = xSemaphoreCreateMutex();
  if (!indicator_sem)
  {
    ESP_LOGE(TAG, "Failed to create indicator mutex");
    return ESP_FAIL;
  }

  // Connection LED configuration (GPIO 8)
  led_strip_config_t connection_cfg = {.strip_gpio_num = CONN_LED_GPIO,
                                       .max_leds = 1,
                                       .led_model = LED_MODEL_SK6812,
                                       .color_component_format =
                                           LED_STRIP_COLOR_COMPONENT_FMT_GRB,
                                       .flags = {.invert_out = false}};

  // Battery LED configuration (GPIO 7)
  led_strip_config_t battery_cfg = {.strip_gpio_num = BATT_LED_GPIO,
                                    .max_leds = 1,
                                    .led_model = LED_MODEL_SK6812,
                                    .color_component_format =
                                        LED_STRIP_COLOR_COMPONENT_FMT_GRB,
                                    .flags = {.invert_out = false}};

  // RMT backend configuration for connection LED
  led_strip_rmt_config_t rmt_config = {.clk_src = RMT_CLK_SRC_DEFAULT,
                                       .resolution_hz =
                                           10 * 1000 * 1000, // 10MHz
                                       .mem_block_symbols = 64,
                                       .flags = {.with_dma = false}};

  ret = led_strip_new_rmt_device(&connection_cfg, &rmt_config,
                                 &conn_indicator_hdl);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to create connection LED strip: %s",
             esp_err_to_name(ret));
    return ret;
  }

  // SPI backend configuration for battery LED (to avoid RMT channel exhaustion)
  led_strip_spi_config_t spi_config = {.clk_src = SPI_CLK_SRC_DEFAULT,
                                       .spi_bus = SPI2_HOST,
                                       .flags = {.with_dma = false}};

  ret =
      led_strip_new_spi_device(&battery_cfg, &spi_config, &batt_indicator_hdl);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to create battery LED strip: %s",
             esp_err_to_name(ret));
    return ret;
  }

  // Clear and initialize LEDs
  led_strip_clear(conn_indicator_hdl);
  led_strip_clear(batt_indicator_hdl);
  led_strip_refresh(conn_indicator_hdl);
  led_strip_refresh(batt_indicator_hdl);

  // Start indicator task
  task_hdl_init(&task_hdl, task, "indicator_task", INDICATOR_PRIORITY,
                INDICATOR_TASK_STACK_SIZE, NULL);

  // Set initial state
  indicator_set_conn_state(CONN_STATE_WAITING);

  ESP_LOGI(TAG, "Indicator system initialized");
  return ESP_OK;
}

// =============================================================================
// PUBLIC API - STATE SETTERS & GETTERS
// =============================================================================

conn_state_t indicator_get_conn_state(void) { return current_conn_state; }

batt_state_t indicator_get_batt_state(void) { return current_batt_state; }

void indicator_set_conn_state(conn_state_t state)
{
  if (current_conn_state == state)
    return;

  current_conn_state = state;
  const color_t off_color = COLOR_OFF;
  const color_t green_color = COLOR_GREEN;
  const color_t blue_color = COLOR_BLUE;

  switch (state)
  {
  case CONN_STATE_CONNECTED:
    stop_blinking(conn_indicator_hdl);
    set_color(green_color, conn_indicator_hdl);
    ESP_LOGI(TAG, "Connection state: CONNECTED (Green)");
    break;

  case CONN_STATE_WAITING:
    start_blinking(conn_indicator_hdl, blue_color);
    ESP_LOGI(TAG, "Connection state: WAITING (Blue blinking)");
    break;

  case CONN_STATE_SLEEPING:
    stop_blinking(conn_indicator_hdl);
    set_color(off_color, conn_indicator_hdl);
    ESP_LOGI(TAG, "Connection state: SLEEPING (Off)");
    break;
  }
}

void indicator_set_batt_state(batt_state_t state)
{
  if (indicator_get_batt_state() == state)
    return;

  current_batt_state = state;
  const color_t green_color = COLOR_GREEN;
  const color_t yellow_color = COLOR_YELLOW;
  const color_t red_color = COLOR_RED;
  const color_t blue_color = COLOR_BLUE;

  switch (state)
  {
  case BATT_STATE_GOOD:
    stop_blinking(batt_indicator_hdl);
    set_color(green_color, batt_indicator_hdl);
    ESP_LOGI(TAG, "Battery state: GOOD (Green)");
    break;

  case BATT_STATE_LOW:
    stop_blinking(batt_indicator_hdl);
    set_color(yellow_color, batt_indicator_hdl);
    ESP_LOGI(TAG, "Battery state: LOW (Yellow)");
    break;

  case BATT_STATE_CRITICAL:
    start_blinking(batt_indicator_hdl, red_color);
    ESP_LOGI(TAG, "Battery state: CRITICAL (Red blinking)");
    break;

  case BATT_STATE_CHARGING:
    stop_blinking(batt_indicator_hdl);
    set_color(blue_color, batt_indicator_hdl);
    ESP_LOGI(TAG, "Battery state: CHARGING (Blue)");
    break;
  }
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - LED CONTROL
// =============================================================================

static void set_color(color_t color, led_strip_handle_t hdl)
{
  if (!hdl)
    return;

  led_strip_set_pixel(hdl, 0, color.red, color.green, color.blue);
  led_strip_refresh(hdl);
}

static void start_blinking(led_strip_handle_t hdl, color_t color)
{
  if (indicator_sem && xSemaphoreTake(indicator_sem, portMAX_DELAY) == pdTRUE)
  {
    if (hdl == conn_indicator_hdl)
    {
      conn_blink_active = true;
      conn_blink_color = color;
    }
    else if (hdl == batt_indicator_hdl)
    {
      batt_blink_active = true;
      batt_blink_color = color;
    }
    xSemaphoreGive(indicator_sem);
  }
}

static void stop_blinking(led_strip_handle_t hdl)
{
  if (!hdl)
    return;

  if (indicator_sem && xSemaphoreTake(indicator_sem, portMAX_DELAY) == pdTRUE)
  {
    if (hdl == conn_indicator_hdl)
    {
      conn_blink_active = false;
      led_strip_clear(hdl);
      led_strip_refresh(hdl);
    }
    else if (hdl == batt_indicator_hdl)
    {
      batt_blink_active = false;
      led_strip_clear(hdl);
      led_strip_refresh(hdl);
    }
    xSemaphoreGive(indicator_sem);
  }
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - INDICATOR TASK
// =============================================================================

static void task(void *pvParameters)
{
  bool     blink_state = false;
  uint32_t last_blink_time = 0;

  ESP_LOGI(TAG, "Indicator task started");

  while (1)
  {
    // Handle blinking logic for both LEDs
    if (get_current_time_ms() - last_blink_time >= BLINK_INTERVAL_MS)
    {
      blink_state = !blink_state;

      // Connection LED blinking
      if (conn_blink_active)
      {
        if (blink_state)
        {
          set_color(conn_blink_color, conn_indicator_hdl);
        }
        else
        {
          color_t off_color = COLOR_OFF;
          set_color(off_color, conn_indicator_hdl);
        }
      }

      // Battery LED blinking
      if (batt_blink_active)
      {
        if (blink_state)
        {
          set_color(batt_blink_color, batt_indicator_hdl);
        }
        else
        {
          color_t off_color = COLOR_OFF;
          set_color(off_color, batt_indicator_hdl);
        }
      }

      last_blink_time = get_current_time_ms();
    }

    vTaskDelay(pdMS_TO_TICKS(BLINK_INTERVAL_MS));
  }
}
