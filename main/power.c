#include "power.h"
#include "config.h"
#include "indicator.h"
#include "handler.h"

TaskHandle_t power_task_hdl = NULL;

static const char *TAG = "POWER";

power_state_t power_state = {
  .usb_powered = false,
  .voltage_charging = false,
  .battery_voltage_mv = 0,
};

esp_err_t usb_power_init(void) {
  esp_err_t ret;

  ret = usb_serial_jtag_driver_install(&USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT());
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install USB-JTAG driver: %d", ret);
    return ret;
  }

  ESP_LOGI(TAG, "USB-JTAG driver installed");

  return ESP_OK;
}

uint32_t read_battery_voltage(void) {
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  adc_oneshot_chan_cfg_t config = {
    .bitwidth = BATT_BIT_WIDTH,
    .atten = BATT_ADC_ATTEN,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATT_ADC_CHAN, &config));

  int adc_raw;
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATT_ADC_CHAN, &adc_raw));

  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));

  uint32_t voltage_mv = (adc_raw * 3300 * 2) / 4095;

  ESP_LOGD(TAG, "Battery voltage: %lu mV (ADC: %d)", voltage_mv, adc_raw);

  return voltage_mv;
}

void power_task_start(void) {
  if (!power_task_hdl) {
    xTaskCreate(power_task, "power_task", POWER_TASK_STACK_SIZE, NULL, POWER_PRIORITY, &power_task_hdl);
  }
}

void power_task_stop(void) {
  if (power_task_hdl) {
    vTaskDelete(power_task_hdl);
    power_task_hdl = NULL;
  }
}

void power_task(void *pvParameters) {
  ESP_LOGI(TAG, "Power task started");

  while (1) {
    power_state.usb_powered = usb_serial_jtag_is_connected();
    power_state.battery_voltage_mv = read_battery_voltage();
    power_state.voltage_charging = (power_state.battery_voltage_mv > BATT_VOLTAGE_THRESHOLD_MV);

    // Charging if either USB is connected || Voltage not in nominal value
    if (power_state.usb_powered || power_state.voltage_charging) {
      indicator_set_batt_state(BATT_STATE_CHARGING);
      ESP_LOGI(TAG, "Charging state detected");
    } else {
      if (power_state.battery_voltage_mv < BATT_VOLTAGE_CRITICAL_MV) {
        indicator_set_batt_state(BATT_STATE_CRITICAL);
        ESP_LOGI(TAG, "Critical battery voltage detected");
      } else if (power_state.battery_voltage_mv < BATT_VOLTAGE_NOMINAL_MV) {
        indicator_set_batt_state(BATT_STATE_LOW);
        ESP_LOGI(TAG, "Low battery voltage detected");
      } else {
        indicator_set_batt_state(BATT_STATE_GOOD);
        ESP_LOGI(TAG, "Good battery voltage detected");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(BATTERY_READ_INTERVAL_MS));
  }
}
