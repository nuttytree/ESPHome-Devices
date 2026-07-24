#pragma once

#include "pump_switch.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace pool_controller {

class PumpAnomalySwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void write_state(bool state) override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

class PumpAnomalyStatusBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

class PumpAnomalyResetButton : public button::Button, public Component {
 public:
  void press_action() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

/// Automation trigger fired when a current-draw anomaly is detected.
/// The trigger argument `x` is one of:
///   "CURRENT_HIGH"     – steady-state current exceeds baseline by threshold_pct
///   "CURRENT_LOW"      – steady-state current is below baseline by threshold_pct
///   "NO_STARTUP_SPIKE" – inrush peak absent on start (possible capacitor fault)
///   "BASELINE_DRIFT"   – slow upward drift detected, suggesting bearing wear
class PumpAnomalyTrigger : public Trigger<std::string> {
 public:
  explicit PumpAnomalyTrigger(PumpSwitch *parent) { parent->set_anomaly_trigger(this); }
};

}  // namespace pool_controller
}  // namespace esphome
