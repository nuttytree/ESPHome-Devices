#include "bed_sensor.h"

#include "esphome/core/log.h"

namespace esphome::bed_sensor {

static const char *const TAG = "bed.sensor";

void BedSensor::setup() {
  // Ensure outputs start in a known state
  if (this->side_one_output_ != nullptr)
    this->side_one_output_->set_state(false);
  if (this->side_two_output_ != nullptr)
    this->side_two_output_->set_state(false);
  this->combined_name_ = std::string(this->side_one_name_) + " and " + std::string(this->side_two_name_);
}

void BedSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Bed Sensor:");
  ESP_LOGCONFIG(TAG, "  Side One: %s", this->side_one_name_);
  ESP_LOGCONFIG(TAG, "  Side Two: %s", this->side_two_name_);
}

void BedSensor::update() {
  if (this->adc_sensor_ == nullptr || this->side_one_output_ == nullptr || this->side_two_output_ == nullptr)
    return;
  if (!this->last_updated_side_one_) {
    this->side_two_output_->set_state(false);
    this->side_one_output_->set_state(true);
    this->adc_sensor_->update();
    this->side_one_value_ = this->adc_sensor_->state;
    ESP_LOGD(TAG, "Side one value: %f", this->side_one_value_);
    this->side_one_value_sensor_->publish_state(this->side_one_value_);
    this->last_updated_side_one_ = true;
  } else {
    this->side_one_output_->set_state(false);
    this->side_two_output_->set_state(true);
    this->adc_sensor_->update();
    this->side_two_value_ = this->adc_sensor_->state;
    ESP_LOGD(TAG, "Side two value: %f", this->side_two_value_);
    this->side_two_value_sensor_->publish_state(this->side_two_value_);
    this->last_updated_side_one_ = false;
  }

  float side_one_percent = (1024 - this->side_one_value_) * 100 / 1024;
  ESP_LOGD(TAG, "Side one percent: %f", side_one_percent);
  bool side_one_in_bed = side_one_percent > 75;
  ESP_LOGD(TAG, "Side one state: %d", side_one_in_bed);
  this->side_one_sensor_->publish_state(side_one_in_bed);

  float side_two_percent = (1024 - this->side_two_value_) * 100 / 1024;
  ESP_LOGD(TAG, "Side two percent: %f", side_two_percent);
  bool side_two_in_bed = side_two_percent > 75;
  ESP_LOGD(TAG, "Side two state: %d", side_two_in_bed);
  this->side_two_sensor_->publish_state(side_two_in_bed);

  bool someone_in_bed = !side_one_in_bed && !side_two_in_bed && side_one_percent > 40 && side_two_percent > 40;
  this->someone_in_bed_->publish_state(someone_in_bed);

  int count = (side_one_in_bed ? 1 : 0) + (side_two_in_bed ? 1 : 0) + (someone_in_bed ? 1 : 0);
  this->count_->publish_state(count);

  if (count == 0) {
    this->status_->publish_state("Empty");
  } else if (count == 2) {
    this->status_->publish_state(this->combined_name_);
  } else if (side_one_in_bed) {
    this->status_->publish_state(this->side_one_name_);
  } else if (side_two_in_bed) {
    this->status_->publish_state(this->side_two_name_);
  } else if (someone_in_bed) {
    this->status_->publish_state(this->someone_name_);
  }
}

}  // namespace esphome::bed_sensor
