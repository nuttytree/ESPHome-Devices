#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace binary_power {

class BinaryLightWithPower : public light::LightOutput, public PollingComponent {
 public:
  void set_output(output::BinaryOutput *output) { output_ = output; }
  void set_light_wattage(float light_wattage) { this->light_wattage_ = light_wattage; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  void update() override;

 protected:
  output::BinaryOutput *output_;
  bool state_{false};
  optional<float> light_wattage_{};
  sensor::Sensor *power_sensor_;
};

}  // namespace binary_power
}  // namespace esphome
