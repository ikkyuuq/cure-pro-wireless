idf_component_register(SRCS "cure.c" "gap.c" "hid_svc.c" "kb_matrix.c" "keymap.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt driver esp_wifi nvs_flash esp_hid
)
