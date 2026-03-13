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

void TreoPoolLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Treo LED Pool Light:");
  ESP_LOGCONFIG(TAG, "  Current Color: %u", this->current_color_);
}

void TreoPoolLightOutput::setup_state(light::LightState *state) {
  auto *effect_slow = new TreoPoolLightEffect("Slow Change", this, 1);    // NOLINT
  auto *effect_white = new TreoPoolLightEffect("White", this, 2);         // NOLINT
  auto *effect_blue = new TreoPoolLightEffect("Blue", this, 3);           // NOLINT
  auto *effect_green = new TreoPoolLightEffect("Green", this, 4);         // NOLINT
  auto *effect_red = new TreoPoolLightEffect("Red", this, 5);             // NOLINT
  auto *effect_amber = new TreoPoolLightEffect("Amber", this, 6);         // NOLINT
  auto *effect_magenta = new TreoPoolLightEffect("Magenta", this, 7);     // NOLINT
  auto *effect_fast = new TreoPoolLightEffect("Fast Change", this, 8);    // NOLINT
  state->add_effects({effect_slow, effect_white, effect_blue, effect_green, effect_red,
                      effect_amber, effect_magenta, effect_fast});
  this->state_ = state;

  // Persistent color storage
  uint32_t key = state->get_object_id_hash() ^ 0x7B715952UL;
  this->color_pref_ = global_preferences->make_preference<uint8_t>(key);
  this->color_pref_.load(&this->current_color_);
  this->target_color_ = this->current_color_;
  ESP_LOGD(TAG, "Restored color: %u", this->current_color_);
}

void TreoPoolLightOutput::write_state(light::LightState *state) {
  if (!this->is_changing_colors_) {
    this->apply_state_();
  }
}

void TreoPoolLightOutput::next_color() {
  if (this->state_ != nullptr) {
    auto call = this->state_->turn_on();
    call.set_effect(this->current_color_ < 8 ? this->current_color_ + 1 : 1);
    call.perform();
  }
}

void TreoPoolLightOutput::apply_state_() {
  if (this->state_ == nullptr || this->output_ == nullptr)
    return;
  bool target_state;
  this->state_->current_values_as_binary(&target_state);
  this->output_->set_state(target_state);
  // The default when the light is turned on is to have no effect but the Treo lights always
  // have an "effect" of the current color so we update the effect to the current color.
  if (target_state && this->state_->get_effect_name() == "None") {
    auto call = this->state_->turn_on();
    call.set_effect(this->current_color_);
    call.perform();
  }
}

void TreoPoolLightOutput::set_color_(uint8_t color) {
  this->target_color_ = color;
  if (this->is_changing_colors_) {
    return;
  }
  if (this->current_color_ != color) {
    ESP_LOGD(TAG, "Changing color from %u to %u", this->current_color_, color);
    this->is_changing_colors_ = true;
    this->output_->set_state(false);
    this->set_timeout("COLOR_CHANGE", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_callback_(); });
  }
}

void TreoPoolLightOutput::color_change_callback_() {
  this->output_->set_state(true);
  this->current_color_ = this->current_color_ < 8 ? this->current_color_ + 1 : 1;
  this->color_pref_.save(&this->current_color_);
  ESP_LOGV(TAG, "Color change step: current=%u, target=%u", this->current_color_, this->target_color_);
  if (this->current_color_ != this->target_color_) {
    this->set_timeout("COLOR_CHANGE_OFF", COLOR_CHANGE_ON_OFF_TIME, [this]() {
      this->output_->set_state(false);
      this->set_timeout("COLOR_CHANGE", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_callback_(); });
    });
  } else {
    this->is_changing_colors_ = false;
    this->apply_state_();
  }
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
  ESP_LOGI(TAG, "Starting color reset (current color: %u, target color: %u)", this->current_color_,
           this->target_color_);
  this->cancel_timeout("COLOR_CHANGE");
  this->cancel_timeout("COLOR_CHANGE_OFF");
  this->is_changing_colors_ = true;
  if (this->output_ != nullptr)
    this->output_->set_state(false);

  this->set_timeout("COLOR_RESET_ON_1", 5500, [this]() {
    if (this->output_ != nullptr)
      this->output_->set_state(true);
    this->set_timeout("COLOR_RESET_OFF_1", 250, [this]() {
      if (this->output_ != nullptr)
        this->output_->set_state(false);
    });

    this->set_timeout("COLOR_RESET_ON_2", 500, [this]() {
      if (this->output_ != nullptr)
        this->output_->set_state(true);
    });
    this->set_timeout("COLOR_RESET_OFF_2", 750, [this]() {
      if (this->output_ != nullptr)
        this->output_->set_state(false);
    });

    this->set_timeout("COLOR_RESET_ON_3", 1000, [this]() {
      if (this->output_ != nullptr)
        this->output_->set_state(true);
    });
    this->set_timeout("COLOR_RESET_OFF_3", 1250, [this]() {
      if (this->output_ != nullptr)
        this->output_->set_state(false);
    });

    this->set_timeout("COLOR_RESET_FINISH", 6750, [this]() {
      this->current_color_ = 1;
      this->color_pref_.save(&this->current_color_);
      this->is_changing_colors_ = false;
      ESP_LOGI(TAG, "Color reset complete, resuming at color %u", this->target_color_);

      if (this->state_ != nullptr) {
        auto call = this->state_->make_call();
        call.set_state(true);
        call.set_effect(this->target_color_);
        call.perform();
      }
    });
  });
}

}  // namespace esphome::treo_light
