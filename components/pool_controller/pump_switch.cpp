#include "pump_switch.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

// Minimum time the pump must stay off between duty cycles, protecting the motor from short-cycling.
static constexpr uint32_t MIN_OFF_TIME_MS = 5u * 60u * 1000u;
// Length of a runtime slot; runtime targets are expressed per 1-hour slot.
static constexpr uint32_t SLOT_SECONDS = 60u * 60u;

// A preference's storage key is the entity's object-id hash XORed with the `version` argument,
// and the stored size is *not* part of the key. A pump owns two preferences, so both need a
// distinct non-zero version or they share one key and silently overwrite each other — load()
// then fails on the size mismatch and whichever wrote last is the only one that survives.
// Zero is reserved: it is what switch_::Switch's own restore-mode preference uses.
static constexpr uint32_t RUN_STATE_PREF_VERSION = 0x504D5253u;  // 'PMRS'
static constexpr uint32_t ANOMALY_PREF_VERSION = 0x504D414Eu;    // 'PMAN'

void PumpSwitch::dump_config() { LOG_SWITCH("", "Pool Controller Pump", this); }

void PumpSwitch::setup() {
  this->run_state_pref_ = this->make_entity_preference<PumpRunState>(RUN_STATE_PREF_VERSION);
  if (!this->run_state_pref_.load(&this->saved_run_state_))
    this->saved_run_state_ = PumpRunState();
    // The snapshot is deliberately not applied here. Deciding whether it is still worth trusting
    // needs a valid wall clock, which isn't available this early in boot, so PoolController calls
    // restore_run_state() once time is valid. Until then the pump behaves as if starting cold.

#ifdef USE_SENSOR
  if (this->current_sensor_ != nullptr) {
    this->anomaly_pref_ = this->make_entity_preference<AnomalyBaseline>(ANOMALY_PREF_VERSION);
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
  // Hold the full minimum off-time from boot. restore_run_state() relaxes this if the saved
  // snapshot shows the pump was mid-run, or credits off-time already served.
  this->can_turn_on_at_ms_ = millis_64() + MIN_OFF_TIME_MS;
}

void PumpSwitch::reset_runtime() {
  if (this->runtime_start_ms_ != 0) {
    // Pump is still running; restart the window so elapsed time in the new period is accurate.
    this->runtime_start_ms_ = millis_64();
  }
  this->runtime_seconds_ = 0;
  this->save_run_state();
  ESP_LOGD(TAG, "Runtime counter reset");
}

void PumpSwitch::save_run_state() {
  if (this->rtc_ == nullptr)
    return;
  const ESPTime now = this->rtc_->now();
  if (!now.is_valid())
    return;

  PumpRunState state{};
  state.saved_utc = static_cast<uint32_t>(now.timestamp);
  state.slot_hour = state.saved_utc / SLOT_SECONDS;
  // get_runtime_seconds() folds in the portion of the current run not yet accumulated, so a
  // crash mid-run loses only the time since the last snapshot rather than the whole run.
  state.runtime_seconds = this->get_runtime_seconds();

  if (this->runtime_start_ms_ != 0) {
    state.off_since_utc = 0;  // Running: restore reads this as "the only stop was the restart".
  } else {
    // Off, but with no recorded stop time — the pump has not run yet, or it stopped before the
    // clock was valid. Stamp now rather than leaving 0, which restore would misread as "was
    // running" and let the pump start with no minimum off-time at all. Under-estimating how
    // long it has been off is the safe direction: it can only lengthen that wait, never skip it.
    if (this->last_off_utc_ == 0)
      this->last_off_utc_ = state.saved_utc;
    state.off_since_utc = this->last_off_utc_;
  }

  this->saved_run_state_ = state;
  this->run_state_pref_.save(&state);
  // Commit now rather than leaving it to the global flash_write_interval. A snapshot still
  // sitting in RAM when the power drops is a snapshot that never existed, and that interval is
  // set for chatty components — it would otherwise silently decide how much runtime a restart
  // can recover, regardless of state_save_interval. Writes here are deliberate and bounded by
  // state_save_interval plus pump start/stop events.
  global_preferences->sync();
}

void PumpSwitch::restore_run_state(const ESPTime &now, uint32_t max_age_s) {
  const PumpRunState &state = this->saved_run_state_;
  if (state.saved_utc == 0) {
    ESP_LOGD(TAG, "'%s' no saved state to resume from", this->get_name().c_str());
    return;
  }

  const uint32_t now_utc = static_cast<uint32_t>(now.timestamp);
  if (now_utc < state.saved_utc) {
    ESP_LOGW(TAG, "'%s' saved state is timestamped in the future — ignoring", this->get_name().c_str());
    return;
  }

  const uint32_t age_s = now_utc - state.saved_utc;
  if (age_s > max_age_s) {
    ESP_LOGI(TAG, "'%s' saved state is %" PRIu32 "s old (limit %" PRIu32 "s) — starting cold", this->get_name().c_str(),
             age_s, max_age_s);
    return;
  }

  // Runtime only carries over within the same hour slot. A slot boundary crossed while the
  // device was down is an hourly reset nobody was running to perform: without this the stale
  // count would be compared against the new slot's target and suppress the run entirely.
  const uint32_t slot_hour = now_utc / SLOT_SECONDS;
  this->runtime_seconds_ = (slot_hour == state.slot_hour) ? state.runtime_seconds : 0;

  // Minimum off-time. If the pump was running when the snapshot was taken, the stop was the
  // restart itself rather than the end of a duty cycle — there is no short-cycling to protect
  // against, so let the schedule bring it straight back up. Otherwise credit the off-time
  // already served, which the wall clock covers across the outage.
  uint32_t remaining_ms = 0;
  if (state.off_since_utc != 0) {
    this->last_off_utc_ = state.off_since_utc;
    const uint32_t off_for_s = (now_utc > state.off_since_utc) ? now_utc - state.off_since_utc : 0;
    if (off_for_s < MIN_OFF_TIME_MS / 1000)
      remaining_ms = MIN_OFF_TIME_MS - off_for_s * 1000;
  }
  this->can_turn_on_at_ms_ = millis_64() + remaining_ms;

  ESP_LOGI(TAG, "'%s' resumed from state %" PRIu32 "s old: runtime %" PRIu32 "s%s, may start in %" PRIu32 "s",
           this->get_name().c_str(), age_s, this->runtime_seconds_,
           (slot_hour == state.slot_hour) ? "" : " (new slot, reset)", remaining_ms / 1000);
}

void PumpSwitch::track_runtime(bool new_state) {
  if (new_state && this->runtime_start_ms_ == 0) {
    this->runtime_start_ms_ = millis_64();
    this->turned_on_ms_ = millis_64();
    // Reset per-run anomaly state so each pump cycle gets a fresh startup check.
    this->startup_peak_current_ = 0.0f;
    this->startup_processed_ = false;
    this->oob_streak_ = 0;
    this->run_flow_confirmed_ = false;
    // This is the clean start a pending baseline reset was waiting for: capture may resume,
    // and it now begins at a real turn-on with the startup window still ahead of it.
    this->awaiting_fresh_start_ = false;
    this->last_off_utc_ = 0;
    // Snapshot the start immediately: a restart moments from now needs to know the pump was
    // mid-run, which is what lets it resume without waiting out the minimum off-time.
    this->save_run_state();
  } else if (!new_state && this->runtime_start_ms_ != 0) {
    this->runtime_seconds_ += (millis_64() - this->runtime_start_ms_) / 1000;
    this->runtime_start_ms_ = 0;
    this->can_turn_on_at_ms_ = millis_64() + MIN_OFF_TIME_MS;
    if (this->rtc_ != nullptr) {
      const ESPTime off_at = this->rtc_->now();
      if (off_at.is_valid())
        this->last_off_utc_ = static_cast<uint32_t>(off_at.timestamp);
    }
    this->save_run_state();
    // Deliberately NOT clearing anomaly_detected_ here: turning off is not evidence the
    // problem is resolved. Once tripped, the flag stays latched across the off period and
    // into subsequent runs until tick_anomaly_()'s in-spec hysteresis check (current back
    // within the clear band, drift settled, spread settled) actually confirms recovery.
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
