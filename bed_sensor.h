#include "esphome.h"

class BedSensor : public PollingComponent {
  private:
    const int melissaGpio = D0;
    const int chrisGpio = D1;
    const int sensorGpio = A0;
 
  public:
    BinarySensor *melissaInBed = new BinarySensor();
    BinarySensor *chrisInBed = new BinarySensor();
    BinarySensor *someoneInBed = new BinarySensor();

    BedSensor() : PollingComponent(5000) { }

    void setup() override {
      pinMode(melissaGpio, OUTPUT);
      pinMode(chrisGpio, OUTPUT);
    }

    void update() override {
      digitalWrite(melissaGpio, HIGH);
      digitalWrite(chrisGpio, LOW);
      int melissaValue = analogRead(sensorGpio);
      ESP_LOGD("bed_sensor_melissa", "The value of sensor is: %i", melissaValue);
      melissaInBed->publish_state(melissaValue < 300);

      digitalWrite(melissaGpio, LOW);
      digitalWrite(chrisGpio, HIGH);
      int chrisValue = analogRead(sensorGpio);
      ESP_LOGD("bed_sensor_chris", "The value of sensor is: %i", chrisValue);
      chrisInBed->publish_state(chrisValue < 300);

      someoneInBed->publish_state(!melissaInBed->state && !chrisInBed->state && melissaValue < 800 && chrisValue < 800);
    }
};