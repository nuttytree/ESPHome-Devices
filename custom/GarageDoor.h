#include "esphome.h"

using namespace esphome;

static const char* TAG = "NuttyGarageDoor";

// Open/Close durations
const uint32_t NORMAL_OPEN_DURATION = 10000;
const uint32_t NORMAL_CLOSE_DURATION = 10000;

// Amount of time to keep the control pin active/inactive when changing states
const uint32_t CONTROL_PIN_ACTIVE_DURATION = 10;
const uint32_t CONTROL_PIN_INACTIVE_DURATION = 10;

// Number of milliseconds between publishing the state while the door is opening or closing
const uint32_t DOOR_MOVING_PUBLISH_INTERVAL = 250;

// Number of milliseconds between reads of the ADC to get local button state, this is needed to prevent wifi issues from reading to frequently
const uint32_t LOCAL_BUTTON_READ_INTERVAL = 50;

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

// Special ESPHome cover operation states
const cover::CoverOperation COVER_OPERATION_UNKNOWN = static_cast<cover::CoverOperation>(100);
const cover::CoverOperation COVER_OPERATION_SET_POSITION = static_cast<cover::CoverOperation>(200);

// Lock Binary sensor state aliases
const bool LOCK_STATE_LOCKED = false;
const bool LOCK_STATE_UNLOCKED = true;

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
  STATE_STOPPED_CLOSING
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

  private:
    rtttl::Rtttl *buzzer_;
    GPIOPin *control_pin_;
    GPIOPin *closed_pin_;
    GPIOPin *open_pin_;
    GPIOPin *remote_pin_;
    GPIOPin *remote_light_pin_;
    GarageDoorLock *lock_sensor_;

    InternalState internal_state_{STATE_UNKNOWN};
    bool lock_requested_{false};
    float target_position_{0};
    cover::CoverOperation target_operation_{COVER_OPERATION_IDLE};
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
    bool ensure_target_operation_();
    bool check_for_closed_position_();
    bool check_for_open_position_();
    bool check_for_position_update_();
    bool check_local_buttons_();
    bool check_remote_buttons_();
    void change_to_next_state_(bool is_from_button_press = false);
    void set_internal_state_(InternalState state, bool is_initial_state = false);
    InternalState get_internal_state_() { return this->internal_state_; }
    std::string get_event_(std::string event_type);
    const char *internal_state_to_str_(InternalState state);
    const char *operation_to_str_(cover::CoverOperation operation);
};

GarageDoorCover::GarageDoorCover()
{
  this->set_device_class("garage");
  this->control_pin_ = new GPIOPin(PIN_CONTROL_RELAY, OUTPUT);
  this->closed_pin_ = new GPIOPin(PIN_CLOSED_SENSOR, INPUT_PULLUP, true);
  this->open_pin_ = new GPIOPin(PIN_OPEN_SENSOR, INPUT_PULLUP, true);
  this->remote_pin_ = new GPIOPin(PIN_REMOTE_BUTTON, INPUT_PULLUP, true);
  this->remote_light_pin_ = new GPIOPin(PIN_REMOTE_LIGHT_BUTTON, INPUT_PULLUP, true);
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

  // TODO: Switch back to reading the sensor when it is actually connected
  // if (this->closed_pin_->digital_read())
  if (true)
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
  // Control doesn't support the lock function so any control request will clear a lock request
  this->lock_requested_ = false;

  if (call.get_stop()) 
  {
    this->target_operation_ = COVER_OPERATION_IDLE;
    if (this->current_operation == COVER_OPERATION_OPENING || this->current_operation == COVER_OPERATION_CLOSING)
    {
      this->change_to_next_state_();
    }
  }
  else if (call.get_position().has_value())
  {
    this->target_position_ = *call.get_position();
    if (this->target_position_ == this->position && this->current_operation == COVER_OPERATION_IDLE)
    {
      return;
    }
    else if (this->current_operation == COVER_OPERATION_UNKNOWN)
    {
      if (this->get_internal_state_() == STATE_UNKNOWN)
      {
        this->target_operation_ = COVER_OPERATION_SET_POSITION;
        this->change_to_next_state_();
      }
    }
    else if (this->target_position_ < this->position)
    {
      this->target_operation_ = COVER_OPERATION_CLOSING;
      this->change_to_next_state_();
    }
    else if (this->target_position_ > this->position)
    {
      this->target_operation_ = COVER_OPERATION_OPENING;
      this->change_to_next_state_();
    }
  }
}

void GarageDoorCover::loop()
{
  if (this->check_control_pin_())
  {
    return;
  }

  if (this->ensure_target_operation_())
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
  switch (this->get_internal_state_())
  {
    case STATE_LOCKED:
      break;

    case STATE_CLOSED:
      this->set_internal_state_(STATE_LOCKED);
      break;

    default:
      this->lock_requested_ = true;
      this->target_position_ = COVER_CLOSED;
      this->target_operation_ = COVER_OPERATION_CLOSING;
      this->change_to_next_state_();
      break;
  }
}

void GarageDoorCover::unlock_()
{
  if (this->get_internal_state_() == STATE_LOCKED)
  {
    this->set_internal_state_(STATE_CLOSED);
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

bool GarageDoorCover::ensure_target_operation_()
{
  if (this->target_operation_ != COVER_OPERATION_SET_POSITION && this->target_operation_ != this->current_operation && !this->buzzer_->is_playing())
  {
    ESP_LOGD(TAG, "Ensuring Target Operation:");
    ESP_LOGD(TAG, "  Current Operation: '%s'", this->operation_to_str_(this->current_operation));
    ESP_LOGD(TAG, "  Target Operation:  '%s'", this->operation_to_str_(this->target_operation_));

    this->change_to_next_state_();
    return true;
  }
  else
  {
    return false;
  }
}

bool GarageDoorCover::check_for_closed_position_()
{
  // TODO: Switch back to reading the sensor when it is actually connected
  // if (this->current_operation != COVER_OPERATION_IDLE && this->closed_pin_->digital_read())
  if (this->current_operation == COVER_OPERATION_CLOSING && this->target_operation_ == COVER_OPERATION_CLOSING && this->position <= .02)
  {
    if (this->lock_requested_)
    {
      this->set_internal_state_(STATE_LOCKED);
      this->lock_requested_ = false;
    }
    else
    {
      this->set_internal_state_(STATE_CLOSED);
    }

    // Have to use the static cast because of the extra values that are used
    switch (static_cast<uint8_t>(this->target_operation_))
    {
      case COVER_OPERATION_CLOSING:
        this->target_operation_ = COVER_OPERATION_IDLE;
        break;

      case COVER_OPERATION_SET_POSITION:
        this->target_operation_ = this->target_position_ == COVER_CLOSED ? COVER_OPERATION_IDLE : COVER_OPERATION_OPENING;
        if (this->target_position_ != COVER_CLOSED)
        {
          this->change_to_next_state_();
        }
        break;

      case COVER_OPERATION_OPENING:
        this->fire_homeassistant_event(this->get_event_(EVENT_FAILED_OPEN));
        break;

      default:
        break;
    }

    return true;
  }
  else
  {
    return false;
  }
}

bool GarageDoorCover::check_for_open_position_()
{
  // TODO: Switch back to reading the sensor when it is actually connected
  // if (this->current_operation != COVER_OPERATION_IDLE && this->open_pin_->digital_read())
  if (this->current_operation == COVER_OPERATION_OPENING && this->target_operation_ == COVER_OPERATION_OPENING && this->position >= .98)
  {
    this->set_internal_state_(STATE_OPEN);

    // Have to use the static cast because of the extra values that are used
    switch (static_cast<uint8_t>(this->target_operation_))
    {
      case COVER_OPERATION_OPENING:
        this->target_operation_ = COVER_OPERATION_IDLE;
        break;

      case COVER_OPERATION_SET_POSITION:
        this->target_operation_ = this->target_position_ == COVER_OPEN ? COVER_OPERATION_IDLE : COVER_OPERATION_CLOSING;
        if (this->target_position_ != COVER_OPEN)
        {
          this->change_to_next_state_();
        }
        break;

      case COVER_OPERATION_CLOSING:
        this->fire_homeassistant_event(this->get_event_(EVENT_FAILED_CLOSE));
        break;

      default:
        break;
    }

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
  switch (this->current_operation)
  {
    case COVER_OPERATION_OPENING:
      direction = 1.0f;
      normal_duration = NORMAL_OPEN_DURATION;
      break;
  
    case COVER_OPERATION_CLOSING:
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
    if ((direction == 1.0f && this->position >= this->target_position_)
      || (direction == -1.0f && this->position <= this->target_position_))
    {
      this->target_operation_ = COVER_OPERATION_IDLE;
      this->change_to_next_state_();
      return true;
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
          this->change_to_next_state_(true);
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
      this->change_to_next_state_(true);
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

void GarageDoorCover::change_to_next_state_(bool is_from_button_press)
{
  if (millis() < this->control_pin_inactive_time_ + CONTROL_PIN_INACTIVE_DURATION)
  {
    return;
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
      this->set_internal_state_(is_from_button_press || this->target_operation_ == COVER_OPERATION_OPENING ? STATE_CLOSING : STATE_CLOSE_WARNING);
      break;

    case STATE_OPEN:
      this->set_internal_state_(is_from_button_press ? STATE_CLOSING : STATE_CLOSE_WARNING);
      break;

    case STATE_CLOSE_WARNING:
      if (is_from_button_press)
      {
        this->set_internal_state_(this->position == COVER_OPEN ? STATE_OPEN : STATE_STOPPED_OPENING);
      }
      else if (this->target_operation_ == COVER_OPERATION_CLOSING && !this->buzzer_->is_playing())
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

  if (is_from_button_press)
  {
    this->target_operation_ = this->current_operation;
    if (this->current_operation == COVER_OPERATION_OPENING)
    {
      this->target_position_ = COVER_OPEN;
    }
    else if (this->current_operation == COVER_OPERATION_CLOSING)
    {
      this->target_position_ = COVER_CLOSED;
    }
  }

  if (this->get_internal_state_() != STATE_CLOSE_WARNING)
  {
    // "Press" the control "button"
    this->control_pin_->digital_write(true);
    this->control_pin_active_time_ = millis();
  }
  else if (!this->buzzer_->is_playing())
  {
    this->buzzer_->play(CLOSE_WARNING_RTTTL);
  }
}

void GarageDoorCover::set_internal_state_(InternalState state, bool is_initial_state)
{
  if (this->internal_state_ == state)
  {
    return;
  }

  ESP_LOGD(TAG, "Setting Internal State:");
  ESP_LOGD(TAG, "  Current State: '%s'", this->internal_state_to_str_(this->get_internal_state_()));
  ESP_LOGD(TAG, "  New State:     '%s'", this->internal_state_to_str_(state));

  this->internal_state_ = state;

  if (state == STATE_UNKNOWN)
  {
    this->position = 0.5f;
    this->target_operation_ = COVER_OPERATION_UNKNOWN;
    this->current_operation = COVER_OPERATION_UNKNOWN;
  }
  else if (state == STATE_MOVING)
  {
    this->position = 0.5f;
    this->target_operation_ = COVER_OPERATION_SET_POSITION;
    this->current_operation = COVER_OPERATION_SET_POSITION;
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

const char *GarageDoorCover::operation_to_str_(cover::CoverOperation operation)
{
  // Have to use the static cast because of the extra values that are used
  switch (static_cast<uint8_t>(operation))
  {
    case COVER_OPERATION_UNKNOWN:
      return "UNKNOWN";
  
    case COVER_OPERATION_SET_POSITION:
      return "SET_POSITION";
  
    case COVER_OPERATION_IDLE:
      return "IDLE";
  
    case COVER_OPERATION_OPENING:
      return "OPENING";
  
    case COVER_OPERATION_CLOSING:
      return "CLOSING";
  
    default:
      return "OPERATION_UNKNOWN";
  }
}

GarageDoorCover *GarageDoor = new GarageDoorCover();