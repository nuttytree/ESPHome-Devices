#pragma once

#include "pump_switch.h"

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace pool_controller {

/// A Select entity whose options are the named schedules of a PumpSwitch.
/// Selecting an option sets the pump's active schedule index.
class ScheduleSelect : public select::Select, public Component {
 public:
  void set_pump_switch(PumpSwitch *pump) { this->pump_ = pump; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void control(size_t index) override;

  PumpSwitch *pump_{nullptr};
  ESPPreferenceObject pref_;
};

}  // namespace pool_controller
}  // namespace esphome
