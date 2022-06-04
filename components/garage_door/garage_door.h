#pragma once

#include <stdio.h>
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/lock/lock.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/rtttl/rtttl.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace cover {

#define PHYSICAL_STATES \
P(UNKNOWN) \
P(MOVING) \
P(CLOSED) \
P(OPENING) \
P(STOPPED_OPENING) \
P(OPEN) \
P(CLOSING) \
P(STOPPED_CLOSING)
#define P(x) PHYSICAL_STATE_##x,
enum PhysicalState { PHYSICAL_STATES P };
#undef P
#define P(x) #x,
const char * const physical_state_names[] = { PHYSICAL_STATES };

#define INTERNAL_STATES \
I(UNKNOWN) \
I(MOVING) \
I(LOCKED) \
I(CLOSED) \
I(OPENING) \
I(STOPPED) \
I(OPEN) \
I(CLOSE_WARNING) \
I(CLOSING)
#define I(x) INTERNAL_STATE_##x,
enum InternalState { INTERNAL_STATES I };
#undef I
#define I(x) #x,
const char * const internal_state_names[] = { INTERNAL_STATES };

enum TargetState {
  TARGET_STATE_NONE,
  TARGET_STATE_LOCKED,
  TARGET_STATE_CLOSED,
  TARGET_STATE_OPEN,
  TARGET_STATE_STOPPED,
  TARGET_STATE_POSITION,
};

enum LocalButton {
  LOCAL_BUTTON_DISCONNECTED,  // The button is not connected resulting in 0 volts at pin A0
  LOCAL_BUTTON_NONE,
  LOCAL_BUTTON_DOOR,
  LOCAL_BUTTON_LOCK,
  LOCAL_BUTTON_LIGHT
};

class GarageDoor;

class GarageDoorLock : public lock::Lock, public Component
{
 public:
  GarageDoorLock(GarageDoor *garage_door) : garage_door_(garage_door) {}
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void loop() override {}

 protected:
  void control(const lock::LockCall &call) override;
  
  GarageDoor *garage_door_{nullptr};
};

class GarageDoor : public cover::Cover, public Component, public api::CustomAPIDevice {
 public:
  GarageDoor();
  void set_name(const std::string &name) { Cover::set_name(name); this->lock_comp_->set_name(name); }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_control_output(output::BinaryOutput *control_output) { this->control_output_ = control_output; }
  void set_button_sensor(sensor::Sensor *button_sensor);
  void set_closed_sensor(binary_sensor::BinarySensor *closed_sensor);
  void set_open_sensor(binary_sensor::BinarySensor *open_sensor);
  void set_remote_sensor(binary_sensor::BinarySensor *remote_sensor);
  void set_remote_light_sensor(binary_sensor::BinarySensor *remote_light_sensor);
  void set_warning_rtttl(rtttl::Rtttl *warning_rtttl);
  void set_close_warning_tones(const std::string &close_warning_tones) { this->close_warning_tones_ = close_warning_tones; }
  void set_last_open_time_sensor(sensor::Sensor *last_open_time_sensor) { this->last_open_time_sensor_ = last_open_time_sensor; }
  void set_last_close_time_sensor(sensor::Sensor *last_close_time_sensor) { this->last_close_time_sensor_ = last_close_time_sensor; }
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;
  void setup() override;
  void loop() override;

 protected:
  friend GarageDoorLock;
  GarageDoorLock *lock_comp_;

  uint32_t open_duration_;
  uint32_t close_duration_;
  output::BinaryOutput *control_output_;
  sensor::Sensor *button_sensor_;
  binary_sensor::BinarySensor *closed_sensor_;
  binary_sensor::BinarySensor *open_sensor_;
  binary_sensor::BinarySensor *remote_sensor_;
  binary_sensor::BinarySensor *remote_light_sensor_;
  rtttl::Rtttl *warning_rtttl_;
  std::string close_warning_tones_;
  sensor::Sensor *last_open_time_sensor_{nullptr};
  sensor::Sensor *last_close_time_sensor_{nullptr};

  PhysicalState previous_physical_state_{PHYSICAL_STATE_UNKNOWN};
  PhysicalState physical_state_{PHYSICAL_STATE_UNKNOWN};
  uint32_t last_physical_state_change_time_{0};
  InternalState internal_state_{INTERNAL_STATE_UNKNOWN};
  TargetState target_state_{TARGET_STATE_NONE};
  float target_position_{0};
  LocalButton last_local_button_{LOCAL_BUTTON_NONE};
  uint32_t last_recompute_time_{0};
  bool control_output_state_{false};
  uint32_t control_output_state_change_time_{0};
  uint32_t last_publish_time_{0};

  void control(const cover::CoverCall &call) override;

  void lock_();
  void unlock_();

  void recompute_position_();
  void ensure_target_state_();

  bool is_at_target_position_() { return (this->target_position_ - 0.05f) <= this->position && this->position <= (this->target_position_ + 0.05f); }
  void activate_control_output_();
  void handle_button_press_(bool is_local);
  void set_state_(PhysicalState physical_state, InternalState internal_state, bool is_initial_state = false);
  std::string get_event_(std::string event_type);
};

}  // namespace cover
}  // namespace esphome
