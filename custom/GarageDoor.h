#include "esphome.h"

using namespace esphome;

// Open/Close durations
const uint32_t NORMAL_OPEN_DURATION = 5000;
const uint32_t FAILED_OPEN_DURATION = NORMAL_OPEN_DURATION * 2;
const uint32_t NORMAL_CLOSE_DURATION = 5000;
const uint32_t FAILED_CLOSE_DURATION = NORMAL_CLOSE_DURATION * 2;

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
  // The door is "locked" which is the same as closed but will not open for remotes (but buttons and Home Assistant can open or unlock it)
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

enum LocalButtonsState : uint8_t {
  LOCAL_BUTTON_NONE = 0,
  LOCAL_BUTTON_DOOR,
  LOCAL_BUTTON_LOCK,
  LOCAL_BUTTON_LIGHT
}

class GarageDoorLock;

class GarageDoor : public cover::Cover, public Component, public api::CustomAPIDevice
{
  public:
    GarageDoor(uint8_t closed_pin, uint8_t open_pin, uint8_t control_pin);
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override;
    cover::CoverTraits get_traits() override;
    void control(const cover::CoverCall &call) override;
    void loop() override;
    GarageDoorLock *get_lock_sensor() { return this->lock_sensor_; }

  private:
    GPIOPin *closed_pin_;
    GPIOPin *open_pin_;
    GPIOPin *control_pin_;
    GarageDoorLock *lock_sensor_;

    InternalState internal_state_{STATE_UNKNOWN};
    bool lock_requested_{false};
    float target_position_{0};
    uint32_t last_state_change_time_{0};
    cover::CoverOperation target_operation_{COVER_OPERATION_IDLE};
    uint32_t last_publish_time_{0};
    LocalButtonsState last_local_button_{LOCAL_BUTTON_NONE};

    void lock_();
    void unlock_();
    
    void handle_closed_position_();
    void handle_open_position_();
    void determine_current_position_();
    bool check_local_buttons_();
    bool check_remote_buttons_();
    void change_to_next_state_(bool is_local = false);
    void set_current_operation_(bool set_target_also = false);
};

class GarageDoorLock : public binary_sensor::BinarySensor, public Component
{
  public:
    GarageDoorLock() {}
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override {}
    void loop() override {}
};

GarageDoor::GarageDoor(uint8_t closed_pin, uint8_t open_pin, uint8_t control_pin)
{
  this->closed_pin_ = new GPIOPin(closed_pin, INPUT);
  this->open_pin_ = new GPIOPin(open_pin, INPUT);
  this->control_pin_ = new GPIOPin(control_pin, OUTPUT);
  this->lock_sensor_ = new GarageDoorLock();
}

void GarageDoor::setup()
{
  this->lock_sensor_->publish_initial_state(LOCK_STATE_UNLOCKED);

  if (this->closed_pin_->digital_read())
  {
    this->internal_state_ = STATE_CLOSED;
    this->current_operation = COVER_OPERATION_IDLE;
    this->position = COVER_CLOSED;
  }
  else if (this->open_pin_->digital_read())
  {
    this->internal_state_ = STATE_OPEN;
    this->current_operation = COVER_OPERATION_IDLE;
    this->position = COVER_OPEN;
  }
  else
  {
    this->internal_state_ = STATE_UNKNOWN;
    this->target_operation_ = COVER_OPERATION_UNKNOWN;
    this->current_operation = COVER_OPERATION_UNKNOWN;
    this->position = 0.5f;
  }

  this->register_service(&GarageDoor::lock_, "lock");
  this->register_service(&GarageDoor::unlock_, "unlock");
}

cover::CoverTraits GarageDoor::get_traits() {
  auto traits = CoverTraits();
  traits.set_is_assumed_state(false);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  return traits;
}

void GarageDoor::control(const cover::CoverCall &call)
{
  // Control doesn't support the lock function so any control request will clear a lock request
  this->lock_requested_ = false;

  if (call.get_stop()) 
  {
    this->target_operation_ = COVER_OPERATION_IDLE;
    if (this->current_operation != COVER_OPERATION_IDLE)
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
      if (this->internal_state_ == STATE_UNKNOWN)
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

void GarageDoor::loop()
{
  // "Release" the control "button"
  this->control_pin_->digital_write(false);

  if (this->internal_state_ == STATE_CLOSE_WARNING)
  {
    // TODO: If done with warning change to closing as the target operation
  }
  else if (this->closed_pin_->digital_read())
  {
    this->handle_closed_position_();
  }
  else if (this->open_pin_->digital_read())
  {
    this->handle_open_position_();
  }
  else if (this->current_operation == COVER_OPERATION_OPENING || this->current_operation == COVER_OPERATION_CLOSING)
  {
    this->determine_current_position_();
  }

  if (this->check_local_buttons_())
  {
    return;
  }
  else if (this->check_remote_buttons_())
  {
    return;
  }
  else if (this->target_operation_ != this->current_operation)
  {
    this->change_to_next_state_();
  }

  if (this->internal_state_ != STATE_LOCKED && this->lock_sensor_->state == LOCK_STATE_LOCKED)
  {
    this->lock_sensor_->publish_state(LOCK_STATE_UNLOCKED);
  }
}

void GarageDoor::handle_closed_position_()
{
  this->internal_state_ = STATE_CLOSED;
  this->position = COVER_CLOSED;
  this->current_operation = COVER_OPERATION_IDLE;
  this->publish_state(false);

  if (this->lock_requested_)
  {
    this->internal_state_ = STATE_LOCKED;
    this->lock_requested_ = false;
    this->lock_sensor_->publish_state(LOCK_STATE_LOCKED);
  }

  if (this->target_operation_ == COVER_OPERATION_SET_POSITION)
  {
    if (this->target_position_ != COVER_CLOSED)
    {
      this->target_operation_ = COVER_OPERATION_OPENING;
    }
    else
    {
      this->target_operation_ = COVER_OPERATION_IDLE;
    }
  }
}

void GarageDoor::handle_open_position_()
{
  this->internal_state_ = STATE_OPEN;
  this->position = COVER_OPEN;
  this->current_operation = COVER_OPERATION_IDLE;
  this->publish_state(false);

  if (this->target_operation_ == COVER_OPERATION_SET_POSITION)
  {
    if (this->target_position_ != COVER_OPEN)
    {
      this->target_operation_ = COVER_OPERATION_CLOSING;
    }
    else
    {
      this->target_operation_ = COVER_OPERATION_IDLE;
    }
  }
}

void GarageDoor::determine_current_position_()
{
  float direction;
  float normal_duration;
  float failed_duration;
  if (this->current_operation == COVER_OPERATION_OPENING)
  {
    direction = 1.0f;
    normal_duration = NORMAL_OPEN_DURATION;
    failed_duration = FAILED_OPEN_DURATION;
  }
  else
  {
    direction = -1.0f;
    normal_duration = NORMAL_CLOSE_DURATION;
    failed_duration = FAILED_CLOSE_DURATION;
  }

  const uint32_t now = millis();
  const uint32_t current_duration = now - this->last_state_change_time_;
  if (current_duration >= failed_duration)
  {
    // This should never happen but if it does we go into an unknown state
    this->internal_state_ = STATE_UNKNOWN;
    this->target_operation_ = COVER_OPERATION_UNKNOWN;
    this->current_operation = COVER_OPERATION_UNKNOWN;
    this->position = 0.5f;
  }
  else
  {
    this->position = direction * current_duration / normal_duration;
    this->position = clamp(this->position, 0.01f, 0.99f);

    if ((direction == 1.0f && this->position >= this->target_position_)
      || (direction == -1.0f && this->position <= this->target_position_))
    {
      this->target_operation_ = COVER_OPERATION_IDLE;
    }
  }

  if (now - this->last_publish_time_ > 1000) 
  {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}

bool GarageDoor::check_local_buttons_()
{
  float buttonValue = analogRead(A0) / 1024.0f;

  LocalButtonsState currentButton;
  if (buttonValue > 0 && buttonValue <= .25)
  {
    currentButton = LOCAL_BUTTON_DOOR;
  }
  else if (buttonValue > .25 && buttonValue <= .5)
  {
    currentButton = LOCAL_BUTTON_LOCK;
  }
  else if (buttonValue > .5 && buttonValue <= .75)
  {
    currentButton = LOCAL_BUTTON_LIGHT;
  }
  else
  {
    currentButton = LOCAL_BUTTON_NONE;
  }

  if (this->last_local_button_ == currentButton)
  {
    return false;
  }
  else
  {
    // TODO: Handle buttons
    return true;
  }
}

bool GarageDoor::check_remote_buttons_()
{
  // TODO: Actually check the buttons

  return false;
}

void GarageDoor::change_to_next_state_(bool is_local)
{
  switch (this->internal_state_)
  {
    case STATE_UNKNOWN:
      this->internal_state_ = STATE_MOVING;
      break;
  
    case STATE_MOVING:
      this->internal_state_ = STATE_UNKNOWN;
      break;

    case STATE_LOCKED:
      this->internal_state_ = STATE_OPENING;
      break;

    case STATE_CLOSED:
      this->internal_state_ = STATE_OPENING;
      break;

    case STATE_OPENING:
      this->internal_state_ = STATE_STOPPED_OPENING;
      break;

    case STATE_STOPPED_OPENING:
      this->internal_state_ = is_local ? STATE_CLOSING : STATE_CLOSE_WARNING;
      break;

    case STATE_OPEN:
      this->internal_state_ = is_local ? STATE_CLOSING : STATE_CLOSE_WARNING;
      break;

    case STATE_CLOSE_WARNING:
      this->internal_state_ = STATE_CLOSING;
      break;

    case STATE_CLOSING:
      this->internal_state_ = STATE_STOPPED_CLOSING;
      break;

    case STATE_STOPPED_CLOSING:
      this->internal_state_ = STATE_OPENING;
      break;

    default:
      break;
  }

  this->set_current_operation_();

  this->last_state_change_time_ = millis();
  if (this->internal_state_ != STATE_CLOSE_WARNING)
  {
    this->control_pin_->digital_write(true);
  }
  else
  {
    // TODO: Add BEEP, BEEP, BEEP, ....
  }
}

void GarageDoor::set_current_operation_(bool set_target_also)
{
  if (this->internal_state_ == STATE_UNKNOWN 
    || this->internal_state_ == STATE_MOVING)
  {
    this->current_operation = COVER_OPERATION_UNKNOWN;
  }
  else if (this->internal_state_ == STATE_LOCKED 
    || this->internal_state_ == STATE_CLOSED 
    || this->internal_state_ == STATE_STOPPED_OPENING 
    || this->internal_state_ == STATE_OPEN
    || this->internal_state_ == STATE_CLOSE_WARNING
    || this->internal_state_ == STATE_STOPPED_CLOSING)
  {
    this->current_operation = COVER_OPERATION_IDLE;
  }
  else if (this->internal_state_ == STATE_OPENING)
  {
    this->current_operation = COVER_OPERATION_OPENING;
  }
  else if (this->internal_state_ == STATE_CLOSING)
  {
    this->current_operation = COVER_OPERATION_CLOSING;
  }

  if (set_target_also)
  {
    this->target_operation_ = this->current_operation;
  }

  this->publish_state();
}

void GarageDoor::lock_()
{
  switch (this->internal_state_)
  {
    case STATE_LOCKED:
      break;

    case STATE_CLOSED:
      this->internal_state_ = STATE_LOCKED;
      this->lock_sensor_->publish_state(LOCK_STATE_LOCKED);
      break;

    default:
      this->lock_requested_ = true;
      this->change_to_next_state_();
      break;
  }
}

void GarageDoor::unlock_()
{
  if (this->internal_state_ == STATE_LOCKED)
  {
    this->internal_state_ = STATE_CLOSED;
    this->lock_sensor_->publish_state(LOCK_STATE_UNLOCKED);
  }
}

GarageDoor *GarageDoorDevice;