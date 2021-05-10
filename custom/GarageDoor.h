#include "esphome.h"

using namespace esphome;

// Open/Close durations
const uint32_t NORMAL_OPEN_DURATION = 5000;
const uint32_t MAX_OPEN_DURATION = NORMAL_OPEN_DURATION * 2;
const uint32_t NORMAL_CLOSE_DURATION = 5000;
const uint32_t MAX_CLOSE_DURATION = NORMAL_OPEN_DURATION * 2;

// Special ESPHome states
const uint8_t COVER_OPERATION_UNKNOWN = 100;
const uint8_t COVER_OPERATION_SET_POSITION = 200;

// Internal states
const uint8_t STATE_UNKNOWN         = 0;
const uint8_t STATE_MOVING          = 1;
const uint8_t STATE_LOCKED          = 2;
const uint8_t STATE_CLOSED          = 3;
const uint8_t STATE_OPENING         = 4;
const uint8_t STATE_STOPPED_OPENING = 5;
const uint8_t STATE_OPEN            = 6;
const uint8_t STATE_CLOSE_WARNING   = 7;
const uint8_t STATE_CLOSING         = 8;
const uint8_t STATE_STOPPED_CLOSING = 9;

class GarageDoor : public cover::Cover, public Component, public CustomAPIDevice
{
  public:
    GarageDoor(uint8_t closed_pin, uint8_t open_pin, uint8_t control_pin);
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override;
    CoverTraits get_traits() override;
    void control(const CoverCall &call) override;
    void loop() override;

  private:
    GPIOPin *closed_pin_;
    GPIOPin *opened_pin_;
    GPIOPin *control_pin_;

    uint8_t internal_state_{STATE_UNKNOWN};
    bool lock_requested_{false};
    float target_position_{0};
    uint32_t started_moving_time_{0};
    uint8_t target_operation_{COVER_OPERATION_IDLE};

    void set_next_state_(bool is_automation);
    void set_current_operation_();
}

GarageDoor::GarageDoor(uint8_t closed_pin, uint8_t opened_pin, uint8_t control_pin) :
  StateMachine(ST_MAX_STATES)
{
  this->closed_pin_ = new GPIOPin(closed_pin, INPUT);
  this->opened_pin_ = new GPIOPin(opened_pin, INPUT);
  this->control_pin_ = new GPIOPin(control_pin, OUTPUT);
}

void GarageDoor::setup()
{
  if (this->closed_pin_->digital_read())
  {
    this->internal_state_ = STATE_CLOSED;
    this->current_operation = COVER_OPERATION_IDLE;
    this->postion = COVER_CLOSED;
  }
  else if (this->open_pin_->digital_read())
  {
    this->internal_state_ = STATE_OPEN;
    this->current_operation = COVER_OPERATION_IDLE;
    this->postion = COVER_OPEN;
  }
  else
  {
    // We make some assumptions about the current state reported to Home Assistant
    this->internal_state_ = STATE_UNKNOWN;
    this->current_operation = COVER_OPERATION_UNKNOWN;
    this->postion = 0.5f;
  }
}

GarageDoor::CoverTraits get_traits() {
  auto traits = CoverTraits();
  traits.set_is_assumed_state(false);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  return traits;
}

void GarageDoor::control(const CoverCall &call)
{
  // Control doesn't support the lock function so any control request will clear a lock request
  this->lock_requested_ = false;

  if (call.get_stop()) 
  {
    this->target_operation_ = COVER_OPERATION_IDLE;
    if (this->current_operation != COVER_OPERATION_IDLE)
    {
      this->set_next_state_(true);
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
        this->set_next_state_(true);
      }
    }
    else if (this->target_position_ < this->position)
    {
      this->target_operation_ = COVER_OPERATION_CLOSING;
      this->set_next_state_(true);
    }
    else if (this->target_position_ > this->position)
    {
      this->target_operation_ = COVER_OPERATION_OPENING;
      this->set_next_state_(true);
    }
  }
}

void GarageDoor::loop()
{
  if (this->closed_pin_->digital_read())
  {
  }
  else if (this->open_pin_->digital_read())
  {
  }
  else
  {
    // TODO: Calculate position
  }
}

void GarageDoor::set_next_state_(bool is_automation)
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
      this->internal_state_ = is_automation ? STATE_CLOSE_WARNING : STATE_CLOSING;
      break;

    case STATE_OPEN:
      this->internal_state_ = is_automation ? STATE_CLOSE_WARNING : STATE_CLOSING;
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

  if (this->internal_state_ != STATE_CLOSE_WARNING)
  {
    // "click" the button
  }

  this->set_current_operation_();
}

void GarageDoor::set_current_operation_()
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

  this->publish_state();
}
