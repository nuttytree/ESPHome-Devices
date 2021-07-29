#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace light {

class TreoColorLightEffect;

class TreoLedPoolLight : public Component, public LightOutput {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  LightTraits get_traits() override;
  void setup() override;
  void setup_state(LightState *state) override;
  void write_state(LightState *state) override;

 protected:
  friend TreoColorLightEffect;

  void set_color_(uint8_t color, bool is_recursive = false);

  GPIOPin *pin_;
  ESPPreferenceObject rtc_;
  uint8_t current_color_;
  LightState *state_{nullptr};
  optional<LightState> desired_state_{};
  bool is_changing_colors_{false};
};

class TreoColorLightEffect : public light::LightEffect {
  public:
    TreoColorLightEffect(const std::string &name, TreoLedPoolLight *treo_light, const uint8_t color)
      : LightEffect(name), treo_light_(treo_light), color_(color) {}
    void apply() override { this-> treo_light_->set_color_(this->color_); }

  protected:
    TreoLedPoolLight *treo_light_{nullptr};
    uint8_t color_;
};

}  // namespace light
}  // namespace esphome
