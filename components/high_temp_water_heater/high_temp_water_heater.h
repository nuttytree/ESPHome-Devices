#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/water_heater/water_heater.h"

namespace esphome {
namespace high_temp_water_heater {

class HighTempWaterHeater : public water_heater::WaterHeater, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void set_source_water_heater(water_heater::WaterHeater *source) { this->source_ = source; }
  void set_temperature_sensor(sensor::Sensor *sens) { this->temperature_sensor_ = sens; }
  void set_temperature_sensor_offset(float offset) { this->temperature_sensor_offset_ = offset; }
  void set_min_temperature(float min_temperature) { this->min_temperature_ = min_temperature; }
  void set_max_temperature(float max_temperature) { this->max_temperature_ = max_temperature; }
  void set_target_temperature_step(float step) { this->target_temperature_step_ = step; }
  void set_dead_band(float dead_band) { this->dead_band_ = dead_band; }
  void set_over_run(float over_run) { this->over_run_ = over_run; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  water_heater::WaterHeaterCallInternal make_call() override {
    return water_heater::WaterHeaterCallInternal(this);
  }

 protected:
  void control(const water_heater::WaterHeaterCall &call) override;
  water_heater::WaterHeaterTraits traits() override;

  /// Sync current_temperature and monitored_temp_ from source; returns true if anything changed.
  bool sync_from_source_();

  /// Evaluate control logic and command the child water heater accordingly.
  void apply_control_();

  /// The child water heater whose temperature and traits are mirrored.
  water_heater::WaterHeater *source_{nullptr};
  /// Optional sensor for the temperature; falls back to source current temperature.
  sensor::Sensor *temperature_sensor_{nullptr};
  /// True if the temperature sensor reports in °F and its values must be converted to °C.
  bool temperature_sensor_is_fahrenheit_{false};
  /// Degrees (°C) added to the temperature sensor reading to derive monitored_temp_.
  float temperature_sensor_offset_{0.0f};
  /// Hysteresis band below target temperature before heating engages (°C).
  float dead_band_{5.0f};
  /// Hysteresis band above target temperature before heating is cut (°C).
  float over_run_{0.0f};
  /// The temperature value used for bang-bang control decisions.
  float monitored_temp_{NAN};
  /// True while the bang-bang controller is actively calling for heat.
  bool heating_active_{false};
  /// Timestamp (ms) of the last periodic status log in apply_control_().
  uint32_t last_status_log_ms_{0};
  float min_temperature_{40.0f};
  float max_temperature_{65.0f};
  float target_temperature_step_{1.0f};
};

}  // namespace high_temp_water_heater
}  // namespace esphome
