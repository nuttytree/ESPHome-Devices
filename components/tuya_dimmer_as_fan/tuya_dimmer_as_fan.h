#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace tuya {

class TuyaDimmerAsFan : public Component {
 public:
  TuyaDimmerAsFan(Tuya *parent, fan::FanState *fan) : parent_(parent), fan_(fan) {}
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_dimmer_max_value(uint32_t max_value) { this->dimmer_max_value_ = max_value; }

 protected:
  Tuya *parent_;
  optional<uint8_t> switch_id_{};
  optional<uint8_t> dimmer_id_{};
  uint32_t dimmer_max_value_ = 255;
  fan::FanState *fan_;
};

}  // namespace tuya
}  // namespace esphome
