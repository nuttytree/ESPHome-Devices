#include "esphome.h"

using namespace esphome;

// Open/Close durations
const uint32_t NORMAL_OPEN_DURATION = 10000;
const uint32_t NORMAL_CLOSE_DURATION = 10000;

// Close warning
const std::string CLOSE_WARNING_RTTTL = "Imperial:d=4, o=5, b=100:e, e, e, 8c, 16p, 16g, e, 8c, 16p, 16g, e, p, b, b, b, 8c6, 16p, 16g, d#, 8c, 16p, 16g, e, 8p";

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
};

class GarageDoorLock;

class GarageDoorCover : public cover::Cover, public Component, public api::CustomAPIDevice
{
  public:
    GarageDoorCover(uint8_t closed_pin, uint8_t open_pin, uint8_t control_pin, uint8_t remote_pin, uint8_t remote_light_pin);
    void set_rtttl_buzzer(rtttl::Rtttl *buzzer) { this->buzzer_ = buzzer; }
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override;
    cover::CoverTraits get_traits() override;
    void control(const cover::CoverCall &call) override;
    void loop() override;
    GarageDoorLock *get_lock_sensor() { return this->lock_sensor_; }

  private:
    rtttl::Rtttl *buzzer_;
    GPIOPin *closed_pin_;
    GPIOPin *open_pin_;
    GPIOPin *control_pin_;
    GPIOPin *remote_pin_;
    GPIOPin *remote_light_pin_;
    GarageDoorLock *lock_sensor_;

    InternalState internal_state_{STATE_UNKNOWN};
    bool lock_requested_{false};
    float target_position_{0};
    cover::CoverOperation target_operation_{COVER_OPERATION_IDLE};
    uint32_t last_state_change_time_{0};
    uint32_t last_position_time_{0};
    uint32_t last_publish_time_{0};
    LocalButtonsState last_local_button_{LOCAL_BUTTON_NONE};
    bool last_remote_state_{false};
    bool last_remote_light_state_{false};

    void lock_();
    void unlock_();
    
    void handle_closed_position_();
    void handle_open_position_();
    void determine_current_position_();
    bool check_local_buttons_();
    bool check_remote_buttons_();
    void change_to_next_state_(bool is_from_button_press = false);
    void set_state_(InternalState state, bool is_initial_state = false);
    InternalState get_state_() { return this->internal_state_; }
    const char *internal_state_to_str_(InternalState state);
};

class GarageDoorLock : public binary_sensor::BinarySensor, public Component
{
  public:
    GarageDoorLock() {}
    float get_setup_priority() const override { return setup_priority::DATA; }
    void setup() override {}
    void loop() override {}
};

GarageDoorCover::GarageDoorCover(uint8_t closed_pin, uint8_t open_pin, uint8_t control_pin, uint8_t remote_pin, uint8_t remote_light_pin)
{
  this->set_device_class("garage");
  this->closed_pin_ = new GPIOPin(closed_pin, INPUT_PULLUP, true);
  this->open_pin_ = new GPIOPin(open_pin, INPUT_PULLUP, true);
  this->control_pin_ = new GPIOPin(control_pin, OUTPUT);
  this->remote_pin_ = new GPIOPin(remote_pin, INPUT_PULLUP, true);
  this->remote_light_pin_ = new GPIOPin(remote_light_pin, INPUT_PULLUP, true);
  this->lock_sensor_ = new GarageDoorLock();
}

void GarageDoorCover::setup()
{
  this->closed_pin_->setup();
  this->open_pin_->setup();
  this->control_pin_->setup();
  this->remote_pin_->setup();
  this->remote_light_pin_->setup();

  this->lock_sensor_->publish_initial_state(LOCK_STATE_UNLOCKED);

  if (this->closed_pin_->digital_read())
  {
    this->set_state_(STATE_CLOSED, true);
  }
  else if (this->open_pin_->digital_read())
  {
    this->set_state_(STATE_OPEN, true);
  }
  else
  {
    // this->set_state_(STATE_UNKNOWN, true);
    this->set_state_(STATE_OPEN, true);
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
      if (this->get_state_() == STATE_UNKNOWN)
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
  // "Release" the control "button"
  // this->control_pin_->digital_write(false);

  // if (this->get_state_() == STATE_CLOSE_WARNING)
  // {
  //   if (!this->buzzer_->is_playing())
  //   {
  //     this->change_to_next_state_();
  //   }
  // }
  // else if (this->closed_pin_->digital_read())
  // {
  //   this->handle_closed_position_();
  // }
  // else if (this->open_pin_->digital_read())
  // {
  //   this->handle_open_position_();
  // }
  // else if (this->current_operation == COVER_OPERATION_OPENING || this->current_operation == COVER_OPERATION_CLOSING)
  // {
  //   this->determine_current_position_();
  // }

  // if (this->check_local_buttons_())
  // {
  //   return;
  // }
  // else if (this->check_remote_buttons_())
  // {
  //   return;
  // }
  // else if (this->target_operation_ != this->current_operation)
  // {
  //   this->change_to_next_state_();
  // }
}

void GarageDoorCover::lock_()
{
  switch (this->get_state_())
  {
    case STATE_LOCKED:
      break;

    case STATE_CLOSED:
      this->set_state_(STATE_LOCKED);
      break;

    default:
      this->lock_requested_ = true;
      this->target_operation_ = COVER_OPERATION_CLOSING;
      this->change_to_next_state_();
      break;
  }
}

void GarageDoorCover::unlock_()
{
  if (this->get_state_() == STATE_LOCKED)
  {
    this->set_state_(STATE_CLOSED);
  }
}

void GarageDoorCover::handle_closed_position_()
{
  if (this->lock_requested_)
  {
    this->set_state_(STATE_LOCKED);
    this->lock_requested_ = false;
  }
  else
  {
    this->set_state_(STATE_CLOSED);
  }

  if (this->target_operation_ == COVER_OPERATION_SET_POSITION)
  {
    this->target_operation_ = this->target_position_ == COVER_CLOSED ? COVER_OPERATION_IDLE : COVER_OPERATION_OPENING;
  }
}

void GarageDoorCover::handle_open_position_()
{
  this->set_state_(STATE_OPEN);

  if (this->target_operation_ == COVER_OPERATION_SET_POSITION)
  {
    this->target_operation_ = this->target_position_ == COVER_OPEN ? COVER_OPERATION_IDLE : COVER_OPERATION_CLOSING;
  }
}

void GarageDoorCover::determine_current_position_()
{
  float direction;
  float normal_duration;
  if (this->current_operation == COVER_OPERATION_OPENING)
  {
    direction = 1.0f;
    normal_duration = NORMAL_OPEN_DURATION;
  }
  else
  {
    direction = -1.0f;
    normal_duration = NORMAL_CLOSE_DURATION;
  }

  const uint32_t now = millis();
  const uint32_t current_duration = now - this->last_state_change_time_;
  if (current_duration > normal_duration * 2)
  {
    // This should never happen but if it does we go into an unknown state
    this->set_state_(STATE_UNKNOWN);
  }
  else
  {
    float new_position = clamp(this->position + direction * (now - this->last_position_time_) / normal_duration, 0.01f, 0.99f);
    if (new_position == this->position);
    {
      return;
    }

    this->position = new_position;
    this->last_position_time_ = now;

    if ((direction == 1.0f && new_position >= this->target_position_)
      || (direction == -1.0f && new_position <= this->target_position_))
    {
      this->target_operation_ = COVER_OPERATION_IDLE;
      this->change_to_next_state_();
    }
    else if (now - this->last_publish_time_ > 500) 
    {
      this->publish_state(false);
      this->last_publish_time_ = now;
    }
  }
}

bool GarageDoorCover::check_local_buttons_()
{
  // // This is an initial guess on how the local buttons state will get read
  // float buttonValue = analogRead(A0) / 1024.0f;

  // LocalButtonsState currentButton;
  // if (buttonValue > 0 && buttonValue <= .25)
  // {
  //   currentButton = LOCAL_BUTTON_DOOR;
  // }
  // else if (buttonValue > .25 && buttonValue <= .5)
  // {
  //   currentButton = LOCAL_BUTTON_LOCK;
  // }
  // else if (buttonValue > .5 && buttonValue <= .75)
  // {
  //   currentButton = LOCAL_BUTTON_LIGHT;
  // }
  // else
  // {
  //   currentButton = LOCAL_BUTTON_NONE;
  // }

  // if (this->last_local_button_ != currentButton)
  // {
  //   this->last_local_button_ = currentButton;
  //   switch (currentButton)
  //   {
  //     case LOCAL_BUTTON_DOOR:
  //       this->change_to_next_state_(true);
  //       return true;

  //     case LOCAL_BUTTON_LOCK:
  //       if (this->get_state_() == STATE_LOCKED)
  //       {
  //         this->unlock_();
  //       }
  //       else
  //       {
  //         this->lock_();
  //       }
  //       return true;

  //     case LOCAL_BUTTON_LIGHT:
  //       this->fire_homeassistant_event("second_garage_door_local_light_button");
  //       break;

  //     default:
  //       break;
  //   }
  // }

  return false;
}

bool GarageDoorCover::check_remote_buttons_()
{
  // bool currentRemoteLightState = this->remote_light_pin_->digital_read();
  // if (currentRemoteLightState != this->last_remote_light_state_)
  // {
  //   last_remote_light_state_ = currentRemoteLightState;
  //   if (currentRemoteLightState)
  //   {
  //     this->fire_homeassistant_event("second_garage_door_remote_light_button");
  //   }
  // }

  // bool currentRemoteState = this->remote_pin_->digital_read();
  // if (currentRemoteState != this->last_remote_state_)
  // {
  //   last_remote_state_ = currentRemoteState;
  //   if (currentRemoteState && this->get_state_() != STATE_LOCKED)
  //   {
  //     this->change_to_next_state_(true);
  //     return true;
  //   }
  // }

  return false;
}

void GarageDoorCover::change_to_next_state_(bool is_from_button_press)
{
  switch (this->get_state_())
  {
    case STATE_UNKNOWN:
      this->set_state_(STATE_MOVING);
      break;
  
    case STATE_MOVING:
      this->set_state_(STATE_UNKNOWN);
      break;

    case STATE_LOCKED:
      this->set_state_(STATE_OPENING);
      break;

    case STATE_CLOSED:
      this->set_state_(STATE_OPENING);
      break;

    case STATE_OPENING:
      this->set_state_(STATE_STOPPED_OPENING);
      break;

    case STATE_STOPPED_OPENING:
      this->set_state_(is_from_button_press ? STATE_CLOSING : STATE_CLOSE_WARNING);
      break;

    case STATE_OPEN:
      this->set_state_(is_from_button_press ? STATE_CLOSING : STATE_CLOSE_WARNING);
      break;

    case STATE_CLOSE_WARNING:
      this->set_state_(STATE_CLOSING);
      break;

    case STATE_CLOSING:
      this->set_state_(STATE_STOPPED_CLOSING);
      break;

    case STATE_STOPPED_CLOSING:
      this->set_state_(STATE_OPENING);
      break;

    default:
      break;
  }

  if (is_from_button_press)
  {
    this->target_operation_ = this->current_operation;
  }

  if (this->get_state_() != STATE_CLOSE_WARNING)
  {
    // "Press" the control "button"
    this->control_pin_->digital_write(true);
  }
  else
  {
    this->buzzer_->play(CLOSE_WARNING_RTTTL);
  }
}

void GarageDoorCover::set_state_(InternalState state, bool is_initial_state)
{
  if (this->internal_state_ == state)
  {
    return;
  }

  ESP_LOGD("NuttyGarageDoor", "Internal state: '%s'", this->internal_state_to_str_(state));

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

GarageDoorCover *GarageDoor = new GarageDoorCover(D1, D2, D5, D6, D7);