#ifndef ESPNOW_H

#define ESPNOW_H

#include "common.h"
#include "kb_matrix.h"
#include "kb_mgt.h"

typedef enum {
  CONN,
  TAP,
  BRIEF_TAP,
  LAYER_SYNC,
  LAYER_DESYNC,
  MOD_SYNC,
  MOD_DESYNC,
} espnow_event_info_data_type_t;

typedef enum {
  LEFT_SIDE,
  RIGHT_SIDE
} side_t;

typedef struct {
  kb_mgt_hid_report_t           *report;
  uint8_t                       layer;
  bool                          conn;
} espnow_requied_data_t;

typedef struct {
  side_t                          from;
  espnow_event_info_data_type_t   type;
  union {
    kb_mgt_hid_report_t           report;
    uint8_t                       layer;
    bool                          conn;
  };
} espnow_event_info_data_t;

typedef enum {
  EVENT_RECV_CB,
  EVENT_SEND_CB,
} espnow_event_type_t;

typedef struct {
  uint8_t status;
  uint8_t to[ESP_NOW_ETH_ALEN];
} espnow_send_cb_t;

typedef struct {
  uint8_t to[ESP_NOW_ETH_ALEN];
  uint8_t from[ESP_NOW_ETH_ALEN];
  espnow_event_info_data_t *data;
  uint8_t data_len;
} espnow_recv_cb_t;

typedef struct {
  union {
    espnow_recv_cb_t recv_cb;
    espnow_send_cb_t send_cb;
  };
} espnow_event_info_t;

typedef struct {
  espnow_event_type_t type;
  espnow_event_info_t info;
} espnow_event_t;

esp_err_t espnow_init(void);

void send_to_espnow(side_t side, espnow_event_info_data_type_t type, espnow_requied_data_t *data);

#endif  // ESPNOW_H
