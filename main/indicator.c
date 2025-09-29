#include "indicator.h"
#include "config.h"
#include "handler.h"

static const char *TAG = "INDICATOR";

TaskHandle_t indicator_task_hdl = NULL;

// LED handles
led_strip_handle_t batt_indicator_hdl = NULL;
led_strip_handle_t conn_indicator_hdl = NULL;

// State variables
conn_state_t current_conn_state = CONN_STATE_WAITING;
batt_state_t current_batt_state = BATT_STATE_GOOD;

// Task and timer handles
static bool conn_blink_active = false;
static bool batt_blink_active = false;
static color_t conn_blink_color = COLOR_OFF;
static color_t batt_blink_color = COLOR_OFF;

esp_err_t indicator_init(void) {
  esp_err_t ret = ESP_OK;

  // Connection LED configuration (GPIO 8)
  led_strip_config_t connection_cfg = {
      .strip_gpio_num = CONN_LED_GPIO,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags = {.invert_out = false}};

  // Battery LED configuration (GPIO 15)
  // led_strip_config_t battery_cfg = {
  //     .strip_gpio_num = GPIO_NUM_15,
  //     .max_leds = 1,
  //     .led_model = LED_MODEL_WS2812,
  //     .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
  //     .flags = {.invert_out = false}};

  // RMT backend configuration
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
      .mem_block_symbols = 64,
      .flags = {.with_dma = false}};

  ret = led_strip_new_rmt_device(&connection_cfg, &rmt_config, &conn_indicator_hdl);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create connection LED strip: %s", esp_err_to_name(ret));
    return ret;
  }

//   ret = led_strip_new_rmt_device(&battery_cfg, &rmt_config, &batt_indicator_hdl);
//   if (ret != ESP_OK) {
// 
//     ESP_LOGE(TAG, "Failed to create battery LED strip: %s", esp_err_to_name(ret));
// #endif
//     return ret;
//   }

  // Clear both LEDs initially
  led_strip_clear(conn_indicator_hdl);
  // led_strip_clear(batt_indicator_hdl);

  led_strip_refresh(conn_indicator_hdl);
  // led_strip_refresh(batt_indicator_hdl);

  xTaskCreate(indicator_task, "indicator_task", 4096, NULL, INDICATOR_PRIORITY, &indicator_task_hdl);

  indicator_set_conn_state(CONN_STATE_WAITING);

  ESP_LOGI(TAG, "Indicator system initialized");
  return ESP_OK;
}

void set_color(color_t color, led_strip_handle_t hdl) {
  if (!hdl) return;

  led_strip_set_pixel(hdl, 0, color.red, color.green, color.blue);
  led_strip_refresh(hdl);
}

void start_blinking(led_strip_handle_t hdl, color_t color) {
  if (hdl == conn_indicator_hdl) {
    conn_blink_active = true;
    conn_blink_color = color;
  } else if (hdl == batt_indicator_hdl) {
    batt_blink_active = true;
    batt_blink_color = color;
  }
}

void stop_blinking(led_strip_handle_t hdl) {
  if (!hdl) return;  // Check for NULL handle

  if (hdl == conn_indicator_hdl) {
    conn_blink_active = false;
    led_strip_clear(hdl);
    led_strip_refresh(hdl);
  } else if (hdl == batt_indicator_hdl) {
    batt_blink_active = false;
    led_strip_clear(hdl);
    led_strip_refresh(hdl);
  }
}

void indicator_set_conn_state(conn_state_t state) {
  if (current_conn_state == state) return;

  current_conn_state = state;
  color_t off_color = COLOR_OFF;
  color_t green_color = COLOR_GREEN;
  color_t blue_color = COLOR_BLUE;

  switch (state) {
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

void indicator_set_batt_state(batt_state_t state) {
  if (current_batt_state == state) return;

  current_batt_state = state;
  color_t green_color = COLOR_GREEN;
  color_t yellow_color = COLOR_YELLOW;
  color_t red_color = COLOR_RED;
  color_t blue_color = COLOR_BLUE;

  switch (state) {
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


void indicator_task(void *pvParameters) {
  bool blink_state = false;
  uint32_t last_blink_time = 0;

  ESP_LOGI(TAG, "Indicator wrapper task started");

  while (1) {
    uint32_t current_time = esp_timer_get_time() / 1000;

    // Handle blinking logic for both LEDs
    if (current_time - last_blink_time >= BLINK_INTERVAL_MS) {
      blink_state = !blink_state;

      if (conn_blink_active) {
        if (blink_state) {
          set_color(conn_blink_color, conn_indicator_hdl);
        } else {
          color_t off_color = COLOR_OFF;
          set_color(off_color, conn_indicator_hdl);
        }
      }

      if (batt_blink_active) {
        if (blink_state) {
          set_color(batt_blink_color, batt_indicator_hdl);
        } else {
          color_t off_color = COLOR_OFF;
          set_color(off_color, batt_indicator_hdl);
        }
      }

      last_blink_time = current_time;
    }

    vTaskDelay(pdMS_TO_TICKS(BLINK_INTERVAL_MS));
  }
}
