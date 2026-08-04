#pragma once

#include <vector>

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/output/binary_output.h"

namespace esphome::power_cycle_pool_light {

// One step of the reset sequence: hold the output in `state` for `duration` milliseconds.
struct ResetStep {
  bool state;
  uint32_t duration;
};

class PowerCyclePoolLightOutput : public light::LightOutput, public Component, public api::CustomAPIDevice {
 public:
  void set_output(output::BinaryOutput *output) { output_ = output; }
  void set_color_count(uint8_t color_count) { color_count_ = color_count; }
  void set_min_off_time(uint32_t min_off_time) { min_off_time_ = min_off_time; }
  void set_color_change_time(uint32_t color_change_time) { color_change_time_ = color_change_time; }
  void reserve_reset_steps(size_t count) { reset_sequence_.reserve(count); }
  void add_reset_step(bool state, uint32_t duration) { reset_sequence_.push_back(ResetStep{state, duration}); }

  light::LightTraits get_traits() override;
  void setup() override;
  void dump_config() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

  void color_reset();

 protected:
  void set_output_state_(bool state);
  void color_change_off_();
  void color_change_on_();
  void run_reset_step_();
  uint8_t get_current_color_();
  void set_current_color_(uint8_t color);
  uint8_t get_target_color_();

  output::BinaryOutput *output_ = nullptr;
  light::LightState *state_ = nullptr;
  ESPPreferenceObject color_pref_;
  std::vector<ResetStep> reset_sequence_;
  uint32_t min_off_time_ = 0;
  uint32_t color_change_time_ = 0;
  // millis() when the output last went from on to off, used to enforce min_off_time_
  uint32_t output_off_at_ = 0;
  size_t reset_step_index_ = 0;
  uint8_t color_count_ = 0;
  uint8_t current_color_ = 1;
  bool output_state_ = false;
  bool is_changing_colors_ = false;
};

class PowerCyclePoolLightEffect : public light::LightEffect {
 public:
  PowerCyclePoolLightEffect(const char *name) : LightEffect(name) {}
  void apply() override {}
};

}  // namespace esphome::power_cycle_pool_light
