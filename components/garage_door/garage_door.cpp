#include "garage_door.h"

namespace esphome {
namespace cover {

using namespace esphome::lock;

static const char* TAG = "GarageDoor";

// Minimum number of milliseconds to keep the control pin active/inactive when changing states
const uint32_t CONTROL_PIN_ACTIVE_DURATION = 200;
const uint32_t CONTROL_PIN_INACTIVE_DURATION = 200;

// Number of milliseconds between publishing the state while the door is opening or closing
const uint32_t DOOR_MOVING_PUBLISH_INTERVAL = 1000;

// Events
const std::string EVENT_LOCAL_LIGHT = "local_light_button";
const std::string EVENT_REMOTE_LIGHT = "remote_light_button";
const std::string EVENT_FAILED_OPEN = "open_failed";
const std::string EVENT_FAILED_CLOSE = "close_failed";
const std::string EVENT_BUTTON_DISCONNECTED = "button_disconnected";

void GarageDoorLock::setup() {
  LockState restored_state{};
  this->rtc_.load(&restored_state);
  if (restored_state == LOCK_STATE_LOCKED) {
    this->garage_door_->lock_();
  }
}

void GarageDoorLock::control(const lock::LockCall &call) {
  auto state = *call.get_state();
  if (state == lock::LOCK_STATE_LOCKED) {
    this->publish_state(lock::LOCK_STATE_LOCKING);
    this->garage_door_->lock_();
  } else if (state == lock::LOCK_STATE_UNLOCKED) {
    this->garage_door_->unlock_();
  }
}

GarageDoor::GarageDoor() {
  this->lock_comp_ = new GarageDoorLock(this);
  App.register_component(this->lock_comp_);
  App.register_lock(this->lock_comp_);
}

void GarageDoor::set_button_sensor(sensor::Sensor *button_sensor) {
  this->button_sensor_ = button_sensor;
  this->button_sensor_->add_on_state_callback([this](float state) {
    // Expected values:
    // 0   = Disconnected
    // 133 = None
    // 434 = Light
    // 714 = Lock
    // 993 = Door
    LocalButton currentButton;
    if (state < 50) {
      currentButton = LOCAL_BUTTON_DISCONNECTED;
    } else if (state < 284) {
      currentButton = LOCAL_BUTTON_NONE;
    } else if (state < 574) {
      currentButton = LOCAL_BUTTON_LIGHT;
    } else if (state < 854) {
      currentButton = LOCAL_BUTTON_LOCK;
    } else {
      currentButton = LOCAL_BUTTON_DOOR;
    }

    if (currentButton != this->last_local_button_) {
      this->last_local_button_ = currentButton;

      switch (currentButton) {
        case LOCAL_BUTTON_DISCONNECTED:
          ESP_LOGD(TAG, "Local button is disconnected");
          ESP_LOGD(TAG, "  Read Value: %u", state);
          this->fire_homeassistant_event(this->get_event_(EVENT_BUTTON_DISCONNECTED));
          break;
        case LOCAL_BUTTON_NONE:
          ESP_LOGD(TAG, "No local buttons currently pressed");
          ESP_LOGD(TAG, "  Read Value: %u", state);
          break;
        case LOCAL_BUTTON_LIGHT:
          ESP_LOGD(TAG, "Local light button was pressed");
          ESP_LOGD(TAG, "  Read Value: %u", state);
          this->fire_homeassistant_event(this->get_event_(EVENT_LOCAL_LIGHT));
          break;
        case LOCAL_BUTTON_LOCK:
          ESP_LOGD(TAG, "Local lock button was pressed");
          ESP_LOGD(TAG, "  Read Value: %u", state);
          if (this->internal_state_ == INTERNAL_STATE_LOCKED) {
            this->unlock_();
          } else {
            this->lock_();
          }
          break;
        case LOCAL_BUTTON_DOOR:
          ESP_LOGD(TAG, "Local door button was pressed");
          ESP_LOGD(TAG, "  Read Value: %u", state);
          this->handle_button_press_(true);
          break;
        default:
          break;
      }
    }
  });
}

void GarageDoor::set_closed_sensor(binary_sensor::BinarySensor *closed_sensor) {
  this->closed_sensor_ = closed_sensor;
  this->closed_sensor_->add_on_state_callback([this](bool state) {
    if (state) {
      ESP_LOGD(TAG, "Closed sensor is active");
      this->set_state_(PHYSICAL_STATE_CLOSED, INTERNAL_STATE_CLOSED);
    } else {
      ESP_LOGD(TAG, "Closed sensor is inactive");
    }
  });
}

void GarageDoor::set_open_sensor(binary_sensor::BinarySensor *open_sensor) {
  this->open_sensor_ = open_sensor;
  this->open_sensor_->add_on_state_callback([this](bool state) {
    if (state) {
      ESP_LOGD(TAG, "Open sensor is active");
      this->set_state_(PHYSICAL_STATE_OPEN, INTERNAL_STATE_OPEN);
    } else {
      ESP_LOGD(TAG, "Open sensor is inactive");
    }
  });
}

void GarageDoor::set_remote_sensor(binary_sensor::BinarySensor *remote_sensor) {
  this->remote_sensor_ = remote_sensor;
  this->remote_sensor_->add_on_state_callback([this](bool state) {
    ESP_LOGD(TAG, "Remote door button was pressed");
    if (state && this->internal_state_ != INTERNAL_STATE_LOCKED) {
      this->handle_button_press_(false);
    }
  });
}

void GarageDoor::set_remote_light_sensor(binary_sensor::BinarySensor *remote_light_sensor) {
  this->remote_light_sensor_ = remote_light_sensor;
  this->remote_light_sensor_->add_on_state_callback([this](bool state) {
    ESP_LOGD(TAG, "Remote light button was pressed");
    this->fire_homeassistant_event(this->get_event_(EVENT_REMOTE_LIGHT));
  });
}

void GarageDoor::set_warning_rtttl(rtttl::Rtttl *warning_rtttl) {
  this->warning_rtttl_ = warning_rtttl;
  this->warning_rtttl_->add_on_finished_playback_callback([this]() {
    this->activate_control_output_();
  });
}

cover::CoverTraits GarageDoor::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  traits.set_is_assumed_state(false);
  return traits;
}

void GarageDoor::setup() {
  if (this->closed_sensor_->state) {
    this->set_state_(PHYSICAL_STATE_CLOSED, INTERNAL_STATE_CLOSED, true);
  } else if (this->open_sensor_->state) {
    this->set_state_(PHYSICAL_STATE_OPEN, INTERNAL_STATE_OPEN, true);
  } else {
    this->set_state_(PHYSICAL_STATE_UNKNOWN, INTERNAL_STATE_UNKNOWN, true);
  }
}

void GarageDoor::loop() {
  this->recompute_position_();
  this->ensure_target_state_();
  
  const uint32_t now = millis();
  if (this->current_operation != COVER_OPERATION_IDLE && now - this->last_publish_time_ > DOOR_MOVING_PUBLISH_INTERVAL) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}

void GarageDoor::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->target_state_ = TARGET_STATE_STOPPED;
  } else if (call.get_position().has_value()) {
    this->target_position_ = *call.get_position();
    if (this->target_position_ == COVER_CLOSED) {
      this->target_state_ = this->internal_state_ == INTERNAL_STATE_LOCKED ? TARGET_STATE_LOCKED : TARGET_STATE_CLOSED;
    } else if (this->target_position_ == COVER_OPEN) {
      this->target_state_ = TARGET_STATE_OPEN;
    } else {
      this->target_state_ = TARGET_STATE_POSITION;
    }

    if (!this->is_at_target_position_() && this->target_position_ < this->position && this->current_operation != COVER_OPERATION_CLOSING) {
      this->warning_rtttl_->play(this->close_warning_tones_);
      this->set_state_(this->physical_state_, INTERNAL_STATE_CLOSE_WARNING);
    }
  }
}

void GarageDoor::lock_() {
  this->target_state_ = TARGET_STATE_LOCKED;
}

void GarageDoor::unlock_() {
  if (this->internal_state_ == INTERNAL_STATE_LOCKED) {
    this->target_state_ = TARGET_STATE_CLOSED;
  }
}

void GarageDoor::recompute_position_() {
  float direction;
  float normal_duration;
  switch (this->internal_state_) {
    case INTERNAL_STATE_OPENING:
      direction = 1.0f;
      normal_duration = this->open_duration_;
      break;
    case INTERNAL_STATE_CLOSING:
      direction = -1.0f;
      normal_duration = this->close_duration_;
      break;
    case INTERNAL_STATE_UNKNOWN:
      if (this->closed_sensor_->state) {
        this->set_state_(PHYSICAL_STATE_CLOSED, INTERNAL_STATE_CLOSED);
      } else if (this->open_sensor_->state) {
        this->set_state_(PHYSICAL_STATE_OPEN, INTERNAL_STATE_OPEN);
      }
      return;
    default:
      return;
  }

  const uint32_t now = millis();
  this->position += direction * (now - this->last_recompute_time_) / normal_duration;
  this->position = clamp(this->position, 0.01f, .99f);
  this->last_recompute_time_ = now;
}

void GarageDoor::ensure_target_state_() {
  const uint32_t now = millis();

  if (this->control_output_state_) {
    if (now - this->control_output_state_change_time_ >= CONTROL_PIN_ACTIVE_DURATION) {
      this->control_output_->turn_off();
      this->control_output_state_ = false;
      this->control_output_state_change_time_ = now;
    }
    return;
  } else if (now - this->control_output_state_change_time_ < CONTROL_PIN_INACTIVE_DURATION) {
    return;
  }

  switch (this->target_state_) {
    case TARGET_STATE_LOCKED:
      switch (this->internal_state_) {
        case INTERNAL_STATE_LOCKED:
          this->target_state_ = TARGET_STATE_NONE;
          break;
        case INTERNAL_STATE_CLOSED:
          this->set_state_(PHYSICAL_STATE_CLOSED, INTERNAL_STATE_LOCKED);
          this->target_state_ = TARGET_STATE_NONE;
          break;
        case INTERNAL_STATE_UNKNOWN:
        case INTERNAL_STATE_OPENING:
        case INTERNAL_STATE_STOPPED:
        case INTERNAL_STATE_OPEN:
          this->activate_control_output_();
          break;
        case INTERNAL_STATE_MOVING:
        case INTERNAL_STATE_CLOSE_WARNING:
        case INTERNAL_STATE_CLOSING:
        default:
          break;
      }
      break;
    case TARGET_STATE_CLOSED:
      switch (this->internal_state_) {
        case INTERNAL_STATE_CLOSED:
          this->target_state_ = TARGET_STATE_NONE;
          break;
        case INTERNAL_STATE_LOCKED:
          this->set_state_(PHYSICAL_STATE_CLOSED, INTERNAL_STATE_CLOSED);
          this->target_state_ = TARGET_STATE_NONE;
          break;
        case INTERNAL_STATE_UNKNOWN:
        case INTERNAL_STATE_OPENING:
        case INTERNAL_STATE_STOPPED:
        case INTERNAL_STATE_OPEN:
          this->activate_control_output_();
          break;
        case INTERNAL_STATE_MOVING:
        case INTERNAL_STATE_CLOSE_WARNING:
        case INTERNAL_STATE_CLOSING:
        default:
          break;
      }
      break;
    case TARGET_STATE_OPEN:
      switch (this->internal_state_) {
        case INTERNAL_STATE_OPEN:
          this->target_state_ = TARGET_STATE_NONE;
          break;
        case INTERNAL_STATE_UNKNOWN:
        case INTERNAL_STATE_LOCKED:
        case INTERNAL_STATE_CLOSED:
        case INTERNAL_STATE_STOPPED:
        case INTERNAL_STATE_CLOSING:
          this->activate_control_output_();
          break;
        case INTERNAL_STATE_CLOSE_WARNING:
          this->warning_rtttl_->stop();
          if (this->physical_state_ == PHYSICAL_STATE_OPEN) {
            this->set_state_(PHYSICAL_STATE_OPEN, INTERNAL_STATE_OPEN);
            this->target_state_ = TARGET_STATE_NONE;
          } else {
            this->activate_control_output_();
          }
          break;
        case INTERNAL_STATE_MOVING:
        case INTERNAL_STATE_OPENING:
        default:
          break;
      }
      break;
    case TARGET_STATE_STOPPED:
      switch (this->internal_state_) {
        case INTERNAL_STATE_MOVING:
        case INTERNAL_STATE_OPENING:
        case INTERNAL_STATE_CLOSING:
          this->activate_control_output_();
          break;
        case INTERNAL_STATE_CLOSE_WARNING:
          this->warning_rtttl_->stop();
          if (this->physical_state_ == PHYSICAL_STATE_OPEN) {
            this->set_state_(PHYSICAL_STATE_OPEN, INTERNAL_STATE_OPEN);
          } else {
            this->set_state_(this->physical_state_, INTERNAL_STATE_STOPPED);
          }
          this->target_state_ = TARGET_STATE_NONE;
          break;
        case INTERNAL_STATE_UNKNOWN:
        case INTERNAL_STATE_LOCKED:
        case INTERNAL_STATE_CLOSED:
        case INTERNAL_STATE_STOPPED:
        case INTERNAL_STATE_OPEN:
          this->target_state_ = TARGET_STATE_NONE;
          break;
        default:
          break;
      }
      break;
    case TARGET_STATE_POSITION:
      switch (this->internal_state_) {
        case INTERNAL_STATE_UNKNOWN:
        case INTERNAL_STATE_LOCKED:
        case INTERNAL_STATE_CLOSED:
        case INTERNAL_STATE_OPEN:
          this->activate_control_output_();
          break;
        case INTERNAL_STATE_OPENING:
          if (this->is_at_target_position_() || this->position > this->target_position_) {
            this->activate_control_output_();
          }
          break;
        case INTERNAL_STATE_STOPPED:
          if (!this->is_at_target_position_()) {
            this->activate_control_output_();
          } else {
            this->target_state_ = TARGET_STATE_NONE;
          }
          break;
        case INTERNAL_STATE_CLOSE_WARNING:
          if (this->is_at_target_position_()) {
            this->warning_rtttl_->stop();
            this->set_state_(this->physical_state_, INTERNAL_STATE_STOPPED);
            this->target_state_ = TARGET_STATE_NONE;
          } else if (this->position < this->target_position_) {
            this->warning_rtttl_->stop();
            this->activate_control_output_();
          }
          break;
        case INTERNAL_STATE_CLOSING:
          if (this->is_at_target_position_() || this->position < this->target_position_) {
            this->activate_control_output_();
          }
          break;
        case INTERNAL_STATE_MOVING:
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void GarageDoor::activate_control_output_() {
  const uint32_t now = millis();
  this->control_output_->turn_on();
  this->control_output_state_ = true;
  this->control_output_state_change_time_ = now;
  this->last_recompute_time_ = now;
  switch (this->physical_state_) {
    case PHYSICAL_STATE_UNKNOWN:
      this->set_state_(PHYSICAL_STATE_MOVING, INTERNAL_STATE_MOVING);
      break;
    case PHYSICAL_STATE_MOVING:
      this->set_state_(PHYSICAL_STATE_UNKNOWN, INTERNAL_STATE_UNKNOWN);
      break;
    case PHYSICAL_STATE_CLOSED:
      this->set_state_(PHYSICAL_STATE_OPENING, INTERNAL_STATE_OPENING);
      break;
    case PHYSICAL_STATE_OPENING:
      this->set_state_(PHYSICAL_STATE_STOPPED_OPENING, INTERNAL_STATE_STOPPED);
      break;
    case PHYSICAL_STATE_STOPPED_OPENING:
      this->set_state_(PHYSICAL_STATE_CLOSING, INTERNAL_STATE_CLOSING);
      break;
    case PHYSICAL_STATE_OPEN:
      this->set_state_(PHYSICAL_STATE_CLOSING, INTERNAL_STATE_CLOSING);
      break;
    case PHYSICAL_STATE_CLOSING:
      this->set_state_(PHYSICAL_STATE_STOPPED_CLOSING, INTERNAL_STATE_STOPPED);
      break;
    case PHYSICAL_STATE_STOPPED_CLOSING:
      this->set_state_(PHYSICAL_STATE_OPENING, INTERNAL_STATE_OPENING);
      break;
    default:
      break;
  }
}

void GarageDoor::handle_button_press_(bool is_local) {
  switch (this->internal_state_) {
    case INTERNAL_STATE_UNKNOWN:
    case INTERNAL_STATE_OPEN:
      this->target_state_ = TARGET_STATE_CLOSED;
      break;
    case INTERNAL_STATE_MOVING:
    case INTERNAL_STATE_OPENING:
    case INTERNAL_STATE_CLOSE_WARNING:
    case INTERNAL_STATE_CLOSING:
      this->target_state_ = TARGET_STATE_STOPPED;
      break;
    case INTERNAL_STATE_LOCKED:
      if (is_local) {
        this->target_state_ = TARGET_STATE_OPEN;
      }
      break;
    case INTERNAL_STATE_CLOSED:
      this->target_state_ = TARGET_STATE_OPEN;
      break;
    case INTERNAL_STATE_STOPPED:
      if (this->physical_state_ == PHYSICAL_STATE_STOPPED_OPENING) {
        this->target_state_ = TARGET_STATE_CLOSED;
      } else if (this->physical_state_ == PHYSICAL_STATE_STOPPED_CLOSING) {
        this->target_state_ = TARGET_STATE_OPEN;
      }
      break;
    default:
      break;
  }
}

void GarageDoor::set_state_(PhysicalState physical_state, InternalState internal_state, bool is_initial_state) {
  if (is_initial_state) {
    ESP_LOGD(TAG, "Setting Initial Physical State: '%s'", physical_state_names[physical_state]);
    ESP_LOGD(TAG, "Setting Initial Internal State: '%s'", internal_state_names[internal_state]);
  } else if (this->physical_state_ != physical_state || this->internal_state_ != internal_state) {
    if (this->physical_state_ != physical_state) {
      ESP_LOGD(TAG, "Setting Physical State:");
      ESP_LOGD(TAG, "  Current State: '%s'", physical_state_names[this->physical_state_]);
      ESP_LOGD(TAG, "  New State:     '%s'", physical_state_names[physical_state]);
    }
    if (this->internal_state_ != internal_state) {
      ESP_LOGD(TAG, "Setting Internal State:");
      ESP_LOGD(TAG, "  Current State: '%s'", internal_state_names[this->internal_state_]);
      ESP_LOGD(TAG, "  New State:     '%s'", internal_state_names[internal_state]);
    }
  } else {
    return;
  }

  if (this->physical_state_ != physical_state) {
    const uint32_t now = millis();
    if (this->last_open_time_sensor_ != nullptr && this->previous_physical_state_ == PHYSICAL_STATE_CLOSED && physical_state == PHYSICAL_STATE_OPEN) {
      uint32_t open_time = now - this->last_physical_state_change_time_;
      this->last_open_time_sensor_->publish_state(open_time);
    } else if (this->last_close_time_sensor_ != nullptr && this->previous_physical_state_ == PHYSICAL_STATE_OPEN && physical_state == PHYSICAL_STATE_CLOSED) {
      uint32_t close_time = now - this->last_physical_state_change_time_;
      this->last_close_time_sensor_->publish_state(close_time);
    }
    this->previous_physical_state_ = this->physical_state_;
    this->last_physical_state_change_time_ = now;
    this->physical_state_ = physical_state;
  }

  this->internal_state_ = internal_state;

  switch (internal_state) {
    case INTERNAL_STATE_UNKNOWN:
      this->position = 0.5f;
      this->current_operation = COVER_OPERATION_IDLE;
      break;
    case INTERNAL_STATE_MOVING:
      this->position = 0.5f;
      this->current_operation = COVER_OPERATION_CLOSING;
      break;
    case INTERNAL_STATE_LOCKED:
    case INTERNAL_STATE_CLOSED:
      this->position = COVER_CLOSED;
      this->current_operation = COVER_OPERATION_IDLE;
      break;
    case INTERNAL_STATE_OPENING:
      this->current_operation = COVER_OPERATION_OPENING;
      break;
    case INTERNAL_STATE_STOPPED:
      this->current_operation = COVER_OPERATION_IDLE;
      break;
    case INTERNAL_STATE_OPEN:
      this->position = COVER_OPEN;
      this->current_operation = COVER_OPERATION_IDLE;
      break;
    case INTERNAL_STATE_CLOSE_WARNING:
    case INTERNAL_STATE_CLOSING:
      this->current_operation = COVER_OPERATION_CLOSING;
      break;
    default:
      break;
  }

  this->publish_state(false);
  this->lock_comp_->publish_state(internal_state == INTERNAL_STATE_LOCKED ? LOCK_STATE_LOCKED : LOCK_STATE_UNLOCKED);
}

std::string GarageDoor::get_event_(std::string event_type)
{
  std::string event = "esphome.";
  event.append(this->get_object_id());
  event.append(".");
  event.append(event_type);
  return event;
}

}  // namespace cover
}  // namespace esphome
