#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/api/custom_api_device.h"

namespace esphome {
namespace tuya {

enum DayNightSensorType {
  BINARY,
  TEXT,
};

class TuyaLightPlus : public Component, public light::LightOutput, public api::CustomAPIDevice {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_min_value_datapoint_id(uint8_t min_value_datapoint_id) { this->min_value_datapoint_id_ = min_value_datapoint_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { this->min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { this->max_value_ = max_value; }
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;
  void loop() override;

  void set_linked_lights(const std::string linked_lights) { this->linked_lights_ = linked_lights; }
  void set_day_night_sensor(const std::string day_night_sensor) { this->day_night_sensor_ = day_night_sensor; }
  void set_day_night_sensor_type(DayNightSensorType day_night_sensor_type) { this->day_night_sensor_type_ = day_night_sensor_type; }
  void set_day_night_day_value(const std::string day_night_day_value) { this->day_night_day_value_ = day_night_day_value; }
  void set_day_night_night_value(const std::string day_night_night_value) { this->day_night_night_value_ = day_night_night_value; }
  void set_day_default_brightness(float brightness) { this->day_default_brightness_ = brightness == 0 ? optional<float>{} : std::min(brightness, 1.0f); }
  void set_night_default_brightness(float brightness) { this->night_default_brightness_ = brightness == 0 ? optional<float>{} : std::min(brightness, 1.0f); }
  void set_day_auto_off_time(uint32_t auto_off_time) { this->day_auto_off_time_ = auto_off_time_ == 0 ? optional<uint32_t>{} : auto_off_time; }
  void set_night_auto_off_time(uint32_t auto_off_time) { this->night_auto_off_time_ = auto_off_time_ == 0 ? optional<uint32_t>{} : auto_off_time; }

  void set_default_brightness(float brightness);
  void set_auto_off_time(int auto_off_time) { this->auto_off_time_ = auto_off_time <= 0 ? optional<uint32_t>{} : auto_off_time; }

 protected:
  float tuya_level_to_brightness_(uint32_t level) { return static_cast<float>(level) / static_cast<float>(this->max_value_); }
  uint32_t brightness_to_tuya_level_(float brightness) { return static_cast<uint32_t>(brightness * this->max_value_); }
  void handle_tuya_datapoint_(TuyaDatapoint datapoint);
  void on_day_night_changed_(std::string state);

  Tuya *parent_;
  optional<uint8_t> switch_id_{};
  optional<uint8_t> dimmer_id_{};
  optional<uint8_t> min_value_datapoint_id_{};
  uint32_t min_value_ = 0;
  uint32_t max_value_ = 255;
  light::LightState *state_{nullptr};

  optional<float> default_brightness_{};
  optional<uint32_t> auto_off_time_{};
  optional<std::string> linked_lights_;
  optional<std::string> day_night_sensor_{};
  optional<DayNightSensorType> day_night_sensor_type_{};
  optional<std::string> day_night_day_value_{};
  optional<std::string> day_night_night_value_{};
  optional<float> day_default_brightness_{};
  optional<float> night_default_brightness_{};
  optional<uint32_t> day_auto_off_time_{};
  optional<uint32_t> night_auto_off_time_{};
  bool tuya_state_is_on_{false};
  long last_state_change_ = 0;
};

}  // namespace tuya
}  // namespace esphome
