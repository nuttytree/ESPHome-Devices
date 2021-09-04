#pragma once

#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace pool_controller {

class PumpSwitch : public switch_::Switch, public PollingComponent {
 public:
  PumpSwitch(switch_::Switch *physical_switch);
  void add_turn_on_check(std::function<bool()> &&turn_on_check) { this->turn_on_checks_.push_back(std::move(turn_on_check)); }
  void add_turn_off_check(std::function<bool()> &&turn_off_check) { this->turn_off_checks_.push_back(std::move(turn_off_check)); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void update() override;

  uint32_t get_current_on_time();
  uint32_t get_current_off_time();
  uint32_t get_runtime() { return this->runtime_; }
  void reset_runtime() { this->runtime_ = 0; }
  void set_is_disabled(bool is_disabled);
  void emergency_stop();
  
 protected:
  void write_state(bool state) override;

  void update_physical_switch_();
  
  switch_::Switch *physical_switch_;
  bool is_disabled_{false};
  uint32_t turned_on_at_{0};
  uint32_t turned_off_at_{0};
  uint32_t runtime_{0};
  uint32_t last_runtime_update_{0};
  std::vector<std::function<bool()>> turn_on_checks_;
  std::vector<std::function<bool()>> turn_off_checks_;
};

}  // namespace pool_controller
}  // namespace esphome
