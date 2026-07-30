#pragma once

#include "./schedule_select.h"
#include "./pump_switch.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include "esphome/core/time.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace pool_controller {

class PoolHeater;  // forward declaration — full type in pool_heater.h

class PoolController : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;

  void set_rtc(time::RealTimeClock *rtc) { this->rtc_ = rtc; }
  void set_primary_pump(PrimaryPumpSwitch *primary_pump) { this->primary_pump_ = primary_pump; }
  void set_auxiliary_pumps(std::vector<AuxiliaryPumpSwitch *> auxiliary_pumps) {
    this->auxiliary_pumps_ = std::move(auxiliary_pumps);
  }
  void set_sequence_delay(uint32_t delay_ms) { this->sequence_delay_ms_ = delay_ms; }
  void set_disable_pumps_sensor(binary_sensor::BinarySensor *sensor) { this->disable_pumps_sensor_ = sensor; }
  void set_pool_heater(PoolHeater *heater) { this->pool_heater_ = heater; }
  /// How often each pump's operating state is written to flash. Default: 5 minutes.
  void set_state_save_interval(uint32_t interval_ms) { this->state_save_interval_ms_ = interval_ms; }
  /// How stale a saved state may be and still be resumed after a restart. Default: 15 minutes.
  void set_max_resume_age(uint32_t max_age_s) { this->max_resume_age_s_ = max_age_s; }

 protected:
  PrimaryPumpSwitch *primary_pump_{nullptr};
  time::RealTimeClock *rtc_{nullptr};
  std::vector<AuxiliaryPumpSwitch *> auxiliary_pumps_;
  binary_sensor::BinarySensor *disable_pumps_sensor_{
      nullptr};  ///< Optional sensor that turns off pumps and blocks turn-ons when active.
  PoolHeater *pool_heater_{
      nullptr};  ///< Optional pool heater — turned off before the primary pump during sequenced shutdown.

  /// Mirrors CronTrigger::last_check_ — used for time-drift-safe :00 detection.
  optional<ESPTime> last_check_;

  /// Sequenced shutdown state: auxiliaries off immediately, primary off after sequence_delay_ms_.
  bool primary_turn_off_pending_{false};
  uint64_t primary_turn_off_at_ms_{0};
  uint32_t sequence_delay_ms_{2000};  ///< Configurable delay (ms) between primary and auxiliary pump state changes.

  // ── Restart resume ─────────────────────────────────────────────────────────
  bool state_restored_{false};                    ///< True once the boot-time restore has run.
  uint64_t next_state_save_ms_{0};                ///< millis_64() of the next periodic snapshot.
  uint32_t state_save_interval_ms_{5u * 60000u};  ///< Interval between periodic snapshots.
  uint32_t max_resume_age_s_{15u * 60u};          ///< Snapshots older than this are discarded on restore.

  /// Applies every pump's saved state. Runs once, as soon as the clock is valid and before any
  /// schedule tick, so resumed runtimes are in place before anything acts on them.
  void restore_pump_states_(const ESPTime &now);
  /// Writes every pump's state if the snapshot interval has elapsed.
  void save_pump_states_if_due_();

  void reset_all_pump_runtimes_();
  void tick_all_pump_schedules_(const ESPTime &now);
  void tick_pump_schedule_(PumpSwitch *pump, const ESPTime &now);

  /// Returns true if any auxiliary pump has a user-defined schedule with remaining
  /// runtime in the given hour-long slot — used to keep the primary pump running
  /// even when its own schedule would otherwise turn it off.
  bool any_auxiliary_needs_primary_(uint16_t slot_start, uint8_t day_of_week) const;

  /// Turns off all auxiliary pumps immediately and queues the primary pump to
  /// turn off 2 seconds later (sequenced shutdown).
  void request_primary_turn_off_();

  /// Returns true when the primary pump has been physically on for at least 2 seconds,
  /// meaning it is safe to start auxiliary pumps.
  bool primary_is_ready_for_aux_() const;
};

}  // namespace pool_controller
}  // namespace esphome
