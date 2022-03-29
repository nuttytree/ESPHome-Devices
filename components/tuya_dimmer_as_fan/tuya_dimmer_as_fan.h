#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan_state.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tuya/tuya.h"

namespace esphome {
namespace tuya {

class TuyaDimmerAsFan : public PollingComponent, public fan::Fan {
 public:
  TuyaDimmerAsFan(Tuya *parent) : parent_(parent) {}
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_dimmer_max_value(uint32_t max_value) { this->dimmer_max_value_ = max_value; }
  void set_fan_wattage(float fan_wattage) { this->fan_wattage_ = fan_wattage; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  fan::FanTraits get_traits() override { return fan::FanTraits(false, false, false, 1); }
  void update() override;

 protected:
  void control(const fan::FanCall &call) override;

  Tuya *parent_;
  optional<uint8_t> switch_id_{};
  optional<uint8_t> dimmer_id_{};
  uint32_t dimmer_max_value_ = 255;
  optional<float> fan_wattage_{};
  sensor::Sensor *power_sensor_;
};

}  // namespace tuya
}  // namespace esphome
