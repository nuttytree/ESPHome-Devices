#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "garage_fridge.h"

namespace esphome {
namespace garage_fridge {

static const char *const TAG = "garage_fridge";

static const uint32_t UPDATE_INTERVAL_MS = 10000;
static const uint32_t SAMPLE_WINDOW_MINUTES = 10;
static const uint32_t TREND_TEMPERATURE_MINUTES = 2;

static const uint32_t SAMPLE_TEMPERATURE_READINGS_COUNT = SAMPLE_WINDOW_MINUTES * 60 * 1000 / UPDATE_INTERVAL_MS;
static const uint32_t TREND_TEMPERATURE_READINGS_COUNT = TREND_TEMPERATURE_MINUTES * 60 * 1000 / UPDATE_INTERVAL_MS;

GarageFridge::GarageFridge() {
  this->set_update_interval(UPDATE_INTERVAL_MS);

  this->fridge_pid_ = new pid::PIDClimate();
  this->fridge_pid_->set_component_source("garage.fridge");
  App.register_component(this->fridge_pid_);
  App.register_climate(this->fridge_pid_);
  this->fridge_pid_->set_internal(true);

  this->freezer_pid_ = new pid::PIDClimate();
  this->freezer_pid_->set_component_source("garage.fridge");
  App.register_component(this->freezer_pid_);
  App.register_climate(this->freezer_pid_);
  this->freezer_pid_->set_internal(true);

  this->fridge_heater_sensor_ = new sensor::Sensor();
  App.register_sensor(this->fridge_heater_sensor_);
  this->fridge_heater_sensor_->set_accuracy_decimals(0);
  this->fridge_heater_sensor_->set_icon("mdi:radiator");
  this->fridge_heater_sensor_->set_state_class(sensor::StateClass::STATE_CLASS_MEASUREMENT);
  this->fridge_heater_sensor_->set_unit_of_measurement("%");

  this->fridge_output_ = new GarageFridgeHeatOutput();
  this->fridge_output_->set_component_source("garage.fridge");
  App.register_component(this->fridge_output_);
  this->fridge_output_->set_sensor(this->fridge_heater_sensor_);

  this->freezer_output_ = new GarageFridgeHeatOutput();
  this->freezer_output_->set_component_source("garage.fridge");
  App.register_component(this->freezer_output_);
  this->freezer_output_->set_sensor(this->fridge_heater_sensor_);
}

void GarageFridge::set_fridge_heat_output(output::FloatOutput *heat_output) {
  this->fridge_output_->set_physical_output(heat_output);
  this->freezer_output_->set_physical_output(heat_output);
}

void GarageFridge::set_fridge_sensor(sensor::Sensor *sensor) { 
  this->fridge_pid_->set_sensor(sensor);
  this->freezer_pid_->set_sensor(sensor);
}

void GarageFridge::setup() {
  auto freezer_call = this->freezer_pid_->make_call();
  freezer_call.set_target_temperature(this->cool_trigger_temp_);
  freezer_call.set_mode(climate::CLIMATE_MODE_HEAT);
  freezer_call.perform();

  auto fridge_call = this->fridge_pid_->make_call();
  fridge_call.set_target_temperature(this->fridge_min_temp_);
  fridge_call.set_mode(climate::CLIMATE_MODE_HEAT);
  fridge_call.perform();

  this->fridge_output_->set_active();
}

void GarageFridge::update() {
  float freezer_temp = this->freezer_sensor_->get_state();
  if (isnan(freezer_temp)) {
    return;
  }

  while (this->freezer_temps_.size() >= SAMPLE_TEMPERATURE_READINGS_COUNT) {
    this->freezer_temps_.pop_front();
  }
  this->freezer_temps_.push_back(freezer_temp);

  // We only make decisions based on a full sample window of temperatures
  if (this->freezer_temps_.size() < SAMPLE_TEMPERATURE_READINGS_COUNT) {
    return;
  }

  if (freezer_temp <= this->freezer_max_temp_) {
    // We are below the max temp so we are good
    this->set_freezer_needs_cooling(false);
  } else if (freezer_temp > this->freezer_max_temp_ + 10.0f) {
    // If the current temp is more than 10 degrees over the max temp then the door is probably open so heating the fridge will only make matters worse
    this->set_freezer_needs_cooling(false);
  } else if (!this->freezer_needs_cooling_) {
    float min = *(std::min_element(this->freezer_temps_.begin(), this->freezer_temps_.end()));
    if (min > this->freezer_max_temp_) {
      // If all values are over the max temp then we look at which way the temp is currently trending
      float trend = this->get_freezer_trend();
      if (trend > 0) {
        this->set_freezer_needs_cooling(true);
      }
    }
  }
}

void GarageFridge::set_freezer_needs_cooling(bool needs_cooling) {
  if (needs_cooling == this->freezer_needs_cooling_) {
    return;
  }

  this->freezer_needs_cooling_ = needs_cooling;
  if (needs_cooling) {
    this->fridge_output_->set_inactive();
    this->freezer_output_->set_active();
  } else {
    this->freezer_output_->set_inactive();
    this->fridge_output_->set_active();
  }
}

float GarageFridge::get_freezer_trend() {
  // https://stackoverflow.com/questions/11449617/how-to-fit-the-2d-scatter-data-with-a-line-with-c
  auto start_trend_values = this->freezer_temps_.end() - TREND_TEMPERATURE_READINGS_COUNT + 1;
  std::vector<float> trend_values = {start_trend_values, this->freezer_temps_.end()};

  double sumX=0, sumY=0, sumXY=0, sumX2=0;
  for(int i=0; i<TREND_TEMPERATURE_READINGS_COUNT; i++) {
    sumX += i;
    sumY += trend_values[i];
    sumXY += i * trend_values[i];
    sumX2 += i * i;
  }

  double xMean = sumX / TREND_TEMPERATURE_READINGS_COUNT;
  double yMean = sumY / TREND_TEMPERATURE_READINGS_COUNT;
  double denominator = sumX2 - sumX * xMean;

  return (sumXY - sumX * yMean) / denominator;
}

}  // namespace garage_fridge
}  // namespace esphome
