#include "esphome/core/log.h"
#include "treo_led_pool_light.h"

namespace esphome {
namespace light {

static const char *TAG = "light.treo_pool";

LightTraits TreoLedPoolLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supports_brightness(false);
  traits.set_supports_rgb(false);
  traits.set_supports_rgb_white_value(false);
  traits.set_supports_color_temperature(false);
  return traits;
}

void TreoLedPoolLight::setup() {
  this->pin_->setup();
  this->rtc_ = global_preferences.make_preference<uint8_t>(1944399030U ^ 12345);
  this->rtc_.load(&this->current_color_);
}

void TreoLedPoolLight::setup_state(light::LightState *state) {
  auto *effectSlow = new TreoColorLightEffect("Slow Change", this, 1);
  auto *effectWhite = new TreoColorLightEffect("White", this, 2);
  auto *effectBlue = new TreoColorLightEffect("Blue", this, 3);
  auto *effectGreen = new TreoColorLightEffect("Green", this, 4);
  auto *effectRed = new TreoColorLightEffect("Red", this, 5);
  auto *effectAmber = new TreoColorLightEffect("Amber", this, 6);
  auto *effectMagenta = new TreoColorLightEffect("Magenta", this, 7);
  auto *effectFast = new TreoColorLightEffect("Fast Change", this, 8);
  state->add_effects({ effectSlow, effectWhite , effectBlue , effectGreen , effectRed , effectAmber , effectMagenta , effectFast });

  this->state_ = state;
}

void TreoLedPoolLight::write_state(LightState *state) {
  if (this->is_changing_colors_) {
    return;
  }

  bool currentState;
  state->current_values_as_binary(&currentState);

  this->pin_->digital_write(currentState);

  if (currentState && state->get_effect_name() == "None") {
    auto call = state->turn_on();
    call.set_effect(this->current_color_);
    call.perform();
  }
}

void TreoLedPoolLight::set_color_(uint8_t color, bool is_recursive) {
  if (this->current_color_ != color) {
    this->is_changing_colors_ = true;
    this->pin_->digital_write(false);
    this->set_timeout("ON", 200, [this]() { 
      this->pin_->digital_write(true); 
      this->current_color_ = this->current_color_ < 8 ? this->current_color_ + 1 : 1;
    });
    this->set_timeout("NEXT_COLOR", 400, [this, color]() { this->set_color_(color, true); });
  } else if (is_recursive) {
    this->rtc_.save(&this->current_color_);
    this->is_changing_colors_ = false;
    if (this->desired_state_.has_value())
    {
      this->write_state(*this->desired_state_);
      this->desired_state_.reset();
    }
  }
}

}  // namespace light
}  // namespace esphome
