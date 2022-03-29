#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace gpio_power {

enum GPIOSwitchRestoreMode {
  GPIO_SWITCH_RESTORE_DEFAULT_OFF,
  GPIO_SWITCH_RESTORE_DEFAULT_ON,
  GPIO_SWITCH_ALWAYS_OFF,
  GPIO_SWITCH_ALWAYS_ON,
  GPIO_SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
  GPIO_SWITCH_RESTORE_INVERTED_DEFAULT_ON,
};

class GPIOSwitchWithPower : public switch_::Switch, public PollingComponent {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_restore_mode(GPIOSwitchRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }
  void set_interlock(const std::vector<Switch *> &interlock) { this->interlock_ = interlock; }
  void set_interlock_wait_time(uint32_t interlock_wait_time) { interlock_wait_time_ = interlock_wait_time; }
  void set_device_wattage(float device_wattage) { this->device_wattage_ = device_wattage; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void write_state(bool state) override;

  GPIOPin *pin_;
  GPIOSwitchRestoreMode restore_mode_{GPIO_SWITCH_RESTORE_DEFAULT_OFF};
  std::vector<Switch *> interlock_;
  uint32_t interlock_wait_time_{0};
  optional<float> device_wattage_{};
  sensor::Sensor *power_sensor_;
};

}  // namespace gpio_power
}  // namespace esphome
