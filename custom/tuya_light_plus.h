#include "esphome.h"

using namespace esphome;

#define DOUBLE_TAP_TIMEOUT 300

class TuyaLightPlus : public Component, public light::LightOutput, public api::CustomAPIDevice
{
  public:
    void setup() override;
    void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
    void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
    void set_tuya_parent(tuya::Tuya *parent) { this->parent_ = parent; }
    void set_min_value(uint32_t min_value) { this->min_value_ = min_value; }
    void set_max_value(uint32_t max_value) { this->max_value_ = max_value; }
    light::LightTraits get_traits() override;
    void setup_state(light::LightState *state) override;
    void write_state(light::LightState *state) override;
    void loop() override;

    void set_day_default_brightness(float brightness) { this->day_default_brightness_ = brightness; }
    void set_night_default_brightness(float brightness) { this->night_default_brightness_ = brightness; }
    void set_day_auto_off_minutes(uint32_t minutes) { this->day_auto_off_time_ = minutes * 60 * 1000; }
    void set_night_auto_off_minutes(uint32_t minutes) { this->night_auto_off_time_ = minutes * 60 * 1000; }
    void set_api_server(api::APIServer *api_server) { this->api_server_ = api_server; };
    void set_day_night_sensor(const std::string day_night_sensor);
    void set_linked_lights(const std::string linked_lights);
    void add_double_tap_while_on_callback(const std::function<void()> &func);
    void add_double_tap_while_off_callback(const std::function<void()> &func);
    void set_double_tap_while_off_stays_on(bool stays_on) { this->double_tap_while_off_stays_on_ = stays_on; }
    void add_double_tap_callback(const std::function<void()> &func);

    void set_default_brightness(float brightness);
    void set_auto_off_minutes(int minutes) { this->current_auto_off_time_ = minutes * 60 * 1000; }

  protected:
    float tuya_level_to_brightness(uint32_t level) { return static_cast<float>(level) / static_cast<float>(this->max_value_); }
    uint32_t brightness_to_tuya_level(float brightness) { return static_cast<uint32_t>(brightness * this->max_value_); }
    float brightness_pct() { return static_cast<uint32_t>(this->state_->current_values.get_brightness() * 100); }
    void on_day_night_changed(std::string state);
    void handle_tuya_datapoint(tuya::TuyaDatapoint datapoint);
    void set_tuya_state(bool state);
    void set_tuya_level(uint32_t level);
    void update_linked_lights();
    void set_default_auto_off_time(int time) { this->default_auto_off_time_ = time; this->current_auto_off_time_ = time; }

    tuya::Tuya *parent_;
    optional<uint8_t> dimmer_id_{};
    optional<uint8_t> switch_id_{};
    uint32_t min_value_ = 0;
    uint32_t max_value_ = 255;
    light::LightState *state_{nullptr};

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
    bool has_double_tap_while_on_ = false;
    std::vector<std::function<void()>> double_tap_while_on_callbacks_;
    bool has_double_tap_while_off_ = false;
    std::vector<std::function<void()>> double_tap_while_off_callbacks_;
    bool double_tap_while_off_stays_on_ = true;

    bool tuya_state_;
    unsigned long state_changed_at_;
    unsigned long double_tap_while_on_timeout_;
    bool was_double_tapped_while_on_;
    unsigned long double_tap_while_off_timeout_;
    bool was_double_tapped_while_off_;
};

void TuyaLightPlus::setup()
{
  this->parent_->register_listener(*this->switch_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint(datapoint); });

  this->parent_->register_listener(*this->dimmer_id_, [this](tuya::TuyaDatapoint datapoint) { this->handle_tuya_datapoint(datapoint); });

  this->register_service(&TuyaLightPlus::set_auto_off_minutes, "set_auto_off_minutes", {"minutes"});

  this->register_service(&TuyaLightPlus::set_default_brightness, "set_default_brightness", {"brightness"});
}

light::LightTraits TuyaLightPlus::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supports_brightness(this->dimmer_id_.has_value());
  return traits;
}

void TuyaLightPlus::setup_state(light::LightState *state)
{
  this->state_ = state;
  this->tuya_state_ = this->state_->current_values.is_on();
}

void TuyaLightPlus::write_state(light::LightState *state)
{
  float brightness;
  state->current_values_as_brightness(&brightness);

  if (brightness == 0.0f)
  {
    this->set_tuya_state(false);
  }
  else if (this->tuya_state_)
  {
    // The light is already on so just set the brightness
    this->set_tuya_level(this->brightness_to_tuya_level(brightness));
  }
  else
  {
    // If the light is currently off only turn it on, we will set the brightness in the datapoint handler
    this->set_tuya_state(true);
  }
}

void TuyaLightPlus::loop()
{
  // Double tap while on timed out
  if (this->double_tap_while_on_timeout_ != 0 && millis() > this->double_tap_while_on_timeout_)
  {
    this->double_tap_while_on_timeout_ = 0;
  }

  // Handle double tap while on callbacks
  if (this->was_double_tapped_while_on_)
  {
    this->was_double_tapped_while_on_ = false;
    for (auto &callback : this->double_tap_while_on_callbacks_)
    {
      callback();
    }
  }

  // Double tap while off timed out, turn the light on
  if (this->double_tap_while_off_timeout_ != 0 && millis() > this->double_tap_while_off_timeout_)
  {
    this->double_tap_while_off_timeout_ = 0;
    this->set_tuya_state(true);
  }

  // Handle double tap while off callbacks
  if (this->was_double_tapped_while_off_)
  {
    this->was_double_tapped_while_off_ = false;
    for (auto &callback : this->double_tap_while_off_callbacks_)
    {
      callback();
    }
  }

  // Handle auto turn off
  if (this->current_auto_off_time_ != 0 && this->tuya_state_ && millis() >= this->state_changed_at_ + this->current_auto_off_time_)
  {
    this->set_tuya_state(false);
  }
}

void TuyaLightPlus::set_day_night_sensor(const std::string day_night_sensor)
{
  if (day_night_sensor != "")
  {
    this->subscribe_homeassistant_state(&TuyaLightPlus::on_day_night_changed, day_night_sensor);
  }
}

void TuyaLightPlus::set_linked_lights(const std::string linked_lights)
{
  if (linked_lights != "")
  {
    this->has_linked_lights_ = true;

    this->linked_lights_turn_on_action_ = new api::HomeAssistantServiceCallAction<>(this->api_server_, false);
    this->linked_lights_turn_on_action_->set_service("light.turn_on");
    this->linked_lights_turn_on_action_->add_data("entity_id", linked_lights);
    this->linked_lights_turn_on_action_->add_variable("brightness_pct", [=]() { return this->brightness_pct(); });
    this->linked_lights_turn_on_action_->add_data_template("brightness_pct", "{{ brightness_pct }}");

    this->linked_lights_turn_off_action_ = new api::HomeAssistantServiceCallAction<>(this->api_server_, false);
    this->linked_lights_turn_off_action_->set_service("light.turn_off");
    this->linked_lights_turn_off_action_->add_data("entity_id", linked_lights);
  }
}

void TuyaLightPlus::add_double_tap_while_on_callback(const std::function<void()> &func)
{
  this->has_double_tap_while_on_ = true;
  this->double_tap_while_on_callbacks_.push_back(func);
}

void TuyaLightPlus::add_double_tap_while_off_callback(const std::function<void()> &func)
{
  this->has_double_tap_while_off_ = true;
  this->double_tap_while_off_callbacks_.push_back(func);
}

void TuyaLightPlus::add_double_tap_callback(const std::function<void()> &func)
{
  this->add_double_tap_while_off_callback(func);
  this->add_double_tap_while_on_callback(func);
}

void TuyaLightPlus::set_default_brightness(float brightness)
{
  this->default_brightness_ = brightness > 1 ? 1 : brightness;

  // If the light is off update the brightness in the state and publish so regardless of how the light is turned on the brightness will be the default
  if (!this->tuya_state_)
  {
    this->state_->current_values.set_brightness(this->default_brightness_);
    this->state_->remote_values = this->state_->current_values;
    this->state_->publish_state();
  }
}

void TuyaLightPlus::on_day_night_changed(std::string state)
{
  if (state == "Day")
  {
    this->set_default_brightness(day_default_brightness_);
    this->set_default_auto_off_time(day_auto_off_time_);
  }
  else if (state == "Night")
  {
    this->set_default_brightness(night_default_brightness_);
    this->set_default_auto_off_time(night_auto_off_time_);
  }
}

void TuyaLightPlus::handle_tuya_datapoint(tuya::TuyaDatapoint datapoint)
{
  if (datapoint.id == *this->switch_id_)
  {
    // Light turned on
    if (datapoint.value_bool)
    {
      // Turned on with the physical button
      if (!this->tuya_state_)
      {
        if (this->has_double_tap_while_on_)
        {
          // We are in a double tap while on timeout period so this is a double tap
          if (this->double_tap_while_on_timeout_ != 0)
          {
            // Double tap while on always stays off otherwise it results in weird flashing behavior
            this->set_tuya_state(false);
            this->double_tap_while_on_timeout_ = 0;
            this->was_double_tapped_while_on_ = true;
            return;
          }
        }
        if (this->has_double_tap_while_off_)
        {
          // We are not in a double tap while off timeout period
          if (this->double_tap_while_off_timeout_ == 0)
          {
            // Turn the light back off and wait to see if we get a double tap
            this->set_tuya_state(false);
            this->double_tap_while_off_timeout_ = millis() + DOUBLE_TAP_TIMEOUT;
            return;
          }
          // We are in a double tap while off timeout period so this is a double tap
          else
          {
            this->double_tap_while_off_timeout_ = 0;
            this->was_double_tapped_while_off_ = true;
            
            // If double tap while off triggers an event but does not turn on the light then turn it back off
            if (!this->double_tap_while_off_stays_on_)
            {
              this->set_tuya_state(false);
              return;
            }
          }
        }
      
        // We got through all the double tap logic and the light is still on so update the Tuya state
        this->tuya_state_ = true;
      }

      // Set the brightness to the correct level (it currently is at 0)
      this->set_tuya_level(this->brightness_to_tuya_level(this->state_->current_values.get_brightness()));
    }
    // Light turned off
    else
    {
      // Turned off with physical button
      if (this->tuya_state_)
      {
        if (has_double_tap_while_on_)
        {
          // Start the double tap while on timeout
          this->double_tap_while_on_timeout_ = millis() + DOUBLE_TAP_TIMEOUT;
        }

        // Update the Tuya state
        this->tuya_state_ = false;
      }

      // Set the Tuya level to 0 to prevent flashes during double taps
      this->set_tuya_level(0);

      // Set the current brightness to the default so that it will turn on at the default brightness
      this->state_->current_values.set_brightness(this->default_brightness_);
    }

    // Update the current values state
    this->state_->current_values.set_state(this->tuya_state_);
  }
  else if (datapoint.id == *this->dimmer_id_)
  {
    // Only react to dimmer level changes if the light is on
    if(this->tuya_state_)
    {
      this->state_->current_values.set_brightness(tuya_level_to_brightness(datapoint.value_uint));
    }
  }

  // Update state changed at time
  this->state_changed_at_ = millis();

  // If the remote values do not reflect the current values update and publish the values
  if (this->state_->current_values != this->state_->remote_values)
  {
    this->state_->remote_values = this->state_->current_values;
    this->state_->publish_state();
  }

  // Update any linked lights
  this->update_linked_lights();
}

void TuyaLightPlus::set_tuya_state(bool state)
{
  this->tuya_state_ = state;
  tuya::TuyaDatapoint datapoint{};
  datapoint.id = *this->switch_id_;
  datapoint.type = tuya::TuyaDatapointType::BOOLEAN;
  datapoint.value_bool = state;
  this->parent_->set_datapoint_value(datapoint);
}

void TuyaLightPlus::set_tuya_level(uint32_t level)
{
  tuya::TuyaDatapoint datapoint{};
  datapoint.id = *this->dimmer_id_;
  datapoint.type = tuya::TuyaDatapointType::INTEGER;
  datapoint.value_uint = std::max(level, this->min_value_);
  this->parent_->set_datapoint_value(datapoint);
}

void TuyaLightPlus::update_linked_lights()
{
  if (this->has_linked_lights_)
  {
    if (this->state_->current_values.is_on())
    {
      this->linked_lights_turn_on_action_->play();
    }
    else
    {
      this->linked_lights_turn_off_action_->play();
    }
  }
}

TuyaLightPlus *TuyaLight;