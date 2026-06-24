#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/water_heater/water_heater.h"

namespace esphome {
namespace pool_controller {

// Forward declaration — full type visible via pump_switch.h in pool_heater.cpp.
class PrimaryPumpSwitch;

class PoolHeater : public water_heater::WaterHeater, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }

  // ── Configuration setters ─────────────────────────────────────────────────
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_deadband(float deadband) { this->deadband_ = deadband; }
  void set_overrun(float overrun) { this->overrun_ = overrun; }
  void set_heater_output(output::BinaryOutput *output) { this->heater_output_ = output; }
  void set_primary_pump(PrimaryPumpSwitch *pump) { this->primary_pump_ = pump; }
  void set_min_temperature(float min_temp) { this->min_temperature_ = min_temp; }
  void set_max_temperature(float max_temp) { this->max_temperature_ = max_temp; }
  void set_target_temperature_step(float step) { this->target_temperature_step_ = step; }

  // ── Component lifecycle ────────────────────────────────────────────────────
  void setup() override;
  void loop() override;
  void dump_config() override;

  water_heater::WaterHeaterCallInternal make_call() override { return water_heater::WaterHeaterCallInternal(this); }

  // ── Integration helpers ────────────────────────────────────────────────────
  /// Force heater off immediately — called during the primary pump sequenced shutdown.
  void request_heater_off();

  /// Returns true when the heater output is currently energised.
  bool is_heater_active() const { return this->heater_active_; }

 protected:
  // ── WaterHeater overrides ─────────────────────────────────────────────────
  water_heater::WaterHeaterTraits traits() override;
  void control(const water_heater::WaterHeaterCall &call) override;

  // ── Internal helpers ───────────────────────────────────────────────────────
  /// Reads and caches the temperature sensor value when permitted by pump state.
  void update_temperature_();
  /// Bang-bang heater controller.  Called every loop tick and on every control() call.
  void apply_control_();

  // ── Configuration ──────────────────────────────────────────────────────────
  /// Defaults are 0.5 °F expressed in °C (= 5/18 ≈ 0.27778 °C).
  float deadband_{0.27778f};
  float overrun_{0.27778f};
  /// Defaults are 60 °F, 90 °F in °C.
  float min_temperature_{15.5556f};
  float max_temperature_{32.2222f};
  /// Default step is 1 °F in °C (= 5/9 ≈ 0.55556 °C).
  float target_temperature_step_{0.55556f};

  sensor::Sensor *temperature_sensor_{nullptr};
  output::BinaryOutput *heater_output_{nullptr};
  PrimaryPumpSwitch *primary_pump_{nullptr};

  // ── Runtime state ──────────────────────────────────────────────────────────
  bool sensor_is_fahrenheit_{false};       ///< True when the sensor reports in °F.
  bool has_reading_since_pump_on_{false};  ///< Cleared each time the primary pump starts.
  bool heater_active_{false};              ///< True when heater_output_ is energised.
  /// current_temperature_ (inherited from WaterHeater) holds the last accepted
  /// °C reading. It starts as NAN and is only written by update_temperature_(),
  /// so it doubles as the "have I ever received a valid reading?" guard.
};

}  // namespace pool_controller
}  // namespace esphome
