#include "esphome.h"

using namespace esphome;

class TuyaDimmerBinaryFanOutput : public Component, public output::BinaryOutput
{
  public:
    void setup() override;

    void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
    void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
    void set_tuya_parent(tuya::Tuya *parent) { this->parent_ = parent; }
    void set_max_value(uint32_t max_value) { max_value_ = max_value; }
    void set_fan(fan::FanState *fan) { fan_ = fan; }

  protected:
    void write_state(bool state) override;
    void handle_tuya_datapoint(tuya::TuyaDatapoint datapoint);

    tuya::Tuya *parent_;
    optional<uint8_t> dimmer_id_{};
    optional<uint8_t> switch_id_{};
    uint32_t max_value_ = 1000;
    fan::FanState *fan_;
};

void TuyaDimmerBinaryFanOutput::setup()
{
  parent_->register_listener(*switch_id_, [this](tuya::TuyaDatapoint datapoint) { handle_tuya_datapoint(datapoint); });

  parent_->register_listener(*dimmer_id_, [this](tuya::TuyaDatapoint datapoint) { handle_tuya_datapoint(datapoint); });
}

void TuyaDimmerBinaryFanOutput::write_state(bool state)
{
  tuya::TuyaDatapoint datapoint{};
  datapoint.id = *switch_id_;
  datapoint.type = tuya::TuyaDatapointType::BOOLEAN;
  datapoint.value_bool = state;
  parent_->set_datapoint_value(datapoint);
}

void TuyaDimmerBinaryFanOutput::handle_tuya_datapoint(tuya::TuyaDatapoint datapoint)
{
  if (datapoint.id == *switch_id_)
  {
    auto call = fan_->make_call();
    call.set_state(datapoint.value_bool);
    call.perform();
  }
  else if (datapoint.id == *dimmer_id_)
  {
    if (datapoint.value_uint < max_value_)
    {
      tuya::TuyaDatapoint datapoint{};
      datapoint.id = *dimmer_id_;
      datapoint.type = tuya::TuyaDatapointType::INTEGER;
      datapoint.value_uint = max_value_;
      parent_->set_datapoint_value(datapoint);
    }
  }
}

TuyaDimmerBinaryFanOutput *TuyaFanOutput;