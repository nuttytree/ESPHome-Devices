#include "pump_flow.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

void PumpSwitch::set_flow_loss_(bool latched) {
  if (latched == this->flow_loss_latched_)
    return;
  this->flow_loss_latched_ = latched;
  if (this->flow_loss_sensor_ != nullptr)
    this->flow_loss_sensor_->publish_state(latched);
}

void PumpSwitch::set_unexpected_flow_(bool detected) {
  if (detected == this->unexpected_flow_detected_)
    return;
  this->unexpected_flow_detected_ = detected;
  if (this->unexpected_flow_sensor_ != nullptr)
    this->unexpected_flow_sensor_->publish_state(detected);
}

// Whenever a flow sensor is configured it owns runtime accounting, current sensor or not.
// Moving water is what a pool schedule is actually buying, and unlike current it stays valid
// through a current-sensor outage — a stalled meter reading 0 A would otherwise silently stop
// the clock on a pump that is running perfectly well.
void PumpSwitch::update_flow_based_runtime_(uint64_t now) {
  const bool flow_running = this->state && this->flow_sensor_->state;
  if (flow_running == this->flow_running_)
    return;

  ESP_LOGD(TAG, "'%s' flow running state: %s (output=%s)", this->get_name().c_str(), flow_running ? "YES" : "NO",
           this->state ? "ON" : "OFF");
  this->flow_running_ = flow_running;
  // With no current sensor, flow is also the only evidence the motor is turning, so it has to
  // anchor the turn-on timestamp as well. When current is available it does that job instead,
  // because it sees the motor a second or more earlier.
  if (!this->has_current_sensor() && flow_running && this->turned_on_ms_ == 0)
    this->turned_on_ms_ = millis_64();
  this->track_runtime(flow_running);
}

void PumpSwitch::update_flow_watchdogs_(uint64_t now) {
  // ── Flow sensor watchdog ───────────────────────────────────────────────────
  // Only shuts the pump down when a current sensor confirms the motor is actually running.
  // Without that confirmation, "commanded on but no flow" is indistinguishable from a manual
  // override switch physically holding the pump off, so no-flow alone must not trigger a shutdown.
  if (this->has_current_sensor()) {
    const bool should_have_flow = this->motor_running_;

    if (should_have_flow && !this->flow_sensor_->state) {
      // Pump expected to produce flow but sensor reports none.
      if (this->flow_check_start_ms_ == 0) {
        this->flow_check_start_ms_ = now;
        ESP_LOGD(TAG, "'%s' no flow detected — starting %" PRIu32 " ms watchdog", this->get_name().c_str(),
                 this->flow_timeout_ms_);
      } else if (now - this->flow_check_start_ms_ >= this->flow_timeout_ms_) {
        ESP_LOGW(TAG, "'%s' no flow for %" PRIu32 " ms — shutting down pump", this->get_name().c_str(),
                 this->flow_timeout_ms_);
        this->flow_check_start_ms_ = 0;
        this->set_flow_loss_(true);
        this->turn_off();
      }
    } else {
      // Flow is present, or pump is not expected to be running — reset watchdog.
      if (this->flow_check_start_ms_ != 0) {
        ESP_LOGD(TAG, "'%s' flow confirmed — clearing no-flow watchdog", this->get_name().c_str());
        this->flow_check_start_ms_ = 0;
      }
    }

    // Clear the latched flow-loss fault only once the pump is on, current confirms the motor is
    // running, AND flow is detected again — all three at once.
    if (should_have_flow && this->flow_sensor_->state)
      this->set_flow_loss_(false);
  }

  // ── Unexpected flow watchdog ────────────────────────────────────────────────
  // Detects water flow while the pump is commanded off — e.g. a stuck valve, a neighboring pump
  // pushing water through this branch, or a manual override running the pump. Debounced by
  // flow_timeout to allow for residual flow immediately after shutdown.
  if (!this->state && this->flow_sensor_->state) {
    if (this->unexpected_flow_check_start_ms_ == 0) {
      this->unexpected_flow_check_start_ms_ = now;
    } else if (now - this->unexpected_flow_check_start_ms_ >= this->flow_timeout_ms_) {
      this->set_unexpected_flow_(true);
    }
  } else {
    this->unexpected_flow_check_start_ms_ = 0;
    this->set_unexpected_flow_(false);
  }
}

void PumpUnexpectedFlowBinarySensor::setup() {
  Component::setup();
  this->publish_initial_state(false);
}

#ifdef USE_SENSOR
void PumpFlowLossBinarySensor::setup() {
  Component::setup();
  this->publish_initial_state(false);
}
#endif  // USE_SENSOR

}  // namespace pool_controller
}  // namespace esphome
