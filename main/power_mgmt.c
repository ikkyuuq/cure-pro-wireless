/**
 * @file power_mgmt.c
 * @brief Power Management Implementation
 *
 * Clean implementation of power management through workload optimization.
 * Replaces sleep-based power management with intelligent adaptive
 * performance scaling for ESP32-C6 split keyboard.
 */

#include "power_mgmt.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "indicator.h"
#include "utils.h"
#include <string.h>

static const char *TAG = "POWER_MGMT";

// =============================================================================
// DEFAULT CONFIGURATION
// =============================================================================

static const power_config_t DEFAULT_CONFIG = {
    // Matrix scanning intervals - optimized for immediate responsiveness
    .active_scan_ms =
        1, // Ultra-fast scan when actively typing (maximum responsiveness)
    .normal_scan_ms = 5, // Quick scan after short inactivity (still responsive)
    .efficient_scan_ms = 25, // Power saving but still reasonable for idle
    .deep_scan_ms = 100,     // Maximum efficiency for long idle periods

    // Mode transition timeouts - more patient for better UX
    .active_timeout_ms =
        5000, // 5 seconds to normal mode (more time for continuous typing)
    .normal_timeout_ms =
        20000, // 20 seconds to efficient mode (longer normal period)
    .efficient_timeout_ms =
        90000, // 90 seconds to deep mode (very long idle before deep)

    // Component intervals
    .battery_read_interval_ms = 30000,   // Read battery every 30 seconds
    .heartbeat_check_interval_ms = 5000, // Check heartbeat every 5 seconds

    // Power thresholds
    .low_battery_threshold_mv = 3200,     // Low battery threshold
    .critical_battery_threshold_mv = 3000 // Critical battery threshold
};

// =============================================================================
// STATE VARIABLES
// =============================================================================

static TaskHandle_t             task_handle = NULL;
static power_management_state_t state = {
    .current_mode = POWER_MODE_ACTIVE,
    .config = DEFAULT_CONFIG,
    .metrics = {0},
    .matrix_state = COMPONENT_STATE_ACTIVE,
    .heartbeat_state = COMPONENT_STATE_ACTIVE,
    .battery_state = COMPONENT_STATE_ACTIVE,
    .battery_low = false,
    .battery_critical = false,
    .usb_powered = false};

static SemaphoreHandle_t state_mutex = NULL;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void power_mgmt_task(void *pvParameters);
static void update_power_mode(uint32_t current_time);
static void update_component_states(void);
static void log_mode_transition(power_mode_t old_mode, power_mode_t new_mode);
static void update_power_state_indicator(power_mode_t new_mode);
static const char *mode_to_string(power_mode_t mode);
static const char *component_state_to_string(component_power_state_t state);

// =============================================================================
// PUBLIC API - INITIALIZATION
// =============================================================================

esp_err_t power_mgmt_init(void)
{
  esp_err_t ret = ESP_OK;

  // Create mutex for thread-safe access
  state_mutex = xSemaphoreCreateMutex();
  if (!state_mutex)
  {
    ESP_LOGE(TAG, "Failed to create state mutex");
    return ESP_ERR_NO_MEM;
  }

  // Initialize metrics
  state.metrics.last_activity_time = get_current_time_ms();

  ESP_LOGI(TAG, "Power management initialized - Immediate response strategy");
  ESP_LOGI(TAG, "  Ultra-fast: %dms, Quick: %dms, Efficient: %dms, Deep: %dms",
           state.config.active_scan_ms, state.config.normal_scan_ms,
           state.config.efficient_scan_ms, state.config.deep_scan_ms);
  ESP_LOGI(TAG, "  ⚡ Zero latency activation on key press");

  // Set initial LED state to ACTIVE
  update_power_state_indicator(POWER_MODE_ACTIVE);

  return ret;
}

void power_mgmt_start(void)
{
  if (task_handle == NULL)
  {
    xTaskCreate(power_mgmt_task, "power_mgmt", 2048, NULL, 5, &task_handle);
    ESP_LOGI(TAG, "Power management task started");
  }
}

void power_mgmt_stop(void)
{
  if (task_handle != NULL)
  {
    vTaskDelete(task_handle);
    task_handle = NULL;
    ESP_LOGI(TAG, "Power management task stopped");
  }
}

// =============================================================================
// PUBLIC API - MODE CONTROL
// =============================================================================

power_mode_t power_mgmt_get_mode(void)
{
  power_mode_t mode;
  if (state_mutex == NULL)
  {
    return POWER_MODE_ACTIVE; // Fallback if not initialized
  }

  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    mode = state.current_mode;
    xSemaphoreGive(state_mutex);
  }
  else
  {
    mode = POWER_MODE_ACTIVE; // Fallback
  }
  return mode;
}

esp_err_t power_mgmt_set_mode(power_mode_t mode)
{
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    power_mode_t old_mode = state.current_mode;
    state.current_mode = mode;
    update_component_states();
    log_mode_transition(old_mode, mode);
    xSemaphoreGive(state_mutex);
    return ESP_OK;
  }
  return ESP_ERR_TIMEOUT;
}

void power_mgmt_notify_activity(uint32_t timestamp)
{
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    state.metrics.last_activity_time = timestamp;

    // Switch to active mode immediately
    if (state.current_mode != POWER_MODE_ACTIVE)
    {
      power_mode_t old_mode = state.current_mode;
      state.current_mode = POWER_MODE_ACTIVE;
      update_component_states();
      state.metrics.power_mode_transitions++;
      log_mode_transition(old_mode, POWER_MODE_ACTIVE);
    }

    state.metrics.total_scan_cycles++;
    state.metrics.active_scan_cycles++;
    xSemaphoreGive(state_mutex);
  }
}

void power_mgmt_force_active(uint32_t timestamp)
{
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    state.metrics.last_activity_time = timestamp;

    // Force immediate active mode - zero latency
    power_mode_t old_mode = state.current_mode;
    state.current_mode = POWER_MODE_ACTIVE;

    // Reset component states to maximum performance
    update_component_states();

    // Force update of all component states immediately
    state.matrix_state = COMPONENT_STATE_ACTIVE;
    state.heartbeat_state = COMPONENT_STATE_ACTIVE;
    state.battery_state = COMPONENT_STATE_ACTIVE;

    // Always log force activation for debugging
    if (old_mode != POWER_MODE_ACTIVE)
    {
      state.metrics.power_mode_transitions++;
      ESP_LOGD(TAG, "⚡ Forced active mode: %s → ACTIVE",
               mode_to_string(old_mode));
    }

    state.metrics.total_scan_cycles++;
    state.metrics.active_scan_cycles++;
    xSemaphoreGive(state_mutex);
  }
}

bool power_mgmt_is_immediate_response(void)
{
  if (state_mutex == NULL)
  {
    return true; // Assume responsive if not initialized
  }

  bool immediate = false;
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    immediate = (state.current_mode == POWER_MODE_ACTIVE);
    xSemaphoreGive(state_mutex);
  }
  return immediate;
}

// =============================================================================
// PUBLIC API - ADAPTIVE INTERVALS
// =============================================================================

uint32_t power_mgmt_get_matrix_interval(void)
{
  uint32_t interval = state.config.active_scan_ms;

  if (state_mutex == NULL)
  {
    return interval; // Return default if not initialized
  }

  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    switch (state.current_mode)
    {
    case POWER_MODE_ACTIVE:
      interval = state.config.active_scan_ms;
      break;
    case POWER_MODE_NORMAL:
      interval = state.config.normal_scan_ms;
      break;
    case POWER_MODE_EFFICIENT:
      interval = state.config.efficient_scan_ms;
      break;
    case POWER_MODE_DEEP:
      interval = state.config.deep_scan_ms;
      break;
    }
    xSemaphoreGive(state_mutex);
  }

  return interval;
}

uint32_t power_mgmt_get_heartbeat_interval(void)
{
  uint32_t interval = 5000; // Default 5 seconds

  if (state_mutex == NULL)
  {
    return interval; // Return default if not initialized
  }

  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    switch (state.current_mode)
    {
    case POWER_MODE_ACTIVE:
    case POWER_MODE_NORMAL:
      interval = 5000; // 5 seconds - adequate for 30s heartbeat
      break;
    case POWER_MODE_EFFICIENT:
      interval = 10000; // 10 seconds - less frequent when idle
      break;
    case POWER_MODE_DEEP:
      interval = 15000; // 15 seconds - minimal checking
      break;
    }
    xSemaphoreGive(state_mutex);
  }

  return interval;
}

uint32_t power_mgmt_get_battery_interval(void)
{
  return state.config.battery_read_interval_ms;
}

// =============================================================================
// PUBLIC API - BATTERY MANAGEMENT
// =============================================================================

void power_mgmt_update_battery_status(uint16_t voltage_mv, bool usb_powered)
{
  // Check if power management is initialized (mutex created)
  if (state_mutex == NULL)
  {
    return; // Power management not yet initialized
  }

  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    bool old_usb = state.usb_powered;
    bool old_low = state.battery_low;
    bool old_critical = state.battery_critical;

    state.usb_powered = usb_powered;
    state.battery_low = (voltage_mv < state.config.low_battery_threshold_mv);
    state.battery_critical =
        (voltage_mv < state.config.critical_battery_threshold_mv);
    state.metrics.battery_read_count++;

    // Log significant changes
    if (old_usb != usb_powered)
    {
      ESP_LOGI(TAG, "USB power status: %s",
               usb_powered ? "Connected" : "Disconnected");
    }
    if (old_low != state.battery_low)
    {
      ESP_LOGI(TAG, "Battery status: %s", state.battery_low ? "LOW" : "OK");
    }
    if (old_critical != state.battery_critical)
    {
      ESP_LOGW(TAG, "Battery status: CRITICAL");
    }

    xSemaphoreGive(state_mutex);
  }
}

bool power_mgmt_is_battery_low(void)
{
  bool low = false;
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    low = state.battery_low;
    xSemaphoreGive(state_mutex);
  }
  return low;
}

bool power_mgmt_is_battery_critical(void)
{
  bool critical = false;
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    critical = state.battery_critical;
    xSemaphoreGive(state_mutex);
  }
  return critical;
}

// =============================================================================
// PUBLIC API - METRICS AND STATISTICS
// =============================================================================

const power_metrics_t *power_mgmt_get_metrics(void) { return &state.metrics; }

void power_mgmt_reset_metrics(void)
{
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    memset(&state.metrics, 0, sizeof(power_metrics_t));
    state.metrics.last_activity_time = get_current_time_ms();
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG, "Power management metrics reset");
  }
}

void power_mgmt_print_status(void)
{
  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    ESP_LOGI(TAG, "=== Power Management Status ===");
    ESP_LOGI(TAG, "  Current Mode: %s", mode_to_string(state.current_mode));
    ESP_LOGI(TAG, "  Matrix State: %s",
             component_state_to_string(state.matrix_state));
    ESP_LOGI(TAG, "  Battery: %d mV, USB: %s, Low: %s, Critical: %s",
             0, // Add actual battery voltage reading if available
             state.usb_powered ? "Yes" : "No", state.battery_low ? "Yes" : "No",
             state.battery_critical ? "Yes" : "No");
    ESP_LOGI(TAG, "  Total Scans: %u, Active Scans: %u",
             state.metrics.total_scan_cycles, state.metrics.active_scan_cycles);
    ESP_LOGI(TAG, "  Mode Transitions: %u, Battery Reads: %u",
             state.metrics.power_mode_transitions,
             state.metrics.battery_read_count);
    ESP_LOGI(TAG, "  Current Matrix Interval: %d ms",
             power_mgmt_get_matrix_interval());
    ESP_LOGI(TAG, "================================");
    xSemaphoreGive(state_mutex);
  }
}

// =============================================================================
// PUBLIC API - CONFIGURATION
// =============================================================================

const power_config_t *power_mgmt_get_config(void) { return &state.config; }

esp_err_t power_mgmt_update_config(const power_config_t *new_config)
{
  if (new_config == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    state.config = *new_config;
    update_component_states();
    ESP_LOGI(TAG, "Power management configuration updated");
    xSemaphoreGive(state_mutex);
    return ESP_OK;
  }

  return ESP_ERR_TIMEOUT;
}

// =============================================================================
// PRIVATE IMPLEMENTATIONS
// =============================================================================

static void power_mgmt_task(void *pvParameters)
{
  ESP_LOGI(TAG, "Power management task running");

  while (1)
  {
    uint32_t current_time = get_current_time_ms();

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      update_power_mode(current_time);
      xSemaphoreGive(state_mutex);
    }

    // Check every 1 second
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void update_power_mode(uint32_t current_time)
{
  uint32_t     idle_time = current_time - state.metrics.last_activity_time;
  power_mode_t new_mode = state.current_mode;

  if (idle_time < state.config.active_timeout_ms)
  {
    new_mode = POWER_MODE_ACTIVE;
  }
  else if (idle_time < state.config.normal_timeout_ms)
  {
    new_mode = POWER_MODE_NORMAL;
  }
  else if (idle_time < state.config.efficient_timeout_ms)
  {
    new_mode = POWER_MODE_EFFICIENT;
  }
  else
  {
    new_mode = POWER_MODE_DEEP;
  }

  if (new_mode != state.current_mode)
  {
    power_mode_t old_mode = state.current_mode;
    state.current_mode = new_mode;
    state.metrics.power_mode_transitions++;
    update_component_states();
    log_mode_transition(old_mode, new_mode);
    update_power_state_indicator(new_mode);
  }

  // Update idle time metric
  state.metrics.total_idle_time += idle_time;
}

static void update_component_states(void)
{
  // Update matrix component state based on power mode
  switch (state.current_mode)
  {
  case POWER_MODE_ACTIVE:
    state.matrix_state = COMPONENT_STATE_ACTIVE;
    state.heartbeat_state = COMPONENT_STATE_ACTIVE;
    state.battery_state = COMPONENT_STATE_ACTIVE;
    break;
  case POWER_MODE_NORMAL:
    state.matrix_state = COMPONENT_STATE_REDUCED;
    state.heartbeat_state = COMPONENT_STATE_ACTIVE;
    state.battery_state = COMPONENT_STATE_ACTIVE;
    break;
  case POWER_MODE_EFFICIENT:
    state.matrix_state = COMPONENT_STATE_MINIMAL;
    state.heartbeat_state = COMPONENT_STATE_REDUCED;
    state.battery_state = COMPONENT_STATE_REDUCED;
    break;
  case POWER_MODE_DEEP:
    state.matrix_state = COMPONENT_STATE_MINIMAL;
    state.heartbeat_state = COMPONENT_STATE_MINIMAL;
    state.battery_state = COMPONENT_STATE_MINIMAL;
    break;
  }
}

static void log_mode_transition(power_mode_t old_mode, power_mode_t new_mode)
{
  ESP_LOGI(TAG, "Power mode: %s → %s", mode_to_string(old_mode),
           mode_to_string(new_mode));
  ESP_LOGD(TAG, "  Matrix: %s, Heartbeat: %s, Battery: %s",
           component_state_to_string(state.matrix_state),
           component_state_to_string(state.heartbeat_state),
           component_state_to_string(state.battery_state));
}

static const char *mode_to_string(power_mode_t mode)
{
  switch (mode)
  {
  case POWER_MODE_ACTIVE:
    return "ACTIVE";
  case POWER_MODE_NORMAL:
    return "NORMAL";
  case POWER_MODE_EFFICIENT:
    return "EFFICIENT";
  case POWER_MODE_DEEP:
    return "DEEP";
  default:
    return "UNKNOWN";
  }
}

static const char *component_state_to_string(component_power_state_t state)
{
  switch (state)
  {
  case COMPONENT_STATE_ACTIVE:
    return "ACTIVE";
  case COMPONENT_STATE_REDUCED:
    return "REDUCED";
  case COMPONENT_STATE_MINIMAL:
    return "MINIMAL";
  default:
    return "UNKNOWN";
  }
}

static void update_power_state_indicator(power_mode_t new_mode)
{
  // Map power management modes to LED power states
  power_state_t led_power_state;

  switch (new_mode)
  {
  case POWER_MODE_ACTIVE:
    led_power_state = POWER_STATE_ACTIVE;
    break;
  case POWER_MODE_NORMAL:
    led_power_state = POWER_STATE_NORMAL;
    break;
  case POWER_MODE_EFFICIENT:
    led_power_state = POWER_STATE_EFFICIENT;
    break;
  case POWER_MODE_DEEP:
    led_power_state = POWER_STATE_DEEP;
    break;
  default:
    led_power_state = POWER_STATE_ACTIVE;
    break;
  }

  // Get current connection and battery states
  conn_state_t conn_state = indicator_get_conn_state();
  batt_state_t batt_state = indicator_get_batt_state();

  // Update LED indicators with combined state (preserves connection indicator!)
  indicator_update_combined_state(conn_state, batt_state, led_power_state);

  ESP_LOGD(TAG, "LED indicators updated - Power: %s, Conn: %d, Batt: %d",
           mode_to_string(new_mode), conn_state, batt_state);
}
