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
  this->oob_streak_ = 0;
  this->spread_ema_ = 0.0f;
  this->run_flow_confirmed_ = false;
  // Clear the cooldown so the next real anomaly isn't silently suppressed by an alert that
  // fired against the baseline we just discarded.
  this->last_anomaly_ms_ = 0;
  this->set_anomaly_detected_(false);
  // Capture is armed, not started. A run already in progress has no startup window left to
  // observe, and its inrush peak (if any) belongs to the discarded baseline — so nothing is
  // sampled until the pump next turns on and a complete run is available. turned_on_ms_ is
  // deliberately left alone; it is shared with heater and auxiliary-pump sequencing.
  this->awaiting_fresh_start_ = true;
  this->anomaly_pref_.save(&this->anomaly_baseline_);
  ESP_LOGI(TAG, "'%s' anomaly baseline reset; capture begins at the next pump start", this->get_name().c_str());
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
// Consecutive out-of-band samples required before an anomaly is asserted (debounce).
static constexpr uint8_t ANOMALY_DEBOUNCE_SAMPLES = 3;
// Fast EWMA alpha for the live spread estimate (responds within ~10 samples).
static constexpr float EMA_SPREAD_ALPHA = 0.1f;
// Live spread must exceed the learned baseline stdev by this factor to trip VARIANCE_SPIKE.
static constexpr float ANOMALY_VARIANCE_MULTIPLIER = 4.0f;
// Alpha for steady-state EMA during the learning phase.
static constexpr float EMA_LEARN_ALPHA = 0.1f;
// Alpha for the long-term drift EMA (very slow; 1 000-sample memory).
static constexpr float EMA_DRIFT_ALPHA = 0.001f;

void PumpSwitch::tick_anomaly_() {
  // A baseline reset arms capture but doesn't start it: wait for the next turn-on so the new
  // baseline is built from a whole run, startup window included. track_runtime() clears this.
  if (this->awaiting_fresh_start_)
    return;

  const float current = this->current_sensor_->state;
  if (std::isnan(current) || current < 0.0f)
    return;

  const uint64_t run_ms = millis_64() - this->turned_on_ms_;
  ESP_LOGD(TAG, "'%s' tick_anomaly_: run_ms=%" PRIu64 " turned_on_ms_=%" PRIu64, this->get_name().c_str(), run_ms,
           this->turned_on_ms_);
  const bool in_startup = (run_ms < ANOMALY_STARTUP_WINDOW_MS);

  if (in_startup) {
    // Track the inrush peak; do not update the steady-state EMA yet. Flow is only noted here,
    // never required — a pump takes a moment to prime, so the whole window is allowed for it.
    if (current > this->startup_peak_current_)
      this->startup_peak_current_ = current;
    if (this->has_flow_sensor() && this->flow_sensor_->state)
      this->run_flow_confirmed_ = true;
    return;
  }

  // Startup window just finished — evaluate the captured inrush peak once.
  if (!this->startup_processed_) {
    this->startup_processed_ = true;
    if (this->startup_capture_valid_()) {
      this->process_startup_peak_();
    } else {
      // The motor drew its inrush but never moved water, so this peak says nothing about a
      // healthy start. Discard it rather than averaging it into the startup reference.
      ESP_LOGW(TAG, "'%s' startup peak %.3fA discarded: no flow established during startup", this->get_name().c_str(),
               this->startup_peak_current_);
    }
  }

  // ── Steady-state phase ──────────────────────────────────────────
  // Flow gate: with a flow sensor present, statistics are only meaningful while water is
  // actually moving. Drop the debounce streak so a no-flow gap can't count toward a trip.
  if (!this->stats_capture_allowed_()) {
    this->oob_streak_ = 0;
    return;
  }

  if (!this->baseline_locked_) {
    // Learning phase: build EMA baseline toward lock.
    if (this->sample_count_ == 0) {
      this->anomaly_baseline_.steady_state = current;
      this->anomaly_baseline_.drift_ema = current;
      this->anomaly_baseline_.var_mean = current;
      this->anomaly_baseline_.var_m2 = 0.0f;
    } else {
      this->anomaly_baseline_.steady_state =
          EMA_LEARN_ALPHA * current + (1.0f - EMA_LEARN_ALPHA) * this->anomaly_baseline_.steady_state;
      this->anomaly_baseline_.drift_ema =
          EMA_LEARN_ALPHA * current + (1.0f - EMA_LEARN_ALPHA) * this->anomaly_baseline_.drift_ema;
      // Welford variance over the learning window; n = sample_count_ + 1 (this sample).
      const float delta = current - this->anomaly_baseline_.var_mean;
      this->anomaly_baseline_.var_mean += delta / static_cast<float>(this->sample_count_ + 1);
      this->anomaly_baseline_.var_m2 += delta * (current - this->anomaly_baseline_.var_mean);
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

  // Trip band: |deviation| beyond this, sustained for ANOMALY_DEBOUNCE_SAMPLES consecutive
  // samples, asserts the anomaly. Clear band is half that (hysteresis) so the flag doesn't
  // re-arm right at the trip boundary — current must settle solidly back toward baseline.
  const float band = baseline * (this->anomaly_threshold_pct_ / 100.0f);
  const float clear_band = band * 0.5f;
  const float deviation = current - baseline;
  const bool out_of_band = std::fabs(deviation) > band;

  // Long-term drift: slow EMA moving more than half the threshold band away from baseline in
  // either direction — rising suggests bearing wear, falling suggests prime/flow loss.
  const float drift_band = baseline * (this->anomaly_threshold_pct_ / 200.0f);
  const bool drifting = std::fabs(this->anomaly_baseline_.drift_ema - baseline) > drift_band;

  // Live spread vs. the learned baseline stdev (Welford, accumulated during learning).
  // Widening spread is often the earliest sign of trouble, well before the mean moves.
  const float abs_dev = std::fabs(deviation);
  this->spread_ema_ = EMA_SPREAD_ALPHA * abs_dev + (1.0f - EMA_SPREAD_ALPHA) * this->spread_ema_;
  const uint32_t var_n = this->anomaly_baseline_.sample_count;
  const float baseline_stdev =
      (var_n > 1) ? std::sqrt(this->anomaly_baseline_.var_m2 / static_cast<float>(var_n - 1)) : 0.0f;
  const bool variance_spike = baseline_stdev > 0.0f && this->spread_ema_ > baseline_stdev * ANOMALY_VARIANCE_MULTIPLIER;

  if (out_of_band) {
    if (this->oob_streak_ < 0xFF)
      this->oob_streak_++;
    if (this->oob_streak_ >= ANOMALY_DEBOUNCE_SAMPLES && !this->anomaly_detected_) {
      this->set_anomaly_detected_(true);
      this->fire_anomaly_(deviation > 0.0f ? "CURRENT_HIGH" : "CURRENT_LOW");
    }
  } else {
    this->oob_streak_ = 0;
  }

  if (drifting && !this->anomaly_detected_) {
    this->set_anomaly_detected_(true);
    this->fire_anomaly_("BASELINE_DRIFT");
  }

  if (variance_spike && !this->anomaly_detected_) {
    this->set_anomaly_detected_(true);
    this->fire_anomaly_("VARIANCE_SPIKE");
  }

  // Explicit clear: only once current is back within the tighter hysteresis band AND
  // long-term drift and live spread have also subsided. Keeps the binary sensor meaning
  // "anomalous right now" rather than "was anomalous at some point since turn-on".
  if (std::fabs(deviation) <= clear_band && !drifting && !variance_spike) {
    this->set_anomaly_detected_(false);
  }
}

void PumpSwitch::process_startup_peak_() {
  if (this->startup_peak_current_ <= 0.0f)
    return;

  auto &bl = this->anomaly_baseline_;
  if (bl.startup_runs == 0 || bl.startup_peak == 0.0f) {
    bl.startup_peak = this->startup_peak_current_;
    bl.startup_runs = 1;
  } else {
    // Compare against the reference as it stood entering this run, before this run's own
    // sample gets folded in below — otherwise a low peak would dilute the average it's
    // being checked against.
    if (bl.startup_runs >= ANOMALY_MIN_STARTUP_RUNS && this->startup_peak_current_ < bl.startup_peak * 0.5f &&
        !this->anomaly_detected_) {
      this->set_anomaly_detected_(true);
      this->fire_anomaly_("NO_STARTUP_SPIKE");
    }
    if (bl.startup_runs < 10) {
      // Average the first 10 runs to establish a stable inrush reference.
      bl.startup_peak = (bl.startup_peak * bl.startup_runs + this->startup_peak_current_) / (bl.startup_runs + 1);
      bl.startup_runs++;
    }
  }

  if (!this->baseline_locked_)
    this->anomaly_pref_.save(&bl);
}

void PumpSwitch::fire_anomaly_(const std::string &reason) {
  if (this->anomaly_reason_sensor_ != nullptr)
    this->anomaly_reason_sensor_->publish_state(reason);

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

/// Always resets — the press is never refused. Contamination of the new baseline is prevented
/// by the capture rules themselves (a complete run is required, and with a flow sensor, flow
/// must be present), not by second-guessing when the button may be pressed.
void PumpAnomalyResetButton::press_action() {
  if (this->pump_ == nullptr)
    return;
  this->pump_->reset_anomaly_baseline();
}

void PumpAnomalyReasonTextSensor::setup() {
  Component::setup();
  this->publish_state("");
}

#endif  // USE_SENSOR

}  // namespace pool_controller
}  // namespace esphome
