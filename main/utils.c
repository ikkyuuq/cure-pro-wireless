/**
 * @file utils.c
 * @brief Utility Functions
 *
 * Common utility functions for task management and timing.
 */

#include "utils.h"

// =============================================================================
// TASK MANAGEMENT
// =============================================================================

void task_hdl_init(TaskHandle_t *task_hdl, TaskFunction_t task_func,
                   const char *task_name, uint32_t task_priority,
                   uint32_t stack_depth, void *task_params)
{
  if (task_hdl != NULL)
  {
    xTaskCreate(task_func, task_name, stack_depth, task_params, task_priority,
                task_hdl);
  }
}

void task_hdl_cleanup(TaskHandle_t task_hdl)
{
  if (task_hdl != NULL)
  {
    // Unsubscribe from watchdog before deleting task
    // This prevents dangling WDT subscriptions and memory corruption
    esp_task_wdt_delete(task_hdl);

    vTaskDelete(task_hdl);
    task_hdl = NULL;
  }
}

// =============================================================================
// TIMING UTILITIES
// =============================================================================

uint32_t get_current_time_ms(void) { return esp_timer_get_time() / 1000; }
