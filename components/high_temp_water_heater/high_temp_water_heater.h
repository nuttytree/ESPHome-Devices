#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/water_heater/water_heater.h"

namespace esphome::high_temp_water_heater {

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
  water_heater::WaterHeaterTraits traits() override;
  void control(const water_heater::WaterHeaterCall &call) override;

  void update_state_();

  void apply_control_();

  water_heater::WaterHeater *source_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  bool temperature_sensor_is_fahrenheit_{false};
  float temperature_sensor_offset_{0.0f};
  float dead_band_{5.0f};
  float over_run_{0.0f};
  float min_temperature_{40.0f};
  float max_temperature_{65.0f};
  float target_temperature_step_{1.0f};
  float monitored_temp_{NAN};
  bool heating_active_{false};
};

}  // namespace esphome::high_temp_water_heater
