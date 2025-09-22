#include "hid_gatt_svr_svc.h"
#include "kb_matrix.h"

static const char * TAG = "HID_SVC";

static hid_param_t s_ble_hid_param = {0};

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

void hid_task_start_up(void) {
  if (s_ble_hid_param.task_hdl) {
    return;
  }
  ESP_LOGI(TAG, "BLE HID Task Start Up");
  xTaskCreate(matrix_scan_task, "matrix_scan", MATRIX_TASK_STACK_SIZE, NULL, MATRIX_SCAN_PRIORITY, NULL);
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
  esp_hidd_event_t event = (esp_hidd_event_t)id;
  esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
  static const char *TAG = "HID_DEV_BLE";

  switch (event) {
  case ESP_HIDD_START_EVENT: {
    ESP_LOGI(TAG, "START");
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
      hid_task_start_up();
    } else {
      // suspend
      hid_task_shut_down();
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

    hid_task_shut_down();
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

void hid_task_shut_down(void) {
  if (s_ble_hid_param.task_hdl) {
    vTaskDelete(s_ble_hid_param.task_hdl);
    s_ble_hid_param.task_hdl = NULL;
  }
}

esp_err_t hid_svc_init(void) {
  ESP_LOGI(TAG, "Initialize HID Service");
  esp_err_t ret;
  ret = esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev);
  if (ret != 0) {
    ESP_LOGE(TAG, "failed to init hid device, ret: %d", ret);
    return ret;
  }

  hid_dev = s_ble_hid_param.hid_dev;

  return ret;
}

const unsigned char keyboardReportMap[] = {
    // 7 bytes input (modifiers, resrvd, keys*5), 1 byte output
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06, // Usage (Keyboard)
    0xA1, 0x01, // Collection (Application)
    0x85, 0x01, //   Report ID (1)
    0x05, 0x07, //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0, //   Usage Minimum (0xE0)
    0x29, 0xE7, //   Usage Maximum (0xE7)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x08, //   Report Count (8)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01, //   Report Count (1)
    0x75, 0x08, //   Report Size (8)
    0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05, //   Report Count (5)
    0x75, 0x01, //   Report Size (1)
    0x05, 0x08, //   Usage Page (LEDs)
    0x19, 0x01, //   Usage Minimum (Num Lock)
    0x29, 0x05, //   Usage Maximum (Kana)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01, //   Report Count (1)
    0x75, 0x03, //   Report Size (3)
    0x91, 0x03, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x05, //   Report Count (5)
    0x75, 0x08, //   Report Size (8)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x65, //   Logical Maximum (101)
    0x05, 0x07, //   Usage Page (Kbrd/Keypad)
    0x19, 0x00, //   Usage Minimum (0x00)
    0x29, 0x65, //   Usage Maximum (0x65)
    0x81, 0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, // End Collection

    // 65 bytes
};

