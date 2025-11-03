/**
 * @file power_mgmt.h
 * @brief Power Management through Workload Optimization
 *
 * Provides power management for ESP32-C6 split keyboard through
 * workload reduction rather than sleep modes. Focuses on
 * adaptive scanning and task optimization for maximum efficiency.
 *
 * Key responsibilities:
 * - Adaptive matrix scanning intervals based on user activity
 * - Component power state management
 * - Workload optimization without sleep modes
 * - Battery-aware power management
 * - Performance metrics and statistics
 */

#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include "common.h"
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// POWER MANAGEMENT MODES
// =============================================================================

typedef enum
{
  POWER_MODE_ACTIVE,    // High performance - actively typing
  POWER_MODE_NORMAL,    // Balanced - short inactivity
  POWER_MODE_EFFICIENT, // Power saving - idle periods
  POWER_MODE_DEEP       // Maximum efficiency - long idle
} power_mode_t;

// =============================================================================
// COMPONENT POWER STATES
// =============================================================================

typedef enum
{
  COMPONENT_STATE_ACTIVE,  // Full operation
  COMPONENT_STATE_REDUCED, // Reduced frequency operation
  COMPONENT_STATE_MINIMAL  // Minimal operation
} component_power_state_t;

// =============================================================================
// PERFORMANCE METRICS
// =============================================================================

typedef struct
{
  uint32_t total_scan_cycles;
  uint32_t active_scan_cycles;
  uint32_t power_mode_transitions;
  uint32_t last_activity_time;
  uint32_t total_idle_time;
  uint32_t battery_read_count;
  float    average_power_consumption;
} power_metrics_t;

// =============================================================================
// POWER MANAGEMENT CONFIGURATION
// =============================================================================

typedef struct
{
  // Matrix scanning intervals (ms)
  uint32_t active_scan_ms;
  uint32_t normal_scan_ms;
  uint32_t efficient_scan_ms;
  uint32_t deep_scan_ms;

  // Mode transition timeouts (ms)
  uint32_t active_timeout_ms;
  uint32_t normal_timeout_ms;
  uint32_t efficient_timeout_ms;

  // Component intervals (ms)
  uint32_t battery_read_interval_ms;
  uint32_t heartbeat_check_interval_ms;

  // Power thresholds
  uint16_t low_battery_threshold_mv;
  uint16_t critical_battery_threshold_mv;
} power_config_t;

// =============================================================================
// MAIN POWER MANAGEMENT STATE
// =============================================================================

typedef struct
{
  power_mode_t            current_mode;
  power_config_t          config;
  power_metrics_t         metrics;
  component_power_state_t matrix_state;
  component_power_state_t heartbeat_state;
  component_power_state_t battery_state;
  bool                    battery_low;
  bool                    battery_critical;
  bool                    usb_powered;
} power_management_state_t;

// =============================================================================
// PUBLIC API - INITIALIZATION
// =============================================================================

/**
 * @brief Initialize power management system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t power_mgmt_init(void);

/**
 * @brief Start power management task
 */
void power_mgmt_start(void);

/**
 * @brief Stop power management task
 */
void power_mgmt_stop(void);

// =============================================================================
// PUBLIC API - MODE CONTROL
// =============================================================================

/**
 * @brief Get current power mode
 * @return Current power mode
 */
power_mode_t power_mgmt_get_mode(void);

/**
 * @brief Set power mode manually
 * @param mode Desired power mode
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t power_mgmt_set_mode(power_mode_t mode);

/**
 * @brief Notify system of user activity
 * @param timestamp Current timestamp in milliseconds
 */
void power_mgmt_notify_activity(uint32_t timestamp);

/**
 * @brief Force immediate active mode for zero latency response
 * @param timestamp Current timestamp in milliseconds
 */
void power_mgmt_force_active(uint32_t timestamp);

/**
 * @brief Check if system is in immediate response mode
 * @return true if in active mode with immediate response
 */
bool power_mgmt_is_immediate_response(void);

// =============================================================================
// PUBLIC API - ADAPTIVE INTERVALS
// =============================================================================

/**
 * @brief Get optimal matrix scan interval
 * @return Recommended scan interval in milliseconds
 */
uint32_t power_mgmt_get_matrix_interval(void);

/**
 * @brief Get optimal heartbeat check interval
 * @return Recommended check interval in milliseconds
 */
uint32_t power_mgmt_get_heartbeat_interval(void);

/**
 * @brief Get optimal battery read interval
 * @return Recommended read interval in milliseconds
 */
uint32_t power_mgmt_get_battery_interval(void);

// =============================================================================
// PUBLIC API - BATTERY MANAGEMENT
// =============================================================================

/**
 * @brief Update battery status
 * @param voltage_mv Battery voltage in millivolts
 * @param usb_powered USB power status
 */
void power_mgmt_update_battery_status(uint16_t voltage_mv, bool usb_powered);

/**
 * @brief Check if battery is low
 * @return true if battery is low
 */
bool power_mgmt_is_battery_low(void);

/**
 * @brief Check if battery is critical
 * @return true if battery is critical
 */
bool power_mgmt_is_battery_critical(void);

// =============================================================================
// PUBLIC API - METRICS AND STATISTICS
// =============================================================================

/**
 * @brief Get current performance metrics
 * @return Pointer to performance metrics structure
 */
const power_metrics_t *power_mgmt_get_metrics(void);

/**
 * @brief Reset performance metrics
 */
void power_mgmt_reset_metrics(void);

/**
 * @brief Print current power management status
 */
void power_mgmt_print_status(void);

// =============================================================================
// PUBLIC API - CONFIGURATION
// =============================================================================

/**
 * @brief Get current power configuration
 * @return Pointer to current configuration
 */
const power_config_t *power_mgmt_get_config(void);

/**
 * @brief Update power configuration
 * @param new_config New configuration parameters
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t power_mgmt_update_config(const power_config_t *new_config);

#endif // POWER_MGMT_H
