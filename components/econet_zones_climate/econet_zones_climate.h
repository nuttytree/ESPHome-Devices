#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::econet_zones_climate {

struct FanModeEntry {
  float minimum_temperature_delta;
  const char *fan_mode;
};

class EcoNetZonesClimate : public climate::Climate, public Component {
 public:
  void add_zone(climate::Climate *zone) { zones_.push_back(zone); }
  void set_operating_mode_sensor(text_sensor::TextSensor *sensor) { operating_mode_sensor_ = sensor; }
  void set_supports_heat_mode(bool v) { supports_heat_mode_ = v; }
  void set_supports_cool_mode(bool v) { supports_cool_mode_ = v; }
  void set_supports_current_humidity(bool v) { supports_current_humidity_ = v; }
  void set_supports_target_humidity(bool v) { supports_target_humidity_ = v; }
  void set_automatic_fan_mode(const char *mode) { automatic_fan_mode_ = mode; }
  void add_fan_mode(float min_delta, const char *fan_mode) { fan_modes_.push_back({min_delta, fan_mode}); }

  void setup() override;
  void dump_config() override;

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void update_current_temperature_();
  void update_current_humidity_();
  void update_current_action_();
  void update_zone_fan_mode_();
  void sync_zones_from_this_();
  void push_state_to_zones_();
  void sync_this_from_zone_(climate::Climate *zone);

  enum class UpdateSource { NONE, ZONE, CONTROL };
  uint32_t lockout_until_{0};
  UpdateSource lockout_source_{UpdateSource::NONE};
  climate::Climate *lockout_zone_{nullptr};  // set when lockout_source_ == ZONE
  const char *current_fan_mode_{nullptr};

  std::vector<climate::Climate *> zones_;
  text_sensor::TextSensor *operating_mode_sensor_{nullptr};
  bool supports_heat_mode_{true};
  bool supports_cool_mode_{true};
  bool supports_current_humidity_{true};
  bool supports_target_humidity_{true};
  const char *automatic_fan_mode_{nullptr};
  std::vector<FanModeEntry> fan_modes_;
};

}  // namespace esphome::econet_zones_climate
