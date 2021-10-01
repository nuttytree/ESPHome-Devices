#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "garage_fridge.h"

namespace esphome {
namespace garage_fridge {

static const char *const TAG = "garage_fridge";
static const float DISABLED_TEMP = -100.0f;

GarageFridge::GarageFridge() {
  this->fridge_pid_ = new pid::PIDClimate();
  this->fridge_pid_->set_component_source("garage.fridge");
  App.register_component(this->fridge_pid_);
  App.register_climate(this->fridge_pid_);

  this->freezer_pid_ = new pid::PIDClimate();
  this->freezer_pid_->set_component_source("garage.fridge");
  App.register_component(this->freezer_pid_);
  App.register_climate(this->freezer_pid_);

  this->fridge_heater_sensor_ = new sensor::Sensor();
  this->fridge_heater_sensor_->set_accuracy_decimals(0);
  this->fridge_heater_sensor_->set_device_class("heat");
  this->fridge_heater_sensor_->set_icon("mdi:radiator");
  this->fridge_heater_sensor_->set_state_class(sensor::StateClass::STATE_CLASS_MEASUREMENT);
  this->fridge_heater_sensor_->set_unit_of_measurement("%");
  App.register_sensor(this->fridge_heater_sensor_);

  this->fridge_output_ = new GarageFridgeHeatOutput();
  this->fridge_output_->set_component_source("garage.fridge");
  App.register_component(this->fridge_output_);
  this->fridge_output_->set_sensor(this->fridge_heater_sensor_);

  this->freezer_output_ = new GarageFridgeHeatOutput();
  this->freezer_output_->set_component_source("garage.fridge");
  App.register_component(this->freezer_output_);
  this->freezer_output_->set_sensor(this->fridge_heater_sensor_);
}

void GarageFridge::set_fridge_heat_output(output::FloatOutput *heat_output) {
  this->fridge_output_->set_physical_output(heat_output);
  this->freezer_output_->set_physical_output(heat_output);
}

void GarageFridge::set_fridge_sensor(sensor::Sensor *sensor) { 
  this->fridge_pid_->set_sensor(sensor);
  this->freezer_pid_->set_sensor(sensor);
}

void GarageFridge::setup() {
  this->freezer_sensor_->add_on_state_callback([this](float state) {
    if (state > this->freezer_max_temp_ && this->freezer_pid_->target_temperature == DISABLED_TEMP) {
      this->fridge_output_->set_inactive();
      this->freezer_output_->set_active();
      auto call = this->freezer_pid_->make_call();
      call.set_target_temperature(this->cool_trigger_temp_);
      call.perform();
    } else if (state <= this->freezer_max_temp_ && this->freezer_pid_->target_temperature != DISABLED_TEMP) {
      auto call = this->freezer_pid_->make_call();
      call.set_target_temperature(DISABLED_TEMP);
      call.perform();
      this->freezer_output_->set_inactive();
      this->fridge_output_->set_active();
    }
  });

  this->fridge_output_->set_active();
  auto call = this->fridge_pid_->make_call();
  call.set_target_temperature(this->fridge_min_temp_);
  call.perform();
}

}  // namespace garage_fridge
}  // namespace esphome
