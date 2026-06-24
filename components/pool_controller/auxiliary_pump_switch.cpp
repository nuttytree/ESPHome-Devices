#include "pump_switch.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

void AuxiliaryPumpSwitch::set_primary_pump(PrimaryPumpSwitch *primary_pump) {
  if (primary_pump != nullptr) {
    this->primary_pump_ = primary_pump;
    primary_pump->add_auxiliary_pump(this);
  }
}

void AuxiliaryPumpSwitch::write_state(bool state) {
  if (state && this->is_disabled()) {
    ESP_LOGD(TAG, "'%s' turn-on blocked — disable-pumps sensor is active", this->get_name().c_str());
    this->publish_state(false);
    return;
  }

  if (state && this->primary_pump_ != nullptr && !this->primary_pump_->state) {
    // Primary is off — turn it on first, then retry after 2 s.
    // Named timeout cancels any previous pending attempt, preventing stacking.
    ESP_LOGD(TAG, "Primary off — turning it on and delaying auxiliary start by %" PRIu32 " ms", this->sequence_delay_ms_);
    this->primary_pump_->turn_on();
    this->set_timeout("aux_seq_on", this->sequence_delay_ms_, [this]() {
      this->turn_on();  // Re-enter write_state; primary is on now so it proceeds.
    });
    return;
  }

  this->output_->set_state(state);
  this->publish_state(state);
  if (!this->use_current_for_state_) {
    this->track_runtime(state);
  } else if (!state) {
    // Output commanded off — stop runtime counting immediately regardless of current.
    this->motor_running_ = false;
    this->track_runtime(false);
  }
  // When use_current_for_state_ && state==true: runtime starts in loop() when current confirms motor is running.
}

}  // namespace pool_controller
}  // namespace esphome
