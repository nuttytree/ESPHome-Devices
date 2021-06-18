#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/api/custom_api_device.h"

namespace esphome {
namespace tuya {

class TuyaLightPlus : public Component, public light::LightOutput, public api::CustomAPIDevice {
 public:
  void setup() override;
  void dump_config() override;
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_min_value_datapoint_id(uint8_t min_value_datapoint_id) { this->min_value_datapoint_id_ = min_value_datapoint_id; }
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { max_value_ = max_value; }
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

  void set_day_default_brightness(float brightness) { this->day_default_brightness_ = brightness; }
  void set_night_default_brightness(float brightness) { this->night_default_brightness_ = brightness; }

  void set_default_brightness(float brightness);

 protected:
  float tuya_level_to_brightness(uint32_t level) { return static_cast<float>(level) / static_cast<float>(this->max_value_); }
  uint32_t brightness_to_tuya_level(float brightness) { return static_cast<uint32_t>(brightness * this->max_value_); }
  float brightness_pct() { return static_cast<uint32_t>(this->state_->current_values.get_brightness() * 100); }
  void handle_tuya_datapoint(TuyaDatapoint datapoint);

  Tuya *parent_;
  optional<uint8_t> dimmer_id_{};
  optional<uint8_t> min_value_datapoint_id_{};
  optional<uint8_t> switch_id_{};
  uint32_t min_value_ = 0;
  uint32_t max_value_ = 255;
  light::LightState *state_{nullptr};

  float day_default_brightness_ = 1;
  float night_default_brightness_ = .03;
  float default_brightness_ = 1;
};

}  // namespace tuya
}  // namespace esphome
