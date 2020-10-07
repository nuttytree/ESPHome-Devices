#pragma once
#include "esphome.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/uart/uart.h"

class GosundSW2Light : public Component, public uart::UARTDevice, public light::LightOutput {
  public:
    GosundSW2Light(UARTComponent *uart, BinaryOutput *indicator) : UARTDevice(uart) { indicator_ = indicator; }
    void setup() override {}
    void loop() override;
    LightTraits get_traits() override;
    void setup_state(light::LightState *state) override  { state_ = state; }
    void write_state(light::LightState *state) override;
  
  protected:
    light::LightState *state_;
    output::BinaryOutput *indicator_;
};

LightTraits GosundSW2Light::get_traits() {
  auto traits = LightTraits();
  traits.set_supports_brightness(true);
  return traits;
}

void GosundSW2Light::write_state(light::LightState *state) {
  float brightness;
  state->current_values.as_brightness(&brightness);
  uint8_t command;

  if (brightness == 0.0f) {
    command = 0;
    indicator_->turn_off();
  }
  else {
    command = (127.0 * brightness) + 0x80;
    indicator_->turn_on();
  }

  this->write_byte(command);
}

void GosundSW2Light::loop() {
  if (this->available() >= 5) {
    uint8_t data[5];
    if (this->read_array(data, 5)) {
      auto brightness = float(data[1] == 0 ? 1 : data[1] > 100 ? 100 : data[1]) / 100.0;
      auto call = this->state_->make_call();
      call.set_brightness(brightness);
      call.set_transition_length(0);
      call.perform();
    }
  }
}
