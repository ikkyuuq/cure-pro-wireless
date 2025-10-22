/**
 * @file ble_gap.c
 * @brief BLE GAP (Generic Access Profile) Manager
 * 
 * Manages Bluetooth Low Energy advertising, connection handling, and pairing.
 * Handles BLE connection lifecycle, security configuration, and advertising parameters.
 * 
 * Key responsibilities:
 * - BLE advertising initialization and management
 * - GAP event handling (connect, disconnect, pairing)
 * - Connection parameter updates for low latency
 * - Security and bonding configuration
 * - Integration with keyboard matrix and indicators
 */

#include "ble_gap.h"
#include "esp_bt.h"
#include "espnow.h"
#include "kb_matrix.h"
#include "indicator.h"

static const char *TAG = "GAP";

#if CONFIG_BT_NIMBLE_ENABLED

// =============================================================================
// CONSTANTS AND MACROS
// =============================================================================

#define GATT_SVR_SVC_HID_UUID 0x1812
#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(*a))

// Semaphore control macros
#define WAIT_BT_CB()  xSemaphoreTake(bt_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BT_CB()  xSemaphoreGive(bt_hidh_cb_semaphore)
#define WAIT_BLE_CB() xSemaphoreTake(ble_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BLE_CB() xSemaphoreGive(ble_hidh_cb_semaphore)

// =============================================================================
// STATE VARIABLES
// =============================================================================

static struct ble_hs_adv_fields fields;

static SemaphoreHandle_t bt_hidh_cb_semaphore  = NULL;
static SemaphoreHandle_t ble_hidh_cb_semaphore = NULL;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static int gap_event_cb(struct ble_gap_event *event, void *arg);
static esp_err_t init_low_level(uint8_t mode);

// =============================================================================
// PUBLIC API - ADVERTISING INITIALIZATION
// =============================================================================

esp_err_t gap_adv_init(uint16_t appearance) {
  ble_uuid16_t *uuid16, *uuid16_1;
  /**
   *  Set the advertisement data included in our advertisements:
   *     - Flags (indicates advertisement type and other general info).
   *     - Advertising tx power.
   *     - Device name.
   *     - 16-bit service UUIDs (HID).
   */

  memset(&fields, 0, sizeof fields);

  fields.name = (uint8_t *)DEVICE_NAME;
  fields.name_len = strlen(DEVICE_NAME);
  fields.name_is_complete = 1;

  /* Advertise two flags:
   *     - Discoverability in forthcoming advertisement (general)
   *     - BLE-only (BR/EDR unsupported).
   */
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  fields.appearance = appearance;
  fields.appearance_is_present = 1;

  /* Indicate that the TX power level field should be included; have the
   * stack fill this value automatically.  This is done by assigning the
   * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
   */
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  /* Use default info to advertising as GATT_SVR_SVC_HID */
  uuid16 = (ble_uuid16_t *)malloc(sizeof(ble_uuid16_t));
  uuid16_1 = (ble_uuid16_t[]){BLE_UUID16_INIT(GATT_SVR_SVC_HID_UUID)};

  memcpy(uuid16, uuid16_1, sizeof(ble_uuid16_t));
  fields.uuids16 = uuid16;
  fields.num_uuids16 = 1;
  fields.uuids16_is_complete = 1;

  /* Initialize the security configuration */
  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 0;
  ble_hs_cfg.sm_our_key_dist =
      BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
  ble_hs_cfg.sm_their_key_dist |=
      BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;

  return ESP_OK;
}

// =============================================================================
// PUBLIC API - ADVERTISING CONTROL
// =============================================================================

esp_err_t gap_adv_start(void) {
  int rc;
  struct ble_gap_adv_params adv_params;
  int32_t adv_duration_ms = 180000;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
    return rc;
  }

  /* Begin Advertising */
  memset(&adv_params, 0, sizeof adv_params);
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
  adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
  rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, adv_duration_ms,
                         &adv_params, gap_event_cb, NULL);

  if (rc != 0) {
    MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
    return rc;
  }
  return rc;
}

// =============================================================================
// PUBLIC API - GAP INITIALIZATION
// =============================================================================

esp_err_t gap_init(uint8_t mode) {
  esp_err_t ret;
  if (!mode || mode > ESP_BT_MODE_BTDM) {
    ESP_LOGE(TAG, "Invalid mode given!");
    return ESP_FAIL;
  }

  if (bt_hidh_cb_semaphore != NULL) {
    ESP_LOGE(TAG, "Already initialised");
    return ESP_FAIL;
  }

  bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
  if (bt_hidh_cb_semaphore == NULL) {
    ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
    return ESP_FAIL;
  }

  ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
  if (ble_hidh_cb_semaphore == NULL) {
    ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
    vSemaphoreDelete(bt_hidh_cb_semaphore);
    bt_hidh_cb_semaphore = NULL;
    return ESP_FAIL;
  }

  ret = init_low_level(mode);
  if (ret != ESP_OK) {
    vSemaphoreDelete(bt_hidh_cb_semaphore);
    bt_hidh_cb_semaphore = NULL;
    vSemaphoreDelete(ble_hidh_cb_semaphore);
    ble_hidh_cb_semaphore = NULL;
    return ret;
  }

  return ESP_OK;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - GAP EVENT HANDLERS
// =============================================================================

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
  struct ble_gap_conn_desc desc;
  int rc;

  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    /* A new connection was established or a connection attempt failed. */
    ESP_LOGI(TAG, "connection %s; status=%d",
             event->connect.status == 0 ? "established" : "failed",
             event->connect.status);

    if (event->connect.status == 0) {
      struct ble_gap_upd_params params = {
        .itvl_min = 6,
        .itvl_max = 9,
        .latency = 0,
        .supervision_timeout = 100
      };
      int rc = ble_gap_update_params(event->connect.conn_handle, &params);
      if (rc != 0) {
        ESP_LOGW(TAG, "Failed to request low latency params; rc=%d", rc);
      }
      matrix_scan_start();
      bool conn_state = true;
      send_to_espnow(MASTER, CONN, &conn_state);
      indicator_set_conn_state(CONN_STATE_CONNECTED);
    } else {
      matrix_scan_stop();
      bool conn_state = false;
      send_to_espnow(MASTER, CONN, &conn_state);
      indicator_set_conn_state(CONN_STATE_WAITING);
    }
    return 0;
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);

    matrix_scan_stop();
    bool conn_state = false;
    send_to_espnow(MASTER, CONN, &conn_state);
    indicator_set_conn_state(CONN_STATE_WAITING);

    gap_adv_start();
    return 0;
  case BLE_GAP_EVENT_CONN_UPDATE:
    /* The central has updated the connection parameters. */
    ESP_LOGI(TAG, "connection updated; status=%d", event->conn_update.status);
    return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
    gap_adv_start();
    return 0;

  case BLE_GAP_EVENT_SUBSCRIBE:
    ESP_LOGI(TAG,
             "subscribe event; conn_handle=%d attr_handle=%d "
             "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
             event->subscribe.conn_handle, event->subscribe.attr_handle,
             event->subscribe.reason, event->subscribe.prev_notify,
             event->subscribe.cur_notify, event->subscribe.prev_indicate,
             event->subscribe.cur_indicate);
    return 0;

  case BLE_GAP_EVENT_MTU:
    ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
             event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
    return 0;

  case BLE_GAP_EVENT_ENC_CHANGE:
    /* Encryption has been enabled or disabled for this connection. */
    MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                event->enc_change.status);
    rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
    if (rc != 0) {
      ESP_LOGW(TAG, "Connection not found in enc_change event; rc=%d", rc);
      return 0;
    }
    return 0;

  case BLE_GAP_EVENT_NOTIFY_TX:
    MODLOG_DFLT(INFO,
                "notify_tx event; conn_handle=%d attr_handle=%d "
                "status=%d is_indication=%d",
                event->notify_tx.conn_handle, event->notify_tx.attr_handle,
                event->notify_tx.status, event->notify_tx.indication);
    return 0;

  case BLE_GAP_EVENT_REPEAT_PAIRING:
    /* We already have a bond with the peer, but it is attempting to
     * establish a new secure link.  This app sacrifices security for
     * convenience: just throw away the old bond and accept the new link.
     */

    /* Delete the old bond. */
    rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
    if (rc != 0) {
      ESP_LOGW(TAG, "Connection not found in repeat_pairing event; rc=%d", rc);
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    ble_store_util_delete_peer(&desc.peer_id_addr);

    /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
     * continue with the pairing operation.
     */
    return BLE_GAP_REPEAT_PAIRING_RETRY;
  }
  return 0;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS - LOW-LEVEL INITIALIZATION
// =============================================================================

static esp_err_t init_low_level(uint8_t mode) {
  esp_err_t ret;
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

  ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (ret) {
    ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
    return ret;
  }
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret) {
    ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
    return ret;
  }

  ret = esp_bt_controller_enable(mode);
  if (ret) {
    ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
    return ret;
  }

  ret = esp_nimble_init();
  if (ret) {
    ESP_LOGE(TAG, "esp_nimble_init failed: %d", ret);
    return ret;
  }

  return ret;
}


#endif // CONFIG_BT_NIMBLE_ENABLED
