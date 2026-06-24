#include "pump_switch.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>
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

  if (this->enable_anomaly_detection_) {
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
  }
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
  // ── Current sensor processing (1 Hz) ──────────────────────────────────────
  if (this->current_sensor_ != nullptr && (this->use_current_for_state_ || this->enable_anomaly_detection_)) {
    if (now - this->last_sample_ms_ >= 1000) {
      this->last_sample_ms_ = now;
      const float current = this->current_sensor_->state;

      // Current-based runtime tracking: state always reflects the output command.
      // Runtime is only accumulated while the output is on AND current confirms the motor is running.
      if (this->use_current_for_state_ && !std::isnan(current)) {
        const bool motor_running = this->state && (current >= this->current_on_threshold_);
        if (motor_running != this->motor_running_) {
          ESP_LOGD(TAG, "'%s' motor running state: %s (output=%s, %.3fA %s %.3fA threshold)", this->get_name().c_str(),
                   motor_running ? "YES" : "NO", this->state ? "ON" : "OFF", current, motor_running ? ">=" : "<",
                   this->current_on_threshold_);
          this->motor_running_ = motor_running;
          this->track_runtime(motor_running);
        }
      }

      // Anomaly detection (only active while pump is considered on).
      if (this->enable_anomaly_detection_ && this->state)
        this->tick_anomaly_();
    }
  }
#endif

  // ── Flow sensor watchdog ───────────────────────────────────────────────────
  // "Should be moving water" means:
  //   - no current sensor: output is on (state == true)
  //   - with current sensor: output is on AND current confirms motor is running
  if (this->flow_sensor_ != nullptr) {
    const bool should_have_flow = this->use_current_for_state_ ? this->motor_running_ : this->state;

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
        this->turn_off();
      }
    } else {
      // Flow is present, or pump is not expected to be running — reset watchdog.
      if (this->flow_check_start_ms_ != 0) {
        ESP_LOGD(TAG, "'%s' flow confirmed — clearing no-flow watchdog", this->get_name().c_str());
        this->flow_check_start_ms_ = 0;
      }
    }
  }
}

// All anomaly methods below are compiled only when the sensor platform is present.
#ifdef USE_SENSOR

// Startup window: 15 s after turn-on.  After this, steady-state EMA begins.
static constexpr uint32_t ANOMALY_STARTUP_WINDOW_MS = 15000;
// Minimum interval between successive anomaly alerts for the same pump.
static constexpr uint32_t ANOMALY_COOLDOWN_MS = 5u * 60u * 1000u;  // 5 minutes
// Minimum startup runs before inrush-peak comparison begins.
static constexpr uint8_t ANOMALY_MIN_STARTUP_RUNS = 3;
// Alpha for steady-state EMA during the learning phase.
static constexpr float EMA_LEARN_ALPHA = 0.1f;
// Alpha for the long-term drift EMA (very slow; 1 000-sample memory).
static constexpr float EMA_DRIFT_ALPHA = 0.001f;

void PumpSwitch::tick_anomaly_() {
  const float current = this->current_sensor_->state;
  if (std::isnan(current) || current < 0.0f)
    return;

  const uint64_t run_ms = millis_64() - this->turned_on_ms_;
  const bool in_startup = (run_ms < ANOMALY_STARTUP_WINDOW_MS);

  if (in_startup) {
    // Track the inrush peak; do not update the steady-state EMA yet.
    if (current > this->startup_peak_current_)
      this->startup_peak_current_ = current;
    return;
  }

  // Startup window just finished — evaluate the captured inrush peak once.
  if (!this->startup_processed_) {
    this->startup_processed_ = true;
    this->process_startup_peak_();
  }

  // ── Steady-state phase ──────────────────────────────────────────
  if (!this->baseline_locked_) {
    // Learning phase: build EMA baseline toward lock.
    if (this->sample_count_ == 0) {
      this->anomaly_baseline_.steady_state = current;
      this->anomaly_baseline_.drift_ema = current;
    } else {
      this->anomaly_baseline_.steady_state =
          EMA_LEARN_ALPHA * current + (1.0f - EMA_LEARN_ALPHA) * this->anomaly_baseline_.steady_state;
      this->anomaly_baseline_.drift_ema =
          EMA_LEARN_ALPHA * current + (1.0f - EMA_LEARN_ALPHA) * this->anomaly_baseline_.drift_ema;
    }
    this->anomaly_baseline_.sample_count = ++this->sample_count_;
    if (this->sample_count_ >= this->learning_samples_) {
      this->baseline_locked_ = true;
      this->anomaly_pref_.save(&this->anomaly_baseline_);
      ESP_LOGI(TAG, "'%s' anomaly baseline locked at %.3fA after %" PRIu32 " samples", this->get_name().c_str(),
               this->anomaly_baseline_.steady_state, this->sample_count_);
    }
    return;
  }

  // Monitoring phase: check deviation and update the slow drift EMA.
  this->anomaly_baseline_.drift_ema =
      EMA_DRIFT_ALPHA * current + (1.0f - EMA_DRIFT_ALPHA) * this->anomaly_baseline_.drift_ema;

  const float baseline = this->anomaly_baseline_.steady_state;
  if (baseline <= 0.0f)
    return;

  const float band = baseline * (this->anomaly_threshold_pct_ / 100.0f);
  if (current > baseline + band) {
    this->fire_anomaly_("CURRENT_HIGH");
  } else if (current < baseline - band) {
    this->fire_anomaly_("CURRENT_LOW");
  }

  // Long-term drift: slow EMA rising above half the threshold band suggests bearing wear.
  const float drift_band = baseline * (this->anomaly_threshold_pct_ / 200.0f);
  if (this->anomaly_baseline_.drift_ema > baseline + drift_band) {
    this->fire_anomaly_("BASELINE_DRIFT");
  }
}

void PumpSwitch::process_startup_peak_() {
  if (this->startup_peak_current_ <= 0.0f)
    return;

  auto &bl = this->anomaly_baseline_;
  if (bl.startup_runs == 0 || bl.startup_peak == 0.0f) {
    bl.startup_peak = this->startup_peak_current_;
    bl.startup_runs = 1;
  } else if (bl.startup_runs < 10) {
    // Average the first 10 runs to establish a stable inrush reference.
    bl.startup_peak = (bl.startup_peak * bl.startup_runs + this->startup_peak_current_) / (bl.startup_runs + 1);
    bl.startup_runs++;
  } else if (bl.startup_runs >= ANOMALY_MIN_STARTUP_RUNS) {
    // Established baseline: flag if the inrush peak is less than 50 % of normal.
    if (this->startup_peak_current_ < bl.startup_peak * 0.5f) {
      this->fire_anomaly_("NO_STARTUP_SPIKE");
    }
  }

  if (!this->baseline_locked_)
    this->anomaly_pref_.save(&bl);
}

void PumpSwitch::fire_anomaly_(const std::string &reason) {
  const uint64_t now = millis_64();
  if (now - this->last_anomaly_ms_ < ANOMALY_COOLDOWN_MS)
    return;
  this->last_anomaly_ms_ = now;
  ESP_LOGW(TAG, "'%s' pump anomaly: %s (baseline=%.3fA)", this->get_name().c_str(), reason.c_str(),
           this->anomaly_baseline_.steady_state);
  if (this->anomaly_trigger_ != nullptr)
    this->anomaly_trigger_->trigger(reason);
}

#endif  // USE_SENSOR

}  // namespace pool_controller
}  // namespace esphome
