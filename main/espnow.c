#include "espnow.h"
#include "config.h"
#include "handler.h"
#include "kb_matrix.h"
#include "kb_mgt.h"
#include "heartbeat.h"

TaskHandle_t espnow_task_hdl = NULL;
QueueHandle_t espnow_queue;

static const char *TAG = "ESPNOW";

static void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void espnow_task(void *pvParameters);
void send_to_espnow(espnow_from_t from, espnow_event_info_data_type_t type, void *data);

esp_err_t espnow_init(void) {
  esp_err_t ret;

  ret = esp_event_loop_create_default();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create event loop: %d", ret);
    return ret;
  }

  ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize netif: %d", ret);
    return ret;
  }

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&wifi_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi initialization failed: %d", ret);
    return ret;
  }

#if IS_MASTER
  ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode to APSTA: %d", ret);
    return ret;
  }

  // Enable power save for BLE coexistence
  ret = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to enable WiFi power save: %d", ret);
  } else {
    ESP_LOGI(TAG, "WiFi modem sleep enabled for BLE coexistence");
  }
#else
  esp_wifi_set_mode(WIFI_MODE_STA);
#endif

  ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  ESP_ERROR_CHECK(ret);

  ret = esp_wifi_start();
  ESP_ERROR_CHECK(ret);

  esp_wifi_set_channel(ESP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  ret = esp_now_init();
  ESP_ERROR_CHECK(ret);

  ret = esp_now_register_recv_cb(espnow_recv_cb);
  ESP_ERROR_CHECK(ret);

  ret = esp_now_register_send_cb(espnow_send_cb);
  ESP_ERROR_CHECK(ret);

  uint8_t peer_addr[] = ESPNOW_PEER_ADDR;
  esp_now_peer_info_t peer_info = {
      .channel = ESP_NOW_CHANNEL,
      .ifidx = WIFI_IF_STA,
      .encrypt = false,
  };
  memcpy(peer_info.peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
  ret = esp_now_add_peer(&peer_info);
  ESP_ERROR_CHECK(ret);

  espnow_queue = xQueueCreate(ESP_NOW_QUEUE_SIZE, sizeof(espnow_event_t));
  if (espnow_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create queue");
    return ESP_FAIL;
  }

  xTaskCreate(espnow_task, "espnow_task", ESPNOW_TASK_STACK_SIZE, NULL,
              ESPNOW_PRIORITY, &espnow_task_hdl);

  ESP_LOGI(TAG, "ESP-NOW Initialized!");
  return ret;
}

void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info,
                    const uint8_t *data, int data_len) {

  espnow_event_t event;
  event.type = EVENT_RECV_CB;

  espnow_recv_cb_t *recv_cb = &event.info.recv_cb;

  recv_cb->data = malloc(sizeof(espnow_event_info_data_t));
  if (!recv_cb->data) {
    ESP_LOGE(TAG, "failed to allocate recv_cb->data");
    return;
  }

  memcpy(recv_cb->from, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
  memcpy(recv_cb->to, esp_now_info->des_addr, ESP_NOW_ETH_ALEN);
  memcpy(recv_cb->data, data, data_len);
  recv_cb->data_len = data_len;

  xQueueSend(espnow_queue, &event, portMAX_DELAY);
}

void espnow_send_cb(const esp_now_send_info_t *tx_info,
                    esp_now_send_status_t status) {
  espnow_event_t event;
  event.type = EVENT_SEND_CB;

  espnow_send_cb_t *send_cb = &event.info.send_cb;

  memcpy(send_cb->to, tx_info->des_addr, ESP_NOW_ETH_ALEN);
  if (status != ESP_NOW_SEND_SUCCESS) {
    ESP_LOGE(TAG, "failed to send event to destination, status: %d", status);
    return;
  }
  send_cb->status = status;

  xQueueSend(espnow_queue, &event, portMAX_DELAY);
}

void espnow_task(void *pvParameters) {
  static const char *TAG = "ESPNOW_TASK";
  espnow_event_t event;

  while (xQueueReceive(espnow_queue, &event, portMAX_DELAY)) {
    switch (event.type) {
    case EVENT_RECV_CB:
      espnow_recv_cb_t *recv_cb = &event.info.recv_cb;
      espnow_event_info_data_t *data = recv_cb->data;

      ESP_LOGI(TAG, "received data from: %d", data->from);

#if !IS_MASTER
      // NOTE: this is just for updating heartbeat when SLAVE recieve any message
      update_heartbeat();
#endif

      switch (data->type) {
#if !IS_MASTER
      case CONN:
        if (data->conn) {
          matrix_scan_start();
          heartbeat_start();
          ESP_LOGI(TAG, "Master connected - slave should show green");
        } else {
          matrix_scan_stop();
          heartbeat_stop();
          ESP_LOGI(TAG, "Master disconnected - slave should show blue blinking");
        }
        break;
#endif
#if IS_MASTER
      case TAP:
        memcpy(kb_mgt_hid_get_current_report(), &data->key_report, sizeof(kb_mgt_hid_key_report_t));
        kb_mgt_hid_send_report();
        break;
      case BRIEF_TAP:
        memcpy(kb_mgt_hid_get_current_report(), &data->key_report, sizeof(kb_mgt_hid_key_report_t));
        kb_mgt_hid_send_report();

        kb_mgt_hid_clear_report();
        kb_mgt_hid_send_report();
        break;
      case CONSUMER:
        memcpy(kb_mgt_hid_get_current_consumer_report(), &data->consumer_report, sizeof(kb_mgt_hid_consumer_report_t));
        kb_mgt_hid_send_consumer_report();
        break;
#endif
      case LAYER_SYNC:
        ESP_LOGI(TAG, "layer sync to %d", data->layer);
        kb_mgt_sync_layer(data->layer);
        break;
      case LAYER_DESYNC:
        ESP_LOGI(TAG, "layer desync from %d", data->layer);
        kb_mgt_desync_layer(data->layer);
        break;
      case MOD_SYNC:
        kb_mgt_sync_modifier(data->key_report.modifiers);
        break;
      case MOD_DESYNC:
        kb_mgt_desync_modifier(data->key_report.modifiers);
        break;
      case REQ_HEARTBEAT:
#if IS_MASTER
        send_to_espnow(MASTER, RES_HEARTBEAT, NULL);
#endif
        break;
      case RES_HEARTBEAT:
#if !IS_MASTER
        update_heartbeat();
        ESP_LOGI(TAG, "Heartbeat response received from master");
#endif
        break;
      default:
        break;
      }

      if (recv_cb->data) free(recv_cb->data);
      break;
    case EVENT_SEND_CB:
      ESP_LOGI(TAG, "sent event to destination!");
      break;
    default:
      break;
    }
  }
}

void send_to_espnow(espnow_from_t from, espnow_event_info_data_type_t type, void *data) {
  esp_err_t ret;
  uint8_t espnow_peer_addr[] = ESPNOW_PEER_ADDR;

  espnow_event_info_data_t *info_data;
  info_data = malloc(sizeof(espnow_event_info_data_t));
  if (!info_data) {
    ESP_LOGE(TAG, "failed to allocate data");
    return;
  }

  info_data->from = from;
  info_data->type = type;

  switch (type) {
  case CONN:
    info_data->conn = *(bool *)data;
    break;
  case TAP:
    memcpy(&info_data->key_report, (kb_mgt_hid_key_report_t *)data, sizeof(kb_mgt_hid_key_report_t));
    break;
  case CONSUMER:
    memcpy(&info_data->consumer_report, (kb_mgt_hid_consumer_report_t *)data, sizeof(kb_mgt_hid_consumer_report_t));
    break;
  case BRIEF_TAP:
    memcpy(&info_data->key_report, (kb_mgt_hid_key_report_t *)data, sizeof(kb_mgt_hid_key_report_t));
    break;
  case LAYER_SYNC:
    info_data->layer = *(uint8_t *)data;
    break;
  case LAYER_DESYNC:
    info_data->layer = *(uint8_t *)data;
    break;
  case MOD_SYNC:
    memcpy(&info_data->key_report, (kb_mgt_hid_key_report_t *)data, sizeof(kb_mgt_hid_key_report_t));
    break;
  case MOD_DESYNC:
    memcpy(&info_data->key_report, (kb_mgt_hid_key_report_t *)data, sizeof(kb_mgt_hid_key_report_t));
    break;
#if !IS_MASTER
  case REQ_HEARTBEAT:
    // NOTE: this is just for indicator heartbeat when SLAVE send REQ_HEARTBEAT
    break;
#endif
#if IS_MASTER
  case RES_HEARTBEAT:
    // NOTE: this is just for indicator heartbeat when SLAVE recieve response from REQ_HEARTBEAT
    break;
#endif
  default:
    break;
  }

  ret = esp_now_send(espnow_peer_addr, (uint8_t *)info_data, sizeof(espnow_event_info_data_t));
  if (info_data) free(info_data);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "failed to send to data to destination, ret: %d", ret);
    return;
  }
}
