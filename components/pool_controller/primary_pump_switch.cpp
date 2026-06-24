#include "pump_switch.h"
#include "pool_heater.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.switch";

void PrimaryPumpSwitch::add_auxiliary_pump(AuxiliaryPumpSwitch *aux_switch) {
  if (aux_switch != nullptr) {
    this->auxiliary_pumps_.push_back(aux_switch);
  }
}

void PrimaryPumpSwitch::write_state(bool state) {
  if (state && this->is_disabled()) {
    ESP_LOGD(TAG, "'%s' turn-on blocked — disable-pumps sensor is active", this->get_name().c_str());
    this->publish_state(false);
    return;
  }

  if (!state) {
    // Turn auxiliary pumps and pool heater off immediately, then bring the
    // primary pump down after the sequence delay so everything settles first.
    bool heater_was_active = (this->pool_heater_ != nullptr && this->pool_heater_->is_heater_active());
    if (this->pool_heater_ != nullptr)
      this->pool_heater_->request_heater_off();

    bool any_aux_on = false;
    for (auto *aux : auxiliary_pumps_) {
      if (aux && aux->state) {
        aux->turn_off();
        any_aux_on = true;
      }
    }
    if (any_aux_on || heater_was_active) {
      ESP_LOGD(TAG, "Sequenced shutdown: auxiliaries/heater off now, primary off in %" PRIu32 " ms",
               this->sequence_delay_ms_);
      this->set_timeout("primary_seq_off", this->sequence_delay_ms_, [this]() {
        this->output_->set_state(false);
        this->publish_state(false);
        this->motor_running_ = false;
        this->track_runtime(false);
      });
      return;
    }
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
