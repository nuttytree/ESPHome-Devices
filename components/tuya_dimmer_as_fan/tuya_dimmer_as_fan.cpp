#include "tuya_dimmer_as_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.fan";

void TuyaDimmerAsFan::setup() {
  this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
    this->state = datapoint.value_bool;
    this->publish_state();
    this->update();
  });

  this->parent_->register_listener(*this->dimmer_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.value_int != this->dimmer_max_value_)
    {
      this->parent_->set_integer_datapoint_value(*this->dimmer_id_, this->dimmer_max_value_);
    }
  });

  // Make sure we start at the max value
  this->parent_->set_integer_datapoint_value(*this->dimmer_id_, this->dimmer_max_value_);
}

void TuyaDimmerAsFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer as Fan:");
  ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  ESP_LOGCONFIG(TAG, "  Dimmer has datapoint ID %u", *this->dimmer_id_);
}

void TuyaDimmerAsFan::control(const fan::FanCall &call) {
  this->parent_->set_boolean_datapoint_value(*this->switch_id_, *call.get_state());
}

void TuyaDimmerAsFan::update()
{
  if (this->fan_wattage_.has_value() && this->power_sensor_ != nullptr)
  {
    float power = this->state ? this->fan_wattage_.value() : 0.0f;
    this->power_sensor_->publish_state(power);
  }
}

}  // namespace tuya
}  // namespace esphome
