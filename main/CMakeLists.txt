idf_component_register(SRCS "cure.c" "ble_gap.c" "hid_gatt_svr_svc.c" "kb_matrix.c" "keymap.c" "espnow.c" "kb_mgt.c" "indicator.c" "power.c" "heartbeat.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt driver esp_wifi nvs_flash esp_hid esp_adc
)
