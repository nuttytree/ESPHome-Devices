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
    ESP_LOGD(TAG, "Primary off — turning it on and delaying auxiliary start by %" PRIu32 " ms",
             this->sequence_delay_ms_);
    this->primary_pump_->turn_on();
    this->set_timeout("aux_seq_on", this->sequence_delay_ms_, [this]() {
      this->turn_on();  // Re-enter write_state; primary is on now so it proceeds.
    });
    return;
  }

  if (state)
    this->arm_new_run_();
  this->output_->set_state(state);
  this->publish_state(state);
  if (!this->has_current_sensor() && !this->has_flow_sensor()) {
    // No sensors at all — the command is the only evidence available for either job.
    if (state)
      this->turned_on_ms_ = millis_64();
    this->track_runtime(state);
  } else if (!state) {
    // Output commanded off — stop runtime counting immediately regardless of sensor confirmation.
    this->motor_running_ = false;
    this->flow_running_ = false;
    this->track_runtime(false);
  }
  // When a current or flow sensor is configured and state==true: runtime starts in loop() once the
  // sensor confirms the pump is actually running.
  this->update_no_current_();
}

}  // namespace pool_controller
}  // namespace esphome
