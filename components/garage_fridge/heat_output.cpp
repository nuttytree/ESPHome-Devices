#include "heat_output.h"

namespace esphome {
namespace garage_fridge {

void GarageFridgeHeatOutput::write_state(float state) { 
  this->state_ = state;
  if (this->active_) {
    this->physical_output_->set_level(state);
    this->sensor_->publish_state(state * 100);
  }
}

void GarageFridgeHeatOutput::set_active() {
  this->active_ = true;
  this->write_state(this->state_);
}

}  // namespace garage_fridge
}  // namespace esphome
