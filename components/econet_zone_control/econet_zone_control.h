#pragma once

#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/econet/econet.h"

namespace esphome::econet_zone_control {

struct ModeEntry {
  uint8_t id;
  climate::ClimateMode mode;
};

struct FanModeEntry {
  float minimum_temperature_delta;
  uint8_t id;
};

struct EconetZone {
  int8_t request_mod;
  uint32_t src_adr;
  bool is_primary;
  float cached_temperature{NAN};
  float cached_humidity{NAN};
  float cached_target_low_f{NAN};
  float cached_target_high_f{NAN};
  climate::ClimateMode cached_mode{climate::CLIMATE_MODE_OFF};
  int16_t cached_fan_mode{-1};           // -1 = not yet received
  int16_t cached_fan_mode_no_schedule{-1};
};

class EcoNetZoneControl : public climate::Climate, public Component, public econet::EconetClient {
 public:
  void add_zone(int8_t request_mod, uint32_t src_adr, bool is_primary) {
    zones_.push_back({request_mod, src_adr, is_primary});
  }
  void set_operating_mode_id(const char *id) { operating_mode_id_ = id; }
  void set_mode_id(const char *id) { mode_id_ = id; }
  void init_modes(size_t size) { modes_.reserve(size); }
  void add_mode(uint8_t id, climate::ClimateMode mode) { modes_.push_back({id, mode}); }
  void set_automatic_fan_mode(uint8_t mode) { automatic_fan_mode_ = mode; }
  void add_fan_mode(float min_delta, uint8_t id) { fan_modes_.push_back({min_delta, id}); }
  void set_current_temperature_id(const char *id) { current_temperature_id_ = id; }
  void set_target_temperature_low_id(const char *id) { target_temperature_low_id_ = id; }
  void set_target_temperature_high_id(const char *id) { target_temperature_high_id_ = id; }
  void set_fan_mode_id(const char *id) { fan_mode_id_ = id; }
  void set_fan_mode_no_schedule_id(const char *id) { fan_mode_no_schedule_id_ = id; }
  void set_current_humidity_id(const char *id) { current_humidity_id_ = id; }

  void setup() override;
  void dump_config() override;

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  // Called by listeners to re-evaluate all sync and fan logic
  void update_zones_();
  // Called by the operating mode listener
  void update_current_action_();
  // Called by update_zones_
  void update_zone_fan_mode_();

  // Pointer to primary zone — set in setup() after zones_ vector is finalized
  EconetZone *primary_zone_{nullptr};
  std::vector<EconetZone> zones_;
  std::string operating_mode_state_;
  uint8_t automatic_fan_mode_{0};
  std::vector<FanModeEntry> fan_modes_;
  std::vector<ModeEntry> modes_;
  const char *current_temperature_id_{nullptr};
  const char *target_temperature_low_id_{nullptr};
  const char *target_temperature_high_id_{nullptr};
  const char *fan_mode_id_{nullptr};
  const char *fan_mode_no_schedule_id_{nullptr};
  const char *current_humidity_id_{nullptr};
  const char *operating_mode_id_{nullptr};
  const char *mode_id_{nullptr};
  const EconetZone *locked_min_zone_{nullptr};
  const EconetZone *locked_max_zone_{nullptr};
  uint64_t zone_lock_until_{0};
};

}  // namespace esphome::econet_zone_control
