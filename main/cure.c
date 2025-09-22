#include "common.h"
#include "ble_gap.h"
#include "hid_gatt_svr_svc.h"
#include "kb_matrix.h"

// Expose the pin arrays for debugging
extern const gpio_num_t row_pins[MATRIX_ROW];
extern const gpio_num_t col_pins[MATRIX_COL];

static const char *TAG = "DEV";

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED

void ble_host_tasks(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started!");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

#endif

void app_main(void) {
  esp_err_t ret;
#if HID_DEV_MODE == HIDD_IDLE_MODE
  ESP_LOGE(TAG, "Please turn on BT HID device or BLE!");
  return;
#endif

  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "setting hid gap, mode: %d", HID_DEV_MODE);
  ret = gap_init(HID_DEV_MODE);
  ESP_ERROR_CHECK(ret);

  ret = gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD);
  ESP_ERROR_CHECK(ret);

  ret = hid_svc_init();
  ESP_ERROR_CHECK(ret);

  ble_store_config_init();

  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ret = esp_nimble_enable(ble_host_tasks);
  if (ret) {
    ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
  }

  ret = gap_adv_start();
  ESP_ERROR_CHECK(ret);

  ret = matrix_init();
  ESP_ERROR_CHECK(ret);
}
