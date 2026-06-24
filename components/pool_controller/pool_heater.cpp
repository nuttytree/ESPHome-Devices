#include "pool_heater.h"
#include "pump_switch.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.heater";

/// NaN-safe float comparison with a small epsilon (0.05 °C ≈ 0.09 °F).
static bool floats_equal(float a, float b) {
  if (std::isnan(a) && std::isnan(b))
    return true;
  if (std::isnan(a) || std::isnan(b))
    return false;
  return std::abs(a - b) < 0.05f;
}

// ── Component lifecycle ────────────────────────────────────────────────────────

void PoolHeater::setup() {
  // Determine sensor unit so we can convert °F → °C when needed.
  if (this->temperature_sensor_ != nullptr) {
    auto unit = this->temperature_sensor_->get_unit_of_measurement_ref();
    // "\xc2\xb0F" is the UTF-8 encoding of "°F".
    this->sensor_is_fahrenheit_ = (unit == "\xc2\xb0\x46");
    ESP_LOGD(TAG, "Temperature sensor '%s' unit: '%s' — treating as %s", this->temperature_sensor_->get_name().c_str(),
             unit.c_str(), this->sensor_is_fahrenheit_ ? "\xc2\xb0\x46" : "\xc2\xb0\x43");
  }

  // Invalidate reading whenever the primary pump starts so we wait 15 s before trusting the sensor.
  if (this->primary_pump_ != nullptr) {
    this->primary_pump_->add_on_state_callback([this](bool state) {
      if (state) {
        this->has_reading_since_pump_on_ = false;
        ESP_LOGD(TAG, "Primary pump ON — deferring temperature reads for 15 s");
      }
    });
  }

  // Set factory defaults (Off / 80 °F = 26.67 °C) before restoring so they are
  // in place if no saved state exists.
  this->set_mode_(water_heater::WATER_HEATER_MODE_OFF);
  this->set_target_temperature_(26.6667f);

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    // perform() validates the call against our supported-modes list, then calls
    // control() which calls apply_control_() and publish_state().
    restore->perform();
  } else {
    // First boot — defaults already set above.  Kick the controller and persist
    // the defaults so next boot restores them correctly.
    this->apply_control_();
    this->publish_state();
    ESP_LOGD(TAG, "No saved state — defaults: mode=OFF, target=26.67\xc2\xb0\x43 (80\xc2\xb0\x46)");
  }
}

void PoolHeater::loop() {
  this->update_temperature_();
  this->apply_control_();
}

void PoolHeater::dump_config() {
  LOG_WATER_HEATER("", "Pool Heater (Pool Controller)", this);
  if (this->temperature_sensor_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Temperature Sensor: %s", this->temperature_sensor_->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Deadband: %.4f\xc2\xb0\x43 (%.4f\xc2\xb0\x46)", this->deadband_, this->deadband_ * 9.0f / 5.0f);
  ESP_LOGCONFIG(TAG, "  Overrun:  %.4f\xc2\xb0\x43 (%.4f\xc2\xb0\x46)", this->overrun_, this->overrun_ * 9.0f / 5.0f);
  this->dump_traits_(TAG);
}

// ── WaterHeater overrides ──────────────────────────────────────────────────────

water_heater::WaterHeaterTraits PoolHeater::traits() {
  auto traits = water_heater::WaterHeaterTraits();
  traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE |
                           water_heater::WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE |
                           water_heater::WATER_HEATER_SUPPORTS_OPERATION_MODE);
  traits.set_supported_modes({water_heater::WATER_HEATER_MODE_OFF, water_heater::WATER_HEATER_MODE_GAS});
  traits.set_min_temperature(this->min_temperature_);
  traits.set_max_temperature(this->max_temperature_);
  traits.set_target_temperature_step(this->target_temperature_step_);
  return traits;
}

void PoolHeater::control(const water_heater::WaterHeaterCall &call) {
  if (call.get_mode().has_value())
    this->set_mode_(*call.get_mode());
  if (!std::isnan(call.get_target_temperature()))
    this->set_target_temperature_(call.get_target_temperature());
  this->apply_control_();
  this->publish_state();  // base class publish_state() persists mode + target temp
}

// ── Integration helpers ────────────────────────────────────────────────────────

void PoolHeater::request_heater_off() {
  if (this->heater_active_) {
    ESP_LOGD(TAG, "Sequenced shutdown: forcing heater output OFF");
    this->heater_output_->set_state(false);
    this->heater_active_ = false;
    this->publish_state();
  }
}

// ── Internal helpers ───────────────────────────────────────────────────────────

void PoolHeater::update_temperature_() {
  if (this->temperature_sensor_ == nullptr)
    return;

  // Only accept readings while the primary pump is on and has been stable for 15 s.
  if (this->primary_pump_ == nullptr || !this->primary_pump_->state)
    return;

  const uint64_t on_ms = millis_64() - this->primary_pump_->get_turned_on_ms();
  if (on_ms < 15000u)
    return;

  const float raw = this->temperature_sensor_->state;
  if (std::isnan(raw))
    return;

  const float temp_c = this->sensor_is_fahrenheit_ ? (raw - 32.0f) * (5.0f / 9.0f) : raw;

  bool changed = !floats_equal(temp_c, this->current_temperature_);

  if (!this->has_reading_since_pump_on_) {
    this->has_reading_since_pump_on_ = true;
    changed = true;
    ESP_LOGD(TAG, "First accepted reading since pump start: %.2f\xc2\xb0\x43 (%.2f\xc2\xb0\x46)", temp_c,
             temp_c * 9.0f / 5.0f + 32.0f);
  }

  if (changed) {
    this->set_current_temperature(temp_c);
    this->publish_state();
  }
}

void PoolHeater::apply_control_() {
  // ── OFF mode ────────────────────────────────────────────────────────────────
  if (this->mode_ == water_heater::WATER_HEATER_MODE_OFF) {
    if (this->heater_active_) {
      ESP_LOGD(TAG, "Mode OFF — deactivating heater output");
      this->heater_output_->set_state(false);
      this->heater_active_ = false;
    }
    return;
  }

  // ── GAS mode ─────────────────────────────────────────────────────────────────
  // Prerequisites: primary pump running and we have at least one stable reading.
  const bool pump_on = (this->primary_pump_ != nullptr && this->primary_pump_->state);
  if (!pump_on || !this->has_reading_since_pump_on_ || std::isnan(this->current_temperature_)) {
    if (this->heater_active_) {
      ESP_LOGD(TAG, "Pump not ready or no temperature reading — deactivating heater output");
      this->heater_output_->set_state(false);
      this->heater_active_ = false;
    }
    return;
  }

  // Bang-bang control.
  const float temp = this->current_temperature_;
  const float target = this->target_temperature_;

  if (this->heater_active_) {
    // Running: turn off once temperature reaches target + overrun.
    if (temp >= target + this->overrun_) {
      ESP_LOGD(TAG, "Temp %.2f\xc2\xb0\x43 reached overrun threshold (%.2f\xc2\xb0\x43) — heater OFF", temp,
               target + this->overrun_);
      this->heater_output_->set_state(false);
      this->heater_active_ = false;
    }
  } else {
    // Idle: turn on once temperature drops to target - deadband.
    if (temp <= target - this->deadband_) {
      ESP_LOGD(TAG, "Temp %.2f\xc2\xb0\x43 below deadband threshold (%.2f\xc2\xb0\x43) — heater ON", temp,
               target - this->deadband_);
      this->heater_output_->set_state(true);
      this->heater_active_ = true;
    }
  }
}

}  // namespace pool_controller
}  // namespace esphome
