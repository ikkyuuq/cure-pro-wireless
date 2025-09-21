#include "common.h"
#include "esp_err.h"
#include "gap.h"
#include "hid_svc.h"
#include "kb_matrix.h"

// Expose the pin arrays for debugging
extern const gpio_num_t row_pins[MATRIX_ROW];
extern const gpio_num_t col_pins[MATRIX_COL];

static const char *TAG = "DEV";

#define DEVICE_NAME "CureProWL"

typedef struct {
  TaskHandle_t task_hdl;
  esp_hidd_dev_t *hid_dev;
  uint8_t protocol_mode;
  uint8_t *buffer;
} local_param_t;

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
static local_param_t s_ble_hid_param = {0};

// Expose HID device for kb_matrix.c
esp_hidd_dev_t *hid_dev = NULL;

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {.data = keyboardReportMap, .len = sizeof(keyboardReportMap)}};

static esp_hid_device_config_t ble_hid_config = {.vendor_id = 0x16C0,
                                                 .product_id = 0x05DF,
                                                 .version = 0x0100,
                                                 .device_name = "CureProWL",
                                                 .manufacturer_name = "Kppras",
                                                 .serial_number = "1234567890",
                                                 .report_maps = ble_report_maps,
                                                 .report_maps_len = 1};

void ble_hid_task_start_up(void) {
  if (s_ble_hid_param.task_hdl) {
    return;
  }
  ESP_LOGI(TAG, "BLE HID Task Start Up");
  xTaskCreate(matrix_scan_task, "matrix_scan", 4 * 1024, NULL, 5, NULL);
}

void ble_hid_task_shut_down(void) {
  if (s_ble_hid_param.task_hdl) {
    vTaskDelete(s_ble_hid_param.task_hdl);
    s_ble_hid_param.task_hdl = NULL;
  }
}

void ble_hid_device_host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started!");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base,
                                    int32_t id, void *event_data) {
  esp_hidd_event_t event = (esp_hidd_event_t)id;
  esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
  static const char *TAG = "HID_DEV_BLE";

  switch (event) {
  case ESP_HIDD_START_EVENT: {
    ESP_LOGI(TAG, "START");
    esp_hid_ble_gap_adv_start();
    break;
  }
  case ESP_HIDD_CONNECT_EVENT: {
    ESP_LOGI(TAG, "CONNECT");
    break;
  }
  case ESP_HIDD_PROTOCOL_MODE_EVENT: {
    ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index,
             param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
    break;
  }
  case ESP_HIDD_CONTROL_EVENT: {
    ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index,
             param->control.control ? "EXIT_" : "");
    if (param->control.control) {
      // exit suspend
      ble_hid_task_start_up();
    } else {
      // suspend
      ble_hid_task_shut_down();
    }
    break;
  }
  case ESP_HIDD_OUTPUT_EVENT: {
    ESP_LOGI(TAG,
             "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index,
             esp_hid_usage_str(param->output.usage), param->output.report_id,
             param->output.length);
    ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
    break;
  }
  case ESP_HIDD_FEATURE_EVENT: {
    ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:",
             param->feature.map_index, esp_hid_usage_str(param->feature.usage),
             param->feature.report_id, param->feature.length);
    ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
    break;
  }
  case ESP_HIDD_DISCONNECT_EVENT: {
    ESP_LOGI(TAG, "DISCONNECT: %s",
             esp_hid_disconnect_reason_str(
                 esp_hidd_dev_transport_get(param->disconnect.dev),
                 param->disconnect.reason));

    // Clear all bonds to allow fresh pairing
    ble_store_clear();
    ESP_LOGI(TAG, "Cleared bond store for fresh pairing");

    ble_hid_task_shut_down();
    esp_hid_ble_gap_adv_start();
    break;
  }
  case ESP_HIDD_STOP_EVENT: {
    ESP_LOGI(TAG, "STOP");
    break;
  }
  default:
    break;
  }
  return;
}

void ble_store_config_init(void);

#endif

void app_main(void) {
  esp_log_level_set("*", ESP_LOG_DEBUG);
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
  ret = esp_hid_gap_init(HID_DEV_MODE);
  ESP_ERROR_CHECK(ret);

  ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, DEVICE_NAME);
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "setting ble device");
  ESP_ERROR_CHECK(esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE,
                                    ble_hidd_event_callback,
                                    &s_ble_hid_param.hid_dev));

  // Expose HID device globally for matrix processing
  hid_dev = s_ble_hid_param.hid_dev;

  ble_store_config_init();

  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ret = esp_nimble_enable(ble_hid_device_host_task);
  if (ret) {
    ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
  }

  // Initialize matrix scanning
  ESP_LOGE(TAG, "=== STARTING MATRIX INITIALIZATION ===");
  esp_err_t matrix_ret = matrix_init();
  ESP_LOGE(TAG, "Matrix init result: %d", matrix_ret);
}
