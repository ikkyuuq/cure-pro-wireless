/**
 * @file power.c
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

#include "power.h"
#include "config.h"
#include "indicator.h"
#include "utils.h"

static const char *TAG = "POWER";

// =============================================================================
// STATE VARIABLES
// =============================================================================

static TaskHandle_t task_hdl = NULL;

static power_state_t power_state = {
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
  adc_oneshot_unit_handle_t adc1_handle = NULL;

  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

  adc_oneshot_chan_cfg_t config = {
      .bitwidth = BATT_BIT_WIDTH,
      .atten = BATT_ADC_ATTEN,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc1_handle, BATT_ADC_CHAN, &config));

  int adc_raw = 0;
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATT_ADC_CHAN, &adc_raw));
  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));

  // Calculate voltage: raw * 3.3V / 4095 * divider ratio
  // Using integer math to avoid floating point
  uint32_t voltage_mv = (adc_raw * 3300 * BATT_VOLTAGE_DIVIDER) / (4095 * 100);

  ESP_LOGD(TAG, "RAW ADC: %d | Voltage: %lu mV | Divider: %d", adc_raw,
           voltage_mv, BATT_VOLTAGE_DIVIDER);

  return voltage_mv;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - POWER MONITORING TASK
// =============================================================================

static void task(void *pvParameters)
{
  ESP_LOGI(TAG, "Power task started");

  while (1)
  {
    // Read power status
    power_state.usb_powered = usb_serial_jtag_is_connected();
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

    vTaskDelay(pdMS_TO_TICKS(BATTERY_READ_INTERVAL_MS));
  }
}
