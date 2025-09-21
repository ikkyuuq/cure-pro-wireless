#ifndef GAP_H
#define GAP_H

#include "common.h"

#if CONFIG_BT_NIMBLE_ENABLED
#define HIDD_IDLE_MODE 0x00
#define HIDD_BLE_MODE 0x01

#define HID_DEV_MODE HIDD_BLE_MODE

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_sm.h"
#include "host/ble_hs_adv.h"
#include "host/ble_store.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

esp_err_t esp_hid_gap_init(uint8_t mode);

esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name);
esp_err_t esp_hid_ble_gap_adv_start(void);

#endif
#endif
