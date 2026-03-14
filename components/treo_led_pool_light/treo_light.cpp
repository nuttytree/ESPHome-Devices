#include "treo_light.h"

#include "esphome/core/log.h"

namespace esphome::treo_light {

static const char *const TAG = "treo_led_pool_light";
static constexpr uint16_t COLOR_CHANGE_ON_OFF_TIME = 200;

light::LightTraits TreoPoolLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void TreoPoolLightOutput::setup() {
  register_service(&TreoPoolLightOutput::color_reset, "color_reset");
}

void TreoPoolLightOutput::setup_state(light::LightState *state) {
  auto *effect_slow = new TreoPoolLightEffect("Slow Change");    // NOLINT
  auto *effect_white = new TreoPoolLightEffect("White");         // NOLINT
  auto *effect_blue = new TreoPoolLightEffect("Blue");           // NOLINT
  auto *effect_green = new TreoPoolLightEffect("Green");         // NOLINT
  auto *effect_red = new TreoPoolLightEffect("Red");             // NOLINT
  auto *effect_amber = new TreoPoolLightEffect("Amber");         // NOLINT
  auto *effect_magenta = new TreoPoolLightEffect("Magenta");     // NOLINT
  auto *effect_fast = new TreoPoolLightEffect("Fast Change");    // NOLINT
  state->add_effects({effect_slow, effect_white, effect_blue, effect_green, effect_red,
                      effect_amber, effect_magenta, effect_fast});
  this->state_ = state;

  this->color_pref_ = this->state_->make_entity_preference<uint32_t>();
  if (this->color_pref_.load(&this->current_color_)) {
    ESP_LOGD(TAG, "Restored color %u from flash", this->current_color_);
  } else {
    this->set_current_color_(1);
  }

  // Set hardware to match the restored state from ESPHome
  bool restored_on = false;
  state->current_values_as_binary(&restored_on);
  this->output_->set_state(restored_on);
}

void TreoPoolLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Treo LED Pool Light:");
  ESP_LOGCONFIG(TAG, "  Current Color: %u", this->get_current_color_());
}

void TreoPoolLightOutput::write_state(light::LightState *state) {
  if (this->is_changing_colors_)
    return;

  bool target_state;
  this->state_->current_values_as_binary(&target_state);
  this->output_->set_state(target_state);
  
  if (target_state) {
    int effect_index = this->state_->get_current_effect_index();
    int current_color = this->get_current_color_();
    if (effect_index == 0) {
      // If the light was turned on without specifying an effect, set the effect to match the current color
      auto call = this->state_->turn_on();
      call.set_effect(current_color);
      call.perform();
    } else if (get_target_color_() != current_color) {
      this->is_changing_colors_ = true;
      this->set_timeout("COLOR_CHANGE", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_off_(); });
    }
  }
}

void TreoPoolLightOutput::color_change_off_() {
  this->output_->set_state(false);
  this->set_timeout("COLOR_CHANGE", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_on_(); });
}

void TreoPoolLightOutput::color_change_on_() {
  this->output_->set_state(true);
  int old_color = this->get_current_color_();
  int new_color = old_color < 8 ? old_color + 1 : 1;
  this->set_current_color_(new_color);
  int target_color = get_target_color_();
  ESP_LOGV(TAG, "Color change step: current=%u, target=%u", new_color, target_color);

  if (new_color != target_color) {
    this->set_timeout("COLOR_CHANGE", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_off_(); });
  } else {
    bool target_state;
    this->state_->current_values_as_binary(&target_state);
    if (target_state) {
      this->is_changing_colors_ = false;
    } else {
      this->set_timeout("COLOR_CHANGE", COLOR_CHANGE_ON_OFF_TIME, [this]() { 
        this->is_changing_colors_ = false;
        this->write_state(this->state_);
      });
    }
  }
}

void TreoPoolLightOutput::next_color() {
  if (this->state_ != nullptr) {
    auto call = this->state_->turn_on();
    int current_color = this->get_current_color_();
    call.set_effect(current_color < 8 ? current_color + 1 : 1);
    call.perform();
  }
}

uint8_t TreoPoolLightOutput::get_current_color_() {
  if (this->current_color_ < 1 || this->current_color_ > 8) {
    set_current_color_(1);
  }
  return this->current_color_;
}

void TreoPoolLightOutput::set_current_color_(uint8_t color) {
  this->current_color_ = color;
  this->color_pref_.save(&this->current_color_);
}

int TreoPoolLightOutput::get_target_color_() {
  if (this->state_ == nullptr)
    return this->get_current_color_();
  int idx = this->state_->get_current_effect_index();
  if (idx >= 1 && idx <= 8) return idx;
  return this->get_current_color_();
}

void TreoPoolLightOutput::color_reset() {
  // Occasionally the current color can get out of sync with the physical light to fix this we do a reset
  // which sets the physical light back to color 1 and then update the color back to the "current" color.
  // The steps to accomplish this are:
  // * Ensure the light output is off
  // * Wait for 5.5 seconds
  // * Toggle the light on/off 3 times with a 250ms delay between each change
  // * Wait for 5.5 seconds
  // * Turn the light back on setting the color to the "current" color
  // Use the effect index from the state as the target color
  ESP_LOGI(TAG, "Starting color reset (target color: %u)", this->get_target_color_());
  this->cancel_timeout("COLOR_CHANGE");
  this->is_changing_colors_ = true;
  this->output_->set_state(false);

  this->set_timeout("COLOR_RESET_ON_1", 5500, [this]() {
    this->output_->set_state(true);

    this->set_timeout("COLOR_RESET_OFF_1", 250, [this]() {
      this->output_->set_state(false);
    });

    this->set_timeout("COLOR_RESET_ON_2", 500, [this]() {
      this->output_->set_state(true);
    });

    this->set_timeout("COLOR_RESET_OFF_2", 750, [this]() {
      this->output_->set_state(false);
    });

    this->set_timeout("COLOR_RESET_ON_3", 1000, [this]() {
      this->output_->set_state(true);
    });

    this->set_timeout("COLOR_RESET_OFF_3", 1250, [this]() {
      this->output_->set_state(false);
    });

    this->set_timeout("COLOR_RESET_FINISH", 6750, [this]() {
      this->set_current_color_(1);
      this->is_changing_colors_ = false;
      int target_color = this->get_target_color_();
      if (target_color != 1) {
        auto call = this->state_->turn_on();
        call.set_effect(target_color);
        call.perform();
      }
    });
  });
}

}  // namespace esphome::treo_light
