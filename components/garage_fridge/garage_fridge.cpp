#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "garage_fridge.h"

namespace esphome {
namespace garage_fridge {

static const char *const TAG = "garage_fridge";

GarageFridge::GarageFridge() {
  this->fridge_pid_ = new pid::PIDClimate();
  this->fridge_pid_->set_component_source("pid.climate");
  App.register_component(this->fridge_pid_);
  App.register_climate(this->fridge_pid_);

  this->freezer_pid_ = new pid::PIDClimate();
  this->freezer_pid_->set_component_source("pid.climate");
  App.register_component(this->freezer_pid_);
  App.register_climate(this->freezer_pid_);
}

void GarageFridge::set_fridge_sensor(sensor::Sensor *sensor) { 
  this->fridge_pid_->set_sensor(sensor);
  this->freezer_pid_->set_sensor(sensor);
}

void GarageFridge::setup() {

}

void GarageFridge::loop() {
  
}

}  // namespace garage_fridge
}  // namespace esphome
