/**
 * @file battery.c
 * @brief Battery and Power Management
 *
 * Monitors battery voltage and charging status through ADC readings.
 * Updates battery indicator based on voltage levels and USB power status.
 *
 * Key responsibilities:
 * - USB-JTAG power detection
 * - Battery voltage measurement via ADC
 * - Charging state detection
 * - Battery level indication (good/low/critical)
 */

#include "battery.h"
#include "config.h"
#include "indicator.h"
#include "power_mgmt.h"
#include "utils.h"

static const char *TAG = "POWER";

// =============================================================================
// STATE VARIABLES
// =============================================================================

static TaskHandle_t task_hdl = NULL;

battery_power_state_t power_state = {
    .usb_powered = false,
    .voltage_charging = false,
    .battery_voltage_mv = 0,
};

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void     task(void *pvParameters);
static void     task_stop(void);
static uint32_t read_battery_voltage(void);

// =============================================================================
// PUBLIC API - INITIALIZATION
// =============================================================================

esp_err_t usb_power_init(void)
{
  esp_err_t ret = 0;

  ret =
      usb_serial_jtag_driver_install(&USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT());
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to install USB-JTAG driver: %d", ret);
    return ret;
  }

  ESP_LOGI(TAG, "USB-JTAG driver installed");
  return ESP_OK;
}

// =============================================================================
// PUBLIC API - TASK CONTROL
// =============================================================================

void power_task_start(void)
{
  task_hdl_init(&task_hdl, task, "power_task", POWER_PRIORITY,
                POWER_TASK_STACK_SIZE, NULL);
  ESP_LOGI(TAG, "Power monitoring started");
}

static void task_stop(void)
{
  task_hdl_cleanup(task_hdl);
  ESP_LOGI(TAG, "Power monitoring stopped");
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - BATTERY VOLTAGE READING
// =============================================================================

static uint32_t read_battery_voltage(void)
{
  ESP_LOGI(TAG, "Starting battery voltage read");
  esp_task_wdt_reset(); // Reset WDT before ADC operations

  adc_oneshot_unit_handle_t adc1_handle = NULL;
  esp_err_t                 ret;

  ESP_LOGI(TAG, "Initializing ADC unit");
  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT_1,
  };
  ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "ADC unit init failed: %d", ret);
    return 0;
  }
  esp_task_wdt_reset(); // Reset WDT after ADC init

  ESP_LOGI(TAG, "Configuring ADC channel");
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = BATT_BIT_WIDTH,
      .atten = BATT_ADC_ATTEN,
  };
  ret = adc_oneshot_config_channel(adc1_handle, BATT_ADC_CHAN, &config);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "ADC channel config failed: %d", ret);
    adc_oneshot_del_unit(adc1_handle);
    return 0;
  }
  esp_task_wdt_reset(); // Reset WDT after channel config

  ESP_LOGI(TAG, "Reading ADC value");
  int adc_raw = 0;
  ret = adc_oneshot_read(adc1_handle, BATT_ADC_CHAN, &adc_raw);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "ADC read failed: %d", ret);
  }
  esp_task_wdt_reset(); // Reset WDT after ADC read

  ESP_LOGI(TAG, "Cleaning up ADC unit");
  adc_oneshot_del_unit(adc1_handle);

  uint32_t voltage_mv = (adc_raw * BATT_VOLTAGE_REF * BATT_VOLTAGE_MULT) / 4095;

  ESP_LOGI(TAG, "RAW ADC: %d | Voltage: %lu mV | Multiplier: %d", adc_raw,
           voltage_mv, BATT_VOLTAGE_MULT);

  return voltage_mv;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - POWER MONITORING TASK
// =============================================================================

static void task(void *pvParameters)
{
  ESP_LOGI(TAG, "Power task started");

  // Subscribe to watchdog
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret == ESP_OK)
  {
    ESP_LOGI(TAG, "Power task subscribed to watchdog");
  }
  else
  {
    ESP_LOGW(TAG, "Failed to subscribe to watchdog: %d", wdt_ret);
  }

  // Time-based watchdog reset (handles 30s battery read interval)
  uint32_t       last_wdt_reset_time = get_current_time_ms();
  const uint32_t WDT_RESET_INTERVAL_MS = 2000; // Reset every 2 seconds
  uint32_t       loop_count = 0;

  ESP_LOGI(TAG, "Power task entering main loop");

  while (1)
  {
    loop_count++;

    // Reset WDT at the start of every loop
    esp_task_wdt_reset();
    last_wdt_reset_time = get_current_time_ms();

    // Debug log every 10 loops to see if task is running
    if (loop_count % 10 == 0)
    {
      ESP_LOGI(TAG, "Power task loop %lu, WDT reset at start of loop",
               loop_count);
    }

    // Read power status
    esp_task_wdt_reset(); // Reset before USB check
    power_state.usb_powered = usb_serial_jtag_is_connected();

    esp_task_wdt_reset(); // Reset before battery read
    power_state.battery_voltage_mv = read_battery_voltage();
    power_state.voltage_charging =
        (power_state.battery_voltage_mv > BATT_VOLTAGE_THRESHOLD_MV);

    // Update battery indicator based on state
    if (power_state.usb_powered || power_state.voltage_charging)
    {
      indicator_set_batt_state(BATT_STATE_CHARGING);
      ESP_LOGI(TAG, "Charging state detected");
    }
    else
    {
      if (power_state.battery_voltage_mv < BATT_VOLTAGE_CRITICAL_MV)
      {
        indicator_set_batt_state(BATT_STATE_CRITICAL);
        ESP_LOGI(TAG, "Critical battery voltage: %lu mV",
                 power_state.battery_voltage_mv);
      }
      else if (power_state.battery_voltage_mv < BATT_VOLTAGE_NOMINAL_MV)
      {
        indicator_set_batt_state(BATT_STATE_LOW);
        ESP_LOGI(TAG, "Low battery voltage: %lu mV",
                 power_state.battery_voltage_mv);
      }
      else
      {
        indicator_set_batt_state(BATT_STATE_GOOD);
        ESP_LOGD(TAG, "Good battery voltage: %lu mV",
                 power_state.battery_voltage_mv);
      }
    }

    // Update power management system with battery status
    power_mgmt_update_battery_status(power_state.battery_voltage_mv,
                                     power_state.usb_powered);

    // Time-based watchdog reset (independent of battery read interval)
    uint32_t current_time = get_current_time_ms();
    if ((current_time - last_wdt_reset_time) >= WDT_RESET_INTERVAL_MS)
    {
      ESP_LOGI(TAG, "Power task resetting watchdog after %lu ms",
               current_time - last_wdt_reset_time);
      esp_task_wdt_reset();
      last_wdt_reset_time = current_time;
    }

    // Get adaptive battery interval from power management
    uint32_t battery_interval = power_mgmt_get_battery_interval();
    ESP_LOGD(TAG, "Power task sleeping for %lu ms", battery_interval);

    // Break up long sleep into smaller chunks to reset WDT during sleep
    // Sleep in 1-second chunks to stay well within 5s WDT timeout
    const uint32_t SLEEP_CHUNK_MS = 1000;
    uint32_t       remaining_sleep = battery_interval;
    while (remaining_sleep > 0)
    {
      uint32_t sleep_time =
          (remaining_sleep > SLEEP_CHUNK_MS) ? SLEEP_CHUNK_MS : remaining_sleep;
      vTaskDelay(pdMS_TO_TICKS(sleep_time));
      esp_task_wdt_reset(); // Reset WDT after each 1-second chunk
      remaining_sleep -= sleep_time;
    }
  }
}
