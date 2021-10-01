#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace garage_fridge {

class GarageFridgeHeatOutput : public output::FloatOutput, public Component {
 public:
  void set_physical_output(output::FloatOutput *output) { this->physical_output_ = output; }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void write_state(float state) override;
  void set_inactive() { this->active_ = false; }
  void set_active();

 protected:
  output::FloatOutput *physical_output_;
  sensor::Sensor *sensor_;
  bool active_{false};
  float state_{0};
};

}  // namespace garage_fridge
}  // namespace esphome
