#include "econet_zone_control.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::econet_zone_control {

static const char *const TAG = "econet_zone_control";

namespace {
inline float fahrenheit_to_celsius(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
inline float celsius_to_fahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }
}  // namespace

void EcoNetZoneControl::setup() {
  // Locate primary zone pointer now that zones_ vector is fully populated
  for (auto &zone : this->zones_) {
    if (zone.is_primary) {
      this->primary_zone_ = &zone;
      break;
    }
  }
  if (this->primary_zone_ == nullptr) {
    ESP_LOGE(TAG, "No primary zone configured — component will not function");
    return;
  }

  // Register per-zone listeners
  for (auto &zone_cfg : this->zones_) {
    EconetZone *zp = &zone_cfg;

    if (this->mode_id_ && *this->mode_id_) {
      this->parent_->register_listener(
          this->mode_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                                   [&](const ModeEntry &m) { return m.id == dp.value_enum; });
            if (it == this->modes_.end()) {
              ESP_LOGW(TAG, "Zone src_adr=0x%08X unknown mode enum %u", zp->src_adr, dp.value_enum);
              return;
            }
            zp->cached_mode = it->mode;
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }

    if (this->current_temperature_id_ && *this->current_temperature_id_) {
      this->parent_->register_listener(
          this->current_temperature_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            zp->cached_temperature = fahrenheit_to_celsius(dp.value_float);
            ESP_LOGD(TAG, "Zone src_adr=0x%08X current_temperature=%.1f°C (%.1f°F)",
                     zp->src_adr, zp->cached_temperature, dp.value_float);
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }

    if (this->target_temperature_low_id_ && *this->target_temperature_low_id_) {
      this->parent_->register_listener(
          this->target_temperature_low_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            zp->cached_target_low_f = dp.value_float;
            ESP_LOGD(TAG, "Zone src_adr=0x%08X target_temperature_low=%.1f°F",
                     zp->src_adr, dp.value_float);
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }

    if (this->target_temperature_high_id_ && *this->target_temperature_high_id_) {
      this->parent_->register_listener(
          this->target_temperature_high_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            zp->cached_target_high_f = dp.value_float;
            ESP_LOGD(TAG, "Zone src_adr=0x%08X target_temperature_high=%.1f°F",
                     zp->src_adr, dp.value_float);
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }

    if (this->current_humidity_id_ && *this->current_humidity_id_) {
      this->parent_->register_listener(
          this->current_humidity_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            zp->cached_humidity = dp.value_float;
            ESP_LOGD(TAG, "Zone src_adr=0x%08X current_humidity=%.1f%%",
                     zp->src_adr, zp->cached_humidity);
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }

    if (this->fan_mode_id_ && *this->fan_mode_id_) {
      this->parent_->register_listener(
          this->fan_mode_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            ESP_LOGD(TAG, "Zone src_adr=0x%08X fan_mode reported as enum %u",
                     zp->src_adr, dp.value_enum);
            zp->cached_fan_mode = dp.value_enum;
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }

    if (this->fan_mode_no_schedule_id_ && *this->fan_mode_no_schedule_id_) {
      this->parent_->register_listener(
          this->fan_mode_no_schedule_id_, zone_cfg.request_mod, false,
          [this, zp](const econet::EconetDatapoint &dp) {
            ESP_LOGD(TAG, "Zone src_adr=0x%08X fan_mode_no_schedule reported as enum %u",
                     zp->src_adr, dp.value_enum);
            zp->cached_fan_mode_no_schedule = dp.value_enum;
            this->update_zones_();
          },
          false, zone_cfg.src_adr);
    }
  }

  // Top-level operating mode listener (not per-zone)
  if (this->operating_mode_id_ && *this->operating_mode_id_) {
    this->parent_->register_listener(
        this->operating_mode_id_, this->request_mod_, this->request_once_,
        [this](const econet::EconetDatapoint &dp) {
          ESP_LOGD(TAG, "Operating mode datapoint: %s", dp.value_string.c_str());
          this->operating_mode_state_ = dp.value_string;
          this->update_current_action_();
        },
        false, this->src_adr_);
  }

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->update_current_action_();
}

void EcoNetZoneControl::dump_config() {
  auto dp = [](const char *id) -> const char * { return (id != nullptr && *id) ? id : "(none)"; };
  LOG_CLIMATE("", "EcoNet Zone Control", this);
  ESP_LOGCONFIG(TAG, "  Zones: %zu", this->zones_.size());
  for (const auto &zone : this->zones_)
    ESP_LOGCONFIG(TAG, "    src_adr=0x%08X request_mod=%d %s",
                  zone.src_adr, zone.request_mod, zone.is_primary ? "[PRIMARY]" : "");
  ESP_LOGCONFIG(TAG, "  Mode Datapoint: %s", dp(this->mode_id_));
  ESP_LOGCONFIG(TAG, "  Operating Mode Datapoint: %s", dp(this->operating_mode_id_));
  ESP_LOGCONFIG(TAG, "  Automatic Fan Mode: %u", this->automatic_fan_mode_);
  ESP_LOGCONFIG(TAG, "  Current Temp Datapoint: %s", dp(this->current_temperature_id_));
  ESP_LOGCONFIG(TAG, "  Target Temp Low Datapoint: %s", dp(this->target_temperature_low_id_));
  ESP_LOGCONFIG(TAG, "  Target Temp High Datapoint: %s", dp(this->target_temperature_high_id_));
  ESP_LOGCONFIG(TAG, "  Fan Mode Datapoint: %s", dp(this->fan_mode_id_));
  ESP_LOGCONFIG(TAG, "  Fan Mode No-Schedule Datapoint: %s", dp(this->fan_mode_no_schedule_id_));
  ESP_LOGCONFIG(TAG, "  Humidity Datapoint: %s", dp(this->current_humidity_id_));
  if (!this->fan_modes_.empty()) {
    ESP_LOGCONFIG(TAG, "  Fan Modes:");
    for (const auto &entry : this->fan_modes_)
      ESP_LOGCONFIG(TAG, "    min_delta >= %.1f°C -> enum %u",
                    entry.minimum_temperature_delta, entry.id);
  }
}

climate::ClimateTraits EcoNetZoneControl::traits() {
  auto traits = climate::ClimateTraits();

  uint32_t flags = climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION |
                   climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE;
  if (this->current_humidity_id_ && *this->current_humidity_id_)
    flags |= climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY;
  traits.set_feature_flags(flags);

  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  traits.add_supported_mode(climate::CLIMATE_MODE_COOL);

  return traits;
}

void EcoNetZoneControl::control(const climate::ClimateCall &call) {
  // Write the requested changes directly to the primary zone's econet address.
  // Do NOT update cached values or publish state — wait for the listener to confirm.
  if (this->primary_zone_ == nullptr)
    return;

  if (call.get_mode().has_value() && this->mode_id_ && *this->mode_id_) {
    auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                           [&](const ModeEntry &m) { return m.mode == *call.get_mode(); });
    if (it != this->modes_.end()) {
      ESP_LOGD(TAG, "Control: set primary zone mode to enum %u", it->id);
      this->parent_->set_enum_datapoint_value(this->mode_id_, it->id, this->primary_zone_->src_adr);
    }
  }

  if (call.get_target_temperature_low().has_value() &&
      this->target_temperature_low_id_ && *this->target_temperature_low_id_) {
    float val_f = celsius_to_fahrenheit(*call.get_target_temperature_low());
    ESP_LOGD(TAG, "Control: set primary zone target_low=%.1f°F", val_f);
    this->parent_->set_float_datapoint_value(this->target_temperature_low_id_, val_f,
                                             this->primary_zone_->src_adr);
  }

  if (call.get_target_temperature_high().has_value() &&
      this->target_temperature_high_id_ && *this->target_temperature_high_id_) {
    float val_f = celsius_to_fahrenheit(*call.get_target_temperature_high());
    ESP_LOGD(TAG, "Control: set primary zone target_high=%.1f°F", val_f);
    this->parent_->set_float_datapoint_value(this->target_temperature_high_id_, val_f,
                                             this->primary_zone_->src_adr);
  }
}

void EcoNetZoneControl::update_zones_() {
  bool state_changed = false;

  // 1. Mirror primary zone values to this HA climate entity
  if (this->primary_zone_ != nullptr) {
    if (this->primary_zone_->cached_mode != this->mode) {
      this->mode = this->primary_zone_->cached_mode;
      state_changed = true;
    }

    if (!std::isnan(this->primary_zone_->cached_target_low_f)) {
      float new_low_c = fahrenheit_to_celsius(this->primary_zone_->cached_target_low_f);
      if (std::isnan(this->target_temperature_low) ||
          std::abs(new_low_c - this->target_temperature_low) > 0.05f) {
        this->target_temperature_low = new_low_c;
        state_changed = true;
      }
    }

    if (!std::isnan(this->primary_zone_->cached_target_high_f)) {
      float new_high_c = fahrenheit_to_celsius(this->primary_zone_->cached_target_high_f);
      if (std::isnan(this->target_temperature_high) ||
          std::abs(new_high_c - this->target_temperature_high) > 0.05f) {
        this->target_temperature_high = new_high_c;
        state_changed = true;
      }
    }
  }

  // 2. Current temperature — average across all zones
  {
    float sum = 0.0f;
    int count = 0;
    for (const auto &zone : this->zones_) {
      if (!std::isnan(zone.cached_temperature)) {
        sum += zone.cached_temperature;
        count++;
      }
    }
    float avg = (count > 0) ? (sum / count) : NAN;
    if (avg != this->current_temperature &&
        !(std::isnan(avg) && std::isnan(this->current_temperature))) {
      this->current_temperature = avg;
      state_changed = true;
    }
  }

  // 3. Current humidity — average across all zones (if configured)
  if (this->current_humidity_id_ && *this->current_humidity_id_) {
    float sum = 0.0f;
    int count = 0;
    for (const auto &zone : this->zones_) {
      if (!std::isnan(zone.cached_humidity)) {
        sum += zone.cached_humidity;
        count++;
      }
    }
    float avg = (count > 0) ? (sum / count) : NAN;
    if (avg != this->current_humidity &&
        !(std::isnan(avg) && std::isnan(this->current_humidity))) {
      this->current_humidity = avg;
      state_changed = true;
    }
  }

  if (state_changed)
    this->publish_state();

  // 4. Keep non-primary zone target temps in sync with primary.
  //    Issue write commands only; do NOT update cached values — wait for listeners.
  if (this->primary_zone_ != nullptr) {
    for (auto &zone : this->zones_) {
      if (zone.is_primary)
        continue;

      if (this->target_temperature_low_id_ && *this->target_temperature_low_id_ &&
          !std::isnan(this->primary_zone_->cached_target_low_f) &&
          (std::isnan(zone.cached_target_low_f) ||
           std::abs(zone.cached_target_low_f - this->primary_zone_->cached_target_low_f) >= 0.1f)) {
        ESP_LOGD(TAG, "Zone src_adr=0x%08X target_low out of sync (%.1f vs %.1f°F), correcting",
                 zone.src_adr, zone.cached_target_low_f, this->primary_zone_->cached_target_low_f);
        this->parent_->set_float_datapoint_value(this->target_temperature_low_id_,
                                                  this->primary_zone_->cached_target_low_f,
                                                  zone.src_adr);
      }

      if (this->target_temperature_high_id_ && *this->target_temperature_high_id_ &&
          !std::isnan(this->primary_zone_->cached_target_high_f) &&
          (std::isnan(zone.cached_target_high_f) ||
           std::abs(zone.cached_target_high_f - this->primary_zone_->cached_target_high_f) >= 0.1f)) {
        ESP_LOGD(TAG, "Zone src_adr=0x%08X target_high out of sync (%.1f vs %.1f°F), correcting",
                 zone.src_adr, zone.cached_target_high_f, this->primary_zone_->cached_target_high_f);
        this->parent_->set_float_datapoint_value(this->target_temperature_high_id_,
                                                  this->primary_zone_->cached_target_high_f,
                                                  zone.src_adr);
      }
    }
  }

  // 5. Apply fan mode logic
  this->update_zone_fan_mode_();
}

void EcoNetZoneControl::update_current_action_() {
  climate::ClimateAction new_action;
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    new_action = climate::CLIMATE_ACTION_OFF;
  } else if (this->operating_mode_id_ == nullptr || !*this->operating_mode_id_) {
    new_action = climate::CLIMATE_ACTION_IDLE;
  } else {
    const std::string &state = this->operating_mode_state_;
    if (state.find("Off") != std::string::npos)
      new_action = climate::CLIMATE_ACTION_IDLE;
    else if (state.find("Heat") != std::string::npos)
      new_action = climate::CLIMATE_ACTION_HEATING;
    else if (state.find("Cool") != std::string::npos)
      new_action = climate::CLIMATE_ACTION_COOLING;
    else if (state.find("Fan") != std::string::npos)
      new_action = climate::CLIMATE_ACTION_FAN;
    else
      new_action = climate::CLIMATE_ACTION_IDLE;
  }
  if (new_action == this->action)
    return;
  this->action = new_action;
  this->update_zone_fan_mode_();
  this->publish_state();
}

void EcoNetZoneControl::update_zone_fan_mode_() {
  if (this->action == climate::CLIMATE_ACTION_OFF)
    return;

  // For IDLE and FAN: calculate a target speed from the temp spread,
  // apply it only to the hottest and coldest zones, and leave the rest on automatic.
  // For any other active action: all zones use automatic.
  uint8_t spread_mode = this->automatic_fan_mode_;
  const EconetZone *min_zone = nullptr;
  const EconetZone *max_zone = nullptr;

  if (this->action == climate::CLIMATE_ACTION_IDLE ||
      this->action == climate::CLIMATE_ACTION_FAN) {
    for (const auto &zone : this->zones_) {
      if (std::isnan(zone.cached_temperature))
        continue;
      if (min_zone == nullptr || zone.cached_temperature < min_zone->cached_temperature)
        min_zone = &zone;
      if (max_zone == nullptr || zone.cached_temperature > max_zone->cached_temperature)
        max_zone = &zone;
    }
    if (min_zone == nullptr || max_zone == nullptr) {
      // No temperature data yet — defer to avoid writing automatic mode prematurely
      return;
    }
    float delta = max_zone->cached_temperature - min_zone->cached_temperature;
    // Find the entry with the highest minimum_temperature_delta still <= current delta
    const FanModeEntry *best = nullptr;
    for (const auto &entry : this->fan_modes_) {
      if (entry.minimum_temperature_delta <= delta) {
        if (best == nullptr || entry.minimum_temperature_delta > best->minimum_temperature_delta)
          best = &entry;
      }
    }
    spread_mode = (best != nullptr) ? best->id : this->automatic_fan_mode_;
  }

  // Write to each zone: hottest and coldest get spread_mode, all others get automatic.
  auto write_fan = [&](const char *dp_id, auto EconetZone::*cache_field) {
    if (!dp_id || !*dp_id)
      return;
    for (auto &zone : this->zones_) {
      uint8_t target = (&zone == min_zone || &zone == max_zone)
                           ? spread_mode
                           : this->automatic_fan_mode_;
      if (zone.*cache_field >= 0 && static_cast<uint8_t>(zone.*cache_field) == target)
        continue;
      ESP_LOGD(TAG, "Zone src_adr=0x%08X set %s to enum %u", zone.src_adr, dp_id, target);
      this->parent_->set_enum_datapoint_value(dp_id, target, zone.src_adr);
    }
  };

  write_fan(this->fan_mode_id_, &EconetZone::cached_fan_mode);
  write_fan(this->fan_mode_no_schedule_id_, &EconetZone::cached_fan_mode_no_schedule);
}

}  // namespace esphome::econet_zone_control
