#include "power_cycle_pool_light.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::power_cycle_pool_light {

static const char *const TAG = "power_cycle_pool_light";

light::LightTraits PowerCyclePoolLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void PowerCyclePoolLightOutput::setup() { register_service(&PowerCyclePoolLightOutput::color_reset, "color_reset"); }

// skipcq: cxx-c2014
void PowerCyclePoolLightOutput::setup_state(light::LightState *state) {
  this->state_ = state;

  this->color_pref_ = this->state_->make_entity_preference<uint32_t>();
  if (this->color_pref_.load(&this->current_color_)) {
    ESP_LOGD(TAG, "Restored color %u from flash", this->current_color_);
  } else {
    this->set_current_color_(1);
  }

  // The output was off while the device booted, but we don't know for how long, so treat
  // boot as the start of the off period rather than assume min_off_time has already passed.
  this->output_off_at_ = millis();

  // Set hardware to match the restored state from ESPHome
  bool restored_on = false;
  state->current_values_as_binary(&restored_on);
  this->set_output_state_(restored_on);
}

void PowerCyclePoolLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Cycle Pool Light:");
  ESP_LOGCONFIG(TAG, "  Colors: %u", this->color_count_);
  if (this->state_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Current Color: %s",
                  this->state_->get_effect_name_by_index(this->get_current_color_()).c_str());
  }
  ESP_LOGCONFIG(TAG, "  Minimum Off Time: %ums", this->min_off_time_);
  ESP_LOGCONFIG(TAG, "  Color Change Time: %ums", this->color_change_time_);
  ESP_LOGCONFIG(TAG, "  Reset Sequence:");
  for (const auto &step : this->reset_sequence_) {
    ESP_LOGCONFIG(TAG, "    %s for %ums", step.state ? "On" : "Off", step.duration);
  }
}

void PowerCyclePoolLightOutput::write_state(light::LightState *state) {
  if (this->is_changing_colors_)
    return;

  bool target_state;
  this->state_->current_values_as_binary(&target_state);

  if (!target_state) {
    this->cancel_timeout("TURN_ON");
    this->set_output_state_(false);
    return;
  }

  // These lights advance to the next color when power is cut and restored quickly, so
  // don't turn the output back on until it has been off long enough for the light to
  // treat it as a real power off rather than a color change cycle.
  if (!this->output_state_) {
    uint32_t off_for = millis() - this->output_off_at_;
    if (off_for < this->min_off_time_) {
      uint32_t remaining = this->min_off_time_ - off_for;
      ESP_LOGD(TAG, "Delaying turn on by %ums to satisfy the minimum off time", remaining);
      this->set_timeout("TURN_ON", remaining, [this]() { this->write_state(this->state_); });
      return;
    }
  }

  this->set_output_state_(true);

  uint32_t effect_index = this->state_->get_current_effect_index();
  uint8_t current_color = this->get_current_color_();
  if (effect_index == 0) {
    // If the light was turned on without specifying an effect, set the effect to match the current color
    auto call = this->state_->turn_on();
    call.set_effect(current_color);
    call.perform();
  } else if (this->get_target_color_() != current_color) {
    this->is_changing_colors_ = true;
    this->set_timeout("COLOR_CHANGE", this->color_change_time_, [this]() { this->color_change_off_(); });
  }
}

void PowerCyclePoolLightOutput::set_output_state_(bool state) {
  if (this->output_state_ && !state) {
    this->output_off_at_ = millis();
  }
  this->output_state_ = state;
  this->output_->set_state(state);
}

void PowerCyclePoolLightOutput::color_change_off_() {
  this->set_output_state_(false);
  this->set_timeout("COLOR_CHANGE", this->color_change_time_, [this]() { this->color_change_on_(); });
}

void PowerCyclePoolLightOutput::color_change_on_() {
  this->set_output_state_(true);
  uint8_t old_color = this->get_current_color_();
  uint8_t new_color = old_color < this->color_count_ ? old_color + 1 : 1;
  this->set_current_color_(new_color);
  uint8_t target_color = this->get_target_color_();
  ESP_LOGV(TAG, "Color change step: current=%u, target=%u", new_color, target_color);

  if (new_color != target_color) {
    this->set_timeout("COLOR_CHANGE", this->color_change_time_, [this]() { this->color_change_off_(); });
  } else {
    bool target_state;
    this->state_->current_values_as_binary(&target_state);
    if (target_state) {
      this->is_changing_colors_ = false;
    } else {
      this->set_timeout("COLOR_CHANGE", this->color_change_time_, [this]() {
        this->is_changing_colors_ = false;
        this->write_state(this->state_);
      });
    }
  }
}

uint8_t PowerCyclePoolLightOutput::get_current_color_() {
  if (this->current_color_ < 1 || this->current_color_ > this->color_count_) {
    this->set_current_color_(1);
  }
  return this->current_color_;
}

void PowerCyclePoolLightOutput::set_current_color_(uint8_t color) {
  this->current_color_ = color;
  this->color_pref_.save(&this->current_color_);
}

uint8_t PowerCyclePoolLightOutput::get_target_color_() {
  if (this->state_ == nullptr)
    return this->get_current_color_();
  uint32_t index = this->state_->get_current_effect_index();
  if (index >= 1 && index <= this->color_count_)
    return static_cast<uint8_t>(index);
  return this->get_current_color_();
}

void PowerCyclePoolLightOutput::color_reset() {
  if (this->is_changing_colors_) {
    ESP_LOGW(TAG, "Color reset requested while a color change is already in progress; ignoring.");
    return;
  }

  // Occasionally the current color can get out of sync with the physical light. To fix this we run the
  // reset sequence from the light's user guide, which puts the physical light back on the first color,
  // and then step the light back to the color it is supposed to be showing.
  ESP_LOGI(TAG, "Starting color reset (target color: %u)", this->get_target_color_());
  this->cancel_timeout("COLOR_CHANGE");
  this->cancel_timeout("TURN_ON");
  this->is_changing_colors_ = true;
  this->reset_step_index_ = 0;
  this->run_reset_step_();
}

void PowerCyclePoolLightOutput::run_reset_step_() {
  if (this->reset_step_index_ < this->reset_sequence_.size()) {
    const ResetStep &step = this->reset_sequence_[this->reset_step_index_++];
    this->set_output_state_(step.state);
    this->set_timeout("COLOR_RESET", step.duration, [this]() { this->run_reset_step_(); });
    return;
  }

  // The sequence leaves the physical light on the first color; write_state puts the output
  // back where the light state says it should be and steps to the target color if needed.
  this->set_current_color_(1);
  this->is_changing_colors_ = false;
  this->write_state(this->state_);
}

}  // namespace esphome::power_cycle_pool_light
