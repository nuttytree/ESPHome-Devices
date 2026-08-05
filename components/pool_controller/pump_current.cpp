#include "pump_current.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome::pool_controller {

static const char *const TAG = "pool_controller.switch";

void PumpSwitch::update_no_current_() {
  // Suppressed while the reading is stale: "commanded on but no current" is a claim about the
  // pump, and a sensor that stopped publishing supports no such claim. The stale sensor carries
  // the alarm instead, so the two faults stay distinguishable in the UI and in history.
  const bool fault = this->has_current_sensor_() && this->state && !this->motor_running_ && !this->current_stale_;
  if (fault == this->no_current_detected_)
    return;
  this->no_current_detected_ = fault;
  if (this->no_current_sensor_ != nullptr)
    this->no_current_sensor_->publish_state(fault);
}

void PumpSwitch::set_current_stale_(bool stale) {
  if (stale == this->current_stale_)
    return;
  this->current_stale_ = stale;
  if (this->current_stale_sensor_ != nullptr)
    this->current_stale_sensor_->publish_state(stale);
  // no_current is derived from staleness, so re-evaluate it on every transition.
  this->update_no_current_();
}

#ifdef USE_SENSOR
bool PumpSwitch::update_current_stale_(uint64_t now) {
  const bool stale = (now - this->last_current_update_ms_) >= this->current_timeout_ms_;
  if (stale == this->current_stale_) {
    if (stale && this->runtime_start_ms_ != 0)
      this->run_current_gap_ = true;
    return stale;
  }

  if (stale) {
    ESP_LOGW(TAG, "'%s' current sensor has not published for %" PRIu32 " ms — readings distrusted",
             this->get_name().c_str(), this->current_timeout_ms_);
    // A gap during a run means that run's startup window was not fully observed.
    if (this->runtime_start_ms_ != 0)
      this->run_current_gap_ = true;
  } else {
    ESP_LOGI(TAG, "'%s' current sensor is publishing again", this->get_name().c_str());
    // The first reading back is a step change from a value that was frozen, possibly for a long
    // time. Fed into the detector as-is it is a huge apparent deviation: the spread EWMA spikes
    // past the variance threshold and the flag chatters while it re-converges. Clear both the
    // debounce streak and the spread estimate so detection restarts from live data instead.
    this->oob_streak_ = 0;
    this->spread_ema_ = 0.0f;
  }

  this->set_current_stale_(stale);
  return stale;
}

void PumpSwitch::update_current_sensor_(uint64_t now) {
  // ── Current sensor processing (1 Hz) ──────────────────────────────────────
  if (now - this->last_sample_ms_ >= 1000) {
    this->last_sample_ms_ = now;

    // Nothing below may run on a stale reading: motor_running_ would be recomputed from a frozen
    // value, which either invents a no-current fault or silently stops accumulating runtime for
    // a pump that is in fact running. Holding the last known state is the honest option.
    if (this->update_current_stale_(now))
      return;

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
        // Anchor the anomaly startup window to electrical turn-on. Only stamped when it is still
        // 0 (this run hasn't been confirmed running yet), so regaining sight of a motor that
        // never stopped — after a stale period — cannot restart the window mid-run and record
        // steady-state current as if it were inrush.
        if (motor_running && this->turned_on_ms_ == 0)
          this->turned_on_ms_ = millis_64();
        // Runtime follows flow whenever a flow sensor is configured; see update_flow_based_runtime_().
        if (!this->has_flow_sensor_())
          this->track_runtime_(motor_running);
        this->update_no_current_();
      }
    }

    // Anomaly detection: gate on motor_running_ (current-confirmed), not state (commanded).
    // track_runtime_() above only refreshes turned_on_ms_ on a motor_running_ transition, so
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

void PumpCurrentStaleBinarySensor::setup() {
  Component::setup();
  this->publish_initial_state(false);
}
#endif  // USE_SENSOR

}  // namespace esphome::pool_controller
