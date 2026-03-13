#include "high_temp_water_heater.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace high_temp_water_heater {

// NaN-safe float comparison: treats two NaNs as equal.
static bool floats_equal(float a, float b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

static const char *const TAG = "water_heater.high_temp_water_heater";

void HighTempWaterHeater::setup() {
  // Detect temperature sensor unit once so sync_from_source_() can convert if needed.
  if (this->temperature_sensor_ != nullptr) {
    auto unit = this->temperature_sensor_->get_unit_of_measurement_ref();
    // "\xc2\xb0F" is the UTF-8 encoding of "°F"
    this->temperature_sensor_is_fahrenheit_ = (unit == "\xc2\xb0\x46");
    ESP_LOGD(TAG, "Temperature sensor unit: '%s' — treating as %s",
             unit.c_str(), this->temperature_sensor_is_fahrenheit_ ? "\xc2\xb0\x46" : "\xc2\xb0\x43");
  }
  // Sync temperatures from source first so control logic has valid values.
  this->sync_from_source_();
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    // perform() calls control() which calls apply_control_() and publish_state().
    restore->perform();
  } else {
    // No saved state — seed the target temperature from the source so the HA
    // entity has a sensible starting value. Mode is intentionally left at the
    // default (OFF); the user sets it from HA when ready. Seeding mode from the
    // source would cause apply_control_() to lock the source to OFF on every
    // loop() call whenever the source starts in OFF mode.
    if (this->source_ != nullptr) {
      float src_target = this->source_->get_target_temperature();
      if (!std::isnan(src_target)) {
        this->set_target_temperature_(src_target);
      }
    }
    this->apply_control_();
    this->publish_state();
  }
}

void HighTempWaterHeater::dump_config() {
  LOG_WATER_HEATER("", "High Temp Water Heater", this);
  ESP_LOGCONFIG(TAG, "  Source Water Heater: %s", this->source_->get_name().c_str());
  if (this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature Sensor: %s", this->temperature_sensor_->get_name().c_str());
    ESP_LOGCONFIG(TAG, "  Temperature Sensor Offset: %.1f", this->temperature_sensor_offset_);
  }
  ESP_LOGCONFIG(TAG, "  Dead Band: %.1f", this->dead_band_);
  ESP_LOGCONFIG(TAG, "  Over Run: %.1f", this->over_run_);
}

void HighTempWaterHeater::loop() {
  bool changed = this->sync_from_source_();
  this->apply_control_();
  if (changed)
    this->publish_state();
}

bool HighTempWaterHeater::sync_from_source_() {
  if (this->source_ == nullptr)
    return false;

  bool changed = false;

  float src_temp = this->source_->get_current_temperature();
  if (!floats_equal(src_temp, this->current_temperature_)) {
    this->set_current_temperature(src_temp);
    changed = true;
  }

  // If our target temperature hasn't been set yet (NaN), adopt the source's
  // target temperature as soon as it becomes available.
  if (std::isnan(this->target_temperature_)) {
    float src_target = this->source_->get_target_temperature();
    if (!std::isnan(src_target)) {
      this->set_target_temperature_(src_target);
      changed = true;
    }
  }

  // monitored_temp_ uses the temperature sensor (+ offset) when available,
  // otherwise falls back to current_temperature_. Only track as a separate
  // change when the sensor is configured (otherwise it always equals current_temperature_
  // which is already tracked above).
  if (this->temperature_sensor_ != nullptr) {
    float raw = this->temperature_sensor_->state;
    float sensor_c = (this->temperature_sensor_is_fahrenheit_ && !std::isnan(raw))
                         ? (raw - 32.0f) * (5.0f / 9.0f)
                         : raw;
    float new_monitored = sensor_c + this->temperature_sensor_offset_;
    if (!floats_equal(new_monitored, this->monitored_temp_)) {
      this->monitored_temp_ = new_monitored;
      changed = true;
    }
  } else {
    this->monitored_temp_ = this->current_temperature_;
  }

  return changed;
}

void HighTempWaterHeater::apply_control_() {
  if (this->source_ == nullptr)
    return;

  // Mirror away mode to child if it differs.
  if (this->is_away() != this->source_->is_away()) {
    auto call = this->source_->make_call();
    call.set_away(this->is_away());
    call.perform();
  }

  // If we don't yet have valid temperatures, leave the source alone entirely.
  // NaN target or monitored_temp means the source hasn't delivered values yet;
  // forcing the source to OFF here would override any manually-set state and
  // prevent heating from ever starting once the values arrive.
  if (std::isnan(this->monitored_temp_) || std::isnan(this->target_temperature_)) {
    ESP_LOGD(TAG, "apply_control_: waiting for values (monitored=%.1f, target=%.1f) — source untouched",
             this->monitored_temp_, this->target_temperature_);
    return;
  }

  // Throttle the status log to once per minute to avoid flooding.
  uint32_t now = millis();
  if (now - this->last_status_log_ms_ >= 60000) {
    this->last_status_log_ms_ = now;
    ESP_LOGD(TAG,
             "apply_control_: mode=%s monitored=%.2f target=%.2f dead_band=%.2f over_run=%.2f heating_active=%s",
             LOG_STR_ARG(water_heater::water_heater_mode_to_string(this->mode_)),
             this->monitored_temp_, this->target_temperature_, this->dead_band_, this->over_run_,
             this->heating_active_ ? "YES" : "NO");
  }

  if (this->mode_ == water_heater::WATER_HEATER_MODE_OFF) {
    // High-temp entity is OFF — reset bang-bang state and ensure source is off.
    this->heating_active_ = false;
  } else {
    // Bang-bang control: turn on when monitored drops below (target - dead_band),
    // turn off when it rises above (target + over_run).
    if (this->heating_active_) {
      if (this->monitored_temp_ >= this->target_temperature_ + this->over_run_) {
        ESP_LOGD(TAG, "Heating cut: monitored=%.1f >= target=%.1f + over_run=%.1f",
                 this->monitored_temp_, this->target_temperature_, this->over_run_);
        this->heating_active_ = false;
      }
    } else {
      if (this->monitored_temp_ <= this->target_temperature_ - this->dead_band_) {
        ESP_LOGD(TAG, "Heating start: monitored=%.1f <= target=%.1f - dead_band=%.1f",
                 this->monitored_temp_, this->target_temperature_, this->dead_band_);
        this->heating_active_ = true;
      }
    }
  }

  water_heater::WaterHeaterMode desired_child_mode =
      this->heating_active_ ? this->mode_ : water_heater::WATER_HEATER_MODE_OFF;

  // This component is the sole controller of the source's mode. Any external
  // change is reverted immediately on the next loop() tick.
  if (desired_child_mode != this->source_->get_mode()) {
    ESP_LOGD(TAG, "Setting child mode: %s", LOG_STR_ARG(water_heater::water_heater_mode_to_string(desired_child_mode)));
    auto call = this->source_->make_call();
    call.set_mode(desired_child_mode);
    call.perform();
  }
}

void HighTempWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  if (call.get_mode().has_value()) {
    this->set_mode_(*call.get_mode());
  }
  if (!std::isnan(call.get_target_temperature())) {
    this->set_target_temperature_(call.get_target_temperature());
  }
  this->apply_control_();
  this->publish_state();
}

water_heater::WaterHeaterTraits HighTempWaterHeater::traits() {
  auto traits = water_heater::WaterHeaterTraits();

  // Feature flags are fixed regardless of what the source advertises.
  traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE |
                           water_heater::WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE |
                           water_heater::WATER_HEATER_SUPPORTS_OPERATION_MODE |
                           water_heater::WATER_HEATER_SUPPORTS_AWAY_MODE);

  // Temperature range and step come from component config, not from the source.
  traits.set_min_temperature(this->min_temperature_);
  traits.set_max_temperature(this->max_temperature_);
  traits.set_target_temperature_step(this->target_temperature_step_);

  // Supported operation modes are mirrored from the source when available.
  if (this->source_ != nullptr) {
    traits.set_supported_modes(this->source_->get_traits().get_supported_modes());
  } else {
    traits.set_supported_modes(
        {water_heater::WATER_HEATER_MODE_OFF, water_heater::WATER_HEATER_MODE_ECO,
         water_heater::WATER_HEATER_MODE_ELECTRIC, water_heater::WATER_HEATER_MODE_PERFORMANCE,
         water_heater::WATER_HEATER_MODE_HIGH_DEMAND});
  }

  return traits;
}

}  // namespace high_temp_water_heater
}  // namespace esphome
