#include "econet_zones_climate.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::econet_zones_climate {

static const char *const TAG = "econet_zones_climate";

void EcoNetZonesClimate::setup() {
  for (auto *zone : this->zones_) {
    zone->add_on_state_callback([this, zone](climate::Climate &) {
      this->update_current_temperature_();
      this->update_current_humidity_();
      this->update_zone_fan_mode_();
      this->sync_this_from_zone_(zone);
    });
  }

  if (this->operating_mode_sensor_ != nullptr) {
    this->operating_mode_sensor_->add_on_state_callback([this](const std::string &) {
      this->update_current_action_();
    });
  }

  this->update_current_temperature_();
  this->update_current_humidity_();

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->update_current_action_();
}

void EcoNetZonesClimate::dump_config() {
  LOG_CLIMATE("", "EcoNet Zones Climate", this);
  ESP_LOGCONFIG(TAG, "  Zones: %zu", this->zones_.size());
  ESP_LOGCONFIG(TAG, "  Supports Heat Mode: %s", YESNO(this->supports_heat_mode_));
  ESP_LOGCONFIG(TAG, "  Supports Cool Mode: %s", YESNO(this->supports_cool_mode_));
  ESP_LOGCONFIG(TAG, "  Automatic Fan Mode: %s", this->automatic_fan_mode_ != nullptr ? this->automatic_fan_mode_ : "none");
  if (!this->fan_modes_.empty()) {
    ESP_LOGCONFIG(TAG, "  Fan Modes:");
    for (const auto &entry : this->fan_modes_)
      ESP_LOGCONFIG(TAG, "    min_delta >= %.1f -> %s", entry.minimum_temperature_delta, entry.fan_mode);
  }
}

climate::ClimateTraits EcoNetZonesClimate::traits() {
  auto traits = climate::ClimateTraits();

  uint32_t flags = climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION;
  if (this->supports_current_humidity_)
    flags |= climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY;
  if (this->supports_target_humidity_)
    flags |= climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY;
  if (this->supports_heat_mode_ && this->supports_cool_mode_)
    flags |= climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE;
  traits.set_feature_flags(flags);

  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  if (this->supports_heat_mode_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  if (this->supports_cool_mode_)
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);

  return traits;
}

void EcoNetZonesClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_target_temperature_low().has_value())
    this->target_temperature_low = *call.get_target_temperature_low();
  if (call.get_target_temperature_high().has_value())
    this->target_temperature_high = *call.get_target_temperature_high();
  if (call.get_target_humidity().has_value())
    this->target_humidity = *call.get_target_humidity();

  this->sync_zones_from_this_();
  this->publish_state();
}

void EcoNetZonesClimate::sync_zones_from_this_() {
  this->ignore_zone_updates_until_ = millis() + 10000;
  ESP_LOGD(TAG, "Syncing state to all zones");
  for (auto *zone : this->zones_) {
    auto call = zone->make_call();
    call.set_mode(this->mode);
    if (this->supports_heat_mode_ && this->supports_cool_mode_) {
      if (!std::isnan(this->target_temperature_low))
        call.set_target_temperature_low(this->target_temperature_low);
      if (!std::isnan(this->target_temperature_high))
        call.set_target_temperature_high(this->target_temperature_high);
    } else {
      if (!std::isnan(this->target_temperature))
        call.set_target_temperature(this->target_temperature);
    }
    if (this->supports_target_humidity_ && !std::isnan(this->target_humidity))
      call.set_target_humidity(this->target_humidity);
    call.perform();
  }
}

void EcoNetZonesClimate::sync_this_from_zone_(climate::Climate *zone) {
  if (millis() < this->ignore_zone_updates_until_)
    return;

  bool changed = false;

  if (zone->mode != this->mode) {
    ESP_LOGD(TAG, "Zone mode changed to %d, syncing", static_cast<int>(zone->mode));
    this->mode = zone->mode;
    changed = true;
  }
  if (this->supports_heat_mode_ && this->supports_cool_mode_) {
    if (!std::isnan(zone->target_temperature_low) && zone->target_temperature_low != this->target_temperature_low) {
      this->target_temperature_low = zone->target_temperature_low;
      changed = true;
    }
    if (!std::isnan(zone->target_temperature_high) && zone->target_temperature_high != this->target_temperature_high) {
      this->target_temperature_high = zone->target_temperature_high;
      changed = true;
    }
  } else {
    if (!std::isnan(zone->target_temperature) && zone->target_temperature != this->target_temperature) {
      this->target_temperature = zone->target_temperature;
      changed = true;
    }
  }
  if (this->supports_target_humidity_ && !std::isnan(zone->target_humidity) &&
      zone->target_humidity != this->target_humidity) {
    this->target_humidity = zone->target_humidity;
    changed = true;
  }

  if (!changed)
    return;

  this->sync_zones_from_this_();
  this->publish_state();
}

void EcoNetZonesClimate::update_current_temperature_() {
  float sum = 0.0f;
  int count = 0;
  for (const auto *zone : this->zones_) {
    if (!std::isnan(zone->current_temperature)) {
      sum += zone->current_temperature;
      count++;
    }
  }
  float new_temp = (count > 0) ? (sum / count) : NAN;
  if (new_temp == this->current_temperature || (std::isnan(new_temp) && std::isnan(this->current_temperature)))
    return;
  this->current_temperature = new_temp;
  this->publish_state();
}

void EcoNetZonesClimate::update_current_humidity_() {
  if (!this->supports_current_humidity_)
    return;
  float sum = 0.0f;
  int count = 0;
  for (const auto *zone : this->zones_) {
    if (!std::isnan(zone->current_humidity)) {
      sum += zone->current_humidity;
      count++;
    }
  }
  float new_humidity = (count > 0) ? (sum / count) : NAN;
  if (new_humidity == this->current_humidity || (std::isnan(new_humidity) && std::isnan(this->current_humidity)))
    return;
  this->current_humidity = new_humidity;
  this->publish_state();
}

void EcoNetZonesClimate::update_current_action_() {
  climate::ClimateAction new_action;
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    new_action = climate::CLIMATE_ACTION_OFF;
  } else if (this->operating_mode_sensor_ == nullptr) {
    new_action = climate::CLIMATE_ACTION_IDLE;
  } else {
    const std::string &state = this->operating_mode_sensor_->state;
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

void EcoNetZonesClimate::update_zone_fan_mode_() {
  if (this->action == climate::CLIMATE_ACTION_OFF)
    return;

  const char *desired;
  if (this->action == climate::CLIMATE_ACTION_HEATING || this->action == climate::CLIMATE_ACTION_COOLING) {
    desired = this->automatic_fan_mode_;
  } else {
    // Compute delta between hottest and coldest zone
    float min_temp = NAN;
    float max_temp = NAN;
    for (const auto *zone : this->zones_) {
      if (std::isnan(zone->current_temperature))
        continue;
      if (std::isnan(min_temp) || zone->current_temperature < min_temp)
        min_temp = zone->current_temperature;
      if (std::isnan(max_temp) || zone->current_temperature > max_temp)
        max_temp = zone->current_temperature;
    }
    if (std::isnan(min_temp) || std::isnan(max_temp)) {
      desired = this->automatic_fan_mode_;
    } else {
      float delta = max_temp - min_temp;
      // Find the entry with the highest minimum_temperature_delta that is still <= current delta
      const FanModeEntry *best = nullptr;
      for (const auto &entry : this->fan_modes_) {
        if (entry.minimum_temperature_delta <= delta) {
          if (best == nullptr || entry.minimum_temperature_delta > best->minimum_temperature_delta)
            best = &entry;
        }
      }
      desired = (best != nullptr) ? best->fan_mode : this->automatic_fan_mode_;
    }
  }

  if (desired == nullptr || desired == this->current_fan_mode_)
    return;

  ESP_LOGD(TAG, "Setting zone fan mode to '%s'", desired);
  this->current_fan_mode_ = desired;
  for (auto *zone : this->zones_) {
    auto call = zone->make_call();
    call.set_fan_mode(desired);
    call.perform();
  }
}

}  // namespace esphome::econet_zones_climate
