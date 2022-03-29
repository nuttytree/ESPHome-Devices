#include "treo_led_pool_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const uint16_t COLOR_CHANGE_ON_OFF_TIME = 200;

// Random 32bit value; If this changes existing restore color is invalidated
static const uint32_t RESTORE_COLOR_VERSION = 0x7B715952UL;

LightTraits TreoLedPoolLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void TreoLedPoolLight::setup() {
  this->pin_->setup();

  this->register_service(&TreoLedPoolLight::color_sync_reset, "color_sync_reset");
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

  this->rtc_ = global_preferences->make_preference<uint8_t>(state->get_object_id_hash() ^ RESTORE_COLOR_VERSION);
  this->rtc_.load(&this->current_color_);
  this->target_color_ = this->current_color_;

  this->state_ = state;
}

void TreoLedPoolLight::write_state(LightState *state) {
  if (!this->is_changing_colors_) {
    this->apply_state_();
  }
}

void TreoLedPoolLight::update() {
  if (this->light_wattage_.has_value() && this->power_sensor_ != nullptr && this->current_state_)
  {
    this->power_sensor_->publish_state(this->light_wattage_.value());
  }
}

void TreoLedPoolLight::next_color() {
  auto call = this->state_->turn_on();
  call.set_effect(this->current_color_ < 8 ? this->current_color_ + 1 : 1);
  call.perform();
}

void TreoLedPoolLight::color_sync_reset() {
  this->is_changing_colors_ = true;
  this->target_color_ = 1;
  this->pin_->digital_write(false);

  // After being off for at least 5 seconds we toggle on/off 3 times 
  // and then wait at least 5 seconds before allowing any other changes.
  this->set_timeout("COLOR_RESET_ON_1", 5500, [this]() { 
    this->pin_->digital_write(true);
    this->set_timeout("COLOR_RESET_OFF_1", 250 * 1, [this]() { this->pin_->digital_write(false); });
    
    this->set_timeout("COLOR_RESET_ON_2", 500, [this]() { this->pin_->digital_write(true); });
    this->set_timeout("COLOR_RESET_OFF_2", 750, [this]() { this->pin_->digital_write(false); });
    
    this->set_timeout("COLOR_RESET_ON_3", 1000, [this]() { this->pin_->digital_write(true); });
    this->set_timeout("COLOR_RESET_OFF_3", 1250, [this]() { this->pin_->digital_write(false); });

    this->set_timeout("COLOR_RESET_FINISH", 6750, [this]() { 
      this->current_color_ = 1;
      this->rtc_.save(&this->current_color_);

      this->is_changing_colors_ = false;
      
      bool curent_target_state;
      this->state_->current_values_as_binary(&curent_target_state);
      if (curent_target_state) {
        auto call = this->state_->turn_on();
        call.set_effect(1);
        call.perform();
      }
    });
  });
}

void TreoLedPoolLight::apply_state_() {
  bool target_state;
  this->state_->current_values_as_binary(&target_state);
  if (target_state != this->current_state_) {
    this->pin_->digital_write(target_state);
    this->current_state_ = target_state;
  }

  // The default when the light is turned on is to have no effect but the Treo lights always
  // have an "effect" of the current color so we update the effect to the current color.
  if (target_state && this->state_->get_effect_name() == "None") {
    auto call = this->state_->turn_on();
    call.set_effect(this->current_color_);
    call.perform();
  }

  if (this->light_wattage_.has_value() && this->power_sensor_ != nullptr)
  {
    float power = target_state ? this->light_wattage_.value() : 0.0f;
    this->power_sensor_->publish_state(power);
  }
}

void TreoLedPoolLight::set_color_(uint8_t color) {
  this->target_color_ = color;
  if (this->is_changing_colors_) {
    return;
  } else if (this->current_color_ != color) {
    this->is_changing_colors_ = true;
    this->pin_->digital_write(false);
    this->set_timeout("COLOR_CHANGE_ON", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_callback_(); });
  }
}

void TreoLedPoolLight::color_change_callback_() {
  this->pin_->digital_write(true); 
  this->current_color_ = this->current_color_ < 8 ? this->current_color_ + 1 : 1;
  this->rtc_.save(&this->current_color_);
  if (this->current_color_ != this->target_color_) {
    this->set_timeout("COLOR_CHANGE_OFF", COLOR_CHANGE_ON_OFF_TIME, [this]() { 
      this->pin_->digital_write(false);
      this->set_timeout("COLOR_CHANGE_ON", COLOR_CHANGE_ON_OFF_TIME, [this]() { this->color_change_callback_(); });
    });
  } else {
    this->is_changing_colors_ = false;
    this->apply_state_();
  }
}

}  // namespace light
}  // namespace esphome
