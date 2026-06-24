#pragma once

#include <string>

#include "esphome/core/component.h"

#include "esphome/components/adc/adc_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bed_sensor {

class BedSensor : public PollingComponent {
 public:
  void set_adc_sensor(adc::ADCSensor *adc_sensor) { this->adc_sensor_ = adc_sensor; }

  void set_side_one_output(output::BinaryOutput *side_one_output) { this->side_one_output_ = side_one_output; }
  void set_side_one_name(const char *side_one_name) { this->side_one_name_ = side_one_name; }
  void set_side_one_value_sensor(sensor::Sensor *side_one_value_sensor) {
    this->side_one_value_sensor_ = side_one_value_sensor;
  }
  void set_side_one_sensor(binary_sensor::BinarySensor *side_one_sensor) {
    this->side_one_sensor_ = side_one_sensor;
  }

  void set_side_two_output(output::BinaryOutput *side_two_output) { this->side_two_output_ = side_two_output; }
  void set_side_two_name(const char *side_two_name) { this->side_two_name_ = side_two_name; }
  void set_side_two_value_sensor(sensor::Sensor *side_two_value_sensor) {
    this->side_two_value_sensor_ = side_two_value_sensor;
  }
  void set_side_two_sensor(binary_sensor::BinarySensor *side_two_sensor) {
    this->side_two_sensor_ = side_two_sensor;
  }

  void set_someone_sensor(binary_sensor::BinarySensor *someone_in_bed) { this->someone_in_bed_ = someone_in_bed; }
  void set_someone_name(const char *someone_name) { this->someone_name_ = someone_name; }

  void set_count_sensor(sensor::Sensor *count) { this->count_ = count; }

  void set_status_sensor(text_sensor::TextSensor *status) { this->status_ = status; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  adc::ADCSensor *adc_sensor_{nullptr};

  output::BinaryOutput *side_one_output_{nullptr};
  const char *side_one_name_{nullptr};
  sensor::Sensor *side_one_value_sensor_{nullptr};
  binary_sensor::BinarySensor *side_one_sensor_{nullptr};

  output::BinaryOutput *side_two_output_{nullptr};
  const char *side_two_name_{nullptr};
  sensor::Sensor *side_two_value_sensor_{nullptr};
  binary_sensor::BinarySensor *side_two_sensor_{nullptr};

  binary_sensor::BinarySensor *someone_in_bed_{nullptr};
  const char *someone_name_{nullptr};

  sensor::Sensor *count_{nullptr};
  text_sensor::TextSensor *status_{nullptr};

  bool last_updated_side_one_{false};
  std::string combined_name_;
  float side_one_value_{1024.0f};
  float side_two_value_{1024.0f};
};

}  // namespace esphome::bed_sensor
