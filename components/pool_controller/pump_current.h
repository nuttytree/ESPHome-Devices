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

}  // namespace pool_controller
}  // namespace esphome
