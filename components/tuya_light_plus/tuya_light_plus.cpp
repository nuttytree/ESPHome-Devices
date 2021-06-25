#include "esphome/core/log.h"
#include "tuya_light_plus.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light_plus";

void TuyaLightPlus::setup()
{
  this->parent_->register_listener(*this->switch_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint_(datapoint); });
  this->parent_->register_listener(*this->dimmer_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint_(datapoint); });
  if (this->min_value_datapoint_id_.has_value()) {
    this->parent_->set_datapoint_value(*this->min_value_datapoint_id_, this->min_value_);
  }

  this->register_service(&TuyaLightPlus::set_default_brightness, "set_default_brightness", {"brightness"});
  this->register_service(&TuyaLightPlus::set_auto_off_time, "set_auto_off_time", {"auto_off_time"});

  if (this->day_night_sensor_.has_value())
  {
    this->subscribe_homeassistant_state(&TuyaLightPlus::on_day_night_changed_, *this->day_night_sensor_);
  }
}

void TuyaLightPlus::dump_config()
{
  ESP_LOGCONFIG(TAG, "Tuya Dimmer:");
  ESP_LOGCONFIG(TAG, "   Switch has datapoint ID %u", *this->switch_id_);
  ESP_LOGCONFIG(TAG, "   Dimmer has datapoint ID %u", *this->dimmer_id_);
  if (this->min_value_datapoint_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Min Value has datapoint ID %u", *this->min_value_datapoint_id_);
}

light::LightTraits TuyaLightPlus::get_traits()
{
  auto traits = light::LightTraits();
  traits.set_supports_brightness(true);
  return traits;
}

void TuyaLightPlus::setup_state(light::LightState *state) { state_ = state; }

void TuyaLightPlus::write_state(light::LightState *state)
{
  float brightness;
  state->current_values_as_brightness(&brightness);

  if (brightness == 0.0f) {
    this->parent_->set_datapoint_value(*this->switch_id_, false);
  }
  else
  {
    this->parent_->set_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level_(brightness));
    this->parent_->set_datapoint_value(*this->switch_id_, true);
  }
}

void TuyaLightPlus::loop()
{
  if (this->tuya_state_is_on_ && this->auto_off_time_.has_value() && this->last_state_change_ + *this->auto_off_time_ >= millis())
  {
    auto call = this->state_->turn_off();
    call.perform();
  }
}

void TuyaLightPlus::set_default_brightness(float brightness)
{
  ESP_LOGCONFIG(TAG, "Setting the default brightness to %f", brightness);
  this->default_brightness_ = brightness <= 0 ? optional<float>{} : std::min(brightness, 1.0f);

  // If the light is off update the brightness on the Tuya MCU and in the state and publish so regardless of how the light is turned on the brightness will be the default
  if (this->default_brightness_.has_value() && !this->tuya_state_is_on_)
  {
    this->parent_->set_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level_(*this->default_brightness_));
    this->state_->current_values.set_brightness(*this->default_brightness_);
    this->state_->remote_values = this->state_->current_values;
    this->state_->publish_state();
  }
}

void TuyaLightPlus::handle_tuya_datapoint_(tuya::TuyaDatapoint datapoint)
{
  ESP_LOGD(TAG, "Received Datapoint:");
  if (datapoint.id == *this->switch_id_)
  {
    ESP_LOGD(TAG, "  Type: Switch");
    ESP_LOGD(TAG, "  State: %s", ONOFF(datapoint.value_bool));

    this->tuya_state_is_on_ = datapoint.value_bool;

    this->state_->current_values.set_state(datapoint.value_bool);
    if (!datapoint.value_bool && this->default_brightness_.has_value())
    {
      this->parent_->set_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level_(*this->default_brightness_));
      this->state_->current_values.set_brightness(*this->default_brightness_);
    }
  }
  else if (datapoint.id == *this->dimmer_id_)
  {
    ESP_LOGD(TAG, "  Type: Brightness");
    ESP_LOGD(TAG, "  Value: %u", datapoint.value_uint);

    // Only react to dimmer level changes if the light is on
    if(this->state_->current_values.is_on())
    {
      this->state_->current_values.set_brightness(this->tuya_level_to_brightness_(datapoint.value_uint));
    }
  }

  this->last_state_change_ = millis();

  this->state_->remote_values = this->state_->current_values;
  this->state_->publish_state();
}

void TuyaLightPlus::on_day_night_changed_(std::string state)
{
  if (state == this->day_night_day_value_)
  {
    this->set_default_brightness(this->day_default_brightness_.value_or(0));
    this->set_auto_off_time(this->day_auto_off_time_.value_or(0));
  }
  else if (state == this->day_night_night_value_)
  {
    this->set_default_brightness(this->night_default_brightness_.value_or(0));
    this->set_auto_off_time(this->night_auto_off_time_.value_or(0));
  }
}

}  // namespace tuya
}  // namespace esphome
