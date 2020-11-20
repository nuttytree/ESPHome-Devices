#include "esphome.h"

using namespace esphome;

class BedSensor : public PollingComponent {
  private:
    const int melissaGpio = D0;
    const int chrisGpio = D1;
    const int sensorGpio = A0;
 
  public:
    Sensor *melissaBedSensor = new Sensor();
    Sensor *chrisBedSensor = new Sensor();

    BedSensor() : PollingComponent(5000) { }

    void setup() override {
      pinMode(melissaGpio, OUTPUT);
      pinMode(chrisGpio, OUTPUT);
    }

    void update() override {
      digitalWrite(melissaGpio, HIGH);
      digitalWrite(chrisGpio, LOW);
      int melissaValue = analogRead(sensorGpio);

      digitalWrite(melissaGpio, LOW);
      digitalWrite(chrisGpio, HIGH);
      int chrisValue = analogRead(sensorGpio);

      melissaBedSensor->publish_state(melissaValue);
      chrisBedSensor->publish_state(chrisValue);
    }
};