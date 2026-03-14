#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/output/binary_output.h"

namespace esphome::treo_light {

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
  void color_change_off_();
  void color_change_on_();
  uint8_t get_current_color_();
  void set_current_color_(uint8_t color);
  int get_target_color_();

  output::BinaryOutput *output_ = nullptr;
  light::LightState *state_ = nullptr;
  ESPPreferenceObject color_pref_;
  uint8_t current_color_ = 1;
  bool is_changing_colors_ = false;
};

class TreoPoolLightEffect : public light::LightEffect { // Commented out for clarity
 public:
  TreoPoolLightEffect(const char *name) : LightEffect(name) {}
  void apply() override { }
};

}  // namespace esphome::treo_light
