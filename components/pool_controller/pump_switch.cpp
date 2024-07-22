#include "pump_switch.h"

namespace esphome {
namespace pool_controller {

static const char *const PUMP_SWITCH_TAG = "pool.pump.switch";

// Minimum time the pump needs to be on, turning off before this will result in a delayed turn off (10000 = 10 seconds)
static const uint32_t MINIMUM_PUMP_ON_TIME = 10000;

// Minimum time the pump needs to be off, turning on before this will result in a delayed turn on (10000 = 10 seconds)
static const uint32_t MINIMUM_PUMP_OFF_TIME = 10000;

static const uint32_t FORTY_EIGHT_HOURS = 172800000;

PumpSwitch::PumpSwitch(switch_::Switch *physical_switch) {
  this->set_update_interval(1000);
  this->set_restore_mode(switch_::SWITCH_ALWAYS_OFF);

  physical_switch->add_on_state_callback([this](bool state) -> void {
    this->on_physical_switch_state_change_(state);
  });
  this->physical_switch_ = physical_switch;
}

void PumpSwitch::update() {
  this->update_runtime_();
  this->update_physical_switch_();
  //this->check_current_();
}

uint32_t PumpSwitch::get_current_on_time() {
  if (!this->physical_switch_->state) {
    return 0;
  } else {
    return millis() - this->turned_on_at_;
  }
}

uint32_t PumpSwitch::get_current_off_time() {
  if (this->physical_switch_->state) {
    return 0;
  } else {
    return millis() - this->turned_off_at_;
  }
}

void PumpSwitch::set_is_disabled(bool is_disabled) {
  this->is_disabled_ = is_disabled;
  if (is_disabled && this->state) {
    this->turn_off();
  }
}

void PumpSwitch::emergency_stop() {
  this->physical_switch_->turn_off();
  this->set_is_disabled(true);
}

void PumpSwitch::write_state(bool state) {
  state = state && !this->is_disabled_;
  this->state = state;
  this->publish_state(state);
}

void PumpSwitch::on_physical_switch_state_change_(bool state) {
  if (state) {
    uint32_t now = millis();
    this->turned_on_at_ = now;
    this->last_runtime_update_ = now;
    ESP_LOGW(PUMP_SWITCH_TAG, "'%s': Physical switch turned on", this->get_name().c_str());
  } else {
    uint32_t now = millis();
    this->turned_off_at_ = now;
    this->runtime_ += now - this->last_runtime_update_;
    ESP_LOGW(PUMP_SWITCH_TAG, "'%s': Physical switch turned off", this->get_name().c_str());
  }
}

void PumpSwitch::update_runtime_() {
  if (this->physical_switch_->state) {
    uint32_t now = millis();
    this->runtime_ += now - this->last_runtime_update_;
    this->last_runtime_update_ = now;
  }
}

void PumpSwitch::update_physical_switch_() {
  if (this->state == this->physical_switch_->state) {
    return;
  }

  if (this->state) {
    if (millis() - this->turned_off_at_ < MINIMUM_PUMP_OFF_TIME) {
      return;
    }

    // Check any other registered conditions
    for (auto &check : this->turn_on_checks_) {
      if (!check()){
        return;
      }
    }

    this->physical_switch_->turn_on();
  } else {
    if (millis() - this->turned_on_at_ < MINIMUM_PUMP_ON_TIME) {
      return;
    }

    // Check any other registered conditions
    for (auto &check : this->turn_off_checks_) {
      if (!check()){
        return;
      }
    }

    this->physical_switch_->turn_off();
  }
}

void PumpSwitch::check_current_() {
  if (!this->current_sensor_->has_state()) {
    return;
  }

  auto now = millis();

  // If we haven't had another out of range event in 48 hours it was probably an anomaly so reset
  if (this->last_current_out_of_range_at_ != 0 && now - this->last_current_out_of_range_at_ > FORTY_EIGHT_HOURS) {
    this->last_current_out_of_range_at_ = 0;
  }

  float current = this->current_sensor_->get_state();
  if (current == 0 || (current >= this->min_current_ && current <= this->max_current_)) {
    if (this->current_out_of_range_at_ != 0) {
      this->current_out_of_range_at_ = 0;
    }
  } else {
    if (this->current_out_of_range_at_ == 0) {
      this->current_out_of_range_at_ = now;
    } else if (now - this->current_out_of_range_at_ > this->max_current_out_of_range_time_) {
      this->physical_switch_->turn_off();
      if (this->last_current_out_of_range_at_ != 0) {
        this->is_disabled_ = true;
      }
      this->last_current_out_of_range_at_ = now;
    }
  }
}

}  // namespace pool_controller
}  // namespace esphome
