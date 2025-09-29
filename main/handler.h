#ifndef HANDLER_H

#define HANDLER_H

#include "common.h"

extern TaskHandle_t matrix_task_hdl;
extern TaskHandle_t espnow_task_hdl;
extern TaskHandle_t indicator_task_hdl;
extern TaskHandle_t heartbeat_task_hdl;
extern TaskHandle_t power_task_hdl;
extern TaskHandle_t power_mgt_hdl;

extern led_strip_handle_t conn_indicator_hdl;
extern led_strip_handle_t batt_indicator_hdl;

#endif  // HANDLER_H
