#include "binary_light_with_power.h"

namespace esphome {
namespace binary_power {

light::LightTraits BinaryLightWithPower::get_traits() {
  auto traits = light::LightTraits();
  // TODO: Enable this with the 1.21.x version of ESPHome
  // traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void BinaryLightWithPower::write_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);
  this->state_ = binary;
  if (binary)
    this->output_->turn_on();
  else
    this->output_->turn_off();

  if (this->light_wattage_.has_value() && this->power_sensor_ != nullptr)
  {
    float power = this->state_ ? this->light_wattage_.value() : 0.0f;
    this->power_sensor_->publish_state(power);
  }
}

void BinaryLightWithPower::update() {
  if (this->light_wattage_.has_value() && this->power_sensor_ != nullptr && this->state_)
  {
    this->power_sensor_->publish_state(this->light_wattage_.value());
  }
}

}  // namespace binary_power
}  // namespace esphome
