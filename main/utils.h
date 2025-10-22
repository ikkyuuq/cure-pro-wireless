#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// Tasks Utils
void task_hdl_init(TaskHandle_t *task_hdl, TaskFunction_t task_func, const char *task_name, uint32_t task_priority, uint32_t stack_depth, void *task_params);
void task_hdl_cleanup(TaskHandle_t task_hdl);

// Timer Utils
uint32_t get_current_time_ms(void);

#endif
