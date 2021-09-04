#pragma once

#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "pool_select.h"
#include "pump_switch.h"

namespace esphome {
namespace pool_controller {

enum PumpMode : size_t {
  PUMP_MODE_OFF = 0,
  PUMP_MODE_NORMAL,
  PUMP_MODE_ALWAYS_EXCEPT_PEAK,
  PUMP_MODE_ALWAYS,
};

enum CleanerMode : size_t {
  CLEANER_MODE_OFF = 0,
  CLEANER_MODE_NORMAL,
  CLEANER_MODE_WHEN_PUMP_IS_ON,
};

class PoolController : public Component {
 public:
  PoolController();
  void set_time(time::RealTimeClock *time);
  void set_pump_switch(switch_::Switch *pump_switch);
  void set_cleaner_switch(switch_::Switch *cleaner_switch);
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;

 protected:
  void manage_pump_();
  void manage_cleaner_();

  time::RealTimeClock *time_;
  PoolSelect *pump_select_;
  PoolSelect *cleaner_select_;
  PumpSwitch *pump_switch_;
  PumpSwitch *cleaner_switch_;
  
  PumpMode pump_mode_;
  CleanerMode cleaner_mode_;
};

}  // namespace pool_controller
}  // namespace esphome
