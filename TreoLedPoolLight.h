#pragma once
#include "esphome.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_effect.h"

class TreoColorLightEffect;

class TreoLedPoolLight : public Component, public LightOutput {
  public:
    TreoLedPoolLight(int gpio);
    LightTraits get_traits() override;
    void setup_state(LightState *state) override;
    void write_state(LightState *state) override;
    void next_color();
    void reset();

 protected:
    friend TreoColorLightEffect;

    void set_color(int color);

    int relayGpio;
    int currentColor;
    ESPPreferenceObject rtc;
    LightState *parent;
    TreoColorLightEffect *effectSlow;
    TreoColorLightEffect *effectWhite;
    TreoColorLightEffect *effectBlue;
    TreoColorLightEffect *effectGreen;
    TreoColorLightEffect *effectRed;
    TreoColorLightEffect *effectAmber;
    TreoColorLightEffect *effectMagenta;
    TreoColorLightEffect *effectFast;
};

class TreoColorLightEffect : public LightEffect {
  public:
    TreoColorLightEffect(TreoLedPoolLight *treo, const std::string &name, const uint32_t color);
    void apply() override;

  protected:
    TreoLedPoolLight *treoLight;
    uint32_t effectColor;
};

TreoLedPoolLight::TreoLedPoolLight(int gpio) : LightOutput(), relayGpio(gpio) {}

LightTraits TreoLedPoolLight::get_traits() {
  auto traits = LightTraits();
  traits.set_supports_brightness(false);
  traits.set_supports_rgb(false);
  traits.set_supports_rgb_white_value(false);
  traits.set_supports_color_temperature(false);
  return traits;
}

void TreoLedPoolLight::setup_state(LightState *state) {
  pinMode(this->relayGpio, OUTPUT);
  this->rtc = global_preferences.make_preference<int>(1944399030U ^ 12345);
  this->rtc.load(&this->currentColor);

  this->parent = state;

  this->effectSlow = new TreoColorLightEffect(this, "Slow Change", 1);
  this->effectWhite = new TreoColorLightEffect(this, "White", 2);
  this->effectBlue = new TreoColorLightEffect(this, "Blue", 3);
  this->effectGreen = new TreoColorLightEffect(this, "Green", 4);
  this->effectRed = new TreoColorLightEffect(this, "Red", 5);
  this->effectAmber = new TreoColorLightEffect(this, "Amber", 6);
  this->effectMagenta = new TreoColorLightEffect(this, "Magenta", 7);
  this->effectFast = new TreoColorLightEffect(this, "Fast Change", 8);
  state->add_effects({ this->effectSlow, this->effectWhite , this->effectBlue , this->effectGreen , this->effectRed , this->effectAmber , this->effectMagenta , this->effectFast });
}

void TreoLedPoolLight::write_state(LightState *state) {
  bool currentState;
  state->current_values_as_binary(&currentState);
  digitalWrite(relayGpio, currentState);
  if (currentState && state->get_effect_name() == "None") {
    auto call = state->turn_on();
    call.set_effect(this->currentColor);
    call.perform();
  }
}

void TreoLedPoolLight::next_color() {
  auto call = this->parent->turn_on();
  call.set_effect(this->currentColor < 8 ? this->currentColor + 1 : 1);
  call.perform();
}

void TreoLedPoolLight::reset() {
  bool currentState;
  this->parent->current_values_as_binary(&currentState);
  digitalWrite(relayGpio, LOW);
  delay(5500);
  for (int i = 0; i < 3; i++) {
    digitalWrite(relayGpio, HIGH);
    delay(200);
    digitalWrite(relayGpio, LOW);
    delay(200);
  }
  delay(5500);
  this->currentColor = 1;
  this->rtc.save(&this->currentColor);
  auto call = this->parent->turn_on();
  call.set_effect(1);
  call.perform();
  call = this->parent->make_call();
  call.set_state(currentState);
  call.perform();
}

void TreoLedPoolLight::set_color(int color) {
  if (this->currentColor != color) {
    while (this->currentColor != color) {
      digitalWrite(this->relayGpio, LOW);
      delay(200);
      digitalWrite(this->relayGpio, HIGH);
      delay(200);
      this->currentColor = this->currentColor < 8 ? this->currentColor + 1 : 1;
    }
    this->rtc.save(&this->currentColor);
  }
}

TreoColorLightEffect::TreoColorLightEffect(TreoLedPoolLight *treo, const std::string &name, const uint32_t color)
  : LightEffect(name), treoLight(treo), effectColor(color) {}

void TreoColorLightEffect::apply() {
  this->treoLight->set_color(this->effectColor);
}

TreoLedPoolLight *Treo;