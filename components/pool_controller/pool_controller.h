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
  void set_auxiliary_pumps(std::vector<AuxiliaryPumpSwitch *> auxiliary_pumps) { this->auxiliary_pumps_ = std::move(auxiliary_pumps); }
  void set_sequence_delay(uint32_t delay_ms) { this->sequence_delay_ms_ = delay_ms; }
  void set_disable_pumps_sensor(binary_sensor::BinarySensor *sensor) { this->disable_pumps_sensor_ = sensor; }
  void set_pool_heater(PoolHeater *heater) { this->pool_heater_ = heater; }

 protected:
  PrimaryPumpSwitch *primary_pump_{nullptr};
  time::RealTimeClock *rtc_{nullptr};
  std::vector<AuxiliaryPumpSwitch *> auxiliary_pumps_;
  binary_sensor::BinarySensor *disable_pumps_sensor_{nullptr};  ///< Optional sensor that turns off pumps and blocks turn-ons when active.
  PoolHeater *pool_heater_{nullptr};                            ///< Optional pool heater — turned off before the primary pump during sequenced shutdown.

  /// Mirrors CronTrigger::last_check_ — used for time-drift-safe :00/:30 detection.
  optional<ESPTime> last_check_;

  /// Sequenced shutdown state: auxiliaries off immediately, primary off after sequence_delay_ms_.
  bool primary_turn_off_pending_{false};
  uint64_t primary_turn_off_at_ms_{0};
  uint32_t sequence_delay_ms_{2000};  ///< Configurable delay (ms) between primary and auxiliary pump state changes.

  void reset_all_pump_runtimes_();
  void tick_all_pump_schedules_(const ESPTime &now);
  void tick_pump_schedule_(PumpSwitch *pump, const ESPTime &now);

  /// Returns true if any auxiliary pump has a user-defined schedule with remaining
  /// runtime in the given half-hour slot — used to keep the primary pump running
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
