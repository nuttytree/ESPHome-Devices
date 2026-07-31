#pragma once

#include "pump_switch.h"

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace pool_controller {

/// Problem sensor: true while the pump is commanded on but no current is detected.
class PumpNoCurrentBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

/// Problem sensor: true while the current sensor has stopped publishing.
///
/// An ESPHome sensor keeps its last value forever when its source stops responding, so a dead
/// meter is indistinguishable from a genuine reading — a stalled sensor reporting 0 A looks
/// exactly like a pump that isn't drawing current. Everything downstream then acts on an hour-old
/// number: the no-current sensor cries fault, runtime stops accumulating, and anomaly detection
/// measures noise. This sensor makes the difference visible and gates those consumers.
class PumpCurrentStaleBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

}  // namespace pool_controller
}  // namespace esphome
