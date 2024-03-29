#include "esphome.h"

using namespace esphome;

static const char* TAG = "NuttyGarageDoor";

const bool HAS_INTERNAL_CLOSED_SENSOR = true;
const bool HAS_INTERNAL_OPEN_SENSOR = true;

// Open/Close durations used to determine position while opening/closing
const uint32_t NORMAL_OPEN_DURATION = 13000;
const uint32_t NORMAL_CLOSE_DURATION = 12000;

// Minimum number of milliseconds to keep the control pin active/inactive when changing states
const uint32_t CONTROL_PIN_ACTIVE_DURATION = 200;
const uint32_t CONTROL_PIN_INACTIVE_DURATION = 200;

// Number of milliseconds between publishing the state while the door is opening or closing
const uint32_t DOOR_MOVING_PUBLISH_INTERVAL = 750;

// Number of milliseconds between reads of the ADC to get local button state, this is needed to prevent wifi issues from reading to frequently
const uint32_t LOCAL_BUTTON_READ_INTERVAL = 75;

// Number of milliseconds to wait before checking the open/close sensors after starting to open/close the door to prevent false "failed" triggers
const uint32_t SENSOR_READ_DELAY = 1000;

// Close warning
const std::string CLOSE_WARNING_RTTTL = "Imperial:d=4, o=5, b=100:e, e, e, 8c, 16p, 16g, e, 8c, 16p, 16g, e, p, b, b, b, 8c6, 16p, 16g, d#, 8c, 16p, 16g, e, 8p";

const uint8_t PIN_CONTROL_RELAY = D1;
const uint8_t PIN_CLOSED_SENSOR = D2;
const uint8_t PIN_OPEN_SENSOR = D7;
const uint8_t PIN_REMOTE_BUTTON = D5;
const uint8_t PIN_REMOTE_LIGHT_BUTTON = D6;
const uint8_t PIN_STATUS_LED = D4;
const uint8_t PIN_CLOSE_WARNING_BUZZER = D8;

// Events
const std::string EVENT_LOCAL_LIGHT = "local_light_button";
const std::string EVENT_REMOTE_LIGHT = "remote_light_button";
const std::string EVENT_FAILED_OPEN = "open_failed";
const std::string EVENT_FAILED_CLOSE = "close_failed";
const std::string EVENT_BUTTON_DISCONNECTED = "button_disconnected";

// Lock Binary sensor state aliases
const bool LOCK_STATE_LOCKED = false;
const bool LOCK_STATE_UNLOCKED = true;

enum StateChangeType : uint8_t {
  STATE_CHANGE_INTERNAL = 0,
  STATE_CHANGE_BUTTON,
  STATE_CHANGE_CLOSE_SENSOR,
  STATE_CHANGE_OPEN_SENSOR,
  STATE_CHANGE_CANCEL_WARNING
};

enum InternalState : uint8_t {
  // On startup the state is unknown
  STATE_UNKNOWN = 0,
  // The door is moving from an unknown state so which direction it is moving is unknown
  STATE_MOVING,
  // The door is "locked" which is the same as closed but will not open for remotes (but local buttons and Home Assistant can open or unlock it)
  STATE_LOCKED,
  // The door is closed
  STATE_CLOSED,
  // The door is opening
  STATE_OPENING,
  // The door was stopped while it was opening
  STATE_STOPPED_OPENING,
  // The door is open
  STATE_OPEN,
  // The door is currently at least partially open but was requested closed by Home Assistant so we are waiting to close after an alert (beeper)
  STATE_CLOSE_WARNING,
  // The door is closing
  STATE_CLOSING,
  // The door was stopped while it was closing
  STATE_STOPPED_CLOSING,

  // Special states that the door will never actually be in but are used as target states
  // Stopped where ever it currently is at
  STATE_STOPPED,
  // Stopped at a specific position
  STATE_POSITION,
  // No currently requested state
  STATE_NONE
};

enum LocalButton : uint8_t {
  LOCAL_BUTTON_DISCONNECTED = 0,  // The button is not connected resulting in 0 volts at pin A0
  LOCAL_BUTTON_NONE,
  LOCAL_BUTTON_DOOR,
  LOCAL_BUTTON_LOCK,
  LOCAL_BUTTON_LIGHT
};

class GarageDoorLock : public binary_sensor::BinarySensor, public Component
{
  public:
    GarageDoorLock() {}
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override {}
    void loop() override {}
};

class GarageDoorCover : public cover::Cover, public Component, public api::CustomAPIDevice
{
  public:
    GarageDoorCover();
    void set_rtttl_buzzer(rtttl::Rtttl *buzzer) { this->buzzer_ = buzzer; }
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override;
    cover::CoverTraits get_traits() override;
    void control(const cover::CoverCall &call) override;
    void loop() override;
    GarageDoorLock *get_lock_sensor() { return this->lock_sensor_; }

  protected:
    rtttl::Rtttl *buzzer_;
    esphome::esp8266::ESP8266GPIOPin *control_pin_;
    esphome::esp8266::ESP8266GPIOPin *closed_pin_;
    esphome::esp8266::ESP8266GPIOPin *open_pin_;
    esphome::esp8266::ESP8266GPIOPin *remote_pin_;
    esphome::esp8266::ESP8266GPIOPin *remote_light_pin_;
    GarageDoorLock *lock_sensor_;

    InternalState internal_state_{STATE_UNKNOWN};
    InternalState target_state_{STATE_NONE};
    float target_position_{0};
    uint32_t control_pin_active_time_{0};
    uint32_t control_pin_inactive_time_{0};
    uint32_t last_state_change_time_{0};
    uint32_t last_position_time_{0};
    uint32_t last_publish_time_{0};
    uint32_t last_local_button_read_time_{0};
    LocalButton last_local_button_{LOCAL_BUTTON_NONE};
    bool last_remote_state_{false};
    bool last_remote_light_state_{false};

    void lock_();
    void unlock_();
    
    bool check_control_pin_();
    bool check_for_closed_position_();
    bool check_for_open_position_();
    bool check_for_position_update_();
    bool ensure_target_state_();
    bool check_local_buttons_();
    bool check_remote_buttons_();
    void change_to_next_state_(StateChangeType change_type = STATE_CHANGE_INTERNAL);
    void set_internal_state_(InternalState state, bool is_initial_state = false);
    InternalState get_internal_state_() { return this->internal_state_; }
    std::string get_event_(std::string event_type);
    const char *internal_state_to_str_(InternalState state);
};

GarageDoorCover::GarageDoorCover()
{
  this->set_device_class("garage");
  this->control_pin_ = new esphome::esp8266::ESP8266GPIOPin();
  this->control_pin_->set_pin(PIN_CONTROL_RELAY);
  this->control_pin_->set_inverted(false);
  this->control_pin_->set_flags(gpio::Flags::FLAG_OUTPUT);
  this->closed_pin_ = new esphome::esp8266::ESP8266GPIOPin();
  this->closed_pin_->set_pin(PIN_CLOSED_SENSOR);
  this->closed_pin_->set_inverted(true);
  this->closed_pin_->set_flags(gpio::Flags::FLAG_PULLUP);
  this->open_pin_ = new esphome::esp8266::ESP8266GPIOPin();
  this->open_pin_->set_pin(PIN_OPEN_SENSOR);
  this->open_pin_->set_inverted(true);
  this->open_pin_->set_flags(gpio::Flags::FLAG_PULLUP);
  this->remote_pin_ = new esphome::esp8266::ESP8266GPIOPin();
  this->remote_pin_->set_pin(PIN_REMOTE_BUTTON);
  this->remote_pin_->set_inverted(true);
  this->remote_pin_->set_flags(gpio::Flags::FLAG_PULLUP);
  this->remote_light_pin_ = new esphome::esp8266::ESP8266GPIOPin();
  this->remote_light_pin_->set_pin(PIN_REMOTE_LIGHT_BUTTON);
  this->remote_light_pin_->set_inverted(true);
  this->remote_light_pin_->set_flags(gpio::Flags::FLAG_PULLUP);
  this->lock_sensor_ = new GarageDoorLock();
}

void GarageDoorCover::setup()
{
  this->control_pin_->setup();
  this->closed_pin_->setup();
  this->open_pin_->setup();
  this->remote_pin_->setup();
  this->remote_light_pin_->setup();

  this->lock_sensor_->publish_initial_state(LOCK_STATE_UNLOCKED);

  if (this->closed_pin_->digital_read())
  {
    this->set_internal_state_(STATE_CLOSED, true);
  }
  else if (this->open_pin_->digital_read())
  {
    this->set_internal_state_(STATE_OPEN, true);
  }
  else
  {
    this->set_internal_state_(STATE_UNKNOWN, true);
  }

  this->register_service(&GarageDoorCover::lock_, "lock");
  this->register_service(&GarageDoorCover::unlock_, "unlock");
}

cover::CoverTraits GarageDoorCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_is_assumed_state(false);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  return traits;
}

void GarageDoorCover::control(const cover::CoverCall &call)
{
  this->change_to_next_state_(STATE_CHANGE_CANCEL_WARNING);

  if (call.get_stop()) 
  {
    this->target_state_ = STATE_STOPPED;
  }
  else if (call.get_position().has_value())
  {
    this->target_position_ = *call.get_position();
    if (this->target_position_ == this->position && this->current_operation == COVER_OPERATION_IDLE && this->get_internal_state_() != STATE_UNKNOWN)
    {
      return;
    }
    else if (this->target_position_ == COVER_CLOSED)
    {
      this->target_state_ = this->get_internal_state_() == STATE_LOCKED ? STATE_LOCKED : STATE_CLOSED;
    }
    else if (this->target_position_ == COVER_OPEN)
    {
      this->target_state_ = STATE_OPEN;
    }
    else
    {
      this->target_state_ = STATE_POSITION;
    }
  }
}

void GarageDoorCover::loop()
{
  if (this->check_control_pin_())
  {
    return;
  }

  if (this->check_for_closed_position_())
  {
    return;
  }

  if (this->check_for_open_position_())
  {
    return;
  }

  if (this->check_for_position_update_())
  {
    return;
  }

  if (this->ensure_target_state_())
  {
    return;
  }

  if (this->check_local_buttons_())
  {
    return;
  }

  if (this->check_remote_buttons_())
  {
    return;
  }
}

void GarageDoorCover::lock_()
{
  if (this->get_internal_state_() != STATE_LOCKED)
  {
    this->target_state_ = STATE_LOCKED;
  }
}

void GarageDoorCover::unlock_()
{
  if (this->get_internal_state_() == STATE_LOCKED)
  {
    this->target_state_ = STATE_CLOSED;
  }
}

bool GarageDoorCover::check_control_pin_()
{
  if (this->control_pin_active_time_ > 0)
  {
    const uint32_t now = millis();
    if (now - this->control_pin_active_time_ >= CONTROL_PIN_ACTIVE_DURATION)
    {
      this->control_pin_->digital_write(false);
      this->control_pin_active_time_ = 0;
      this->control_pin_inactive_time_ = now;
    }

    return true;
  }
  else
  {
    return false;
  }
}

bool GarageDoorCover::check_for_closed_position_()
{
  if (this->current_operation != COVER_OPERATION_IDLE && (millis() - this->last_state_change_time_) >= SENSOR_READ_DELAY && this->closed_pin_->digital_read())
  {
    ESP_LOGD(TAG, "Door closed sensor is active");

    if (this->get_internal_state_() == STATE_OPENING)
    {
      this->target_state_ = STATE_NONE;
      this->fire_homeassistant_event(this->get_event_(EVENT_FAILED_OPEN));
    }

    this->change_to_next_state_(STATE_CHANGE_CLOSE_SENSOR);

    return true;
  }
  else
  {
    return false;
  }
}

bool GarageDoorCover::check_for_open_position_()
{
  if (this->current_operation != COVER_OPERATION_IDLE && (millis() - this->last_state_change_time_) >= SENSOR_READ_DELAY && this->open_pin_->digital_read())
  {
    ESP_LOGD(TAG, "Door open sensor is active");

    if (this->get_internal_state_() == STATE_CLOSING)
    {
      this->target_state_ = STATE_NONE;
      this->fire_homeassistant_event(this->get_event_(EVENT_FAILED_CLOSE));
    }

    this->change_to_next_state_(STATE_CHANGE_OPEN_SENSOR);

    return true;
  }
  else
  {
    return false;
  }
}

bool GarageDoorCover::check_for_position_update_()
{
  float direction;
  uint32_t normal_duration;
  switch (this->get_internal_state_())
  {
    case STATE_OPENING:
      direction = 1.0f;
      normal_duration = NORMAL_OPEN_DURATION;
      break;
  
    case STATE_CLOSING:
      direction = -1.0f;
      normal_duration = NORMAL_CLOSE_DURATION;
      break;

    default:
      return false;
  }

  const uint32_t now = millis();
  const uint32_t current_duration = now - this->last_state_change_time_;
  if (current_duration > normal_duration * 2)
  {
    // This should never happen but if it does we go into an unknown state
    this->set_internal_state_(STATE_UNKNOWN);
    return true;
  }
  else
  {
    this->position = clamp(this->position + (direction * (now - this->last_position_time_) / normal_duration), 0.01f, 0.99f);
    this->last_position_time_ = now;
    if (this->target_state_ == STATE_POSITION && this->position == this->target_position_)
    {
      return false;
    }
    else if (now - this->last_publish_time_ >= DOOR_MOVING_PUBLISH_INTERVAL) 
    {
      this->publish_state(false);
      this->last_publish_time_ = now;
      return true;
    }
    else
    {
      return false;
    }
  }
}

bool GarageDoorCover::ensure_target_state_()
{
  if (!this->buzzer_->is_playing() && this->target_state_ != STATE_NONE)
  {
    InternalState current_state = this->get_internal_state_();
    switch (this->target_state_)
    {
      case STATE_LOCKED:
        if (current_state == STATE_LOCKED)
        {
          this->target_state_ = STATE_NONE;
          return false;
        }
        if (current_state == STATE_MOVING || current_state == STATE_CLOSING)
        {
          return false;
        }
        else if (current_state == STATE_CLOSED)
        {
            this->set_internal_state_(STATE_LOCKED);
            return true;
        }
        else
        {
          this->change_to_next_state_();
          return true;
        }
    
      case STATE_CLOSED:
        if (current_state == STATE_CLOSED)
        {
          this->target_state_ = STATE_NONE;
          return false;
        }
        if (current_state == STATE_MOVING || current_state == STATE_CLOSING)
        {
          return false;
        }
        else if (current_state == STATE_LOCKED)
        {
            this->set_internal_state_(STATE_CLOSED);
            this->target_state_ = STATE_NONE;
            return true;
        }
        else
        {
          this->change_to_next_state_();
          return true;
        }

      case STATE_OPEN:
        if (current_state == STATE_OPEN)
        {
          this->target_state_ = STATE_NONE;
          return false;
        }
        if (current_state == STATE_MOVING || current_state == STATE_OPENING)
        {
          return false;
        }
        else
        {
          this->change_to_next_state_();
          return true;
        }
        
      case STATE_POSITION:
        if (current_state == STATE_MOVING)
        {
          return false;
        }
        else if (this->position < this->target_position_ && current_state != STATE_OPENING)
        {
          this->change_to_next_state_();
          return true;
        }
        else if (this->position > this->target_position_ && current_state != STATE_CLOSING)
        {
          this->change_to_next_state_();
          return true;
        }
        else if (this->position == this->target_position_ && (current_state == STATE_OPENING || current_state == STATE_CLOSING))
        {
          this->change_to_next_state_();
          this->target_state_ = STATE_NONE;
          return true;
        }
        else if (this->position == this->target_position_)
        {
          this->target_state_ = STATE_NONE;
          return true;
        }
        else
        {
          return false;
        }
        
      case STATE_STOPPED:
        if (this->current_operation != COVER_OPERATION_IDLE)
        {
          this->change_to_next_state_();
          this->target_state_ = STATE_NONE;
          return true;
        }
        else
        {
          this->target_state_ = STATE_NONE;
          return false;
        }

      default:
        return false;
    }
  }
  else
  {
    return false;
  }
}

bool GarageDoorCover::check_local_buttons_()
{
  const uint32_t now = millis();
  if (now - this->last_local_button_read_time_ > LOCAL_BUTTON_READ_INTERVAL)
  {
    this->last_local_button_read_time_ = now;

    int buttonValue = analogRead(A0);

    // Expected values:
    // 0   = Disconnected
    // 133 = None
    // 434 = Light
    // 714 = Lock
    // 993 = Door

    LocalButton currentButton;
    if (buttonValue < 50)
    {
      currentButton = LOCAL_BUTTON_DISCONNECTED;
    }
    else if (buttonValue < 284)
    {
      currentButton = LOCAL_BUTTON_NONE;
    }
    else if (buttonValue < 574)
    {
      currentButton = LOCAL_BUTTON_LIGHT;
    }
    else if (buttonValue < 854)
    {
      currentButton = LOCAL_BUTTON_LOCK;
    }
    else
    {
      currentButton = LOCAL_BUTTON_DOOR;
    }

    if (currentButton != this->last_local_button_)
    {
      this->last_local_button_ = currentButton;

      switch (currentButton)
      {
        case LOCAL_BUTTON_DISCONNECTED:
          ESP_LOGD(TAG, "Local button is disconnected");
          ESP_LOGD(TAG, "  Read Value: %u", buttonValue);
          this->fire_homeassistant_event(this->get_event_(EVENT_BUTTON_DISCONNECTED));
          break;

        case LOCAL_BUTTON_NONE:
          ESP_LOGD(TAG, "No local buttons currently pressed");
          ESP_LOGD(TAG, "  Read Value: %u", buttonValue);
          break;

        case LOCAL_BUTTON_LIGHT:
          ESP_LOGD(TAG, "Local light button was pressed");
          ESP_LOGD(TAG, "  Read Value: %u", buttonValue);
          this->fire_homeassistant_event(this->get_event_(EVENT_LOCAL_LIGHT));
          break;

        case LOCAL_BUTTON_LOCK:
          ESP_LOGD(TAG, "Local lock button was pressed");
          ESP_LOGD(TAG, "  Read Value: %u", buttonValue);
          if (this->get_internal_state_() == STATE_LOCKED)
          {
            this->unlock_();
          }
          else
          {
            this->lock_();
          }
          break;

        case LOCAL_BUTTON_DOOR:
          ESP_LOGD(TAG, "Local door button was pressed");
          ESP_LOGD(TAG, "  Read Value: %u", buttonValue);
          this->change_to_next_state_(STATE_CHANGE_BUTTON);
          break;

        default:
          break;
      }

      return true;
    }
  }

  return false;
}

bool GarageDoorCover::check_remote_buttons_()
{
  bool currentRemoteState = this->remote_pin_->digital_read();
  if (currentRemoteState != this->last_remote_state_)
  {
    last_remote_state_ = currentRemoteState;
    if (currentRemoteState && this->get_internal_state_() != STATE_LOCKED)
    {
      ESP_LOGD(TAG, "Remote door button was pressed");
      this->change_to_next_state_(STATE_CHANGE_BUTTON);
      return true;
    }
  }

  bool currentRemoteLightState = this->remote_light_pin_->digital_read();
  if (currentRemoteLightState != this->last_remote_light_state_)
  {
    last_remote_light_state_ = currentRemoteLightState;
    if (currentRemoteLightState)
    {
      ESP_LOGD(TAG, "Remote light button was pressed");
      this->fire_homeassistant_event(this->get_event_(EVENT_REMOTE_LIGHT));
      return true;
    }
  }

  return false;
}

void GarageDoorCover::change_to_next_state_(StateChangeType change_type)
{
  if (millis() < this->control_pin_inactive_time_ + CONTROL_PIN_INACTIVE_DURATION)
  {
    return;
  }

  switch (change_type)
  {
    case STATE_CHANGE_CLOSE_SENSOR:
      this->set_internal_state_(STATE_CLOSED);
      if (HAS_INTERNAL_CLOSED_SENSOR)
      {
        return;
      }
      break;
  
    case STATE_CHANGE_OPEN_SENSOR:
      this->set_internal_state_(STATE_OPEN);
      if (HAS_INTERNAL_OPEN_SENSOR)
      {
        return;
      }
      break;

    case STATE_CHANGE_CANCEL_WARNING:
      if (this->get_internal_state_() == STATE_CLOSE_WARNING)
      {
        this->buzzer_->stop();
        this->set_internal_state_(this->position == COVER_OPEN ? STATE_OPEN : STATE_STOPPED_OPENING);
        this->target_state_ = STATE_NONE;
      }
      return;
  
    default:
      if (change_type == STATE_CHANGE_BUTTON)
      {
       this->target_state_ = STATE_NONE;
      }

      switch (this->get_internal_state_())
      {
        case STATE_UNKNOWN:
          this->set_internal_state_(STATE_MOVING);
          break;
  
        case STATE_MOVING:
          this->set_internal_state_(STATE_UNKNOWN);
          break;

        case STATE_LOCKED:
          this->set_internal_state_(STATE_OPENING);
          break;

        case STATE_CLOSED:
          this->set_internal_state_(STATE_OPENING);
          break;

        case STATE_OPENING:
          this->set_internal_state_(STATE_STOPPED_OPENING);
          break;

        case STATE_STOPPED_OPENING:
          if (change_type == STATE_CHANGE_BUTTON || this->position > this->target_position_)
          {
            this->set_internal_state_(STATE_CLOSING);
          }
          else
          {
            this->set_internal_state_(STATE_CLOSE_WARNING);
          }
          break;

        case STATE_OPEN:
          this->set_internal_state_(change_type == STATE_CHANGE_BUTTON ? STATE_CLOSING : STATE_CLOSE_WARNING);
          break;

        case STATE_CLOSE_WARNING:
          if (change_type == STATE_CHANGE_BUTTON)
          {
            this->buzzer_->stop();
            this->set_internal_state_(this->position == COVER_OPEN ? STATE_OPEN : STATE_STOPPED_OPENING);
            return;
          }
          else
          {
            this->set_internal_state_(STATE_CLOSING);
          }
          break;

        case STATE_CLOSING:
          this->set_internal_state_(STATE_STOPPED_CLOSING);
          break;

        case STATE_STOPPED_CLOSING:
          this->set_internal_state_(STATE_OPENING);
          break;

        default:
          break;
      }

      if (this->get_internal_state_() == STATE_CLOSE_WARNING)
      {
        this->buzzer_->play(CLOSE_WARNING_RTTTL);
      }
      else
      {
        // "Press" the control "button"
        this->control_pin_->digital_write(true);
        this->control_pin_active_time_ = millis();
      }
      break;
  }
}

void GarageDoorCover::set_internal_state_(InternalState state, bool is_initial_state)
{
  if (is_initial_state)
  {
    ESP_LOGD(TAG, "Setting Initial Internal State: '%s'", this->internal_state_to_str_(state));
  }
  else if (this->internal_state_ != state)
  {
    ESP_LOGD(TAG, "Setting Internal State:");
    ESP_LOGD(TAG, "  Current State: '%s'", this->internal_state_to_str_(this->get_internal_state_()));
    ESP_LOGD(TAG, "  New State:     '%s'", this->internal_state_to_str_(state));
  }
  else
  {
    return;
  }

  this->internal_state_ = state;

  if (state == STATE_UNKNOWN)
  {
    this->position = 0.5f;
    this->current_operation = COVER_OPERATION_IDLE;
  }
  else if (state == STATE_MOVING)
  {
    this->position = 0.5f;
    this->current_operation = COVER_OPERATION_CLOSING;
  }
  else if (state == STATE_LOCKED || state == STATE_CLOSED)
  {
    this->position = COVER_CLOSED;
    this->current_operation = COVER_OPERATION_IDLE;
  }
  else if (state == STATE_OPENING)
  {
    this->current_operation = COVER_OPERATION_OPENING;
  }
  else if (state == STATE_STOPPED_OPENING || state == STATE_CLOSE_WARNING || state == STATE_STOPPED_CLOSING)
  {
    this->current_operation = COVER_OPERATION_IDLE;
  }
  else if (state == STATE_OPEN)
  {
    this->position = COVER_OPEN;
    this->current_operation = COVER_OPERATION_IDLE;
  }
  else if (state == STATE_CLOSING)
  {
    this->current_operation = COVER_OPERATION_CLOSING;
  }

  this->publish_state(false);
  if (is_initial_state)
  {
    this->lock_sensor_->publish_initial_state(state == STATE_LOCKED ? LOCK_STATE_LOCKED : LOCK_STATE_UNLOCKED);
  }
  else
  {
    this->lock_sensor_->publish_state(state == STATE_LOCKED ? LOCK_STATE_LOCKED : LOCK_STATE_UNLOCKED);
  }

  const uint32_t now = millis();
  this->last_state_change_time_ = now;
  this->last_position_time_ = now;
}

std::string GarageDoorCover::get_event_(std::string event_type)
{
  std::string event = "esphome.";
  event.append(this->get_object_id());
  event.append(".");
  event.append(event_type);
  return event;
}

const char *GarageDoorCover::internal_state_to_str_(InternalState state)
{
  switch (state)
  {
    case STATE_UNKNOWN:
      return "UNKNOWN";
  
    case STATE_MOVING:
      return "MOVING";

    case STATE_LOCKED:
      return "LOCKED";

    case STATE_CLOSED:
      return "CLOSED";

    case STATE_OPENING:
      return "OPENING";

    case STATE_STOPPED_OPENING:
      return "STOPPED_OPENING";

    case STATE_OPEN:
      return "OPEN";

    case STATE_CLOSE_WARNING:
      return "CLOSE_WARNING";

    case STATE_CLOSING:
      return "CLOSING";

    case STATE_STOPPED_CLOSING:
      return "STOPPED_CLOSING";

    default:
      return "INTERNAL_STATE_UNKNOWN";
  }
}

GarageDoorCover *GarageDoor = new GarageDoorCover();