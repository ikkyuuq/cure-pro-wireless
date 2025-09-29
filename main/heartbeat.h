#ifndef HEARTBEAT_H

#define HEARTBEAT_H

#include "common.h"
#include "espnow.h"

#define HEARTBEAT_INTERVAL_MS   30000  // 30 seconds between heartbeats
#define HEARTBEAT_TIMEOUT_MS    10000  // 10 seconds after heartbeat request without response = sleep
#define HEARTBEAT_STABLE_MS     1000   // 1 second after heartbeat request before checking for response
#define WAITING_TIMEOUT_MS      30000  // 30 seconds waiting before sleep

typedef struct {
  bool      received;
  uint64_t  last_req_time;
} heartbeat_state_t;

void heartbeat_start(void);
void heartbeat_stop(void);
void heartbeat_task(void *pvParameters);
void update_heartbeat(void);

#endif  // HEARTBEAT_H
