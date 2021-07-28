#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace light {

class TreoLedPoolLight : public Component, public light::LightOutput {
  public:
    void set_gpio(int gpio) { this->gpio_ = gpio; }

 protected:
    int gpio_;
};

}  // namespace light
}  // namespace esphome
