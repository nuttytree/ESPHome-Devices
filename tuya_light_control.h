#include "esphome.h"

class TuyaLightControl : public Component, public CustomAPIDevice {
  public:
  void setup() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_tuya_parent(esphome::tuya::Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { this->min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { this->max_value_ = max_value; }
  void set_day_brightness(float brightness) { this->day_brightness_ = brightness; }
  void set_night_brightness(float brightness) { this->night_brightness_ = brightness; }
  void set_api_server(api::APIServer *api_server) { this->api_server_ = api_server; };
  void set_linked_lights(const std::string linked_lights);
  void set_default_brightness(float brightness);
  void on_day_night_changed(std::string state);

 protected:
  void handle_datapoint(esphome::tuya::TuyaDatapoint datapoint);
  void set_brightness();

  esphome::tuya::Tuya *parent_;
  optional<uint8_t> switch_id_{};
  optional<uint8_t> dimmer_id_{};
  uint32_t min_value_ = 0;
  uint32_t max_value_ = 1000;
  float day_brightness_ = 1;
  float night_brightness_ = .03;
  float default_brightness_ = 1;
  api::APIServer *api_server_;
  api::HomeAssistantServiceCallAction<> *turn_on_service_;
  api::HomeAssistantServiceCallAction<> *turn_off_service_;
  bool has_linked_lights_ = false;
  bool is_on_;
  uint32_t brightness_pct_;
};

void TuyaLightControl::setup() {
  this->parent_->register_listener(*this->switch_id_, [this](esphome::tuya::TuyaDatapoint datapoint) {
    this->handle_datapoint(datapoint);
  });

  this->parent_->register_listener(*this->dimmer_id_, [this](esphome::tuya::TuyaDatapoint datapoint) {
    this->handle_datapoint(datapoint);
  });

  register_service(&TuyaLightControl::set_default_brightness, "set_default_brightness", {"brightness"});

  subscribe_homeassistant_state(&TuyaLightControl::on_day_night_changed, "sensor.day_night");
}

void TuyaLightControl::set_linked_lights(const std::string linked_lights) {
  if (linked_lights != "") {
    this->has_linked_lights_ = true;

    this->turn_on_service_ = new api::HomeAssistantServiceCallAction<>(this->api_server_, false);
    this->turn_on_service_->set_service("light.turn_on");
    this->turn_on_service_->add_data("entity_id", linked_lights);
    this->turn_on_service_->add_variable("brightness_pct", [=]() { return this->brightness_pct_; });
    this->turn_on_service_->add_data_template("brightness_pct", "{{ brightness_pct }}");

    this->turn_off_service_ = new api::HomeAssistantServiceCallAction<>(this->api_server_, false);
    this->turn_off_service_->set_service("light.turn_off");
    this->turn_off_service_->add_data("entity_id", linked_lights);
  }
}

void TuyaLightControl::set_default_brightness(float brightness) {
  this->default_brightness_ = brightness > 1 ? 1 : brightness;
  this->set_brightness();
}

void TuyaLightControl::on_day_night_changed(std::string state) {
  if (state == "Day") {
    this->default_brightness_ = this->day_brightness_;
  }
  else if (state == "Night")
  {
    this->default_brightness_ = this->night_brightness_;
  }
  this->set_brightness();
}

void TuyaLightControl::handle_datapoint(esphome::tuya::TuyaDatapoint datapoint) {
  if (datapoint.id == *this->switch_id_) {
    this->is_on_ = datapoint.value_bool;
    if (this->has_linked_lights_) {
      if (this->is_on_) {
        this->turn_on_service_->play();
      }
      else {
        this->turn_off_service_->play();
      }
    }
    this->set_brightness();
  }
  else if (datapoint.id == *this->dimmer_id_) {
    this->brightness_pct_ = static_cast<uint32_t>(datapoint.value_uint * 100 / this->max_value_);
    if (this->has_linked_lights_) {
      if (this->is_on_) {
        this->turn_on_service_->play();
      }
    }
  }
}

void TuyaLightControl::set_brightness() {
  if (!this->is_on_) {
    auto brightness_int = static_cast<uint32_t>(this->default_brightness_ * this->max_value_);
    brightness_int = std::max(brightness_int, this->min_value_);
    esphome::tuya::TuyaDatapoint datapoint{};
    datapoint.id = *this->dimmer_id_;
    datapoint.type = esphome::tuya::TuyaDatapointType::INTEGER;
    datapoint.value_uint = brightness_int;
    this->parent_->set_datapoint_value(datapoint);
  }
}