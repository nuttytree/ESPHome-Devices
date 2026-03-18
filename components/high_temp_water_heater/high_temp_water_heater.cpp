#include "high_temp_water_heater.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::high_temp_water_heater {

// NaN-safe, rounding-tolerant float comparison.
// Uses a small epsilon (~0.1 °C / ~0.18 °F) to absorb protocol quantization
// and unit-conversion rounding artifacts without masking real temperature changes
static bool floats_equal(float a, float b) {
  if (std::isnan(a) && std::isnan(b))
    return true;
  if (std::isnan(a) || std::isnan(b))
    return false;
  return std::abs(a - b) < 0.1f;
}

static const char *const TAG = "water_heater.high_temp_water_heater";

void HighTempWaterHeater::setup() {
  // Note: WaterHeater has no state callback, so the source is polled via loop().
  if (this->temperature_sensor_ != nullptr) {
    auto unit = this->temperature_sensor_->get_unit_of_measurement_ref();
    // "\xc2\xb0F" is the UTF-8 encoding of "°F"
    this->temperature_sensor_is_fahrenheit_ = (unit == "\xc2\xb0\x46");
    ESP_LOGD(TAG, "Temperature sensor unit: '%s' — treating as %s",
             unit.c_str(), this->temperature_sensor_is_fahrenheit_ ? "\xc2\xb0\x46" : "\xc2\xb0\x43");

    this->temperature_sensor_->add_on_state_callback([this](float) {
      this->update_state_();
    });
  }

  // Initial sync before restoring state.
  this->update_state_();

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    // perform() calls control() which calls apply_control_() and publish_state().
    restore->perform();
  } else {
    // No saved state — mode is intentionally left at the default (OFF); the user
    // sets it from HA when ready. Seeding mode from the source would cause
    // apply_control_() to lock the source to OFF on every update_state_() call
    // whenever the source starts in OFF mode.
    this->apply_control_();
    this->publish_state();
  }
}

void HighTempWaterHeater::loop() { this->update_state_(); }

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

void HighTempWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  if (call.get_mode().has_value()) {
    this->set_mode_(*call.get_mode());
  }
  if (!std::isnan(call.get_target_temperature())) {
    this->set_target_temperature_(call.get_target_temperature());
  }
  if (call.get_away().has_value()) {
    this->set_state_flag_(water_heater::WATER_HEATER_STATE_AWAY, *call.get_away());
  }
  this->apply_control_();
  this->publish_state();
}

void HighTempWaterHeater::update_state_() {
  if (this->source_ == nullptr)
    return;

  bool changed = false;

  // If our target temperature hasn't been set yet (NaN), adopt the source's
  // target temperature as soon as it becomes available.
  if (std::isnan(this->target_temperature_)) {
    float src_target = this->source_->get_target_temperature();
    if (!std::isnan(src_target)) {
      this->set_target_temperature_(src_target);
      changed = true;
    }
  }

  float src_temp = this->source_->get_current_temperature();
  if (!floats_equal(src_temp, this->current_temperature_)) {
    this->set_current_temperature(src_temp);
    changed = true;
  }

  // Derive monitored_temp_ from the sensor (with unit conversion + offset) when
  // configured, otherwise fall back to current_temperature_.
  float new_monitored;
  if (this->temperature_sensor_ != nullptr) {
    float raw = this->temperature_sensor_->state;
    float sensor_c = (this->temperature_sensor_is_fahrenheit_ && !std::isnan(raw))
                         ? (raw - 32.0f) * (5.0f / 9.0f)
                         : raw;
    new_monitored = sensor_c + this->temperature_sensor_offset_;
  } else {
    new_monitored = this->current_temperature_;
  }
  if (!floats_equal(new_monitored, this->monitored_temp_)) {
    this->monitored_temp_ = new_monitored;
    changed = true;
  }

  this->apply_control_();
  if (changed)
    this->publish_state();
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

  water_heater::WaterHeaterMode desired_child_mode;
  if (this->mode_ == water_heater::WATER_HEATER_MODE_OFF) {
    // High-temp entity is OFF — reset bang-bang state and ensure source is off.
    this->heating_active_ = false;
    desired_child_mode = water_heater::WATER_HEATER_MODE_OFF;
  } else if (floats_equal(this->target_temperature_, this->source_->get_target_temperature())) {
    // Target temperatures match — the source can self-manage at its own setpoint,
    // so bypass bang-bang and simply mirror our mode directly to the child.
    ESP_LOGD(TAG, "Target temperatures match (%.2f), mirroring mode directly to child", this->target_temperature_);
    this->heating_active_ = false;
    desired_child_mode = this->mode_;
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
    desired_child_mode = this->heating_active_ ? this->mode_ : water_heater::WATER_HEATER_MODE_OFF;
  }

  if (desired_child_mode != this->source_->get_mode()) {
    ESP_LOGD(TAG, "Setting child mode: %s", LOG_STR_ARG(water_heater::water_heater_mode_to_string(desired_child_mode)));
    auto call = this->source_->make_call();
    call.set_mode(desired_child_mode);
    call.perform();
  }
}

}  // namespace esphome::high_temp_water_heater
