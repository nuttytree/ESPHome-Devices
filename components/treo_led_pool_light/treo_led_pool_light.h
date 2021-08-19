#pragma once

#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace light {

class TreoColorLightEffect;

class TreoLedPoolLight : public LightOutput, public PollingComponent, public api::CustomAPIDevice {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_light_wattage(float light_wattage) { this->light_wattage_ = light_wattage; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  LightTraits get_traits() override;
  void setup() override;
  void setup_state(LightState *state) override;
  void write_state(LightState *state) override;
  void update() override;

  void next_color();
  void color_sync_reset();

  TreoLedPoolLight& operator= (const LightOutput& x) {return *this;}

 protected:
  friend TreoColorLightEffect;

  void apply_state_();
  void set_color_(uint8_t color);
  void color_change_callback_();

  GPIOPin *pin_;
  ESPPreferenceObject rtc_;
  LightState *state_{nullptr};
  optional<float> light_wattage_{};
  sensor::Sensor *power_sensor_;
  bool current_state_;
  uint8_t current_color_;
  uint8_t target_color_;
  bool is_changing_colors_{false};
};

class TreoColorLightEffect : public LightEffect {
  public:
    TreoColorLightEffect(const std::string &name, TreoLedPoolLight *treo_light, const uint8_t color)
      : LightEffect(name), treo_light_(treo_light), color_(color) {}
    void apply() override { this-> treo_light_->set_color_(this->color_); }

  protected:
    TreoLedPoolLight *treo_light_{nullptr};
    uint8_t color_;
};

}  // namespace light
}  // namespace esphome
