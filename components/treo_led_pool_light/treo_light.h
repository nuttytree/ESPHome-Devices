#pragma once

#include "esphome/components/api/custom_api_device.h"
#include "esphome/core/preferences.h"
#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome::treo_light {

class TreoPoolLightEffect;

class TreoPoolLightOutput : public light::LightOutput, public Component, public api::CustomAPIDevice {
 public:
  void set_output(output::BinaryOutput *output) { output_ = output; }
  light::LightTraits get_traits() override;
  void setup() override;
  void dump_config() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

  void next_color();
  void color_reset();

 protected:
  friend class TreoPoolLightEffect;

  void apply_state_();
  void set_color_(uint8_t color);
  void color_change_callback_();

  output::BinaryOutput *output_ = nullptr;
  light::LightState *state_ = nullptr;
  uint8_t current_color_ = 1;
  uint8_t target_color_ = 1;
  bool is_changing_colors_ = false;
  ESPPreferenceObject color_pref_;
};


class TreoPoolLightEffect : public light::LightEffect {
 public:
  TreoPoolLightEffect(const char *name, TreoPoolLightOutput *light, uint8_t color)
      : LightEffect(name), light_(light), color_(color) {}
  void apply() override { this->light_->set_color_(this->color_); }

 protected:
  TreoPoolLightOutput *light_ = nullptr;
  uint8_t color_ = 1;
};

}  // namespace esphome::treo_light
