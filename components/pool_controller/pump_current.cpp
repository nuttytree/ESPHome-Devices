#include "pump_current.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

void PumpSwitch::update_no_current_() {
  const bool fault = this->has_current_sensor() && this->state && !this->motor_running_;
  if (fault == this->no_current_detected_)
    return;
  this->no_current_detected_ = fault;
  if (this->no_current_sensor_ != nullptr)
    this->no_current_sensor_->publish_state(fault);
}

#ifdef USE_SENSOR
void PumpSwitch::update_current_sensor_(uint64_t now) {
  // ── Current sensor processing (1 Hz) ──────────────────────────────────────
  if (now - this->last_sample_ms_ >= 1000) {
    this->last_sample_ms_ = now;
    const float current = this->current_sensor_->state;

    // Current-based runtime tracking: state always reflects the output command.
    // Runtime is only accumulated while the output is on AND current confirms the motor is running.
    if (!std::isnan(current)) {
      const bool motor_running = this->state && (current >= this->current_on_threshold_);
      if (motor_running != this->motor_running_) {
        ESP_LOGD(TAG, "'%s' motor running state: %s (output=%s, %.3fA %s %.3fA threshold)", this->get_name().c_str(),
                 motor_running ? "YES" : "NO", this->state ? "ON" : "OFF", current, motor_running ? ">=" : "<",
                 this->current_on_threshold_);
        this->motor_running_ = motor_running;
        this->track_runtime(motor_running);
        this->update_no_current_();
      }
    }

    // Anomaly detection: gate on motor_running_ (current-confirmed), not state (commanded).
    // track_runtime() above only refreshes turned_on_ms_ on a motor_running_ transition, so
    // ticking on state alone can run tick_anomaly_() before turned_on_ms_ reflects this run,
    // leaving run_ms computed against the previous run's turn-on time.
    if (this->enable_anomaly_detection_ && this->motor_running_)
      this->tick_anomaly_();
  }
}

void PumpNoCurrentBinarySensor::setup() {
  Component::setup();
  this->publish_initial_state(false);
}
#endif  // USE_SENSOR

}  // namespace pool_controller
}  // namespace esphome
