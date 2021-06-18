#include "esphome/core/log.h"
#include "tuya_light_plus.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light_plus";

void TuyaLightPlus::setup() {
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint(datapoint); });
  }

  if (this->dimmer_id_.has_value()) {
    this->parent_->register_listener(*this->dimmer_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint(datapoint); });
  }

  this->register_service(&TuyaLightPlus::set_default_brightness, "set_default_brightness", {"brightness"});

  if (this->min_value_datapoint_id_.has_value()) {
    this->parent_->set_datapoint_value(*this->min_value_datapoint_id_, this->min_value_);
  }
}

void TuyaLightPlus::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer:");
  if (this->dimmer_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Dimmer has datapoint ID %u", *this->dimmer_id_);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Switch has datapoint ID %u", *this->switch_id_);
  if (this->min_value_datapoint_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Min Value has datapoint ID %u", *this->min_value_datapoint_id_);
}

light::LightTraits TuyaLightPlus::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supports_brightness(this->dimmer_id_.has_value());
  return traits;
}

void TuyaLightPlus::setup_state(light::LightState *state) { state_ = state; }

void TuyaLightPlus::write_state(light::LightState *state) {
  float brightness;
  state->current_values_as_brightness(&brightness);

  if (brightness == 0.0f) {
    this->parent_->set_datapoint_value(*this->switch_id_, false);
  }
  else
  {
    this->parent_->set_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level(brightness));
    this->parent_->set_datapoint_value(*this->switch_id_, true);
  }
}

void TuyaLightPlus::set_default_brightness(float brightness)
{
  this->default_brightness_ = brightness > 1 ? 1 : brightness;

  // If the light is off update the brightness in the state and publish so regardless of how the light is turned on the brightness will be the default
  if (!this->state_->current_values.is_on())
  {
    this->state_->current_values.set_brightness(this->default_brightness_);
    this->state_->remote_values = this->state_->current_values;
    this->state_->publish_state();
  }
}

void TuyaLightPlus::handle_tuya_datapoint(tuya::TuyaDatapoint datapoint)
{
  ESP_LOGD(TAG, "Received Datapoint:");
  if (datapoint.id == *this->switch_id_)
  {
    ESP_LOGD(TAG, "  Type: Switch");
    ESP_LOGD(TAG, "  State: %s", ONOFF(datapoint.value_bool));
  
    this->state_->current_values.set_state(datapoint.value_bool);
    if (!datapoint.value_bool)
    {
      this->parent_->set_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level(this->default_brightness_));
      this->state_->current_values.set_brightness(this->default_brightness_);
    }
  }
  else if (datapoint.id == *this->dimmer_id_)
  {
    ESP_LOGD(TAG, "  Type: Brightness");
    ESP_LOGD(TAG, "  Value: %u", datapoint.value_uint);

    // Only react to dimmer level changes if the light is on
    if(this->state_->current_values.is_on())
    {
      this->state_->current_values.set_brightness(tuya_level_to_brightness(datapoint.value_uint));
    }
  }

  this->state_->remote_values = this->state_->current_values;
  this->state_->publish_state();
}

}  // namespace tuya
}  // namespace esphome
