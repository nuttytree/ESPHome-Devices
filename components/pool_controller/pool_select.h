#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace pool_controller {

class PoolSelect : public select::Select, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  
 protected:
  void control(const std::string &value) override;

  ESPPreferenceObject pref_;
};

}  // namespace pool_controller
}  // namespace esphome
