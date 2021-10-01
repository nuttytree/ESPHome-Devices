#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/pid/pid_climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "heat_output.h"

namespace esphome {
namespace garage_fridge {

class GarageFridge : public Component {
 public:
  GarageFridge();
  void set_fridge_heat_output(output::FloatOutput *heat_output);
  void set_fridge_heat_sensor_name(const std::string &name) { this->fridge_heater_sensor_->set_name(name); }
  void set_fridge_sensor(sensor::Sensor *sensor);
  void set_fridge_min_temp(float temp) { this->fridge_min_temp_ = temp; }
  void set_freezer_max_temp(float temp) { this->freezer_max_temp_ = temp; }
  void set_cool_trigger_temp(float temp) { this->cool_trigger_temp_ = temp; }
  void set_fridge_kp(float kp) { this->fridge_pid_->set_kp(kp); }
  void set_fridge_ki(float ki) { this->fridge_pid_->set_ki(ki); }
  void set_fridge_kd(float kd) { this->fridge_pid_->set_kd(kd); }
  void set_fridge_min_integral(float min_integral) { this->fridge_pid_->set_min_integral(min_integral); }
  void set_fridge_max_integral(float max_integral) { this->fridge_pid_->set_max_integral(max_integral); }
  void set_freezer_sensor(sensor::Sensor *sensor) { this->freezer_sensor_ = sensor; }
  void set_freezer_kp(float kp) { this->freezer_pid_->set_kp(kp); }
  void set_freezer_ki(float ki) { this->freezer_pid_->set_ki(ki); }
  void set_freezer_kd(float kd) { this->freezer_pid_->set_kd(kd); }
  void set_freezer_min_integral(float min_integral) { this->freezer_pid_->set_min_integral(min_integral); }
  void set_freezer_max_integral(float max_integral) { this->freezer_pid_->set_max_integral(max_integral); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;

 protected:
  pid::PIDClimate *fridge_pid_;
  pid::PIDClimate *freezer_pid_;
  sensor::Sensor *freezer_sensor_;
  sensor::Sensor *fridge_heater_sensor_;
  GarageFridgeHeatOutput *fridge_output_;
  GarageFridgeHeatOutput *freezer_output_;
  float fridge_min_temp_{};
  float freezer_max_temp_{};
  float cool_trigger_temp_{};
};

}  // namespace garage_fridge
}  // namespace esphome
