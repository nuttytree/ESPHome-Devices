#pragma once

#include "pump_switch.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"

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

/// Publishes the reason string of the most recent anomaly (see PumpAnomalyTrigger below),
/// so a single anomaly_status_binary_sensor `on` state can be disambiguated in the UI/history.
/// PumpSwitch pushes the reason into this sensor directly via set_anomaly_reason_sensor();
/// unlike the other anomaly entities it has no need to reach back to the pump.
class PumpAnomalyReasonTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void setup() override;
};

/// Automation trigger fired when a current-draw anomaly is detected.
/// The trigger argument `x` is one of:
///   "CURRENT_HIGH"     – steady-state current exceeds baseline by threshold_pct
///   "CURRENT_LOW"      – steady-state current is below baseline by threshold_pct
///   "NO_STARTUP_SPIKE" – inrush peak absent on start (possible capacitor fault)
///   "BASELINE_DRIFT"   – slow drift away from baseline in either direction (bearing wear
///                        if rising, prime/flow loss if falling)
///   "VARIANCE_SPIKE"   – live current spread has grown well beyond its learned baseline
///                        stdev; catches instability (e.g. air ingestion) before the mean
///                        current has moved enough to trip CURRENT_LOW/HIGH
class PumpAnomalyTrigger : public Trigger<std::string> {
 public:
  explicit PumpAnomalyTrigger(PumpSwitch *parent) { parent->set_anomaly_trigger(this); }
};

}  // namespace pool_controller
}  // namespace esphome
