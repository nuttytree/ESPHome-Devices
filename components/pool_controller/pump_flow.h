#pragma once

#include "pump_switch.h"

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace pool_controller {

/// Problem sensor: latches true when the pump is shut down due to lost flow; only clears once
/// the pump is on, current confirms the motor is running, and flow is detected again.
class PumpFlowLossBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

/// Problem sensor: true when flow is detected while the pump is commanded off, debounced by
/// flow_timeout to allow for residual flow immediately after shutdown.
class PumpUnexpectedFlowBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

}  // namespace pool_controller
}  // namespace esphome
