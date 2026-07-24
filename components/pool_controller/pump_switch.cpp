#include "pump_switch.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

void PumpSwitch::dump_config() { LOG_SWITCH("", "Pool Controller Pump", this); }

void PumpSwitch::setup() {
  this->runtime_pref_ = this->make_entity_preference<uint32_t>();
  if (this->runtime_pref_.load(&this->runtime_seconds_)) {
    ESP_LOGD(TAG, "Restored runtime %" PRIu32 "s from flash", this->runtime_seconds_);
  } else {
    this->runtime_seconds_ = 0;
  }

#ifdef USE_SENSOR
  if (this->current_sensor_ != nullptr) {
    this->anomaly_pref_ = this->make_entity_preference<AnomalyBaseline>();
    if (this->anomaly_pref_.load(&this->anomaly_baseline_)) {
      this->sample_count_ = this->anomaly_baseline_.sample_count;
      this->baseline_locked_ = (this->sample_count_ >= this->learning_samples_);
      ESP_LOGD(TAG, "'%s' anomaly baseline restored: %.3fA (%s, %" PRIu32 " samples, %u startup runs)",
               this->get_name().c_str(), this->anomaly_baseline_.steady_state,
               this->baseline_locked_ ? "locked" : "learning", this->sample_count_,
               this->anomaly_baseline_.startup_runs);
    }
  }
#endif

  this->set_anomaly_detected_(false);
  this->turn_off();
  // Enforce the 5-minute cooldown from boot — we don't know the previous pump state.
  this->last_off_ms_ = millis_64();
}

void PumpSwitch::reset_runtime() {
  if (this->runtime_start_ms_ != 0) {
    // Pump is still running; restart the window so elapsed time in the new period is accurate.
    this->runtime_start_ms_ = millis_64();
  }
  this->runtime_seconds_ = 0;
  this->runtime_pref_.save(&this->runtime_seconds_);
  ESP_LOGD(TAG, "Runtime counter reset");
}

void PumpSwitch::track_runtime(bool new_state) {
  if (new_state && this->runtime_start_ms_ == 0) {
    this->runtime_start_ms_ = millis_64();
    this->turned_on_ms_ = millis_64();
    // Reset per-run anomaly state so each pump cycle gets a fresh startup check.
    this->startup_peak_current_ = 0.0f;
    this->startup_processed_ = false;
  } else if (!new_state && this->runtime_start_ms_ != 0) {
    this->runtime_seconds_ += (millis_64() - this->runtime_start_ms_) / 1000;
    this->runtime_start_ms_ = 0;
    this->runtime_pref_.save(&this->runtime_seconds_);
    this->last_off_ms_ = millis_64();
    this->set_anomaly_detected_(false);
  }
}

void PumpSwitch::set_anomaly_detected_(bool detected) {
  if (this->anomaly_detected_ == detected)
    return;
  this->anomaly_detected_ = detected;
  if (this->anomaly_status_sensor_ != nullptr)
    this->anomaly_status_sensor_->publish_state(detected);
}

const ScheduleRuntime *PumpSwitch::find_active_runtime(uint16_t slot_start_minute, uint8_t day_of_week) const {
  // active_schedule_idx_ 0 = Off, 1..N = user schedules (1-based), N+1 = builtin last.
  if (this->active_schedule_idx_ == 0 || this->active_schedule_idx_ > this->schedules_.size())
    return nullptr;

  const Schedule &schedule = this->schedules_[this->active_schedule_idx_ - 1];
  // day_of_week: 1=Sun..7=Sat; bitmask: bit0=Sun..bit6=Sat
  uint8_t day_mask = static_cast<uint8_t>(1u << (day_of_week - 1));

  for (const auto &rt : schedule.runtimes) {
    if (slot_start_minute >= rt.start_minute && slot_start_minute < rt.end_minute) {
      if (rt.days_of_week & day_mask)
        return &rt;
    }
  }
  return nullptr;
}

void PumpSwitch::loop() {
  const uint64_t now = millis_64();

#ifdef USE_SENSOR
  if (this->current_sensor_ != nullptr)
    this->update_current_sensor_(now);
#endif

  if (this->flow_sensor_ != nullptr) {
    if (!this->has_current_sensor())
      this->update_flow_based_runtime_(now);
    this->update_flow_watchdogs_(now);
  }
}

}  // namespace pool_controller
}  // namespace esphome
