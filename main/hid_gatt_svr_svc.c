#include "hid_gatt_svr_svc.h"
#include "kb_matrix.h"

static const char *TAG = "HID_SVC";

static hid_param_t s_ble_hid_param = {0};

esp_hidd_dev_t *hid_dev = NULL;

const unsigned char HID_REPORT_MAPS[] = {
    // Keyboard report (Report ID 1) - 7 bytes input + 1 byte output
    0x05,
    0x01, // Usage Page (Generic Desktop Ctrls)
    0x09,
    0x06, // Usage (Keyboard)
    0xA1,
    0x01, // Collection (Application)
    0x85,
    0x01, //   Report ID (1)
    0x05,
    0x07, //   Usage Page (Kbrd/Keypad)
    0x19,
    0xE0, //   Usage Minimum (0xE0)
    0x29,
    0xE7, //   Usage Maximum (0xE7)
    0x15,
    0x00, //   Logical Minimum (0)
    0x25,
    0x01, //   Logical Maximum (1)
    0x75,
    0x01, //   Report Size (1)
    0x95,
    0x08, //   Report Count (8)
    0x81,
    0x02, //   Input (Data,Var,Abs)
    0x95,
    0x01, //   Report Count (1)
    0x75,
    0x08, //   Report Size (8)
    0x81,
    0x03, //   Input (Const,Var,Abs)
    0x95,
    0x05, //   Report Count (5)
    0x75,
    0x01, //   Report Size (1)
    0x05,
    0x08, //   Usage Page (LEDs)
    0x19,
    0x01, //   Usage Minimum (Num Lock)
    0x29,
    0x05, //   Usage Maximum (Kana)
    0x91,
    0x02, //   Output (Data,Var,Abs)
    0x95,
    0x01, //   Report Count (1)
    0x75,
    0x03, //   Report Size (3)
    0x91,
    0x03, //   Output (Const,Var,Abs)
    0x95,
    0x05, //   Report Count (5)
    0x75,
    0x08, //   Report Size (8)
    0x15,
    0x00, //   Logical Minimum (0)
    0x25,
    0x65, //   Logical Maximum (101)
    0x05,
    0x07, //   Usage Page (Kbrd/Keypad)
    0x19,
    0x00, //   Usage Minimum (0x00)
    0x29,
    0x65, //   Usage Maximum (0x65)
    0x81,
    0x00, //   Input (Data,Array,Abs)
    0xC0, // End Collection

    // Consumer Control report (Report ID 2) - 2 bytes input
    0x05,
    0x0C, // Usage Page (Consumer)
    0x09,
    0x01, // Usage (Consumer Control)
    0xA1,
    0x01, // Collection (Application)
    0x85,
    0x02, //   Report ID (2)
    0x19,
    0x00, //   Usage Minimum (0)
    0x2A,
    0x3C,
    0x02, //   Usage Maximum (0x23C = 572)
    0x15,
    0x00, //   Logical Minimum (0)
    0x26,
    0x3C,
    0x02, //   Logical Maximum (0x23C = 572)
    0x95,
    0x01, //   Report Count (1)
    0x75,
    0x10, //   Report Size (16)
    0x81,
    0x00, //   Input (Data,Array,Abs)
    0xC0, // End Collection
          // Total: 65 + 25 = 90 bytes
};

static esp_hid_raw_report_map_t hid_report_maps[] = {
    {.data = HID_REPORT_MAPS, .len = sizeof(HID_REPORT_MAPS)},
};

static esp_hid_device_config_t ble_hid_config = {.vendor_id = 0x16C0,
                                                 .product_id = 0x05DF,
                                                 .version = 0x0100,
                                                 .device_name = "CureProWL",
                                                 .manufacturer_name = "Kppras",
                                                 .serial_number = "1234567890",
                                                 .report_maps = hid_report_maps,
                                                 .report_maps_len = 1};

esp_err_t hid_svc_init(void)
{
  ESP_LOGI(TAG, "Initialize HID Service");
  esp_err_t ret;
  ret = esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, NULL,
                          &s_ble_hid_param.hid_dev);
  if (ret != 0)
  {
    ESP_LOGE(TAG, "failed to init hid device, ret: %d", ret);
    return ret;
  }

  hid_dev = s_ble_hid_param.hid_dev;

  return ret;
}
