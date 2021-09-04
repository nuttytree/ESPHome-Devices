#include "pump_switch.h"

namespace esphome {
namespace pool_controller {

// Minimum time the pump needs to be on, turning off before this will result in a delayed turn off (10000 = 10 seconds)
static const uint32_t MINIMUM_PUMP_ON_TIME = 10000;

// Minimum time the pump needs to be off, turning on before this will result in a delayed turn on (10000 = 10 seconds)
static const uint32_t MINIMUM_PUMP_OFF_TIME = 10000;

PumpSwitch::PumpSwitch(switch_::Switch *physical_switch) {
  this->set_name(physical_switch->get_name());
  this->set_icon(physical_switch->get_icon());
  this->set_update_interval(10000);

  physical_switch->set_internal(true);
  physical_switch->add_on_state_callback([this](bool state) -> void {
    if (state) {
      uint32_t now = millis();
      this->turned_on_at_ = now;
      this->last_runtime_update_ = now;
      if (!this->state) {
        this->physical_switch_->turn_off();
      }
    } else {
      uint32_t now = millis();
      this->turned_off_at_ = now;
      this->runtime_ += now - this->last_runtime_update_;
      this->last_runtime_update_ = now;
      if (this->state) {
        this->turn_off();
      }
    }
  });
  this->physical_switch_ = physical_switch;
}

void PumpSwitch::setup() {
  this->turn_off();
  this->physical_switch_->turn_off();
}

void PumpSwitch::loop() {
  this->update_physical_switch_();
}

void PumpSwitch::update() {
  if (this->physical_switch_->state) {
    uint32_t now = millis();
    this->runtime_ += now - this->last_runtime_update_;
    this->last_runtime_update_ = now;
  }
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
  if (state && this->is_disabled_) {
    state = false;
  }
  
  this->state = state;
  this->publish_state(state);
  this->update_physical_switch_();
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

}  // namespace pool_controller
}  // namespace esphome
