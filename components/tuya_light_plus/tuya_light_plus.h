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

class TuyaLightPlus : public PollingComponent, public light::LightOutput, public api::CustomAPIDevice {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_min_value_datapoint_id(uint8_t min_value_datapoint_id) { this->min_value_datapoint_id_ = min_value_datapoint_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { this->min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { this->max_value_ = max_value; }
  void set_light_wattage(float light_wattage) { this->light_wattage_ = light_wattage; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;
  void loop() override;
  void update() override;

  void set_linked_lights(const std::string linked_lights) { this->linked_lights_ = linked_lights; }
  void set_day_night_sensor(const std::string day_night_sensor) { this->day_night_sensor_ = day_night_sensor; }
  void set_day_night_sensor_type(DayNightSensorType day_night_sensor_type) { this->day_night_sensor_type_ = day_night_sensor_type; }
  void set_day_night_day_value(const std::string day_night_day_value) { this->day_night_day_value_ = day_night_day_value; }
  void set_day_night_night_value(const std::string day_night_night_value) { this->day_night_night_value_ = day_night_night_value; }
  void set_day_default_brightness(uint8_t brightness) { this->day_default_brightness_ = brightness == 0 ? optional<float>{} : this->ha_brightness_to_brightness_(brightness); }
  void set_night_default_brightness(uint8_t brightness) { this->night_default_brightness_ = brightness == 0 ? optional<float>{} : this->ha_brightness_to_brightness_(brightness); }
  void set_day_auto_off_time(uint32_t auto_off_time) { this->day_auto_off_time_ = auto_off_time_ == 0 ? optional<uint32_t>{} : auto_off_time; }
  void set_night_auto_off_time(uint32_t auto_off_time) { this->night_auto_off_time_ = auto_off_time_ == 0 ? optional<uint32_t>{} : auto_off_time; }
  void add_new_double_click_while_off_callback(std::function<void()> &&callback);
  void set_double_click_while_off_stays_off(bool stays_off) { this->double_click_while_off_stays_off_ = stays_off; }
  void add_new_double_click_while_on_callback(std::function<void()> &&callback);

  void set_default_brightness(int brightness) { this->set_default_brightness_(this->ha_brightness_to_brightness_(brightness)); }
  void set_auto_off_time(int auto_off_time) { this->auto_off_time_ = auto_off_time == 0 ? optional<uint32_t>{} : auto_off_time; }

 protected:
  float ha_brightness_to_brightness_(int brightness) { return clamp(static_cast<float>(brightness) / static_cast<float>(255), 0.01f, 1.0f); }
  float tuya_level_to_brightness_(uint32_t level) { return static_cast<float>(level) / static_cast<float>(this->max_value_); }
  uint32_t brightness_to_tuya_level_(float brightness) { return static_cast<uint32_t>(brightness * this->max_value_); }
  void set_default_brightness_(float brightness);
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
  optional<std::string> linked_lights_{};
  optional<std::string> day_night_sensor_{};
  optional<DayNightSensorType> day_night_sensor_type_{};
  optional<std::string> day_night_day_value_{};
  optional<std::string> day_night_night_value_{};
  optional<float> day_default_brightness_{};
  optional<float> night_default_brightness_{};
  optional<uint32_t> day_auto_off_time_{};
  optional<uint32_t> night_auto_off_time_{};
  optional<float> light_wattage_{};
  sensor::Sensor *power_sensor_;
  CallbackManager<void()> double_click_while_off_callback_{};
  CallbackManager<void()> double_click_while_on_callback_{};
  bool has_double_click_while_off_{false};
  bool double_click_while_off_stays_off_{true};
  bool has_double_click_while_on_{false};
  long double_click_while_off_timeout_ = 0;
  long double_click_while_on_timeout_ = 0;
  bool tuya_state_is_on_{false};
  long last_state_change_ = 0;
};

class DoubleClickWhileOffTrigger : public Trigger<> {
 public:
  DoubleClickWhileOffTrigger(TuyaLightPlus *light) {
    light->add_new_double_click_while_off_callback([this, light]() {
      this->trigger();
    });
  }
};

class DoubleClickWhileOnTrigger : public Trigger<> {
 public:
  DoubleClickWhileOnTrigger(TuyaLightPlus *light) {
    light->add_new_double_click_while_on_callback([this, light]() {
      this->trigger();
    });
  }
};

}  // namespace tuya
}  // namespace esphome
