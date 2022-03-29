#include "tuya_light_plus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light_plus";
static const uint16_t DOUBLE_CLICK_TIMEOUT = 350;

void TuyaLightPlus::setup()
{
  this->parent_->register_listener(*this->switch_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint_(datapoint); });
  this->parent_->register_listener(*this->dimmer_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint_(datapoint); });
  if (this->min_value_datapoint_id_.has_value()) {
    this->parent_->set_integer_datapoint_value(*this->min_value_datapoint_id_, this->min_value_);
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
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void TuyaLightPlus::setup_state(light::LightState *state) { state_ = state; }

void TuyaLightPlus::write_state(light::LightState *state)
{
  ESP_LOGD(TAG, "Writing state:");
  float brightness;
  state->current_values_as_brightness(&brightness);
  this->last_state_change_ = millis();
  ESP_LOGD(TAG, "  Brightness: %.0f%%", brightness * 100.0f);

  if (brightness == 0.0f) {
    this->tuya_state_is_on_ = false;
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, false);
  }
  else
  {
    this->tuya_state_is_on_ = true;
    this->parent_->set_integer_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level_(brightness));
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, true);
  }
}

void TuyaLightPlus::loop()
{
  // Double click while off timed out, turn the light on and reset the timeout
  if (this->double_click_while_off_timeout_ != 0 && millis() > this->double_click_while_off_timeout_)
  {
    ESP_LOGD(TAG, "Double click while off timedout, turning on the light");
    this->double_click_while_off_timeout_ = 0;
    auto on_call = this->state_->turn_on();
    on_call.perform();
    return;
  }

  // Double click while on timed out, reset the timeout
  if (this->double_click_while_on_timeout_ != 0 && millis() > this->double_click_while_on_timeout_)
  {
    this->double_click_while_on_timeout_ = 0;
    return;
  }

  // Auto off
  if (this->state_->current_values.is_on() && this->auto_off_time_.has_value() && millis() > (this->last_state_change_ + *this->auto_off_time_))
  {
    auto call = this->state_->turn_off();
    call.perform();
  }
}

void TuyaLightPlus::update()
{
  if (this->light_wattage_.has_value() && this->power_sensor_ != nullptr && this->state_->current_values.is_on())
  {
    this->power_sensor_->publish_state(this->light_wattage_.value());
  }
}

void TuyaLightPlus::add_new_double_click_while_off_callback(std::function<void()> &&callback)
{
  this->has_double_click_while_off_ = true;
  this->double_click_while_off_callback_.add(std::move(callback));
}

void TuyaLightPlus::add_new_double_click_while_on_callback(std::function<void()> &&callback)
{
  this->has_double_click_while_on_ = true;
  this->double_click_while_on_callback_.add(std::move(callback));
}

void TuyaLightPlus::set_default_brightness_(float brightness)
{
  ESP_LOGCONFIG(TAG, "Setting the default brightness to %f", brightness);
  this->default_brightness_ = brightness <= 0 ? optional<float>{} : std::min(brightness, 1.0f);

  // If the light is off update the brightness state so regardless of how the light is turned on the brightness will be the default
  if (this->default_brightness_.has_value() && this->state_ != nullptr && !this->tuya_state_is_on_)
  {
    this->state_->current_values.set_brightness(*this->default_brightness_);
    this->state_->remote_values = this->state_->current_values;
  }
}

void TuyaLightPlus::handle_tuya_datapoint_(tuya::TuyaDatapoint datapoint)
{
  ESP_LOGD(TAG, "Received Datapoint:");
  if (datapoint.id == *this->switch_id_)
  {
    ESP_LOGD(TAG, "  Type: Switch");
    ESP_LOGD(TAG, "  State: %s", ONOFF(datapoint.value_bool));

    // Turned on with the physical button
    if (datapoint.value_bool && !this->tuya_state_is_on_)
    {
      ESP_LOGD(TAG, "Turned on at switch");

      if (this->has_double_click_while_on_)
      {
        // We are in a double click while on timeout period so this is a double click
        if (this->double_click_while_on_timeout_ != 0)
        {
          ESP_LOGD(TAG, "Switch was double clicked while on");

          this->parent_->set_boolean_datapoint_value(*this->switch_id_, false);

          this->double_click_while_on_timeout_ = 0;
          this->double_click_while_on_callback_.call();

          return;
        }
      }

      if (this->has_double_click_while_off_)
      {
        // We are not in a double click while off timeout period
        if (this->double_click_while_off_timeout_ == 0)
        {
          // Turn the light back off and wait to see if we get a double click
          this->parent_->set_boolean_datapoint_value(*this->switch_id_, false);
          this->double_click_while_off_timeout_ = millis() + DOUBLE_CLICK_TIMEOUT;
          return;
        }
        // We are in a double click while off timeout period so this is a double click
        else
        {
          ESP_LOGD(TAG, "Switch was double clicked while off");

          this->double_click_while_off_timeout_ = 0;
          this->double_click_while_off_callback_.call();

          // Double click while off can be configured to result in the light being off or on
          if (this->double_click_while_off_stays_off_)
          {
            this->parent_->set_boolean_datapoint_value(*this->switch_id_, false);
            return;
          }
        }
      }
    
      // When the light is turned on at the switch the level of the Tuya device will stll be 0 so we set it to the default brightness or 1.0 if no default
      this->parent_->set_integer_datapoint_value(*this->dimmer_id_, this->brightness_to_tuya_level_(this->default_brightness_.value_or(1.0)));
    }

    // Turned off with the physical button
    if(!datapoint.value_bool && this->tuya_state_is_on_)
    {
      ESP_LOGD(TAG, "Turned off at switch");

      if (this->has_double_click_while_on_)
      {
        // Start the double click while on timeout
        this->double_click_while_on_timeout_ = millis() + DOUBLE_CLICK_TIMEOUT;
      }
    }

    this->tuya_state_is_on_ = datapoint.value_bool;
    this->state_->current_values.set_state(datapoint.value_bool);

    // If the light was turned off we set the brightness of the Tuya device to 0 to prevent flashes during double clicks and set the current
    // brightness value to the default value or 1.0 if no default so that if it is turned on remotely it will be at the default value
    if (!datapoint.value_bool)
    {
      this->parent_->set_integer_datapoint_value(*this->dimmer_id_, 0);
      this->state_->current_values.set_brightness(this->default_brightness_.value_or(1.0));
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

  if (this->linked_lights_.has_value())
  {
    if (this->state_->current_values.is_on())
    {
      this->call_homeassistant_service(
        "light.turn_on",
        {
          { "entity_id", *this->linked_lights_ },
          { "brightness", to_string(static_cast<uint32_t>(this->state_->current_values.get_brightness() * 255)) },
        });
    }
    else
    {
      this->call_homeassistant_service(
        "light.turn_off",
        {
          { "entity_id", *this->linked_lights_ },
        });
    }
  }

  if (this->light_wattage_.has_value() && this->power_sensor_ != nullptr)
  {
    float power = this->state_->current_values.is_on() ? this->light_wattage_.value() : 0.0f;
    this->power_sensor_->publish_state(power);
  }
}

void TuyaLightPlus::on_day_night_changed_(std::string state)
{
  if (state == this->day_night_day_value_)
  {
    if (this->day_default_brightness_.has_value())
    {
      this->set_default_brightness_(*this->day_default_brightness_);
    }
    if (this->day_auto_off_time_.has_value())
    {
      this->set_auto_off_time(*this->day_auto_off_time_);
    }
  }
  else if (state == this->day_night_night_value_)
  {
    if (this->night_default_brightness_.has_value())
    {
      this->set_default_brightness_(*this->night_default_brightness_);
    }
    if (this->night_auto_off_time_.has_value())
    {
      this->set_auto_off_time(*this->night_auto_off_time_);
    }
  }
}

}  // namespace tuya
}  // namespace esphome
