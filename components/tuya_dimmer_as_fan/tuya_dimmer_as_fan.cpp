#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"
#include "tuya_dimmer_as_fan.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.fan";

void TuyaDimmerAsFan::setup() {
  auto traits = fan::FanTraits(false, false, false, 1);
  this->fan_->set_traits(traits);

  this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
    auto call = this->fan_->make_call();
    call.set_state(datapoint.value_bool);
    call.perform();
  });

  this->parent_->register_listener(*this->dimmer_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.value_int != this->dimmer_max_value_)
    {
      this->parent_->set_integer_datapoint_value(*this->dimmer_id_, this->dimmer_max_value_);
    }
  });

  this->fan_->add_on_state_callback([this]() { this->parent_->set_boolean_datapoint_value(*this->switch_id_, this->fan_->state); });

  // Make sure we start at the max value
  this->parent_->set_integer_datapoint_value(*this->dimmer_id_, this->dimmer_max_value_);
}

void TuyaDimmerAsFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer as Fan:");
  ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  ESP_LOGCONFIG(TAG, "  Dimmer has datapoint ID %u", *this->dimmer_id_);
}

}  // namespace tuya
}  // namespace esphome
