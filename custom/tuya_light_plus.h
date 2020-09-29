#include "esphome.h"

class TuyaLightPlus : public tuya::TuyaLight, public CustomAPIDevice
{
  public:
    void setup() override;
    void loop() override;
    void write_state(light::LightState *state) override;

    void set_day_default_brightness(float brightness) { day_default_brightness_ = brightness; }
    void set_night_default_brightness(float brightness) { night_default_brightness_ = brightness; }
    void set_day_auto_off_minutes(uint32_t minutes) { day_auto_off_time_ = minutes * 60 * 1000; }
    void set_night_auto_off_minutes(uint32_t minutes) { night_auto_off_time_ = minutes * 60 * 1000; }
    void set_api_server(api::APIServer *api_server) { api_server_ = api_server; };
    void set_day_night_sensor(const std::string day_night_sensor);
    void set_linked_lights(const std::string linked_lights);
    void add_double_tap_off_callback(const std::function<void()> &func);
    void add_double_tap_on_callback(const std::function<void()> &func);
    void set_double_tap_on_stays_on(bool stays_on) { double_tap_on_stays_on_ = stays_on; }
    void add_double_tap_callback(const std::function<void()> &func);

    void set_default_brightness(float brightness);
    void set_auto_off_minutes(int minutes) { current_auto_off_time_ = minutes * 60 * 1000; }

  protected:
    float tuya_level_to_brightness(uint32_t level) { return float(level) / max_value_; }
    uint32_t brightness_to_tuya_level(float brightness) { return static_cast<uint32_t>(brightness * max_value_); }
    bool is_on() { return state_->current_values.is_on(); }
    float brightness_pct() { return static_cast<uint32_t>(state_->current_values.get_brightness() * 100); }
    void on_day_night_changed(std::string state);
    void handle_tuya_datapoint(esphome::tuya::TuyaDatapoint datapoint);
    void set_tuya_state(bool state);
    void set_tuya_level(uint32_t level);
    void update_tuya_level();
    void update_current_state(bool state);
    void update_current_brightness(float brightness);
    void update_linked_lights();
    void set_default_auto_off_time(int time) { default_auto_off_time_ = time; current_auto_off_time_ = time; }

    float day_default_brightness_ = 1;
    float night_default_brightness_ = .03;
    float default_brightness_ = 1;
    uint32_t day_auto_off_time_;
    uint32_t night_auto_off_time_;
    uint32_t default_auto_off_time_;
    uint32_t current_auto_off_time_;
    bool has_linked_lights_;
    api::APIServer *api_server_;
    api::HomeAssistantServiceCallAction<> *linked_lights_turn_on_action_;
    api::HomeAssistantServiceCallAction<> *linked_lights_turn_off_action_;
    bool has_double_tap_off_ = false;
    std::vector<std::function<void()>> double_tap_off_callbacks_;
    bool has_double_tap_on_ = false;
    std::vector<std::function<void()>> double_tap_on_callbacks_;
    bool double_tap_on_stays_on_ = true;

    unsigned long state_changed_at_;
    bool is_software_state_change_;
    bool is_reported_state_change_;
    unsigned long double_tap_off_timeout_;
    bool was_double_tapped_off_;
    unsigned long double_tap_on_timeout_;
    bool was_double_tapped_on_;
};

void TuyaLightPlus::setup()
{
  parent_->register_listener(*switch_id_, [this](esphome::tuya::TuyaDatapoint datapoint) { handle_tuya_datapoint(datapoint); });

  parent_->register_listener(*dimmer_id_, [this](esphome::tuya::TuyaDatapoint datapoint) { handle_tuya_datapoint(datapoint); });

  register_service(&TuyaLightPlus::set_auto_off_minutes, "set_auto_off_minutes", {"minutes"});

  register_service(&TuyaLightPlus::set_default_brightness, "set_default_brightness", {"brightness"});
}

void TuyaLightPlus::loop()
{
  if (double_tap_off_timeout_ != 0 && millis() > double_tap_off_timeout_)
  {
    double_tap_off_timeout_ = 0;
  }

  if (was_double_tapped_off_)
  {
    was_double_tapped_off_ = false;
    for (auto &callback : this->double_tap_off_callbacks_)
    {
      callback();
    }
  }

  if (double_tap_on_timeout_ != 0 && millis() > double_tap_on_timeout_)
  {
    if (!is_on())
    {
      is_software_state_change_ = true;
      set_tuya_state(true);
    }
    
    double_tap_on_timeout_ = 0;
  }

  if (was_double_tapped_on_)
  {
    was_double_tapped_on_ = false;
    for (auto &callback : this->double_tap_on_callbacks_)
    {
      callback();
    }
  }

  if (current_auto_off_time_ != 0 && is_on() && millis() >= state_changed_at_ + current_auto_off_time_)
  {
    set_tuya_state(false);
  }
}

void TuyaLightPlus::write_state(light::LightState *state)
{
  if (!is_reported_state_change_) {
    is_software_state_change_ = true;
  }
  else
  {
    is_reported_state_change_ = false;
  }
  
  TuyaLight::write_state(state);
}

void TuyaLightPlus::set_day_night_sensor(const std::string day_night_sensor)
{
  if (day_night_sensor != "")
  {
    subscribe_homeassistant_state(&TuyaLightPlus::on_day_night_changed, day_night_sensor);
  }
}

void TuyaLightPlus::set_linked_lights(const std::string linked_lights)
{
  if (linked_lights != "")
  {
    has_linked_lights_ = true;

    linked_lights_turn_on_action_ = new api::HomeAssistantServiceCallAction<>(api_server_, false);
    linked_lights_turn_on_action_->set_service("light.turn_on");
    linked_lights_turn_on_action_->add_data("entity_id", linked_lights);
    linked_lights_turn_on_action_->add_variable("brightness_pct", [=]() { return brightness_pct(); });
    linked_lights_turn_on_action_->add_data_template("brightness_pct", "{{ brightness_pct }}");

    linked_lights_turn_off_action_ = new api::HomeAssistantServiceCallAction<>(api_server_, false);
    linked_lights_turn_off_action_->set_service("light.turn_off");
    linked_lights_turn_off_action_->add_data("entity_id", linked_lights);
  }
}

void TuyaLightPlus::add_double_tap_off_callback(const std::function<void()> &func)
{
  has_double_tap_off_ = true;
  this->double_tap_off_callbacks_.push_back(func);
}

void TuyaLightPlus::add_double_tap_on_callback(const std::function<void()> &func)
{
  has_double_tap_on_ = true;
  this->double_tap_on_callbacks_.push_back(func);
}

void TuyaLightPlus::add_double_tap_callback(const std::function<void()> &func)
{
  add_double_tap_on_callback(func);
  add_double_tap_off_callback(func);
}

void TuyaLightPlus::set_default_brightness(float brightness)
{
  default_brightness_ = brightness > 1 ? 1 : brightness;
  update_tuya_level();
}

void TuyaLightPlus::on_day_night_changed(std::string state)
{
  if (state == "Day")
  {
    set_default_brightness(day_default_brightness_);
    set_default_auto_off_time(day_auto_off_time_);
  }
  else if (state == "Night")
  {
    set_default_brightness(night_default_brightness_);
    set_default_auto_off_time(night_auto_off_time_);
  }
}

void TuyaLightPlus::handle_tuya_datapoint(esphome::tuya::TuyaDatapoint datapoint)
{
  if (datapoint.id == *switch_id_)
  {
    bool new_state = datapoint.value_bool;

    if (!is_software_state_change_)
    {
      if (has_double_tap_off_)
      {
        if (is_on() and !new_state)
        {
          double_tap_off_timeout_ = millis() + 400;
        }
        else if (new_state && double_tap_off_timeout_ >= millis())
        {
          set_tuya_state(false);
          new_state = false;
          double_tap_off_timeout_ = 0;
          was_double_tapped_off_ = true;
        }
      }

      if (has_double_tap_on_)
      {
        if (!is_on() && new_state)
        {
          if (double_tap_on_timeout_ == 0)
          {
            set_tuya_state(false);
            new_state = false;
            double_tap_on_timeout_ = millis() + 400;
            update_tuya_level();
          }
          else
          {
            double_tap_on_timeout_ = 0;
            if (!double_tap_on_stays_on_)
            {
              set_tuya_state(false);
              new_state = false;
            }

            was_double_tapped_on_ = true;
          }
        }
      }
    }

    is_software_state_change_ = false;

    if (new_state != is_on())
    {
      is_reported_state_change_ = true;
      update_current_state(new_state);
    }
  }
  else if (datapoint.id == *dimmer_id_)
  {
    is_reported_state_change_ = true;
    update_current_brightness(tuya_level_to_brightness(datapoint.value_uint));
  }
}

void TuyaLightPlus::set_tuya_state(bool state)
{
  is_software_state_change_ = true;
  esphome::tuya::TuyaDatapoint datapoint{};
  datapoint.id = *switch_id_;
  datapoint.type = esphome::tuya::TuyaDatapointType::BOOLEAN;
  datapoint.value_bool = state;
  parent_->set_datapoint_value(datapoint);
}

void TuyaLightPlus::set_tuya_level(uint32_t level)
{
  esphome::tuya::TuyaDatapoint datapoint{};
  datapoint.id = *dimmer_id_;
  datapoint.type = esphome::tuya::TuyaDatapointType::INTEGER;
  datapoint.value_uint = std::max(level, min_value_);
  parent_->set_datapoint_value(datapoint);
}

void TuyaLightPlus::update_tuya_level()
{
  if (!is_on())
  {
    if (double_tap_off_timeout_ != 0)
    {
      set_tuya_level(1);
    }
    else if (has_double_tap_on_ && double_tap_on_timeout_ == 0)
    {
      set_tuya_level(1);
    }
    else
    {
      set_tuya_level(brightness_to_tuya_level(default_brightness_));
    }
  }
}

void TuyaLightPlus::update_current_state(bool state)
{
  state_changed_at_ = millis();

  auto call = state_->make_call();
  call.set_state(state);
  call.perform();
    
  update_linked_lights();

  update_tuya_level();

  current_auto_off_time_ = default_auto_off_time_;
}

void TuyaLightPlus::update_current_brightness(float brightness)
{
  if (is_on())
  {
    auto call = state_->make_call();
    call.set_brightness(brightness);
    call.perform();

    update_linked_lights();
  }
}

void TuyaLightPlus::update_linked_lights()
{
  if (has_linked_lights_)
  {
    if (is_on())
    {
      linked_lights_turn_on_action_->play();
    }
    else
    {
      linked_lights_turn_off_action_->play();
    }
  }
}

TuyaLightPlus *TuyaLight;