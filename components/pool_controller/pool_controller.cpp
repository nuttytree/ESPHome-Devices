#include "pool_controller.h"
#include "pool_heater.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller";

void PoolController::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Pool Controller");
  if (this->disable_pumps_sensor_ != nullptr) {
    this->disable_pumps_sensor_->add_on_state_callback([this](bool state) {
      if (state) {
        ESP_LOGD(TAG, "Disable-pumps sensor activated — shutting down all pumps");
        this->request_primary_turn_off_();
      }
    });
  }
}

void PoolController::reset_all_pump_runtimes_() {
  ESP_LOGD(TAG, "Half-hour boundary – resetting pump runtime counters (%02d:%02d)", this->last_check_->hour,
           this->last_check_->minute);
  if (this->primary_pump_ != nullptr)
    this->primary_pump_->reset_runtime();
  for (auto *aux : this->auxiliary_pumps_)
    aux->reset_runtime();
}

void PoolController::tick_all_pump_schedules_(const ESPTime &now) {
  // If a delayed primary turn-off is in progress, only service the timer.
  if (this->primary_turn_off_pending_) {
    if (millis_64() >= this->primary_turn_off_at_ms_) {
      if (this->primary_pump_ != nullptr && this->primary_pump_->state)
        this->primary_pump_->turn_off();
      this->primary_turn_off_pending_ = false;
    }
    return;
  }

  if (this->primary_pump_ != nullptr)
    this->tick_pump_schedule_(this->primary_pump_, now);

  // If the primary tick just armed the sequenced shutdown, skip aux ticks this
  // cycle so auxiliaries cannot immediately turn themselves back on.
  if (this->primary_turn_off_pending_)
    return;

  for (auto *aux : this->auxiliary_pumps_)
    this->tick_pump_schedule_(aux, now);
}

void PoolController::tick_pump_schedule_(PumpSwitch *pump, const ESPTime &now) {
  // Off schedule — ensure pump is always off.
  if (pump->is_off_schedule()) {
    if (pump == this->primary_pump_) {
      this->request_primary_turn_off_();
    } else {
      if (pump->state)
        pump->turn_off();
    }
    return;
  }

  const uint16_t slot_start = static_cast<uint16_t>(now.hour) * 60 + (now.minute >= 30 ? 30 : 0);
  uint32_t target_seconds = 0;

  if (pump->is_builtin_last_schedule()) {
    if (pump == this->primary_pump_) {
      // "Always": run the full 30-minute window.
      target_seconds = 60u * 30u;  // 1800 s
    } else {
      // "When X is Running": mirror the primary pump's state.
      if (this->primary_pump_ != nullptr && this->primary_pump_->state) {
        if (!pump->state && pump->can_turn_on() && !pump->is_disabled() && this->primary_is_ready_for_aux_())
          pump->turn_on();
      } else {
        if (pump->state)
          pump->turn_off();
      }
      return;
    }
  } else {
    // User-defined schedule.
    // Auxiliary pumps are blocked entirely when the primary pump is on the Off schedule.
    if (pump != this->primary_pump_ && this->primary_pump_ != nullptr && this->primary_pump_->is_off_schedule()) {
      if (pump->state)
        pump->turn_off();
      return;
    }
    // Target on-time for this 30-minute window:
    //   minutes_per_hour / 2 converted to seconds  =  minutes_per_hour * 30
    const ScheduleRuntime *rt = pump->find_active_runtime(slot_start, now.day_of_week);
    if (rt != nullptr && rt->minutes_per_hour > 0)
      target_seconds = static_cast<uint32_t>(rt->minutes_per_hour) * 30;
  }

  const uint32_t current_runtime = pump->get_runtime_seconds();

  // A full-window target (60 min/hr = 1800 s) means run continuously for the entire
  // 30-minute slot.  Never turn the pump off because the runtime counter caught up to
  // the target — the half-hour reset will clear the counter without stopping the pump.
  const bool full_window = (target_seconds == 60u * 30u);

  if (!full_window && (target_seconds == 0 || current_runtime >= target_seconds)) {
    // Own schedule says stop — but keep the primary alive if an auxiliary still needs it.
    if (pump == this->primary_pump_ && this->any_auxiliary_needs_primary_(slot_start, now.day_of_week)) {
      if (!pump->state && pump->can_turn_on() && !pump->is_disabled())
        pump->turn_on();
      return;
    }
    // Sequence the primary turn-off; auxiliaries turn off immediately.
    if (pump == this->primary_pump_) {
      this->request_primary_turn_off_();
    } else {
      if (pump->state)
        pump->turn_off();
    }
    return;
  }

  // Below target (or full-window) — turn on if the pump is off and the cooldown has elapsed.
  if (!pump->state && pump->can_turn_on() && !pump->is_disabled()) {
    // Auxiliaries must wait sequence_delay_ms_ after the primary turned on before starting.
    if (pump != this->primary_pump_ && !this->primary_is_ready_for_aux_())
      return;
    pump->turn_on();
  }
}

void PoolController::request_primary_turn_off_() {
  // Turn off the pool heater immediately so it is off before the primary pump stops.
  if (this->pool_heater_ != nullptr)
    this->pool_heater_->request_heater_off();

  // Turn off all auxiliary pumps immediately.
  for (auto *aux : this->auxiliary_pumps_)
    if (aux->state)
      aux->turn_off();

  // Queue the primary pump turn-off after sequence_delay_ms_ (if not already pending).
  if (this->primary_pump_ != nullptr && this->primary_pump_->state && !this->primary_turn_off_pending_) {
    ESP_LOGD(TAG, "Sequenced shutdown: heater/auxiliaries off now, primary off in %" PRIu32 " ms",
             this->sequence_delay_ms_);
    this->primary_turn_off_pending_ = true;
    this->primary_turn_off_at_ms_ = millis_64() + this->sequence_delay_ms_;
  }
}

bool PoolController::primary_is_ready_for_aux_() const {
  if (this->primary_pump_ == nullptr || !this->primary_pump_->state)
    return false;
  // Auxiliaries may start only after the primary has been running for sequence_delay_ms_.
  return (millis_64() - this->primary_pump_->turned_on_ms_) >= this->sequence_delay_ms_;
}

bool PoolController::any_auxiliary_needs_primary_(uint16_t slot_start, uint8_t day_of_week) const {
  for (auto *aux : this->auxiliary_pumps_) {
    // Only user-defined schedules can independently demand the primary pump.
    if (aux->is_off_schedule() || aux->is_builtin_last_schedule())
      continue;
    if (aux->is_disabled())
      continue;
    const ScheduleRuntime *rt = aux->find_active_runtime(slot_start, day_of_week);
    if (rt == nullptr || rt->minutes_per_hour == 0)
      continue;
    const uint32_t target = static_cast<uint32_t>(rt->minutes_per_hour) * 30;
    // A full-window auxiliary (60 min/hr) always needs the primary for the entire slot.
    if (target == 60u * 30u)
      return true;
    if (aux->get_runtime_seconds() < target)
      return true;
  }
  return false;
}

void PoolController::loop() {
  if (this->rtc_ == nullptr)
    return;

  ESPTime now = this->rtc_->now();
  if (!now.is_valid())
    return;

  // Mirrors the CronTrigger::loop() pattern to handle NTP time jumps correctly.
  static constexpr int MAX_TIMESTAMP_DRIFT = 900;  // seconds

  if (this->last_check_.has_value()) {
    if (*this->last_check_ > now && this->last_check_->timestamp - now.timestamp > MAX_TIMESTAMP_DRIFT) {
      // Clock jumped backwards (e.g. NTP correction) — reset tracking.
      ESP_LOGW(TAG, "Time has jumped back — resetting runtime counter tracking");
      this->last_check_ = now;
      return;
    } else if (*this->last_check_ >= now) {
      // Already handled this second.
      return;
    } else if (now > *this->last_check_ && now.timestamp - this->last_check_->timestamp > MAX_TIMESTAMP_DRIFT) {
      // Clock jumped forward — skip ahead without firing missed resets.
      ESP_LOGW(TAG, "Time has jumped forward — skipping missed runtime resets");
      this->last_check_ = now;
      return;
    }

    // Walk second-by-second so no :00/:30 boundary is ever missed.
    while (true) {
      this->last_check_->increment_second();
      if (*this->last_check_ >= now)
        break;
      if (this->last_check_->second == 0 && this->last_check_->minute % 30 == 0)
        this->reset_all_pump_runtimes_();
      this->tick_all_pump_schedules_(*this->last_check_);
    }
  }

  this->last_check_ = now;

  if (now.second == 0 && now.minute % 30 == 0)
    this->reset_all_pump_runtimes_();
  this->tick_all_pump_schedules_(now);
}

}  // namespace pool_controller
}  // namespace esphome
