#include "pump_anomaly.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

void PumpSwitch::reset_anomaly_baseline() {
#ifdef USE_SENSOR
  this->anomaly_baseline_ = AnomalyBaseline();
  this->sample_count_ = 0;
  this->baseline_locked_ = false;
  this->startup_peak_current_ = 0.0f;
  this->startup_processed_ = false;
  this->set_anomaly_detected_(false);
  this->anomaly_pref_.save(&this->anomaly_baseline_);
  ESP_LOGI(TAG, "'%s' anomaly baseline reset; capturing new samples", this->get_name().c_str());
#endif
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
    this->set_anomaly_detected_(true);
    this->fire_anomaly_("CURRENT_HIGH");
  } else if (current < baseline - band) {
    this->set_anomaly_detected_(true);
    this->fire_anomaly_("CURRENT_LOW");
  }

  // Long-term drift: slow EMA rising above half the threshold band suggests bearing wear.
  const float drift_band = baseline * (this->anomaly_threshold_pct_ / 200.0f);
  if (this->anomaly_baseline_.drift_ema > baseline + drift_band) {
    this->set_anomaly_detected_(true);
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
      this->set_anomaly_detected_(true);
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

void PumpAnomalySwitch::setup() {
  Component::setup();
  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);
  this->publish_state(initial_state);
  if (this->pump_ != nullptr)
    this->pump_->set_enable_anomaly_detection(this->state);
}

void PumpAnomalySwitch::write_state(bool state) {
  this->publish_state(state);
  if (this->pump_ != nullptr)
    this->pump_->set_enable_anomaly_detection(state);
}

void PumpAnomalyStatusBinarySensor::setup() {
  Component::setup();
  this->publish_initial_state(false);
}

void PumpAnomalyResetButton::press_action() {
  if (this->pump_ != nullptr)
    this->pump_->reset_anomaly_baseline();
}

#endif  // USE_SENSOR

}  // namespace pool_controller
}  // namespace esphome
